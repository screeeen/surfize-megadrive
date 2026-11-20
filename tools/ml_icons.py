#!/usr/bin/env python3
"""Iconos de CIRCULAR y MOIRE desde el arte de MonsterLite (Q11).

El GML no tiene arte para estas dos armas, así que (decisión del usuario,
docs/MONSTERLITE_DECISIONES.md, Q11) se toman de MonsterLite:
Assets/Art/iconCirculo.png y Assets/Art/iconMoire.png. Allí son glifos
blancos sobre transparente de 128x64; aquí se meten en el marco de 32x32 de
los iconos del GML (res/sprite/weapons.png, el del icono del LASER con el
interior vaciado) para que el selector del menú sea uno solo, y se pintan con
los grises que usan los iconos del GML.

Salida: res/sprite/weapons_ml.png, 64x32 indexado con la paleta de
weapons.png (CIRCULAR y MOIRE, en ese orden, el de inc/weapon.h).

uso: python3 tools/ml_icons.py [carpeta Assets/Art de MonsterLite]
"""

import os
import struct
import sys
import zlib

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GML_ICONS = os.path.join(REPO, "res", "sprite", "weapons.png")
OUT = os.path.join(REPO, "res", "sprite", "weapons_ml.png")
ART = os.path.expanduser("~/MonsterLite/MonsterLite_ultimo_commit/Assets/Art")
SOURCES = ["iconCirculo.png", "iconMoire.png"]

ICON = 32            # lado de un icono del GML
FRAME_ICON = 1       # el del LASER: el marco con el interior casi vacío
INNER = (6, 6, 26, 26)   # x0, y0, x1, y1 del interior negro del marco
GLYPH = 18           # lado máximo del glifo dentro del interior

# Índices de la paleta de weapons.png que usan los iconos del GML, de más
# oscuro a más claro (ver el contenido de los iconos): 1 negro, 2 (36), 3 (73),
# 13 (109), 12 (146). El blanco (8) sólo va en el marco.
SHADES = [(0.15, 2), (0.35, 3), (0.60, 13), (1.01, 12)]


def read_png(path):
    data = open(path, "rb").read()
    pos, idat, plte = 8, b"", None
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", chunk[:10])
        elif kind == b"PLTE":
            plte = chunk
        elif kind == b"IDAT":
            idat += chunk
        pos += 12 + n
    if depth != 8:
        raise RuntimeError("%s: sólo se leen PNG de 8 bits" % path)
    bpp = {6: 4, 2: 3, 3: 1, 0: 1, 4: 2}[ctype]
    raw = zlib.decompress(idat)
    stride = w * bpp
    out = np.zeros((h, stride), np.int32)
    prev = np.zeros(stride, np.int32)
    o = 0
    for y in range(h):
        ft = raw[o]
        line = np.frombuffer(raw[o + 1:o + 1 + stride], np.uint8).astype(np.int32)
        line = line.copy()
        o += 1 + stride
        if ft == 2:
            line = (line + prev) & 255
        elif ft in (1, 3, 4):
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                if ft == 1:
                    line[i] = (line[i] + a) & 255
                elif ft == 3:
                    line[i] = (line[i] + ((a + b) >> 1)) & 255
                else:
                    pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                    pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                    line[i] = (line[i] + pr) & 255
        prev = line
        out[y] = line
    return out.reshape(h, w, bpp), plte


def write_png(path, idx, plte):
    h, w = idx.shape
    raw = b"".join(b"\0" + bytes(row.astype(np.uint8)) for row in idx)

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)))
        f.write(chunk(b"PLTE", plte))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def glyph(path):
    """Cobertura (0..1) del glifo, recortado a su caja y reducido a GLYPH."""
    px, _ = read_png(path)
    alpha = px[:, :, 3].astype(np.float64) / 255.0
    lum = px[:, :, :3].mean(axis=2) / 255.0
    cover = alpha * lum
    ys, xs = np.nonzero(alpha > 0)
    cover = cover[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    h, w = cover.shape
    scale = GLYPH / float(max(h, w))
    oh, ow = max(1, int(round(h * scale))), max(1, int(round(w * scale)))
    out = np.zeros((oh, ow))
    # Reducción por áreas: cada píxel de salida es la media de su bloque.
    for y in range(oh):
        y0, y1 = int(y * h / oh), max(int(y * h / oh) + 1, int((y + 1) * h / oh))
        for x in range(ow):
            x0, x1 = int(x * w / ow), max(int(x * w / ow) + 1, int((x + 1) * w / ow))
            out[y, x] = cover[y0:y1, x0:x1].mean()
    # Normalizar: el trazo más denso al gris más claro.
    if out.max() > 0:
        out /= out.max()
    return out


def main():
    art = sys.argv[1] if len(sys.argv) > 1 else ART
    icons, plte = read_png(GML_ICONS)
    icons = icons[:, :, 0]

    # El marco: el icono del LASER con el interior pintado de negro (1).
    frame = icons[:, FRAME_ICON * ICON:(FRAME_ICON + 1) * ICON].copy()
    x0, y0, x1, y1 = INNER
    frame[y0:y1, x0:x1] = 1

    sheet = np.zeros((ICON, ICON * len(SOURCES)), np.int32)
    for n, name in enumerate(SOURCES):
        icon = frame.copy()
        g = glyph(os.path.join(art, name))
        gh, gw = g.shape
        oy = y0 + ((y1 - y0) - gh) // 2
        ox = x0 + ((x1 - x0) - gw) // 2
        for y in range(gh):
            for x in range(gw):
                v = g[y, x]
                if v < 0.08:
                    continue
                for limit, index in SHADES:
                    if v < limit:
                        icon[oy + y, ox + x] = index
                        break
        sheet[:, n * ICON:(n + 1) * ICON] = icon

    write_png(OUT, sheet, plte)
    print("%s: %d iconos" % (os.path.relpath(OUT, REPO), len(SOURCES)))


if __name__ == "__main__":
    sys.exit(main())
