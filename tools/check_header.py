#!/usr/bin/env python3
"""Comprueba que la cabecera del cartucho cuadra con el ROM de verdad.

El campo rom_end de la cabecera es la ULTIMA direccion del cartucho. Se
escribe a mano en src/rom_header.c, asi que se queda atras en cuanto el ROM
crece: un ROM mas grande que lo que dice su cabecera es el tipo de cosa que no
se nota en un emulador y si en un flashcart.
"""
import os
import sys

ROM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "rom.bin")


def main():
    data = open(ROM, "rb").read()
    size = len(data)
    rom_end = int.from_bytes(data[0x1A4:0x1A8], "big")
    console = data[0x100:0x110].decode("ascii", "replace")
    title = data[0x150:0x180].decode("ascii", "replace").strip()
    sram = data[0x1B0:0x1B2].decode("ascii", "replace")

    print(f"  consola   {console!r}")
    print(f"  titulo    {title!r}")
    print(f"  tamano    {size} bytes (0x{size:06X})")
    print(f"  rom_end   0x{rom_end:06X}  (deberia ser 0x{size - 1:06X})")
    print(f"  sram      {sram!r}")

    bad = False
    if rom_end != size - 1:
        print(f"  ERROR: rom_end no cuadra con el tamano del ROM")
        bad = True
    if "SAMPLE" in title or "DEMONSTRATION" in title:
        print("  ERROR: la cabecera sigue siendo la plantilla de SGDK")
        bad = True

    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
