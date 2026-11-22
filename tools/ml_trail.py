#!/usr/bin/env python3
"""Los puntos de la estela de la nave (bloque 12).

MonsterLite pinta la estela con un TrailRenderer en el hijo "nave" de
Prefabs/player.prefab (leído con UnityPy): vive m_Time 0,26 s, mide de
m_StartWidth 0,08 u (1,6 px) a m_EndWidth 0,001 u, es blanca y su alfa va de
0x6F (0,44) en la cabeza a 0xEF (0,94) en la cola. El VDP no dibuja líneas ni
mezcla colores: el port la pinta con un punto por vértice (src/trail.c), y
estos son los dos puntos, cada uno en una celda de 8x8 con el centro en
(4, 4), como los de res/sprite/dot.png:

  fila 0: 2x2 px, gris 146 (índice 7): la mitad nueva, la ancha y con poco
          alfa (blanco al 44 % sobre el verde oscuro del campo queda gris);
  fila 1: 1x1 px, gris 219 (índice 11): la mitad vieja, fina y casi opaca.

El mismo punto de 2x2, en azul, es el de las líneas a las naves holo
(tools/ml_lineseg.py).

Los índices son de la paleta de sprites (PAL1, la de dot.png). Cada fila es
una animación para rescomp.

Salida: res/sprite/trail.png, 8x16 indexado.

uso: python3 tools/ml_trail.py
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAL_SRC = os.path.join(REPO, "res", "sprite", "dot.png")
OUT = os.path.join(REPO, "res", "sprite", "trail.png")


def main():
    _, plte = read_png(PAL_SRC)
    idx = np.zeros((16, 8), np.int32)
    idx[3:5, 3:5] = 7       # fila 0: 2x2, gris 146
    idx[8 + 4, 4] = 11      # fila 1: 1x1, gris 219
    write_png(OUT, idx, plte)
    print("  " + os.path.relpath(OUT, REPO))


if __name__ == "__main__":
    main()
