#!/usr/bin/env python3
"""Convierte una imagen de fondo a un plano de Mega Drive.

El problema
-----------
El campo de juego mide 320x240, o sea 40x30 = 1200 celdas de 8x8. Guardar las
1200 distintas son 38.400 bytes de VRAM y en la VRAM caben 1612 tiles EN TOTAL,
de los que el juego ya gasta ~911 en sprites y agujeros. No entra ni de lejos.

rescomp deduplica tiles IDENTICOS (y sus volteos), que es lo que funciona con
arte de tileset. Con una textura organica como esta no sirve de nada: medido,
las 1200 celdas son 1200 tiles distintos, cero repeticiones.

La solucion
-----------
Un diccionario de tiles con TOLERANCIA. Se recorren las celdas en orden y cada
una busca la entrada mas parecida del diccionario que ya hay, probando las
cuatro orientaciones que el VDP sabe dibujar gratis (normal, volteo H, volteo V
y los dos). Si el error cuadratico en RGB queda por debajo del umbral, la celda
reutiliza esa entrada y se queda con los bits de volteo; si no, se da de alta
como entrada nueva.

El umbral se busca por biseccion hasta que el diccionario cabe justo en el
presupuesto de tiles que se le pide. Lo que sale es, en la practica, lo mismo
que hace un artista a mano: la hierba (que es casi toda la imagen) colapsa en
unas pocas variantes y las piedras, los troncos y las flores, que son unicos,
se quedan con su tile propio.

Salida
------
  res/sprite/bg.png  el diccionario como tileset de 8 tiles de ancho
  res/bg_art.h       la paleta de 15 colores y el mapa de 40x30

El indice 0 de la paleta NO se usa: en un plano el color 0 no se pinta, deja
ver el plano de atras (o el color de fondo). Por eso el fondo se cuantiza a 15
colores, de 1 a 15, y asi no hay ni un pixel transparente.

Uso:  python3 tools/bg2md.py <imagen> [--tiles N] [--name NOMBRE] [--crop43]
                                    [--pixel PX[xPY]] [--native] [--soft] [--focus K]

--pixel: tamaño del pixel del arte en la imagen (ver PIXEL en el codigo).
Se mide con la autocorrelacion de los bordes: en ruins.png sale 5,90 x 5,73.

Con --name salen res/sprite/bg_NOMBRE.png y res/bg_NOMBRE.h, con los simbolos
bg_NOMBRE_palette, bg_NOMBRE_map y BG_NOMBRE_TILES: asi caben varios fondos en
la ROM (ver la tabla de src/bg.c). Sin --name, los nombres de siempre.

--crop43 recorta la imagen al centro a 4:3 antes de reducirla, en vez de
estirarla: las imagenes que no son 4:3 salian deformadas.
"""

import os
import struct
import subprocess
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_PNG = os.path.join(HERE, "..", "res", "sprite", "bg.png")
OUT_HDR = os.path.join(HERE, "..", "res", "bg_art.h")

COLS, ROWS = 40, 30
CELLS = COLS * ROWS
COLORS = 15                      # indices 1..15; el 0 queda libre
DEFAULT_TILES = 640

# Los ocho niveles por canal del VDP (3 bits).
LEV = np.array([0, 36, 73, 109, 146, 182, 219, 255], dtype=np.int32)

# Un desenfoque suave antes de reducir colores. Sin el, la reduccion a 15
# colores de una imagen con tanto detalle deja ruido de un pixel que no se ve
# en pantalla y en cambio dispara el numero de tiles distintos.
BLUR = "0x0.8"


def md_color(c):
    r, g, b = [min(7, (v + 18) // 36) for v in c]
    return (b << 9) | (g << 5) | (r << 1)


def snap(a):
    """Lleva cada canal al nivel mas cercano de los ocho del VDP."""
    return LEV[np.abs(a[..., None].astype(np.int32) - LEV).argmin(-1)]


CROP43 = False
# --pixel P: el arte es pixel art ampliado P veces (no siempre un entero: las
# imagenes generadas salen a 5,9 o 6,5 px por pixel). Se reduce primero a su
# rejilla de verdad tomando un pixel por celda (sin mezclar), y de ahi a
# 320x240 por vecino mas cercano: los pixeles quedan nitidos en vez de
# difuminados, aunque algunos salgan de 2 px y otros de 1 si la rejilla no
# encaja en la pantalla. Sin --pixel, filtro y desenfoque, como antes.
PIXEL = None

# --native: la imagen ya es arte de Mega Drive a 1:1 (320 de ancho, 224 o 240
# de alto, pocos colores). No se reescala ni se difumina: cualquier filtro
# emborrona el tramado. Si mide 224 se queda arriba y las dos filas de celdas
# que faltan hasta 240 se rellenan con las de encima en espejo: en NTSC esa
# franja no se ve y en PAL no sale un borde liso.
NATIVE = False


def load(src):
    if NATIVE:
        return load_native(src)
    crop = ["-gravity", "center", "-crop", "4:3", "+repage"] if CROP43 else []
    if PIXEL:
        w, h = [int(v) for v in subprocess.run(
            ["magick", src] + crop + ["-format", "%w %h", "info:"],
            capture_output=True, check=True).stdout.split()]
        nw, nh = int(round(w / PIXEL[0])), int(round(h / PIXEL[1]))
        scale = ["-sample", "%dx%d!" % (nw, nh), "-sample", "%dx%d!" % (COLS * 8, ROWS * 8)]
    else:
        scale = ["-filter", "Triangle", "-resize", "%dx%d!" % (COLS * 8, ROWS * 8),
                 "-blur", BLUR]
    cmd = ["magick", src] + crop + scale + [
           "+dither", "-colors", str(COLORS),
           "-depth", "8", "RGB:-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    rgb = snap(np.frombuffer(raw, np.uint8).reshape(ROWS * 8, COLS * 8, 3))
    pal, inv = np.unique(rgb.reshape(-1, 3), axis=0, return_inverse=True)
    if len(pal) > COLORS:
        raise RuntimeError("salieron %d colores, el maximo es %d" % (len(pal), COLORS))
    # el indice 0 se reserva, asi que la paleta empieza en 1
    return pal.astype(np.int32), (inv.reshape(ROWS * 8, COLS * 8) + 1).astype(np.uint8)


def load_native(src):
    w, h = [int(v) for v in subprocess.run(
        ["magick", src, "-format", "%w %h", "info:"],
        capture_output=True, check=True).stdout.split()]
    W, H = COLS * 8, ROWS * 8
    if w != W or h > H:
        raise RuntimeError("--native espera %d de ancho y como mucho %d de alto (es %dx%d)" % (W, H, w, h))
    raw = subprocess.run(["magick", src, "-depth", "8", "RGB:-"],
                         capture_output=True, check=True).stdout
    img = np.frombuffer(raw, np.uint8).reshape(h, w, 3)
    if h < H:
        img = np.concatenate([img, img[h - (H - h):][::-1]], 0)
    rgb = snap(img)
    pal, inv = np.unique(rgb.reshape(-1, 3), axis=0, return_inverse=True)
    if len(pal) > COLORS:
        raise RuntimeError("salieron %d colores, el maximo es %d" % (len(pal), COLORS))
    return pal.astype(np.int32), (inv.reshape(H, W) + 1).astype(np.uint8)


def to_cells(idx):
    return idx.reshape(ROWS, 8, COLS, 8).transpose(0, 2, 1, 3).reshape(CELLS, 8, 8)


# --soft: comparar las celdas desenfocadas en vez de pixel a pixel. Con arte
# tramado (pixel art de 1:1) dos celdas que se ven igual de lejos no
# comparten casi ningun pixel, y la comparacion exacta empareja ruido con
# ruido: el suelo salia como una sopa de puntos. Desenfocadas, cuenta el tono
# y la forma, que es lo que se ve. Al error se le suma un poco del exacto para
# que, a igualdad de tono, gane el dibujo que mas se parece.
SOFT = False
SOFT_EXACT = float(os.environ.get("BG2MD_SOFT_EXACT", "0.15"))


def blur8(a):
    """[1 2 1] separable, dos pasadas, dentro de la celda. a: (..., 8, 8, 3)."""
    a = a.astype(np.float32)
    for _ in range(2):
        for ax in (-3, -2):
            p = np.concatenate([np.take(a, [0], ax), a, np.take(a, [7], ax)], ax)
            n = a.shape[ax]
            sl = lambda i: np.take(p, range(i, i + n), ax)
            a = (sl(0) + 2 * sl(1) + sl(2)) / 4
    return a


def features(rgb64):
    """(..., 64, 3) -> (..., F): lo que se compara."""
    sh = rgb64.shape[:-2]
    if not SOFT:
        return rgb64.reshape(sh + (192,)).astype(np.float32)
    b = blur8(rgb64.reshape(sh + (8, 8, 3))).reshape(sh + (192,))
    e = rgb64.reshape(sh + (192,)).astype(np.float32) * np.float32(np.sqrt(SOFT_EXACT))
    return np.concatenate([b, e], -1)


# --focus K: reparte el presupuesto segun lo que se VE. Con muchas menos
# entradas que celdas, el error se reparte a partes iguales y el suelo (que es
# ruido y se disimula con cualquier textura parecida) se come tiles que les
# faltan a los edificios: salian ventanas y esquinas cambiadas por otras.
# Cada celda pesa 1 + K * (estructura relativa)^2, donde la estructura es el
# contraste que le queda a la celda vista de lejos (la imagen desenfocada):
# el tramado del suelo casi desaparece al desenfocar, un muro blanco con su
# sombra no. Con K = 0 todas pesan igual, como antes.
FOCUS = 0.0
WEIGHT = None


def cell_weights(idx, lut):
    if not FOCUS:
        return np.ones(CELLS, np.float64)
    lum = lut[idx].astype(np.float32).mean(-1)
    h, w = lum.shape
    raw = subprocess.run(["magick", "-size", "%dx%d" % (w, h), "-depth", "8", "GRAY:-",
                          "-blur", "0x1.5", "GRAY:-"],
                         input=np.clip(lum, 0, 255).astype(np.uint8).tobytes(),
                         capture_output=True, check=True).stdout
    b = np.frombuffer(raw, np.uint8).astype(np.float64).reshape(h, w)
    st = b.reshape(ROWS, 8, COLS, 8).transpose(0, 2, 1, 3).reshape(CELLS, 64).std(-1)
    return 1.0 + FOCUS * (st / max(st.mean(), 1e-6)) ** 2


def dictionary(cell_rgb, var_rgb, thr, budget=None):
    """Diccionario voraz con tolerancia. Devuelve (entradas, asignacion)."""
    book = []
    cell_rgb = features(cell_rgb)
    var_rgb = features(var_rgb)
    book_rgb = np.zeros((CELLS + 1,) + cell_rgb.shape[1:], np.float32)
    assign = np.zeros((CELLS, 2), np.int32)

    for i in range(CELLS):
        if book:
            d = ((var_rgb[i][:, None] - book_rgb[None, :len(book)]) ** 2).sum(-1)
            f, j = np.unravel_index(d.argmin(), d.shape)
            if d[f, j] * WEIGHT[i] <= thr:
                assign[i] = (j, f)
                continue
        book_rgb[len(book)] = cell_rgb[i]
        assign[i] = (len(book), 0)
        book.append(i)
        if budget is not None and len(book) > budget:
            return None, None

    return book, assign


def build(src, budget):
    pal, idx = load(src)
    cells = to_cells(idx)
    # las cuatro orientaciones que el VDP dibuja sin coste
    var = np.stack([cells, cells[:, :, ::-1], cells[:, ::-1, :], cells[:, ::-1, ::-1]], 1)

    # el indice 0 no se usa, asi que la tabla de color lleva un hueco delante
    lut = np.vstack([np.zeros((1, 3), np.int32), pal])
    global WEIGHT
    WEIGHT = cell_weights(idx, lut)
    cell_rgb = lut[cells.reshape(CELLS, 64)]
    var_rgb = lut[var.reshape(CELLS, 4, 64)]

    lo, hi = 0, int(2 * 64 * 3 * 255 * 255 * (WEIGHT.max() if WEIGHT is not None else 1))  # el error maximo posible de una celda
    while lo < hi:
        mid = (lo + hi) // 2
        book, _ = dictionary(cell_rgb, var_rgb, mid, budget)
        if book is None:
            lo = mid + 1
        else:
            hi = mid

    book, assign = dictionary(cell_rgb, var_rgb, hi)
    tiles = var[np.array(book), 0].reshape(len(book), 64).copy()

    # Refinado (k-modas). El diccionario voraz se queda con las PRIMERAS
    # celdas que no se parecen a nada, que en arte nitido suelen ser las mas
    # ruidosas, y todo lo demas se arrima a ellas: salian parches y puntos
    # por todo el fondo. Aqui se itera: cada celda va a la entrada (y volteo)
    # mas parecida, y cada entrada pasa a ser, pixel a pixel, el color MAS
    # FRECUENTE de sus celdas (la moda, no la media: la media difumina). Las
    # entradas que se quedan sin celdas se reponen con la celda peor servida.
    if REFINE and len(tiles) < CELLS:
        tiles, assign = refine(tiles, var, lut, REFINE)

    rec = lut[tiles[assign[:, 0]].reshape(CELLS, 8, 8)]
    # la celda i es la entrada j dibujada con el volteo f, o sea la variante f
    # de la celda i es la entrada j: se compara la variante con la entrada
    rec_var = lut[var[np.arange(CELLS), assign[:, 1]].reshape(CELLS, 64)]
    rmse = float(np.sqrt(((lut[tiles[assign[:, 0]]] - rec_var) ** 2).sum(-1).mean() / 3))
    return pal, tiles, assign, hi, rmse


REFINE = 20


def refine(tiles, var, lut, iters):
    k = len(tiles)
    v = var.reshape(CELLS, 4, 64)
    vr = features(lut[v]).astype(np.float64)
    vn = (vr ** 2).sum(-1)                                  # (CELLS, 4)
    best = None
    for it in range(iters):
        cr = features(lut[tiles]).astype(np.float64)
        cn = (cr ** 2).sum(-1)                              # (k,)
        d = vn[:, :, None] + cn[None, None, :] - 2 * np.einsum("cfp,kp->cfk", vr, cr)
        flat = d.reshape(CELLS, 4 * k)
        a = flat.argmin(1)
        err = flat[np.arange(CELLS), a] * WEIGHT
        f, j = a // k, a % k
        assign = np.stack([j, f], 1)
        tot = err.sum()
        if best is not None and tot >= best:
            break
        best = tot
        aligned = v[np.arange(CELLS), f]                    # (CELLS, 64)
        new = tiles.copy()
        counts = np.zeros((k, 64, 16), np.int32)
        np.add.at(counts, (j[:, None], np.arange(64)[None, :], aligned), 1)
        used = counts.sum((1, 2)) > 0
        if SOFT:
            # el medoide: la celda REAL del grupo que mas se parece a todas.
            # La moda pixel a pixel de un grupo de celdas tramadas sale lisa.
            af = features(lut[aligned]).astype(np.float64)
            for e in np.where(used)[0]:
                mem = np.where(j == e)[0]
                fm = af[mem]
                g = (fm ** 2).sum(-1)
                dd = g[:, None] + g[None, :] - 2 * fm @ fm.T
                new[e] = aligned[mem[(dd @ WEIGHT[mem]).argmin()]]
        else:
            new[used] = counts[used].argmax(-1)
        # entradas vacias: la celda peor servida
        order = np.argsort(-err)
        for n, e in enumerate(np.where(~used)[0]):
            new[e] = aligned[order[n]]
        tiles = new.astype(np.uint8)
    return tiles, assign


def write_png(path, width, height, indices, pal):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    full = [(255, 0, 255)] + [tuple(int(v) for v in c) for c in pal]
    full += [(0, 0, 0)] * (16 - len(full))
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += bytes(indices[y * width:(y + 1) * width])

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", b"".join(bytes(c) for c in full))
    png += chunk(b"tRNS", bytes([0] + [255] * 15))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    args = [a for a in sys.argv[1:]]
    budget = DEFAULT_TILES
    if "--tiles" in args:
        k = args.index("--tiles")
        budget = int(args[k + 1])
        del args[k:k + 2]
    global OUT_PNG, OUT_HDR, CROP43
    name = None
    if "--name" in args:
        k = args.index("--name")
        name = args[k + 1]
        del args[k:k + 2]
    global PIXEL
    if "--pixel" in args:
        k = args.index("--pixel")
        # "5.9" o "5.9x5.73": las imagenes generadas no siempre tienen el
        # pixel cuadrado
        v = [float(t) for t in args[k + 1].split("x")]
        PIXEL = (v[0], v[-1])
        del args[k:k + 2]
    global FOCUS
    if "--focus" in args:
        k = args.index("--focus")
        FOCUS = float(args[k + 1])
        del args[k:k + 2]
    global SOFT
    if "--soft" in args:
        SOFT = True
        args.remove("--soft")
    global NATIVE
    if "--native" in args:
        NATIVE = True
        args.remove("--native")
    if "--crop43" in args:
        CROP43 = True
        args.remove("--crop43")
    if name:
        OUT_PNG = os.path.join(HERE, "..", "res", "sprite", "bg_%s.png" % name)
        OUT_HDR = os.path.join(HERE, "..", "res", "bg_%s.h" % name)
    pfx = ("bg_%s_" % name) if name else "bg"
    guard = ("_BG_%s_H_" % name.upper()) if name else "_BG_ART_H_"
    if not args:
        print(__doc__)
        return 2
    src = args[0]

    pal, tiles, assign, thr, rmse = build(src, budget)

    # ---- el diccionario como tileset de 8 tiles de ancho ----
    n = len(tiles)
    rows = (n + 7) // 8
    sheet = np.zeros((rows * 8, 64), np.uint8)
    tiles = tiles.reshape(n, 8, 8)
    for t in range(n):
        r, c = divmod(t, 8)
        sheet[r * 8:(r + 1) * 8, c * 8:(c + 1) * 8] = tiles[t]
    write_png(OUT_PNG, 64, rows * 8, sheet.reshape(-1), pal)

    # ---- el mapa ----
    # bit 12 volteo vertical, bit 11 volteo horizontal, bits 0..10 el tile.
    # Es la disposicion del atributo del VDP, asi que en ejecucion basta con
    # sumarle la base y meterle la linea de paleta.
    FLIP = [0x0000, 0x0800, 0x1000, 0x1800]           # normal, H, V, H+V
    amap = [int(FLIP[f]) | int(j) for j, f in assign]

    with open(OUT_HDR, "w") as f:
        f.write("// Generado por tools/bg2md.py -- no editar a mano.\n")
        f.write("// Origen: %s\n" % os.path.basename(src))
        f.write("// Diccionario de %d tiles para %d celdas (umbral %d, RMSE %.1f por canal).\n"
                % (n, CELLS, thr, rmse))
        f.write("#ifndef %s\n#define %s\n\n#include <genesis.h>\n\n" % (guard, guard))
        f.write("#ifndef BG_COLS\n#define BG_COLS %d\n#define BG_ROWS %d\n#endif\n" % (COLS, ROWS))
        f.write("#define %s %d\n\n" % (("BG_%s_TILES" % name.upper()) if name else "BG_TILES", n))
        f.write("// Paleta de 15 colores. El indice 0 no se usa: en un plano no se\n")
        f.write("// pinta, deja ver lo que haya detras.\n")
        full = [(255, 0, 255)] + [tuple(int(v) for v in c) for c in pal]
        full += [(0, 0, 0)] * (16 - len(full))
        f.write("static const u16 %s[16] = {\n    " % (pfx + ("palette" if name else "Palette")))
        f.write(", ".join("0x%04X" % md_color(c) for c in full))
        f.write("\n};\n\n")
        f.write("// bit 12 volteo vertical, bit 11 volteo horizontal, 0..10 el tile.\n")
        f.write("static const u16 %s[BG_COLS * BG_ROWS] = {\n" % (pfx + ("map" if name else "Map")))
        for r in range(ROWS):
            f.write("    " + ", ".join("0x%04X" % v for v in amap[r * COLS:(r + 1) * COLS]) + ",\n")
        f.write("};\n\n#endif\n")

    print("%s  %d tiles (%d x 8 px)" % (os.path.relpath(OUT_PNG), n, rows * 8))
    print("%s  mapa de %dx%d, RMSE %.1f por canal, %d colores"
          % (os.path.relpath(OUT_HDR), COLS, ROWS, rmse, len(pal)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
