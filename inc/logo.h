#ifndef _LOGO_H_
#define _LOGO_H_

#include <genesis.h>

// ===========================================================================
// El logo de Screen Implosion al arrancar (logo.unity y S/logoScene.cs de
// MonsterLite; bloque 5)
//
// En MonsterLite: sobre negro, 0,8 s (40 f) con un fundido de entrada; el
// alfa empieza en 0 y sube 1 por segundo, así que al cargar la partida va por
// 0,8, y se pasa al menú de golpe (el fundido de salida está comentado).
// Desde el 2026-11-27 (decisión del usuario) los fundidos se completan:
// entrada de 40 f hasta el blanco, 30 f quieto, salida a negro en 25 f y el
// menú entra fundiendo desde negro en MENU_FADE_IN_FRAMES (src/main.c).
//
// Va ANTES de Game_init, con la VRAM todavía libre: sus tiles se suben donde
// luego irá el juego y no le cuestan ni uno (quedan 7). El arte es de
// MonsterLite (decisión del usuario, 2026-11-16): tools/ml_logo.py.
// ===========================================================================

// Fundido de entrada del menú al salir del logo.
#define MENU_FADE_IN_FRAMES 20

// Enseña el logo (95 f) y deja la pantalla en negro y el plano A limpio.
void Logo_show(void);

#endif
