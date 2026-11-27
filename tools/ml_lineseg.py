#!/usr/bin/env python3
"""Los tramos de las líneas a las naves holo (bloque 12).

Decisión del usuario (2026-11-21): las líneas de la nave a cada holo, del
grosor de la estela (el punto de 2x2) y de puntos muy juntos. Con un sprite
por punto no caben (80 sprites por frame, 20 por línea de pantalla), así que
cada sprite lleva un TRAMO de varios puntos ya dibujados en una de 7
direcciones entre 0 y 90 grados; las de los otros cuadrantes son las mismas
con volteo horizontal o vertical.

Cada dirección es un paso entero (sx, sy) entre puntos:

  d  paso   ángulo  sprite  puntos  avance del tramo   tiles
  0  (3,0)    0,0   32x8    10      30,0 px            4
  1  (3,1)   18,4   16x8     5      15,8 px            2
  2  (3,2)   33,7   16x16    5      18,0 px            4
  3  (3,3)   45,0   16x16    5      21,2 px            4
  4  (2,3)   56,3   16x16    5      18,0 px            4
  5  (1,3)   71,6    8x16    5      15,8 px            2
  6  (0,3)   90,0    8x32   10      30,0 px            4
                                                      --
                                                      24

A 45 grados el paso es (3,3), 4,2 px: con (2,2) los puntos de 2x2 se tocaban
por la esquina y la diagonal se veía como una escalera continua.

El primer punto (su esquina de arriba a la izquierda) va en (ox, oy) y el k en
(ox + k*sx, oy + k*sy): el tramo siguiente empieza donde acabaría el punto
n, así que la separación sigue igual de un tramo al otro. Azul, índice 5 de la
paleta de sprites (la de dot.png).

Dos salidas:

  res/sprite/lineseg.png  una tira de 8 px de ancho con los 24 tiles en el
                          orden en que los lee un sprite del VDP (por
                          columnas). El TILESET se carga con ROW.

  res/lineseg_tab.h       la TABLA DE TRAMOS (2026-11-22, para que las líneas
                          no cuesten): para cada distancia de la holo a la
                          nave en celdas de 8 px, |dx| de 0 a 320 y |dy| de 0 a
                          240, la lista de tramos ya colocados, con la esquina
                          de arriba a la izquierda de cada sprite respecto a la
                          holo. Calculado aquí en coma flotante: la dirección
                          más cercana a la de la línea, tantos tramos enteros
                          como quepan, centrados, y cada uno apoyado en la
                          línea de verdad. Es el cuadrante de abajo a la
                          derecha. Desde la quinta entrega (2026-11-26) cada
                          tramo va ya en la forma de la tabla de sprites del
                          VDP y en las cuatro versiones de volteo (16 bytes),
                          con la caja de cada celda y los repartos por número
                          de holo: la consola sólo suma dos palabras largas
                          por tramo (src/beam_a.s). Pesa ~250 KB de ROM a
                          cambio de no hacer en la consola ni el volteo ni el
                          reparto ni la comprobación de bordes tramo a tramo.

uso: python3 tools/ml_lineseg.py
"""

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ml_icons import read_png, write_png  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAL_SRC = os.path.join(REPO, "res", "sprite", "dot.png")
OUT = os.path.join(REPO, "res", "sprite", "lineseg.png")
TAB = os.path.join(REPO, "res", "lineseg_tab.h")
BLUE = 5

# (sx, sy, celdas de ancho, celdas de alto, puntos, ox, oy)
SEGS = [
    (3, 0, 4, 1, 10, 1, 3),
    (3, 1, 2, 1, 5, 1, 1),
    (3, 2, 2, 2, 5, 1, 3),
    (3, 3, 2, 2, 5, 1, 1),
    (2, 3, 2, 2, 5, 3, 1),
    (1, 3, 1, 2, 5, 1, 1),
    (0, 3, 1, 4, 10, 3, 1),
]

CELL = 8                # la tabla va en celdas de 8 px
QX = 320 // CELL + 1    # 0..40
QY = 240 // CELL + 1    # 0..30


def tiles():
    out = []
    for sx, sy, cw, ch, n, ox, oy in SEGS:
        img = np.zeros((ch * 8, cw * 8), np.int32)
        for k in range(n):
            x, y = ox + k * sx, oy + k * sy
            assert x + 2 <= cw * 8 and y + 2 <= ch * 8, (sx, sy, k)
            img[y:y + 2, x:x + 2] = BLUE
        for c in range(cw):             # el VDP lee por columnas
            for r in range(ch):
                out.append(img[r * 8:(r + 1) * 8, c * 8:(c + 1) * 8])
    return out


def table():
    angles = [math.atan2(sy, sx) for sx, sy, *_ in SEGS]
    idx, rx, ry, dr = [0], [], [], []
    for qy in range(QY):
        for qx in range(QX):
            ax, ay = qx * CELL + CELL / 2, qy * CELL + CELL / 2
            length = math.hypot(ax, ay)
            ang = math.atan2(ay, ax)
            d = min(range(7), key=lambda i: abs(angles[i] - ang))
            sx, sy, cw, ch, n, ox, oy = SEGS[d]
            adv = n * math.hypot(sx, sy)
            segs = int(length // adv)
            off = (length - segs * adv) / 2
            ux, uy = ax / length, ay / length
            cx, cy = ox + 1, oy + 1      # el centro del primer punto
            for k in range(segs):
                t = off + k * adv
                rx.append(int(round(ux * t - cx)))
                ry.append(int(round(uy * t - cy)))
                dr.append(d)
            idx.append(len(rx))
    return idx, rx, ry, dr


def carray(ctype, name, vals, per=16, static=True):
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    return "%sconst %s %s[%d] =\n{\n%s\n};\n" % ("static " if static else "", ctype, name, len(vals),
                                               "\n".join(lines))


# Los tramos por línea según cuántas holo haya (src/beam.c): BEAM_LINE_SPRITES
# (40) a partes iguales. Si la celda tiene más, se toman repartidos a lo largo
# con t = (j * tramos * (32768 // bloque)) >> 15, la misma cuenta que hacía
# el bucle de la cuarta entrega.
LINE_SPRITES = 40
BLOCKS = sorted({LINE_SPRITES // n for n in range(1, 11)}, reverse=True)   # 40 20 13 10 8 6 5 4
REC = 16                # bytes por tramo en lineSegRec


def words32(v):
    v &= 0xFFFFFFFF
    return [v >> 16, v & 0xFFFF]


def main():
    _, plte = read_png(PAL_SRC)
    t = tiles()
    write_png(OUT, np.vstack(t), plte)
    print("  %s: %d tiles" % (os.path.relpath(OUT, REPO), len(t)))

    idx, rx, ry, dr = table()
    assert max(idx) < 65536
    maxc = max(idx[i + 1] - idx[i] for i in range(len(idx) - 1))
    toff = []
    k = 0
    for sx, sy, cw, ch, *_ in SEGS:
        toff.append(k)
        k += cw * ch

    # Cada tramo, 16 bytes: cuatro palabras largas ya con la forma de la
    # tabla del VDP, una por volteo:
    #   +0  y sin volteo vertical   * 65536 + tamaño * 256
    #   +4  y con volteo vertical   * 65536 + tamaño * 256
    #   +8  tile (desde el primero de lineseg.png) * 65536 + x sin volteo
    #   +12 tile * 65536 + x con volteo horizontal
    # x e y son la esquina del sprite respecto a la holo; con volteo, la del
    # sprite reflejado (-r - ancho + 1). La consola sólo les suma, como
    # enteros de 32 bits, (y de la holo en pantalla) * 65536 + enlace y
    # (atributo base) * 65536 + x de la holo en pantalla: ver src/beam_a.s.
    rec = []
    for a, b, d in zip(rx, ry, dr):
        sx, sy, cw, ch, *_ = SEGS[d]
        w, h = cw * 8, ch * 8
        size = ((cw - 1) << 2) | (ch - 1)
        rec += words32(b * 65536 + size * 256)
        rec += words32((-b - h + 1) * 65536 + size * 256)
        rec += words32(toff[d] * 65536 + a)
        rec += words32(toff[d] * 65536 + (-a - w + 1))

    # La caja de todos los tramos de cada celda respecto a la holo, en el
    # cuadrante base: x mínima, x máxima, y mínima, y máxima (inclusive). Si
    # la caja volteada cabe en el campo, ningún tramo se sale y la línea se
    # coloca sin mirar tramo a tramo.
    box = []
    for c in range(len(idx) - 1):
        xs0, xs1, ys0, ys1 = 0, 0, 0, 0
        for i in range(idx[c], idx[c + 1]):
            sx, sy, cw, ch, *_ = SEGS[dr[i]]
            if i == idx[c]:
                xs0, xs1, ys0, ys1 = rx[i], rx[i] + cw * 8 - 1, ry[i], ry[i] + ch * 8 - 1
            else:
                xs0 = min(xs0, rx[i]); xs1 = max(xs1, rx[i] + cw * 8 - 1)
                ys0 = min(ys0, ry[i]); ys1 = max(ys1, ry[i] + ch * 8 - 1)
        box += [xs0, xs1, ys0, ys1]

    # Los repartos: una lista de desplazamientos (t * 16) por bloque y número
    # de tramos. La lista 0 es la identidad (todos, en orden).
    pick = [i * REC for i in range(maxc)]
    rows = []
    for blk in BLOCKS:
        rec_blk = 32768 // blk
        row = []
        for c in range(maxc + 1):
            if c <= blk:
                row.append(0)
            else:
                row.append(len(pick))
                pick += [((j * c * rec_blk) >> 15) * REC for j in range(blk)]
        rows += row
    blk_of_n = [0] + [BLOCKS.index(LINE_SPRITES // n) for n in range(1, 11)]

    with open(TAB, "w") as f:
        f.write("// Generado por tools/ml_lineseg.py: no editar a mano.\n")
        f.write("// La tabla de tramos de las líneas a las naves holo (ver el script).\n")
        f.write("// Sólo la incluye src/beam.c; las que lee src/beam_a.s no son static.\n")
        f.write("#ifndef _LINESEG_TAB_H_\n#define _LINESEG_TAB_H_\n\n")
        f.write("#define LINESEG_CELL %d\n#define LINESEG_QX %d\n#define LINESEG_QY %d\n" % (CELL, QX, QY))
        f.write("#define LINESEG_MAXC %d\n#define LINESEG_REC %d\n\n" % (maxc, REC))
        f.write("// Dónde empieza cada fila de celdas: lineSegRow[qy] + qx es la celda.\n")
        f.write(carray("u16", "lineSegRow", [qy * QX for qy in range(QY)], static=False))
        f.write("\n// Primer tramo de cada celda; el de la siguiente marca el final.\n")
        f.write(carray("u16", "lineSegIdx", idx, static=False))
        f.write("\n// La caja de los tramos de cada celda (x0, x1, y0, y1 respecto a la holo).\n")
        f.write(carray("s16", "lineSegBox", box, static=False))
        f.write("\n// Los repartos: lineSegPickRow[bloque * %d + tramos] es dónde empieza en\n" % (maxc + 1))
        f.write("// lineSegPick la lista de desplazamientos (tramo * %d) a colocar.\n" % REC)
        f.write("// lineSegBlkOf[holo] es el bloque (40 / holo sprites por línea).\n")
        f.write(carray("u8", "lineSegBlkOf", blk_of_n, static=False))
        f.write(carray("u8", "lineSegBlkLen", BLOCKS, static=False))
        f.write(carray("u16", "lineSegPickRow", rows, 23, static=False))
        f.write(carray("u16", "lineSegPick", pick, static=False))
        f.write("\n// Cada tramo, 16 bytes (8 palabras): ver tools/ml_lineseg.py.\n")
        f.write(carray("u16", "lineSegRec", rec, 16, static=False))
        f.write("\n#endif\n")
    nbytes = 2 * QY + 2 * len(idx) + 2 * len(box) + 11 + len(BLOCKS) + 2 * len(rows) + 2 * len(pick) + 2 * len(rec)
    print("  %s: %d celdas, %d tramos (hasta %d por celda), %d bytes de ROM" % (
        os.path.relpath(TAB, REPO), len(idx) - 1, len(rx), maxc, nbytes))


if __name__ == "__main__":
    main()
