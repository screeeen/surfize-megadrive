#!/usr/bin/env python3
"""El logo ARENA del menú, desde el arte de MonsterLite.

Decisión del usuario (2026-11-27): en el menú va el logo de ARENA y no el de
SURFIZE (sprite176 del GML, que sacaba tools/gm2md.py). MonsterLite dibuja en
el menú (S/menu.cs, OnGUI) la textura `splash` del prefab
Prefabs/States/Menu.prefab, que es Assets/Art/splash_alt.png: ARENA en blanco
sobre transparente, 1200x1200 con el dibujo en 974x818.

Tamaño: el mismo hueco que el logo del GML, 7x7 celdas (56x56) en el centro
del menú. No queda VRAM para más: los tiles del menú van a la VRAM sobrante
(VTile_allocSpare) y el logo reutiliza los 49 tiles que tenía. MonsterLite lo
pinta a pantalla completa (ScaleToFit); aquí sale a 56x47. Divergencia
documentada en docs/PORT_AUDIT_GML.md.

Color: cobertura (alfa x luminancia) llevada al gris más cercano de los que
ya tiene HOLE_PAL (PAL2, compartida con los agujeros): 0 transparente, 3, 13,
12, 9 y 8 (blanco). La paleta se copia del logo.png actual para no duplicar
HOLE_PAL.

Salida: res/sprite/logo.png, 56x56 indexado.

uso: python3 tools/ml_arena_logo.py [ruta de splash_alt.png]
"""

import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "res", "sprite", "logo.png")
SRC = os.path.expanduser(
    "~/MonsterLite/MonsterLite_ultimo_commit/Assets/Art/splash_alt.png")

CELL = 56
# (índice de HOLE_PAL, nivel de gris 0..255)
GREYS = [(0, 0), (3, 73), (13, 109), (12, 146), (9, 219), (8, 255)]


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC
    _, plte = read_png(OUT)

    raw = subprocess.run(
        ["magick", src, "-trim", "+repage",
         "-filter", "Box", "-resize", f"{CELL}x{CELL}",
         "-background", "none", "-gravity", "center", "-extent", f"{CELL}x{CELL}",
         "-depth", "8", "RGBA:-"],
        capture_output=True, check=True).stdout
    px = np.frombuffer(raw, np.uint8).reshape(CELL, CELL, 4).astype(np.float64)
    cover = (px[:, :, 3] / 255.0) * (px[:, :, :3].mean(axis=2) / 255.0) * 255.0

    levels = np.array([g for _, g in GREYS], np.float64)
    pick = np.abs(cover[:, :, None] - levels[None, None, :]).argmin(axis=2)
    idx = np.array([i for i, _ in GREYS], np.int32)[pick]

    write_png(OUT, idx, plte)
    used = sum(1 for ty in range(CELL // 8) for tx in range(CELL // 8)
               if idx[ty * 8:(ty + 1) * 8, tx * 8:(tx + 1) * 8].any())
    print("%s: %dx%d (7x7 tiles), %d con dibujo" % (
        os.path.relpath(OUT, REPO), CELL, CELL, used))


if __name__ == "__main__":
    sys.exit(main())
