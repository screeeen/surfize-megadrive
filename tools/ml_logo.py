#!/usr/bin/env python3
"""El logo de Screen Implosion desde el arte de MonsterLite.

Decisión del usuario (2026-11-16, docs/MONSTERLITE_DECISIONES.md): el logo
que MonsterLite enseña al arrancar (escena logo.unity, S/logoScene.cs) se
convierte desde su arte, Assets/screenImplosionLogo_inv_Square_1800.png: un
dibujo blanco sobre transparente de 1800x1800.

En logo.unity el sprite mide 1800 px / 100 px por unidad = 18 u, con una
cámara ortográfica de tamaño 20 (40 u de alto): 18 / 40 de la altura de la
pantalla = 108 px de 240. Va 2 u por debajo del centro de la cámara (logo en
y = -1, cámara en y = +1): 12 px. Todo leído con UnityPy.

Aquí se reduce por áreas a 108x108, se centra en 112x112 (14x14 celdas) y se
pinta con 16 grises: el índice 0 es el negro del fondo (transparente) y el
1-15, blanco con la cobertura del píxel. El fundido de entrada lo hace el
código con la paleta (src/logo.c).

Salida: res/sprite/silogo.png, 112x112 indexado.

uso: python3 tools/ml_logo.py [ruta del PNG de MonsterLite]
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "res", "sprite", "silogo.png")
SRC = os.path.expanduser(
    "~/MonsterLite/MonsterLite_ultimo_commit/Assets/screenImplosionLogo_inv_Square_1800.png")

SIZE = 108
CANVAS = 112


def area_resize(a, size):
    """Media por áreas de una imagen cuadrada (imagen integral)."""
    h = a.shape[0]
    ii = np.zeros((h + 1, h + 1))
    ii[1:, 1:] = a.cumsum(0).cumsum(1)
    edges = np.linspace(0, h, size + 1)
    out = np.zeros((size, size))
    for y in range(size):
        y0, y1 = int(round(edges[y])), max(int(round(edges[y])) + 1, int(round(edges[y + 1])))
        for x in range(size):
            x0, x1 = int(round(edges[x])), max(int(round(edges[x])) + 1, int(round(edges[x + 1])))
            s = ii[y1, x1] - ii[y0, x1] - ii[y1, x0] + ii[y0, x0]
            out[y, x] = s / ((y1 - y0) * (x1 - x0))
    return out


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC
    px, _ = read_png(src)
    cover = (px[:, :, 3].astype(np.float64) / 255.0) * (px[:, :, :3].mean(axis=2) / 255.0)
    small = area_resize(cover, SIZE)

    idx = np.zeros((CANVAS, CANVAS), np.int32)
    off = (CANVAS - SIZE) // 2
    idx[off:off + SIZE, off:off + SIZE] = np.clip(np.round(small * 15), 0, 15).astype(np.int32)

    plte = bytes(v for i in range(16) for v in (i * 17, i * 17, i * 17))
    write_png(OUT, idx, plte)
    print("%s: %dx%d, %d celdas con dibujo" % (
        os.path.relpath(OUT, REPO), CANVAS, CANVAS,
        sum(1 for ty in range(CANVAS // 8) for tx in range(CANVAS // 8)
            if idx[ty * 8:(ty + 1) * 8, tx * 8:(tx + 1) * 8].any())))


if __name__ == "__main__":
    sys.exit(main())
