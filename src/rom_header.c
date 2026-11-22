#include <genesis.h>

// ===========================================================================
// Cabecera del cartucho
//
// SGDK copia aquí su plantilla (src/boot/rom_header.c) SÓLO si el fichero no
// existe: es una dependencia "order-only" en makefile.gen, así que lo de aquí
// no se pisa en cada build. Venía con la plantilla sin tocar, o sea
// "SAMPLE PROGRAM" / "DEMONSTRATION PROGRAM", que es lo que verían el menú de
// un flashcart y cualquier base de datos de ROMs.
//
// Notas de los campos que no son obvios:
//
//   copyright     formato "(C)XXXX AAAA.MMM". SCIM = Screen Implosion, que
//                 es quien firma el juego (la marquesina del GML lleva el
//                 nombre viejo, REDEFINETECLAS; el autor ha confirmado cuál
//                 es el bueno y es el que va aquí y en el splash).
//   rom_end       la ÚLTIMA dirección del cartucho, no su tamaño. Tiene que
//                 cuadrar con el ROM de verdad; lo comprueba
//                 tools/check_header.py en cada build para que no se quede
//                 atrás cuando el ROM crezca. 0x11FFFF desde el bloque 12
//                 (1,125 MB: los sonidos de MonsterLite, menos los del GML que
//                 ya no se usaban); antes 0x0FFFFF. Sin mapper llega hasta
//                 0x3FFFFF (4 MB).
//   sram_*        sin SRAM: en blanco (espacios), que es como se marca un
//                 cartucho sin RAM de respaldo. Decisión del usuario del
//                 2026-11-20: el progreso (récord, monedas, armas compradas,
//                 etc.) vive sólo en RAM y se pierde al apagar. El bloque 6
//                 llegó a declarar 8 KB en 0x200001-0x203FFF y se quitó;
//                 tools/check_header.py comprueba que no vuelva. Un "RA"
//                 aquí hace que emuladores y flashcarts reserven un fichero
//                 de guardado que no se usa.
//   IOSupport     "J" = mando de 3 botones, que es lo que pide el juego
//                 (cruceta, A/B/C y START). La plantilla ponía "JD", y la D
//                 no es un código válido.
// ===========================================================================

__attribute__((externally_visible))
const ROMHeader rom_header = {
    "SEGA MEGA DRIVE ",
    "(C)SCIM 2026.NOV",
    "SURFIZE                                         ",
    "SURFIZE                                         ",
    "GM 00000000-00",
    0x000,
    "J               ",
    0x00000000,
    0x0011FFFF,
    0xE0FF0000,
    0xE0FFFFFF,
    "  ",
    0x2020,
    0x20202020,
    0x20202020,
    "            ",
    "SCREEN IMPLOSION 2012 - MEGA DRIVE PORT ",
    "JUE             "
};
