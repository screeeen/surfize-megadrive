#!/usr/bin/env python3
"""Convierte el arte del proyecto GameMaker (surfize_changes) a recursos de
Mega Drive para rescomp.

El arte original son máscaras: la silueta está en el canal alfa y el RGB es
negro o blanco. GameMaker las dibuja rotadas en tiempo real (image_angle),
cosa que el VDP no puede hacer, así que aquí se generan frames pre-rotados.

Reglas de conversión (ver docs/PORT_AUDIT_GML.md):
  - escala x0.2 exacta: el lienzo de rotación mide cell/0.2 píxeles
  - la rotación es alrededor del ORIGEN del sprite, como en GameMaker, así
    que el origen se coloca en el centro del lienzo y queda en el centro de
    la celda
  - frame k = rumbo 45*k en la convención de SGDK (0 = derecha, 90 = abajo),
    corrigiendo el rumbo natural del arte según la fórmula image_angle de
    cada objeto

Salida: PNG indexados de 16 colores con paleta fija (para que todos los
sprites compartan una línea de paleta) y el header res/gm_art.h.

Uso:  python3 tools/gm2md.py
Requiere: ImageMagick (magick) en el PATH.
"""

import json
import os
import re
import struct
import subprocess
import sys
import zlib

GM = "/Users/miguelangelalvarez/Desktop/___pendireve/game maker move/surfize_changes"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "sprite")
HDR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "gm_art.h")

SCALE = 0.2

# Paleta de sprites (línea PAL1). El índice 0 es el transparente de rescomp.
# Son los colores que el original muestra: silueta negra, detalles blancos de
# la nave y balas amarillas, cuantizados a los 3 bits por componente del VDP.
# El campo de juego es un COLOR PLANO (la capa Compatibility_Colour del room),
# y eso permite la mejor tecnica que hay aqui para la transparencia: el VDP no
# mezcla, pero si el fondo es siempre el mismo se puede calcular el color final
# EXACTO de cada efecto translucido y meterlo en la paleta ya mezclado. Sale
# idéntico al original mientras el efecto esté sobre el fondo, y no cuesta ni
# un ciclo en ejecución.
#
# Los tonos mezclados de abajo salen de componer el arte del original con su
# alfa sobre ROOM_BG y cuantizar al paso de 3 bits del VDP (0, 36, 73, 109,
# 146, 182, 219, 255).
# Los tonos salen del histograma de las capturas del juego publicado,
# cuantizados a los ocho niveles por canal del VDP (0, 36, 73, 109, 146, 182,
# 219, 255). A la izquierda, lo que midio el histograma.
SPRITE_PAL = [
    (255, 0, 255),      # 0  transparente
    (0, 0, 0),          # 1  (7,7,7)       siluetas: araña, gusano, agujero
    (255, 255, 255),    # 2  (255,255,255) nave, oWBlock, HUD
    (255, 255, 0),      # 3  (255,255,0)   oBulletThin
    (73, 73, 73),       # 4  (84,82,82)    humareda, cuerpo
    (0, 0, 255),        # 5  (0,0,253)     azul: sBulletPara y las lineas
    (255, 0, 0),        # 6  rojo opaco: sprLaser y spr3way
    (146, 146, 146),    # 7  (147,147,148) humareda, parte clara
    (36, 36, 36),       # 8  borde de la humareda contra el fondo
    (36, 73, 109),      # 9  (24,85,94)    teal oscuro
    (146, 0, 0),        # 10 (128,0,0)     rojo del agujero activo
    (219, 219, 219),    # 11 (195,195,195) gris claro / fogonazo
    (0, 255, 255),      # 12 (0,255,255)   cian del paquete
    (0, 146, 146),      # 13 (13,162,162)  cian medio
    (73, 36, 36),       # 14 (79,21,21)    rojo oscuro del borde
    (36, 36, 0),        # 15 (51,42,15)    oliva oscuro
]

# Paleta del plano A (línea PAL2): los agujeros y todo lo que espera en el
# suelo. Estaba con cuatro grises, y por eso los paquetes y los power-ups
# salían como manchas oscuras que parecían agujeros de más. En el original:
#
#   oAgujerSuper  arte casi negro (7,7,7) con alfa propia, tintado con c_gray
#                 (que MULTIPLICA por 0,5) -> un agujero oscuro de borde suave
#   oPaquete      sprPaquete es CIAN (0,255,255) con un reborde casi blanco
#   oBoomPowUp    sprCruz, casi blanco (232)
#   oNaveReplica  sprWeapon4, AZUL (0,0,255)
#   oRayosPowUp   sprWeapon7, AZUL y AMARILLO
#
# Los tonos intermedios salen de componer cada arte con su alfa sobre el
# color de fondo del room, igual que en la paleta de sprites.
HOLE_PAL = [
    (255, 0, 255),      # 0  transparente
    (0, 0, 0),          # 1  núcleo del agujero
    (36, 36, 36),       # 2  agujero contra el fondo
    (73, 73, 73),       # 3  gris medio
    (146, 0, 0),        # 4  rojo del agujero por el que sale un gusano
    (73, 36, 36),       # 5  rojo oscuro del borde y los goterones
    (0, 255, 255),      # 6  cian de sprPaquete
    (0, 146, 146),      # 7  cian medio
    (255, 255, 255),    # 8  blanco
    (219, 219, 219),    # 9  blanco roto (sprCruz)
    (0, 0, 255),        # 10 azul (sprWeapon4 y sprWeapon7)
    (255, 255, 0),      # 11 amarillo (sprWeapon7)
    (146, 146, 146),    # 12 gris claro (c_gray de la barra del menú)
    (109, 109, 109),    # 13 gris
]

# Color de fondo del campo.
#
# El .yyp de surfize_changes trae la capa Compatibility_Colour en 0xFF9A9A75
# (ABGR), o sea un teal (117,154,154), y eso es lo que usaba este port. Pero
# las capturas del juego PUBLICADO que ha pasado el autor dicen otra cosa: el
# fondo es un GRIS OSCURO (51,51,51), con una textura de grietas encima que
# apenas contrasta (45,45,45, siete niveles de diferencia).
#
# O sea que el proyecto GameMaker que tengo no es la version que se publico.
# Mandan las capturas, que es lo que el autor llama "el original".
#
# La textura no se porta: siete niveles de 255 quedan muy por debajo del paso
# de 36 que da la rejilla de 3 bits del VDP, asi que en Mega Drive la grieta o
# no se ve o se ve como una mancha. Fondo liso.
ROOM_BG = (51, 51, 51)


def yy(sprite):
    path = os.path.join(GM, "sprites", sprite, sprite + ".yy")
    return json.loads(re.sub(r",(\s*[}\]])", r"\1", open(path).read()))


def frame_files(sprite):
    d = yy(sprite)
    return [os.path.join(GM, "sprites", sprite, f["name"] + ".png") for f in d["frames"]]


def origin(sprite):
    # El origen vive en sequence.xorigin/yorigin en el formato de GMS 2.3.
    d = yy(sprite)
    sq = d["sequence"]
    return sq["xorigin"], sq["yorigin"], d["width"], d["height"]


def render(src, ox, oy, canvas, cell, rot_cw):
    """Devuelve los píxeles RGBA (bytes) de un frame de cell x cell."""
    dx = canvas // 2 - ox
    dy = canvas // 2 - oy
    cmd = [
        "magick", "-size", f"{canvas}x{canvas}", "xc:none",
        src, "-geometry", f"{dx:+d}{dy:+d}", "-composite",
        "-background", "none", "-rotate", str(rot_cw),
        "-gravity", "center", "-extent", f"{canvas}x{canvas}",
        "-filter", "Box", "-resize", f"{cell}x{cell}!",
        "-depth", "8", "RGBA:-",
    ]
    out = subprocess.run(cmd, capture_output=True, check=True).stdout
    if len(out) != cell * cell * 4:
        raise RuntimeError(f"{src}: esperaba {cell*cell*4} bytes, llegaron {len(out)}")
    return out


# Umbral de alfa al indexar. NO es 128 (la mitad) a proposito: el arte viene
# de un room de 1600x1200 y se reduce 5:1, asi que un detalle de un pixel del
# original queda en alfa 51 y con el umbral a la mitad desaparecia. Medido
# frame a frame: con 128 la nave salia de 8x8 cuando el original a escala mide
# 8x12, y la araña de 7x8 cuando mide 13x13 -- la mitad del bicho se perdia.
# Con 48 (19 % de cobertura) las siluetas recuperan su tamaño.
ALPHA_CUT = 48


def to_indices(rgba, cell, pal, cut=ALPHA_CUT):
    """Mapea RGBA a índices de paleta: alfa < cut -> 0, resto al más cercano."""
    idx = bytearray(cell * cell)
    for i in range(cell * cell):
        r, g, b, a = rgba[i * 4:i * 4 + 4]
        if a < cut:
            idx[i] = 0
            continue
        best, bd = 1, None
        for p in range(1, len(pal)):
            pr, pg, pb = pal[p]
            d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
            if bd is None or d < bd:
                bd, best = d, p
        idx[i] = best
    return idx


def write_png(path, width, height, indices, pal):
    """PNG indexado de 8 bits con paleta de 16 entradas, índice 0 transparente."""
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    full = list(pal) + [(0, 0, 0)] * (16 - len(pal))
    plte = b"".join(bytes(c) for c in full)
    trns = bytes([0] + [255] * 15)

    raw = bytearray()
    for y in range(height):
        raw.append(0)                                   # filtro None
        raw += indices[y * width:(y + 1) * width]

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", plte)
    png += chunk(b"tRNS", trns)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


# Sombras (no salen del original, las pidio el autor el 2026-11-12).
#
# Van PINTADAS en cada frame, no como sprites aparte: una sombra por objeto
# serian hasta 60 sprites mas (los eslabones del gusano), y el VDP no pasa de
# 80 por frame ni de 20 por linea. Tampoco con shadow/highlight: ese modo
# oscurece todo lo que esta en planos de prioridad baja (el fondo, los
# agujeros) y para esquivarlo habria que subir de prioridad el fondo o los
# sprites, lo que rompe el orden HUD > sprites > fondo y la banda del menu.
#
# Es la silueta del frame ya girado, desplazada abajo a la derecha (la luz
# viene de arriba a la izquierda, igual para todas las rotaciones) y en negro
# tramado al 50 %: tramada se lee como sombra sobre cualquier fondo y no
# engorda las siluetas, que en las arañas y el gusano ya son negras. Solo
# ocupa pixeles transparentes del frame, asi que nunca tapa al objeto.
SHADOW_INDEX = 1                    # negro de SPRITE_PAL
# Desplazamiento de la sombra. La nave (y sus replicas) vuela: 5 px, lo
# maximo que cabe en la celda de 24 sin recortar la sombra en ninguna de las
# 32 rotaciones (los pixeles de la nave llegan a la columna y la fila 18). Los
# bichos van por el suelo: 2 px.
SHADOW_AIR = (5, 5)
SHADOW_GROUND = (2, 2)


def add_shadow(idx, cell, dx, dy):
    out = bytearray(idx)
    for y in range(cell):
        for x in range(cell):
            if idx[y * cell + x]:
                continue
            sx, sy = x - dx, y - dy
            if sx < 0 or sy < 0 or sx >= cell or sy >= cell:
                continue
            if idx[sy * cell + sx] and ((x + y) & 1) == 0:
                out[y * cell + x] = SHADOW_INDEX
    return out


# Hojas recortadas: nombre -> lista de (dx, dy) por frame, en el orden
# rotacion * columnas + columna. write_header las escribe en gm_art.h.
TRIM_OFFSETS = {}


def trim_window(idx, cell, out):
    """Ventana de out x out que contiene todos los pixeles del frame.

    Devuelve (x0, y0). Revienta si el dibujo no cabe: eso seria recortar arte.
    """
    xs = [i % cell for i in range(cell * cell) if idx[i]]
    ys = [i // cell for i in range(cell * cell) if idx[i]]
    if not xs:
        return (cell - out) // 2, (cell - out) // 2
    res = []
    for lo, hi in ((min(xs), max(xs)), (min(ys), max(ys))):
        if hi - lo + 1 > out:
            raise RuntimeError(f"el frame mide {hi - lo + 1} px y no cabe en {out}")
        v = (lo + hi) // 2 - (out // 2 - 1)
        v = max(hi - (out - 1), min(lo, v))
        v = max(0, min(cell - out, v))
        res.append(v)
    return res[0], res[1]


def sheet(name, sprite, cell, natural, rotations=8, pal=SPRITE_PAL, frame_limit=None,
          shadow=None, trim=None):
    """Genera una hoja de frames: columnas = animación, filas = rotación.

    shadow=(dx, dy) pinta la sombra tramada desplazada (ver add_shadow).

    trim=N recorta cada frame a N x N alrededor de su dibujo (sombra incluida)
    y apunta en TRIM_OFFSETS[name] dónde queda la esquina respecto al centro
    de la celda original. Va por los límites del VDP por línea: un sprite de
    24 px son 3 celdas de las 40 que caben en una línea, y uno de 16, 2 (ver
    docs/PORT_AUDIT_GML.md, "Sprites que desaparecen")."""
    ox, oy, w, h = origin(sprite)
    canvas = int(round(cell / SCALE))
    files = frame_files(sprite)
    if frame_limit:
        files = files[:frame_limit]
    anim = len(files)

    # Corrección del rumbo natural del arte, de la fórmula image_angle del
    # objeto: "up" sale de image_angle = direction - 90, "right" de
    # image_angle = direction, "down" de image_angle = direction + 90.
    offset = {"up": 90, "right": 0, "down": -90}[natural]

    cols, rows = anim, rotations
    oc = trim or cell
    W, H = cols * oc, rows * oc
    out = bytearray(W * H)
    offs = []

    for r in range(rows):
        # Paso de giro = 360 / rotaciones (45 grados con 8, 11,25 con 32).
        rot = ((360.0 / rotations) * r + offset) % 360
        for c in range(cols):
            idx = to_indices(render(files[c], ox, oy, canvas, cell, rot), cell, pal)
            if shadow:
                idx = add_shadow(idx, cell, *shadow)
            x0, y0 = trim_window(idx, cell, oc) if trim else (0, 0)
            offs.append((x0 - cell // 2, y0 - cell // 2))
            for y in range(oc):
                dst = (r * oc + y) * W + c * oc
                src = (y0 + y) * cell + x0
                out[dst:dst + oc] = idx[src:src + oc]

    if trim:
        TRIM_OFFSETS[name] = offs
    path = os.path.join(OUT, name + ".png")
    write_png(path, W, H, out, pal)
    print(f"  {name}.png  {W}x{H}  {cols}x{rows} frames de {oc}x{oc}  "
          f"({oc//8}x{oc//8} tiles" + (", recortados" if trim else ", origen en el centro") + ")")
    return cols, rows


def hole_tiles():
    """El agujero como bloque de tiles del plano A.

    oAgujerSuper lo dibuja con draw_sprite_ext(..., 1.3, 1.3, 0, c_gray, 1) y
    con image_index 0 fijo (image_speed = 0). 197*1.3*0.2 = 51 px, en un
    bloque de 8x8 tiles (64x64) con el origen en el centro: el arte se
    extiende 29 px por debajo del origen y en 7x7 tiles se recortaba.
    """
    ox, oy, w, h = origin("sprAgujero")
    src = frame_files("sprAgujero")[0]
    cell = 64
    # El dibujado a escala 1.3 equivale a escalar el lienzo por 1.3.
    canvas = int(round(cell / SCALE / 1.3))

    # El arte lleva su propia alfa (0,6 de media) y los goterones del borde
    # casi no tapan: componiendolo sobre el fondo del room, en vez de
    # binarizar, el agujero recupera su borde fundido en vez de ser un disco
    # negro recortado.
    def build(bg):
        return subprocess.run([
            "magick", "-size", f"{canvas}x{canvas}", bg,
            "(", src, "-channel", "RGB", "-evaluate", "multiply", "0.5", "+channel", ")",
            "-geometry", f"{canvas//2-ox:+d}{canvas//2-oy:+d}", "-composite",
            "-filter", "Box", "-resize", f"{cell}x{cell}!",
            "-depth", "8", "RGBA:-",
        ], capture_output=True, check=True).stdout

    def quant(color, cover):
        idx = bytearray(cell * cell)
        for i in range(cell * cell):
            if cover[i * 4 + 3] < ALPHA_CUT:
                continue
            r, g, b = color[i * 4:i * 4 + 3]
            best, bd = 1, None
            for pi in range(1, len(HOLE_PAL)):
                pr, pg, pb = HOLE_PAL[pi]
                d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
                if bd is None or d < bd:
                    bd, best = d, pi
            idx[i] = best
        return idx

    idx = quant(build("xc:rgb(%d,%d,%d)" % ROOM_BG), build("xc:none"))
    write_png(os.path.join(OUT, "hole.png"), cell, cell, idx, HOLE_PAL)
    print(f"  hole.png  {cell}x{cell}  ({cell//8}x{cell//8} tiles)")

    # Variante "activa". oAgujerSuper/Draw_0 tiene, antes del agujero normal:
    #
    #   if gus draw_sprite_ext(sprite_index, image_index,
    #                          x + choose(-8,8), y + choose(-8,8), 1, 1, 0,
    #                          choose(c_white,c_red,c_yellow), 0.8)
    #
    # o sea una copia del mismo agujero, desplazada y tintada, asomando por
    # detras. En las capturas del juego publicado se ve clarisimo: el agujero
    # por el que esta saliendo un gusano tiene un reborde rojo y los goterones
    # rojos. La linea que pone gus a 1 esta comentada en el .yyp que tengo,
    # otra senal de que la version publicada es otra.
    def build_red(bg):
        return subprocess.run([
            "magick", "-size", f"{canvas}x{canvas}", bg,
            # la copia roja, un pelin mas grande, por detras
            "(", src, "-filter", "Box", "-resize", "112%",
                 "-fill", "rgb(146,0,0)", "-colorize", "100", ")",
            "-gravity", "center", "-composite",
            "-gravity", "northwest",
            "(", src, "-channel", "RGB", "-evaluate", "multiply", "0.5", "+channel", ")",
            "-geometry", f"{canvas//2-ox:+d}{canvas//2-oy:+d}", "-composite",
            "-filter", "Box", "-resize", f"{cell}x{cell}!",
            "-depth", "8", "RGBA:-",
        ], capture_output=True, check=True).stdout

    red = quant(build_red("xc:rgb(%d,%d,%d)" % ROOM_BG), build_red("xc:none"))
    write_png(os.path.join(OUT, "hole_red.png"), cell, cell, red, HOLE_PAL)
    print(f"  hole_red.png  {cell}x{cell}  (variante con gusano saliendo)")


def art_center(sprite):
    """Centro del dibujo (bbox) respecto al ORIGEN, en píxeles del room."""
    d = yy(sprite)
    sq = d["sequence"]
    cx = (d["bbox_left"] + d["bbox_right"]) / 2.0 - sq["xorigin"]
    cy = (d["bbox_top"] + d["bbox_bottom"]) / 2.0 - sq["yorigin"]
    return cx, cy


def centered_frame(src, sprite, cell, rot_cw):
    """Frame con el DIBUJO centrado en la celda (no el origen).

    Para sprites cuyo origen está lejos del dibujo (sBullet tiene el origen en
    la punta de un bloque de 96x116), centrar el origen obligaría a celdas de
    56x56. Centrando el dibujo basta con 24x24, y el desplazamiento que eso
    introduce se devuelve para que el código lo aplique al colocar el sprite:
    como el original rota alrededor del origen, ese desplazamiento gira con el
    sprite, de ahí que haya uno por rotación.
    """
    ox, oy, w, h = origin(sprite)
    cx, cy = art_center(sprite)
    canvas = int(round(cell / SCALE))
    # el centro del dibujo va al centro del lienzo
    dx = canvas // 2 - int(round(ox + cx))
    dy = canvas // 2 - int(round(oy + cy))
    cmd = [
        "magick", "-size", f"{canvas}x{canvas}", "xc:none",
        src, "-geometry", f"{dx:+d}{dy:+d}", "-composite",
        "-background", "none", "-rotate", str(rot_cw),
        "-gravity", "center", "-extent", f"{canvas}x{canvas}",
        "-filter", "Box", "-resize", f"{cell}x{cell}!",
        "-depth", "8", "RGBA:-",
    ]
    out = subprocess.run(cmd, capture_output=True, check=True).stdout
    # desplazamiento del centro de la celda respecto al origen, ya girado
    import math
    a = math.radians(rot_cw)
    rx = (cx * math.cos(a) - cy * math.sin(a)) * SCALE
    ry = (cx * math.sin(a) + cy * math.cos(a)) * SCALE
    return out, int(round(rx)), int(round(ry))


# Hoja única para todo lo que dispara el jugador: una fila por arma y
# rotación, que es como rescomp genera las animaciones. Así los 14 objetos
# del pool comparten una sola definición y pueden ser de cualquier arma.
PROJECTILES = [
    ("THIN",   "sBulletThin", "down",  8),   # oBulletThin
    ("WBLOCK", "sBullet",     "up",    8),   # oWBlock
    ("THREEW", "spr3way",     "up",    8),   # o3way
    ("LASER",  "sprLaser",    "up",    8),   # oLaser
    ("PARA",   "sBulletPara", "up",    8),   # oBulletPara
    ("BOMB",   "sprWeapon6",  "right", 1),   # oBomba, no rota
    ("OPTION", "sprExplosion", "right", 1),  # oOption, satélite del PIVOT
    ("TRI",    "sprite173",   "up",    8),   # oTri, nave réplica
]
PROJ_CELL = 24


def projectile_sheet():
    cell = PROJ_CELL
    rows = []
    offsets = []
    for name, sprite, natural, rots in PROJECTILES:
        src = frame_files(sprite)[0]
        base = {"up": 90, "right": 0, "down": -90}[natural]
        for r in range(rots):
            rgba, rx, ry = centered_frame(src, sprite, cell, (45 * r + base) % 360)
            if name == "TRI":
                # oTri/Draw_0: draw_sprite_ext(..., c_blue, 0.6). La nave
                # replica es el arte de la nave TEÑIDO DE AZUL: el tinte de
                # GameMaker multiplica el color, asi que el blanco sale azul y
                # los grises, azul oscuro. Sin el tinte la replica salia blanca,
                # igual que el jugador, y tras morir parecia que la nave del
                # jugador seguia en el campo. La transparencia del 0.6 no se
                # porta (el VDP no mezcla colores).
                rgba = bytes(v if (k % 4) in (2, 3) else 0 for k, v in enumerate(rgba))
            idx = to_indices(rgba, cell, SPRITE_PAL)
            if name == "TRI":
                idx = add_shadow(idx, cell, *SHADOW_AIR)
            rows.append(idx)
            offsets.append((rx, ry))

    W, H = cell, cell * len(rows)
    out = bytearray(W * H)
    for i, idx in enumerate(rows):
        out[i * cell * W:(i + 1) * cell * W] = idx
    write_png(os.path.join(OUT, "projectiles.png"), W, H, out, SPRITE_PAL)
    print(f"  projectiles.png  {W}x{H}  {len(rows)} animaciones de {cell}x{cell}")
    return offsets


# Los cuatro objetos que esperan en el suelo. Son estáticos en el original
# (sólo laten o giran al dibujarse), así que van al plano A como tiles y no
# gastan sprites: así caben los 24 paquetes del tope original.
GROUND = [
    ("PACK",    "sprPaquete"),      # oPaquete
    ("BOOM",    "sprCruz"),         # oBoomPowUp
    ("REPLICA", "sprWeapon4"),      # oNaveReplicaPowUp
    ("RAYS",    "sprWeapon7"),      # oRayosPowUp
]


# oExplosion/Draw_0 dibuja el sprite con escala t, que crece de 0 a 6 y a los
# ~4 frames destruye la instancia. El VDP no escala sprites, asi que se
# generan frames pre-escalados.
#
# Pero el fogonazo no es lo que se ve de una muerte en el original: oExplosion
# /Create_0 crea ademas un oca (polvo), que vive 40 frames (0,67 s) y dibuja
# expl_4 (206x206) y expl_1 (128x128) superpuestos a escala random(3) --
# una humareda oscura de hasta 124 px en pantalla. Sin ella la muerte es un
# parpadeo de 80 ms que no se ve. Ver docs/PORT_AUDIT_GML.md.
#
# La hoja lleva las dos cosas, una fila (= animacion de rescomp) por cada una:
#   fila 0: fogonazo, 5 escalas crecientes de sprExplosion
#   filas 1-3: humareda, 5 frames de animacion, una fila por tamano
EXPLOSION_CELL = 64                      # 8x8 tiles -> 4 sprites de hardware
EXPLOSION_SCALES = [1, 2, 3, 4, 5]       # escala t del fogonazo

# size = random(30*0.1) en oca/Create_0: uniforme 0..3. La celda de 64 px topa
# la escala en 64/(206*0.2) = 1,55, asi que se reparten tres tamanos hasta ese
# tope en vez del rango completo (desviacion documentada en el informe).
DUST_SCALES = [0.5, 1.0, 1.55]
DUST_FRAMES = [1, 2, 3, 4, 5]            # el frame 0 de expl_4 esta vacio


def _cell_from_rgba(rgba, cell, pal):
    return to_indices(rgba, cell, pal)


def blended(layers, cell, canvas, cut=ALPHA_CUT, pal=None):
    """Compone capas translucidas sobre el fondo del room y devuelve indices.

    Cada capa es (fichero, escala_en_px, alfa, tinte_rgb_o_None), igual que el
    draw_sprite_ext del original. Se compone DOS veces: una sobre el color de
    fondo, que da el color final exacto que se veria en pantalla, y otra sobre
    transparente, que da la cobertura y por tanto donde el sprite existe.
    """
    def build(bg):
        args = ["magick", "-size", f"{canvas}x{canvas}", bg, "-gravity", "center"]
        for src, size, alpha, tint in layers:
            args += ["(", src]
            if tint is not None:
                args += ["-fill", "rgb(%d,%d,%d)" % tint, "-colorize", "100"]
            args += ["-filter", "Box", "-resize", f"{size}x{size}!"]
            if alpha < 1.0:
                args += ["-channel", "A", "-evaluate", "multiply", str(alpha), "+channel"]
            args += [")", "-composite"]
        args += ["-filter", "Box", "-resize", f"{cell}x{cell}!", "-depth", "8", "RGBA:-"]
        return subprocess.run(args, capture_output=True, check=True).stdout

    if pal is None:
        pal = SPRITE_PAL

    color = build("xc:rgb(%d,%d,%d)" % ROOM_BG)
    cover = build("xc:none")

    idx = bytearray(cell * cell)
    for i in range(cell * cell):
        if cover[i * 4 + 3] < cut:
            continue
        r, g, b = color[i * 4:i * 4 + 3]
        best, bd = 1, None
        for pi in range(1, len(pal)):
            pr, pg, pb = pal[pi]
            d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
            if bd is None or d < bd:
                bd, best = d, pi
        idx[i] = best
    return idx


def _flash_frames(cell, canvas):
    """oExplosion/Draw_0: sprExplosion a escala t, con alfa 1 de dibujo pero
    0,82 de arte, asi que en pantalla no es blanco puro."""
    _, _, w, h = origin("sprExplosion")
    src = frame_files("sprExplosion")[0]
    return [blended([(src, w * sc, 1.0, None)], cell, canvas)
            for sc in EXPLOSION_SCALES]


def _dust_frames(cell, canvas, scale):
    """oca/Draw_0: expl_4 y encima expl_1, mismo centro, misma escala, LOS DOS
    A ALFA 0,8. Esa transparencia es lo que hacia que la humareda del original
    se viera como humo y no como un disco: compuesta sobre el fondo da tonos
    de (51,59,59) a (66,73,73), que son los indices 7, 4 y 8 de la paleta."""
    f4 = frame_files("expl_4")
    f1 = frame_files("expl_1")
    _, _, w4, _ = origin("expl_4")
    _, _, w1, _ = origin("expl_1")
    frames = []
    for i in DUST_FRAMES:
        s4 = max(1, int(round(w4 * scale)))
        s1 = max(1, int(round(w1 * scale)))
        frames.append(blended([(f4[i], s4, 0.8, None),
                               (f1[i], s1, 0.8, None)], cell, canvas))
    return frames


def explosion_sheet():
    cell = EXPLOSION_CELL
    canvas = int(round(cell / SCALE))
    rows = [_flash_frames(cell, canvas)]
    for sc in DUST_SCALES:
        rows.append(_dust_frames(cell, canvas, sc))

    cols = max(len(r) for r in rows)
    W, H = cell * cols, cell * len(rows)
    out = bytearray(W * H)
    for r, row in enumerate(rows):
        for c, idx in enumerate(row):
            for y in range(cell):
                dst = (r * cell + y) * W + c * cell
                out[dst:dst + cell] = idx[y * cell:(y + 1) * cell]

    write_png(os.path.join(OUT, "explosion.png"), W, H, out, SPRITE_PAL)
    for r, row in enumerate(rows):
        counts = [sum(1 for v in f if v) for f in row]
        print(f"  explosion.png fila {r}: {counts}")
    print(f"  explosion.png  {W}x{H}  {len(rows)} animaciones de {cols} frames de {cell}x{cell}")


# ---------------------------------------------------------------------------
# oDestro: la metralla de oEstallido
#
# oDestro/Draw_0 dibuja sprDebri (8x8) con
#     draw_sprite_ext(..., random(8), random(8), 0, choose(c_dkgray,c_black), 1)
# o sea, tamano nuevo cada frame entre 0 y 8 veces el original, en gris oscuro
# o en negro. A escala x0,2 eso es de 0 a 12,8 px.
#
# El VDP no escala ni tinta, asi que se generan los dos colores (una animacion
# cada uno) con cuatro tamanos, y el juego sortea frame y color en cada frame
# igual que el original.
# ---------------------------------------------------------------------------
DEBRIS_CELL = 16
DEBRIS_SCALES = [2, 4, 6, 8]
DEBRIS_COLORS = [(73, 73, 73), (0, 0, 0)]       # c_dkgray, c_black


def debris_sheet():
    cell = DEBRIS_CELL
    canvas = int(round(cell / SCALE))
    src = frame_files("sprDebri")[0]
    ox, oy, w, h = origin("sprDebri")

    # sprDebri es blanco con 0,5 de alfa, y oDestro lo tinta de gris oscuro o
    # de negro: sobre el fondo eso da dos grises, no un negro plano.
    rows = []
    for tint in DEBRIS_COLORS:
        rows.append([blended([(src, w * sc, 1.0, tint)], cell, canvas)
                     for sc in DEBRIS_SCALES])

    _write_rows("debris.png", rows, cell)


# ---------------------------------------------------------------------------
# oMuerte: el fogonazo de la muerte del jugador
#
# oMuerte/Draw_0 dibuja sprExplosion_2 a escala t*8 con t de 1 a 20, o sea que
# arranca en 51 px de pantalla y a los cuatro frames ya tapa la pantalla
# entera. Aqui van los tres primeros tamanos como sprite; a partir de ahi el
# juego pone el fondo en blanco, que es lo que se ve en el original (ver
# src/effect.c y el informe).
# ---------------------------------------------------------------------------
MUERTE_CELL = 64
MUERTE_SCALES = [4, 8, 14]
# El arte de sprExplosion_2 es casi blanco (85-92 % de gris) pero con un alfa
# de 0,03 a 0,56: en GameMaker es un velo translucido que crece. El VDP no
# mezcla, asi que el alfa se binariza con un umbral bajo y lo que queda es la
# silueta en blanco solido.
MUERTE_ALPHA_CUT = "4%"
# De los seis frames de sprExplosion_2 el 4 es el que mas cubre una vez
# binarizado el alfa (88 % de la celda a escala 14); los otros dejan la
# silueta casi hueca y el fogonazo no se lee.
MUERTE_FRAME_IDX = [4, 4, 4]


def muerte_sheet():
    cell = MUERTE_CELL
    canvas = int(round(cell / SCALE))
    files = frame_files("sprExplosion_2")
    ox, oy, w, h = origin("sprExplosion_2")

    frames = []
    for i, sc in enumerate(MUERTE_SCALES):
        src = files[MUERTE_FRAME_IDX[i] % len(files)]
        cmd = [
            "magick", "-size", f"{canvas}x{canvas}", "xc:none", "-gravity", "center",
            "(", src, "-channel", "A", "-threshold", MUERTE_ALPHA_CUT, "+channel",
                 "-fill", "white", "-colorize", "100",
                 "-filter", "Box", "-resize", f"{w*sc}x{h*sc}!", ")", "-composite",
            "-filter", "Box", "-resize", f"{cell}x{cell}!",
            "-depth", "8", "RGBA:-",
        ]
        rgba = subprocess.run(cmd, capture_output=True, check=True).stdout
        frames.append(to_indices(rgba, cell, SPRITE_PAL))

    _write_rows("muerte.png", [frames], cell)


# ---------------------------------------------------------------------------
# object1: el haz del arma PARA
#
# script0(1) lo crea mientras haya municion y object1/Draw_0 encadena 14
# copias de sprite0 (40x16) a lo largo del rumbo de la nave, separadas por la
# altura del sprite, girando con image_angle y cambiando de color cada frame
# entre azul, rojo y gris. Aqui una animacion por rotacion y dos frames por
# color; el gris se deja fuera porque es el que menos se ve.
# ---------------------------------------------------------------------------
BEAM_CELL = 16
BEAM_COLORS = [(0, 0, 255), (255, 0, 0)]


def beam_sheet():
    cell = BEAM_CELL
    canvas = int(round(cell / SCALE))
    src = frame_files("sprite0")[0]
    ox, oy, w, h = origin("sprite0")

    rows = []
    for rot in range(8):
        # image_angle = oPlayer.image_angle, y la nave apunta hacia arriba:
        # misma convencion que el resto de las hojas (ver sheet()).
        deg = (rot * 45)
        frames = []
        for (cr, cg, cb) in BEAM_COLORS:
            # Hay que girar ANTES de mezclar, asi que la composicion sobre el
            # fondo se hace aqui a mano en vez de con blended().
            def shot(bg):
                return subprocess.run([
                    "magick", "-size", f"{canvas}x{canvas}", bg, "-gravity", "center",
                    "(", src, "-fill", f"rgb({cr},{cg},{cb})", "-colorize", "100",
                         "-background", "none", "-rotate", str(-deg), ")",
                    "-composite",
                    "-filter", "Box", "-resize", f"{cell}x{cell}!",
                    "-depth", "8", "RGBA:-",
                ], capture_output=True, check=True).stdout

            color = shot("xc:rgb(%d,%d,%d)" % ROOM_BG)
            cover = shot("xc:none")
            idx = bytearray(cell * cell)
            for i in range(cell * cell):
                if cover[i * 4 + 3] < ALPHA_CUT:
                    continue
                r, g, b = color[i * 4:i * 4 + 3]
                best, bd = 1, None
                for pi in range(1, len(SPRITE_PAL)):
                    pr, pg, pb = SPRITE_PAL[pi]
                    d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
                    if bd is None or d < bd:
                        bd, best = d, pi
                idx[i] = best
            frames.append(idx)
        rows.append(frames)

    _write_rows("beam.png", rows, cell)


# ---------------------------------------------------------------------------
# oPuntoMorte: el numero que sale volando hacia el HUD
#
# Lo dibuja con draw_text_transformed_color a escala 0,4 con global.myFont,
# que es sFont2 (glifos de 80x80). 80 * 0,4 * 0,2 = 6,4 px: una celda de 8.
# ---------------------------------------------------------------------------
DIGIT_CELL = 8


def digits_sheet():
    """Los diez digitos de sFont2, una animacion cada uno."""
    cell = DIGIT_CELL
    files = frame_files("sFont2")
    ox, oy, w, h = origin("sFont2")
    # font_add_sprite(sFont2, ord(" "), ...): el frame 0 es el espacio (32).
    first = ord("0") - ord(" ")

    rows = []
    for d in range(10):
        src = files[first + d]
        cmd = [
            "magick", src, "-background", "none", "-gravity", "center",
            "-filter", "Box", "-resize", f"{cell}x{cell}!",
            "-depth", "8", "RGBA:-",
        ]
        rgba = subprocess.run(cmd, capture_output=True, check=True).stdout
        rows.append([to_indices(rgba, cell, SPRITE_PAL)])

    _write_rows("digits.png", rows, cell)


# ---------------------------------------------------------------------------
# oPlayer/Draw_0: draw_line_color(x, y, oTri.x, oTri.y, c_blue, c_blue)
#
# La nave queda unida a cada replica por una linea azul. El VDP no dibuja
# lineas, asi que se encadenan puntos: este es el punto.
# ---------------------------------------------------------------------------
# Y oRay/Draw_0 hace lo mismo pero con rayos del jugador a CADA agujero:
#
#   draw_set_color(choose(c_blue,c_yellow,c_silver))
#   for (cada oAgujerSuper) { draw_curve(oPlayer.x,oPlayer.y, g.x,g.y, ...) x3 }
#
# o sea que mientras arde la mecha del rayo la pantalla se llena de descargas
# que cambian de color cada frame. Un punto por color: azul para la linea a
# las replicas y los tres para el rayo.
DOT_CELL = 8
DOT_COLORS = [5, 11, 3]     # azul, plata (el 219), amarillo


def dot_sheet():
    cell = DOT_CELL
    rows = []
    for ci in DOT_COLORS:
        idx = bytearray(cell * cell)
        for y in range(cell // 2 - 1, cell // 2 + 2):
            for x in range(cell // 2 - 1, cell // 2 + 2):
                idx[y * cell + x] = ci
        rows.append([idx])
    _write_rows("dot.png", rows, cell)


# ---------------------------------------------------------------------------
# Fuente del HUD
#
# oInGameHUD dibuja BULLETS y KILLS con global.myFont, que es
# font_add_sprite(sFont2, ord(" "), ...): glifos de 80x80 del room, o sea 16
# px en pantalla. El port los pintaba con la fuente de 8 px del sistema, la
# MITAD de alto. Medido sobre las capturas del juego publicado: 36 px de alto
# en una captura de 800x600, que son 14,4 px en 320x240.
#
# Solo se generan las letras que el HUD usa de verdad (BULLETS, KILLS, LAST) y
# los digitos: 19 glifos de 2x2 tiles son 76 tiles, y de la fuente entera no
# habria sitio en VRAM.
HUD_FONT_CELL = 16
# Las letras de BULLETS/KILLS del HUD y las de KILLS/TOP/NEXT del menú, que
# oMenu/Draw_0 también pinta con global.myFont (sFont2). "re" es el NEXT del
# final, cuando ya está todo desbloqueado (minúsculas en la hoja de sFont2).
HUD_CHARS = "BULETSKIA0123456789OPNXre"


def hud_font():
    cell = HUD_FONT_CELL
    files = frame_files("sFont2")
    first = ord(" ")
    glyphs = []
    for ch in HUD_CHARS:
        src = files[ord(ch) - first]
        cmd = ["magick", src, "-background", "none", "-gravity", "center",
               "-filter", "Box", "-resize", f"{cell}x{cell}!",
               "-depth", "8", "RGBA:-"]
        rgba = subprocess.run(cmd, capture_output=True, check=True).stdout
        glyphs.append(to_indices(rgba, cell, HOLE_PAL))

    W, H = cell * len(glyphs), cell
    out = bytearray(W * H)
    for g, idx in enumerate(glyphs):
        for y in range(cell):
            out[y * W + g * cell:y * W + (g + 1) * cell] = idx[y * cell:(y + 1) * cell]
    write_png(os.path.join(OUT, "hudfont.png"), W, H, out, HOLE_PAL)
    print(f"  hudfont.png  {W}x{H}  ({len(glyphs)} glifos de {cell//8}x{cell//8} tiles)")


def _write_rows(name, rows, cell):
    cols = max(len(r) for r in rows)
    W, H = cell * cols, cell * len(rows)
    out = bytearray(W * H)
    for r, row in enumerate(rows):
        for c, idx in enumerate(row):
            for y in range(cell):
                dst = (r * cell + y) * W + c * cell
                out[dst:dst + cell] = idx[y * cell:(y + 1) * cell]
    write_png(os.path.join(OUT, name), W, H, out, SPRITE_PAL)
    print(f"  {name}  {W}x{H}  {len(rows)} animaciones de {cols} frames de {cell}x{cell}")


# Escalas del latido. Los cuatro objetos del suelo laten en el original:
#   oPaquete/Draw_0           t += 0.05;  si t > 1.4  -> t = 0.8
#   oBoomPowUp/Draw_0         t += 0.1;   si t > 2    -> t = 1
#   oNaveReplicaPowUp/oRayos  igual, y ademas giran (ro += 2)
# El giro no se porta (habria que multiplicar por ocho los frames), pero el
# latido si: son tres tamanos por objeto y cambiar de frame no cuesta nada.
GROUND_CELL = 24
GROUND_SCALES = [0.8, 1.1, 1.4]


def ground_sheet():
    """Los cuatro objetos del suelo como hoja de sprites: una animacion por
    objeto y un frame por tamano del latido.

    Antes iban al plano A como tiles, y eso tenia un fallo que se veia: en el
    plano el color 0 no es transparente, pinta el fondo, asi que un paquete
    encima de un agujero le recortaba un rectangulo. Como sprites la
    transparencia es de verdad y ademas sale el latido gratis.
    """
    cell = GROUND_CELL
    canvas = int(round(cell / SCALE))
    rows = []
    for name, sprite in GROUND:
        _, _, w, _ = origin(sprite)
        rows.append([blended([(src_scaled(sprite), int(round(w * sc)), 1.0, None)],
                             cell, canvas)
                     for sc in GROUND_SCALES])
    _write_rows("ground.png", rows, cell)


def src_scaled(sprite):
    return frame_files(sprite)[0]


def ground_tiles_unused():
    """Version antigua en tiles de plano; se deja fuera del build."""
    cell = 16
    canvas = int(round(cell / SCALE))
    blocks = []
    # Todos se dibujan con c_white (sin tinte) y alfa 1, asi que lo unico que
    # hay que respetar es su color y su alfa propia. Antes se indexaban contra
    # una paleta de cuatro grises y salian como manchas oscuras: el paquete
    # cian parecia un agujero pequeño.
    for name, sprite in GROUND:
        _, _, w, _ = origin(sprite)
        src = frame_files(sprite)[0]
        blocks.append(blended([(src, w, 1.0, None)], cell, canvas, pal=HOLE_PAL))

    W, H = cell * len(blocks), cell
    out = bytearray(W * H)
    for b, idx in enumerate(blocks):
        for y in range(cell):
            out[y * W + b * cell:y * W + (b + 1) * cell] = idx[y * cell:(y + 1) * cell]

    write_png(os.path.join(OUT, "ground.png"), W, H, out, HOLE_PAL)
    print(f"  ground.png  {W}x{H}  ({len(blocks)} bloques de 2x2 tiles)")


# Iconos de arma del menú (oMenu/Draw_0 los pinta con draw_sprite en fila,
# separados WEAPON_FRAME_GAP = 128 px del room) y logo del splash.
MENU_ICONS = ["sprWeapon1", "sprWeapon2", "sprWeapon3", "sprWeapon5", "sprWeapon5"]


def menu_tiles():
    # oMenu/Draw_0 dibuja DOS sprites en el mismo sitio por cada arma: el
    # icono y encima sprFrame, el marco. Como los dos van juntos y no se
    # mueven, se componen aqui en un solo bloque de tiles: el marco sale
    # gratis y el menu se parece al original.
    #
    # sprFrame mide 128x128 (26 px a escala) y los iconos 110x110 (22 px), asi
    # que el bloque pasa a 4x4 tiles para que quepa el marco.
    cell = 32
    canvas = int(round(cell / SCALE))
    fsrc = frame_files("sprFrame")[0]
    _, _, fw, _ = origin("sprFrame")
    blocks = []
    for sprite in MENU_ICONS:
        _, _, w, _ = origin(sprite)
        src = frame_files(sprite)[0]
        blocks.append(blended([(src, w, 1.0, None), (fsrc, fw, 1.0, None)],
                              cell, canvas, pal=HOLE_PAL))

    W, H = cell * len(blocks), cell
    out = bytearray(W * H)
    for b, idx in enumerate(blocks):
        for y in range(cell):
            out[y * W + b * cell:y * W + (b + 1) * cell] = idx[y * cell:(y + 1) * cell]
    write_png(os.path.join(OUT, "weapons.png"), W, H, out, HOLE_PAL)
    print(f"  weapons.png  {W}x{H}  ({len(blocks)} iconos de {cell//8}x{cell//8} tiles)")

    # logo del splash (sprite176, 256x256 -> 51 px) en 7x7 tiles
    cell = 56
    canvas = int(round(cell / SCALE))
    ox, oy, w, h = origin("sprite176")
    src = frame_files("sprite176")[0]
    cmd = [
        "magick", "-size", f"{canvas}x{canvas}", "xc:none",
        src, "-geometry", f"{canvas//2-ox:+d}{canvas//2-oy:+d}", "-composite",
        "-filter", "Box", "-resize", f"{cell}x{cell}!",
        "-depth", "8", "RGBA:-",
    ]
    idx = to_indices(subprocess.run(cmd, capture_output=True, check=True).stdout, cell, HOLE_PAL)
    write_png(os.path.join(OUT, "logo.png"), cell, cell, idx, HOLE_PAL)
    print(f"  logo.png  {cell}x{cell}  (7x7 tiles)")

    # tile liso para la barra del interludio. oMenu/Draw_0 la pinta con
    # draw_line_width_color(..., c_gray, c_gray): es GRIS (128,128,128), no
    # gris oscuro, asi que va al indice 12 de la paleta del plano.
    bar = bytearray([12] * 64)
    write_png(os.path.join(OUT, "bar.png"), 8, 8, bar, HOLE_PAL)
    print("  bar.png  8x8  (1 tile)")


def md_color(c):
    """RGB de 8 bits al word de 9 bits del VDP (0000 BBB0 GGG0 RRR0)."""
    r, g, b = [min(7, (v + 18) // 36) for v in c]
    return (b << 9) | (g << 5) | (r << 1)


def write_header():
    with open(HDR, "w") as f:
        f.write("// Generado por tools/gm2md.py -- no editar a mano.\n")
        f.write("// Paletas sacadas del arte del proyecto GameMaker.\n")
        f.write("#ifndef _GM_PALETTES_H_\n#define _GM_PALETTES_H_\n\n")
        f.write("#include <genesis.h>\n\n")
        f.write(f"// Capa Compatibility_Colour del room _1: {ROOM_BG}\n")
        f.write(f"#define ROOM_BG_COLOR 0x{md_color(ROOM_BG):04X}\n\n")
        for nm, pal in (("SPRITE", SPRITE_PAL), ("HOLE", HOLE_PAL)):
            full = list(pal) + [(0, 0, 0)] * (16 - len(pal))
            f.write(f"static const u16 {nm.lower()}Palette[16] = {{\n    ")
            f.write(", ".join(f"0x{md_color(c):04X}" for c in full))
            f.write("\n};\n\n")
        # tabla de desplazamiento del dibujo respecto al origen, por animación
        f.write("// Desplazamiento del dibujo respecto al origen de la instancia,\n")
        f.write("// una entrada por animacion de projectiles.png (ver gm2md.py).\n")
        f.write(f"#define PROJ_CELL {PROJ_CELL}\n")
        f.write(f"#define PROJ_SPR_FRAMES {len(PROJ_OFFSETS)}\n")
        for nm, vals in (("X", [o[0] for o in PROJ_OFFSETS]), ("Y", [o[1] for o in PROJ_OFFSETS])):
            f.write(f"static const s8 projSprOff{nm}[PROJ_SPR_FRAMES] = {{\n    ")
            f.write(", ".join(str(v) for v in vals))
            f.write("\n};\n\n")
        # hojas recortadas: esquina del frame respecto al centro del objeto
        f.write("// Hojas recortadas a 16x16 (ver trim en gm2md.py): esquina de cada\n")
        f.write("// frame respecto al centro del objeto, en el orden rotacion *\n")
        f.write("// columnas + columna. sprPos(x + offX[i], y + offY[i]).\n")
        for nm, offs in TRIM_OFFSETS.items():
            cname = "".join(w.capitalize() if k else w for k, w in enumerate(nm.split("_")))
            for ax, k in (("X", 0), ("Y", 1)):
                f.write(f"static const s8 {cname}Off{ax}[{len(offs)}] = {{\n    ")
                f.write(", ".join(str(o[k]) for o in offs))
                f.write("\n};\n")
        f.write("\n")
        # animacion de cada objeto del suelo dentro de ground.png
        f.write("// ground.png: una animacion por objeto del suelo, un frame\n")
        f.write("// por tamano del latido (ver gm2md.py).\n")
        f.write(f"#define GROUND_CELL {GROUND_CELL}\n")
        f.write(f"#define GROUND_FRAMES {len(GROUND_SCALES)}\n")
        for i, (name, sprite) in enumerate(GROUND):
            f.write(f"#define GROUND_ANIM_{name} {i}\n")
        f.write("\n")
        # índice de la primera animación de cada arma
        base = 0
        for name, sprite, natural, rots in PROJECTILES:
            f.write(f"#define PROJ_ANIM_{name} {base}\n")
            base += rots
        f.write("\n")
        # explosion.png: fila 0 fogonazo, filas 1..n humareda por tamano
        f.write("// explosion.png: una animacion por fila (ver gm2md.py).\n")
        f.write("#define EXPL_ANIM_FLASH 0\n")
        f.write(f"#define EXPL_FLASH_FRAMES {len(EXPLOSION_SCALES)}\n")
        f.write("#define EXPL_ANIM_DUST 1\n")
        f.write(f"#define EXPL_DUST_SIZES {len(DUST_SCALES)}\n")
        f.write(f"#define EXPL_DUST_FRAMES {len(DUST_FRAMES)}\n")
        f.write(f"#define EXPL_CELL {EXPLOSION_CELL}\n\n")
        # metralla, muerte, haz del PARA y digitos
        f.write("// debris.png: animacion 0 gris oscuro, 1 negro (oDestro/Draw_0).\n")
        f.write(f"#define DEBRIS_CELL {DEBRIS_CELL}\n")
        f.write(f"#define DEBRIS_SIZES {len(DEBRIS_SCALES)}\n")
        f.write(f"#define DEBRIS_COLORS {len(DEBRIS_COLORS)}\n\n")
        f.write("// muerte.png: los tres primeros tamanos de oMuerte.\n")
        f.write(f"#define MUERTE_CELL {MUERTE_CELL}\n")
        f.write(f"#define MUERTE_FRAMES {len(MUERTE_SCALES)}\n\n")
        f.write("// beam.png: una animacion por rotacion, un frame por color.\n")
        f.write(f"#define BEAM_CELL {BEAM_CELL}\n")
        f.write(f"#define BEAM_COLORS {len(BEAM_COLORS)}\n\n")
        f.write("// digits.png: una animacion por digito.\n")
        f.write(f"#define DIGIT_CELL {DIGIT_CELL}\n\n")
        f.write("// hudfont.png: los glifos que usa el HUD, 2x2 tiles cada uno.\n")
        f.write(f"#define HUD_FONT_CELL {HUD_FONT_CELL}\n")
        f.write(f"#define HUD_FONT_GLYPHS {len(HUD_CHARS)}\n")
        f.write('static const char hudFontChars[] = "%s";\n\n' % HUD_CHARS)
        f.write("// dot.png: una animacion por color (azul, plata, amarillo).\n")
        f.write(f"#define DOT_CELL {DOT_CELL}\n")
        f.write(f"#define DOT_COLORS {len(DOT_COLORS)}\n")
        f.write("\n#endif\n")
    print(f"  {os.path.relpath(HDR)}")


PROJ_OFFSETS = []


def main():
    if not os.path.isdir(GM):
        sys.exit(f"no encuentro el proyecto GameMaker en {GM}")
    os.makedirs(OUT, exist_ok=True)

    print("sprites:")
    # La nave y las cabezas giran de forma continua en el original y son lo
    # que mas se ve: van con 32 direcciones (pasos de 11,25 grados). Sus
    # frames NO se precargan en VRAM (32 x 3 hojas no caben): viven en ROM y
    # se suben por DMA al cambiar de frame (ver VStream en inc/vramspr.h), asi
    # que en resources.res van SIN comprimir.
    ROT_SMOOTH = 32
    # oPlayer:   image_angle = direction - 90  -> arte hacia arriba
    # Sombras: la nave vuela, asi que la suya va mas separada que la de los
    # bichos, que van por el suelo (ver SHADOW_AIR).
    SH_AIR, SH_GROUND = SHADOW_AIR, SHADOW_GROUND
    sheet("player", "sprite173", 24, "up", rotations=ROT_SMOOTH, shadow=SH_AIR)
    # oSpider:   image_angle = direction - 90, 8 frames de animación
    sheet("spider", "sprite185", 24, "up", shadow=SH_GROUND, trim=16)
    # oCabeza1:  image_angle = direction      -> arte hacia la derecha
    sheet("snake_head1", "spr_rtype_outslay_head", 24, "right", rotations=ROT_SMOOTH, shadow=SH_GROUND, trim=16)
    # oCabeza2b: image_angle = direction
    sheet("snake_head2", "spr_rtype_outslay_head_2", 24, "right", rotations=ROT_SMOOTH, shadow=SH_GROUND)
    # oCu2, el ultimo eslabon, se dibuja con el arte de la cabeza pero sigue
    # precargado a 8 direcciones: es su propia hoja para no cargar las 32.
    sheet("snake_cu2", "spr_rtype_outslay_head", 24, "right", shadow=SH_GROUND, trim=16)
    # oCu1/oCabeza2: image_angle = lookdir
    sheet("snake_tail", "spr_rtype_outslay_tail", 24, "right", shadow=SH_GROUND, trim=16)
    # oCorpo: su rotación está comentada en el original -> un solo frame
    sheet("snake_body", "spr_rtype_outslay_body", 16, "right", rotations=1, shadow=SH_GROUND)
    # oBulletThin va dentro de projectiles.png, con el resto de armas

    global PROJ_OFFSETS
    PROJ_OFFSETS = projectile_sheet()

    explosion_sheet()
    debris_sheet()
    muerte_sheet()
    beam_sheet()
    digits_sheet()
    dot_sheet()

    ground_sheet()

    print("fondo:")
    hole_tiles()
    menu_tiles()
    hud_font()

    print("header:")
    write_header()


if __name__ == "__main__":
    main()
