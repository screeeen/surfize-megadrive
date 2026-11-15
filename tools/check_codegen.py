#!/usr/bin/env python3
"""Busca en el ROM ya enlazado un patrón de código máquina incorrecto.

Este GCC (m68k-elf 16.2.0) tiene al menos un fallo de generación de código con
-O2/-O3: al integrar una función que recibe un puntero a struct, puede perder
el desplazamiento de los campos y escribir dos miembros distintos en el offset
0 del struct. Lo vimos en Effect_spawn (ver src/effect.c): 'e->x = x; e->y = y'
salieron como dos 'move.w N(%sp),(%a2)' seguidos, que machacaban el primer
campo y dejaban x/y sin escribir.

El síntoma en pantalla es mudo (la explosión salía en una posición cualquiera),
así que conviene revisar el ROM entero después de cada build:

    python3 tools/check_codegen.py            # usa out/release/rom.out

Marca dos escrituras consecutivas del mismo tipo al mismo (%aN) sin
desplazamiento. En el código del juego eso no tiene sentido; en las rutinas de
SGDK que hablan con el hardware (puertos del VDP, del Z80, del PSG) sí, así que
esas se ignoran por nombre.
"""

import re
import subprocess
import sys

ROM = sys.argv[1] if len(sys.argv) > 1 else "out/release/rom.out"

# Funciones de SGDK que escriben varias veces al mismo puerto a propósito.
ALLOW = re.compile(
    r"^(DMA_|VDP_|Z80_|PSG_|YM|SND_|XGM|readLightgun|MDDBG|\.fq_loop|sgdk_)"
)


def code_symbols(rom):
    """Nombres de simbolo que son CODIGO.

    objdump -d desensambla la seccion entera, y en un ROM de SGDK ahi dentro
    tambien estan los datos de rescomp (tiles, paletas, mapas). Esos bytes
    interpretados como instrucciones disparan el patron a lo tonto: un aviso
    falso por build es un aviso que se acaba ignorando, asi que se filtran
    mirando el tipo del simbolo (t/T) en vez del nombre.
    """
    out = subprocess.run(["m68k-elf-nm", rom], capture_output=True, text=True,
                         check=True).stdout
    # rescomp emite sus blobs (tiles, paletas, mapas) DENTRO de .text y con
    # tipo 't', asi que el tipo no basta: hay que descartarlos por nombre.
    data = re.compile(r"_(data|tileset|palette|animations?|frames?|sequence|image|map|tilemap|def)$")
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ("t", "T") and not data.search(parts[2]):
            names.add(parts[2])
    return names


def main():
    dis = subprocess.run(["m68k-elf-objdump", "-d", ROM],
                         capture_output=True, text=True, check=True).stdout
    code = code_symbols(ROM)

    func = "?"
    prev = None
    bad = []

    for line in dis.splitlines():
        m = re.match(r"^([0-9a-f]+) <(\S+)>:", line)
        if m:
            func, prev = m.group(2), None
            continue

        m = re.match(r"^\s+([0-9a-f]+):\t[0-9a-f ]+\t(\S+)\s+(.*)$", line)
        if not m:
            continue

        addr, op, args = m.groups()
        dest = re.match(r"^(.*),%(a\d)@$", args)

        if op.startswith("move") and dest:
            if prev and prev[0] == op and prev[1] == dest.group(2):
                if (func in code) and not ALLOW.match(func):
                    bad.append((func, addr, prev[2], args))
            prev = (op, dest.group(2), args)
        else:
            prev = None

    for func, addr, first, second in bad:
        print(f"{func} +{addr}: {first}  y luego  {second}")

    if bad:
        print(f"\n{len(bad)} escrituras duplicadas al mismo destino: "
              f"revisa el desensamblado antes de dar la build por buena.")
        return 1

    print(f"Sin escrituras duplicadas sospechosas ({len(code)} funciones revisadas).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
