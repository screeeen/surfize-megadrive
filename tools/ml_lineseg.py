#!/usr/bin/env python3
"""Los tramos de las líneas a las naves holo (bloque 12).

Decisión del usuario (2026-11-21): las líneas de la nave a cada holo, del
grosor de la estela (el punto de 2x2) y de puntos muy juntos. Con un sprite
por punto no caben (80 sprites por frame, 20 por línea de pantalla), así que
cada sprite lleva un TRAMO de varios puntos ya dibujados en una de 7
direcciones entre 0 y 90 grados; las de 90 a 180 son las mismas con volteo
vertical. src/beam.c los coloca a lo largo de la línea de verdad.

Cada dirección es un paso entero (sx, sy) de unos 3 px entre puntos:

  d  paso   ángulo  sprite  puntos  avance del tramo   tiles
  0  (3,0)    0,0   32x8    10      30,0 px            4
  1  (3,1)   18,4   16x8     5      15,8 px            2
  2  (3,2)   33,7   16x16    5      18,0 px            4
  3  (2,2)   45,0   16x16    7      19,8 px            4
  4  (2,3)   56,3   16x16    5      18,0 px            4
  5  (1,3)   71,6    8x16    5      15,8 px            2
  6  (0,3)   90,0    8x32   10      30,0 px            4
                                                      --
                                                      24

El primer punto (su esquina de arriba a la izquierda) va en (ox, oy) y el k en
(ox + k*sx, oy + k*sy): el tramo siguiente empieza donde acabaría el punto
n, así que la separación sigue igual de un tramo al otro. Azul, índice 5 de la
paleta de sprites (la de dot.png).

Salida: res/sprite/lineseg.png, una tira de 8 px de ancho con los 24 tiles en
el orden en que los lee un sprite del VDP (por columnas): el TILESET se carga
con ROW y cada tramo empieza donde dice LINESEG_TILE en src/beam.c.

uso: python3 tools/ml_lineseg.py
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAL_SRC = os.path.join(REPO, "res", "sprite", "dot.png")
OUT = os.path.join(REPO, "res", "sprite", "lineseg.png")
BLUE = 5

# (sx, sy, celdas de ancho, celdas de alto, puntos, ox, oy)
SEGS = [
    (3, 0, 4, 1, 10, 1, 3),
    (3, 1, 2, 1, 5, 1, 1),
    (3, 2, 2, 2, 5, 1, 3),
    (2, 2, 2, 2, 7, 1, 1),
    (2, 3, 2, 2, 5, 3, 1),
    (1, 3, 1, 2, 5, 1, 1),
    (0, 3, 1, 4, 10, 3, 1),
]


def main():
    _, plte = read_png(PAL_SRC)
    tiles = []
    for sx, sy, cw, ch, n, ox, oy in SEGS:
        img = np.zeros((ch * 8, cw * 8), np.int32)
        for k in range(n):
            x, y = ox + k * sx, oy + k * sy
            assert x + 2 <= cw * 8 and y + 2 <= ch * 8, (sx, sy, k)
            img[y:y + 2, x:x + 2] = BLUE
        for c in range(cw):             # el VDP lee por columnas
            for r in range(ch):
                tiles.append(img[r * 8:(r + 1) * 8, c * 8:(c + 1) * 8])
    write_png(OUT, np.vstack(tiles), plte)
    print("  %s: %d tiles" % (os.path.relpath(OUT, REPO), len(tiles)))


if __name__ == "__main__":
    main()
