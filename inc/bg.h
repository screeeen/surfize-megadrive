#ifndef _BG_H_
#define _BG_H_

#include <genesis.h>

// ===========================================================================
// Fondo del campo de juego (plano B)
//
// El original pinta el campo de un COLOR PLANO (ver ROOM_BG_COLOR). Esto no
// sale del GML: es una imagen que ha pedido el autor para ver qué tal queda.
// Está marcado como divergencia en docs/PORT_AUDIT_GML.md.
//
// Hay varios fondos seleccionables (de momento con arriba/abajo en el menú;
// el autor concretará cómo se eligen). Cada imagen son 40x30 celdas de 8x8 y
// ninguna se repite: guardarlas todas serían 1200 tiles de los 1612 de la
// VRAM. tools/bg2md.py la reduce a un diccionario de VTILE_SHARED_TILES tiles buscando para cada celda la entrada más parecida de las que
// ya hay, probando los cuatro volteos que el VDP dibuja gratis.
//
// Va al plano B, DEBAJO del A, que es donde están los agujeros y el texto:
// en un plano el color 0 no se pinta y deja ver el plano de atrás, así que
// los agujeros se recortan solos sobre el fondo sin tocar su arte.
// ===========================================================================

#define BG_PAL PAL3

// Cuántos fondos hay en la ROM (ver la tabla de src/bg.c).
#define BG_COUNT 1

// Reparte la región de tiles y sube el primer fondo. Una sola vez al arrancar.
void Bg_init(void);

// Pinta el plano B entero (menos las filas tapadas, ver Bg_setCover).
void Bg_draw(void);

// Repinta sólo las filas first..last (las que ha tapado la banda del menú).
void Bg_drawRows(u16 first, u16 last);

// Filas que la banda del menú tiene pisadas en el plano B: un cambio de fondo
// no las toca. (-1, -1) para ninguna.
void Bg_setCover(s16 top, s16 bottom);

// Pide otro fondo; el cambio lo hace Bg_update en tres frames.
void Bg_select(u16 index);
u16  Bg_current(void);

// La paleta del fondo puesto (el fogonazo rojo del game over vuelve a ella).
const u16 *Bg_palette(void);

// Una vez por frame, haya partida o menú.
void Bg_update(void);

#endif
