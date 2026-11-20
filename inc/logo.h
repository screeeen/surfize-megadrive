#ifndef _LOGO_H_
#define _LOGO_H_

#include <genesis.h>

// ===========================================================================
// El logo de Screen Implosion al arrancar (logo.unity y S/logoScene.cs de
// MonsterLite; bloque 5)
//
// Sobre negro, 0,8 s (40 f) con un fundido de entrada: el alfa del sprite
// empieza en 0 y sube 1 por segundo (fadeTime += deltaTime), así que al
// cargar la partida va por 0,8. El fundido de salida está comentado en el
// original: se pasa a la escena del juego de golpe.
//
// Va ANTES de Game_init, con la VRAM todavía libre: sus tiles se suben donde
// luego irá el juego y no le cuestan ni uno (quedan 7). El arte es de
// MonsterLite (decisión del usuario, 2026-11-16): tools/ml_logo.py.
// ===========================================================================

// Enseña el logo 40 f y deja la pantalla en negro y el plano A limpio.
void Logo_show(void);

#endif
