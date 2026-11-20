#!/usr/bin/env python3
"""La moneda desde el arte de MonsterLite (Q11).

El GML no tiene moneda, así que (decisión del usuario,
docs/MONSTERLITE_DECISIONES.md, Q11) se toma de MonsterLite:
Assets/Art/coinAnimation.png, seis frames de 32x32 en columna que coinAnim
pasa a 12 por segundo. El prefab Coin la dibuja a escala 2 con 100 px por
unidad: 0,64 u = 12,8 px de pantalla. Aquí cada frame se reduce a 13x13 por
áreas, centrado en una celda de 16x16, y se pinta con el color más cercano de
la paleta de sprites del port (la de res/sprite/dot.png, que es la de todos
los sprites, PAL1). El PNG es blanco y gris: el SpriteRenderer del prefab lo
tiñe con m_Color (0.937, 0.788, 0.302), leído con UnityPy, y aquí se
multiplica igual antes de buscar el color.

Salida: res/sprite/coin.png, 96x16 indexado: una animación de 6 frames.

uso: python3 tools/ml_coin.py [carpeta Assets/Art de MonsterLite]
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAL_SRC = os.path.join(REPO, "res", "sprite", "dot.png")
OUT = os.path.join(REPO, "res", "sprite", "coin.png")
ART = os.path.expanduser("~/MonsterLite/MonsterLite_ultimo_commit/Assets/Art")

FRAMES = 6
SRC = 32            # lado de un frame de coinAnimation.png
SIZE = 13           # 12,8 px redondeado
CELL = 16
TINT = np.array([0.9372549, 0.7882353, 0.3019608])   # m_Color de Coin.prefab


def shrink(rgba, size):
    """Reducción por áreas de un frame RGBA cuadrado: media premultiplicada."""
    h = rgba.shape[0]
    out = np.zeros((size, size, 4))
    for y in range(size):
        y0, y1 = (y * h) // size, max((y * h) // size + 1, ((y + 1) * h) // size)
        for x in range(size):
            x0, x1 = (x * h) // size, max((x * h) // size + 1, ((x + 1) * h) // size)
            blk = rgba[y0:y1, x0:x1].astype(np.float64)
            a = blk[:, :, 3] / 255.0
            out[y, x, 3] = a.mean()
            if a.sum() > 0:
                out[y, x, :3] = (blk[:, :, :3] * a[:, :, None]).sum(axis=(0, 1)) / a.sum()
    return out


def main():
    art = sys.argv[1] if len(sys.argv) > 1 else ART
    src, _ = read_png(os.path.join(art, "coinAnimation.png"))
    _, plte = read_png(PAL_SRC)
    pal = np.frombuffer(plte, np.uint8).reshape(-1, 3).astype(np.float64)

    sheet = np.zeros((CELL, CELL * FRAMES), np.int32)
    off = (CELL - SIZE) // 2
    for f in range(FRAMES):
        frame = shrink(src[f * SRC:(f + 1) * SRC], SIZE)
        for y in range(SIZE):
            for x in range(SIZE):
                if frame[y, x, 3] < 0.5:
                    continue
                # El color más cercano, sin el 0 (transparente).
                d = ((pal[1:16] - frame[y, x, :3] * TINT) ** 2).sum(axis=1)
                sheet[off + y, f * CELL + off + x] = 1 + int(d.argmin())

    write_png(OUT, sheet, plte)
    print("%s: %d frames de %dx%d" % (os.path.relpath(OUT, REPO), FRAMES, CELL, CELL))


if __name__ == "__main__":
    sys.exit(main())
