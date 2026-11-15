#ifndef _GFXDEBUG_H_
#define _GFXDEBUG_H_

#include <genesis.h>

// ===========================================================================
// Depurador de gráficos en el propio juego (sólo con PORT_DEBUG_GFX=1).
//
//   make GFXDEBUG=1        (tras un `touch src/*.c inc/*.h src/*.s`)
//
// Para buscar por qué desaparecen trozos de gráficos. En la Mega Drive eso
// casi siempre es uno de estos límites del VDP, y el panel los mide en cada
// frame sobre la tabla de sprites que se manda de verdad al VDP:
//
//   - más de 20 sprites en una misma línea de pantalla: el VDP deja de pintar
//     los que van detrás en la tabla (en H40);
//   - más de 40 celdas (320 px) de sprite en una línea: lo mismo, y aquí se
//     corta a mitad de sprite, que es lo que se ve como "un trozo que falta";
//   - más de 80 sprites en total: el montador corta la tabla en 80 y los
//     últimos no se mandan;
//   - un sprite en x = 0 (en coordenadas del VDP): hace de MÁSCARA y oculta los
//     sprites que van detrás en esas líneas;
//   - la cola de DMA por encima de su capacidad por frame: subidas que se
//     salen del vblank o se pierden.
//
// Controles, en partida (el disparo es automático y A y C no hacen nada en
// partida; B es el botón que hace correr el tiempo, Q8 del bloque 2 de
// MonsterLite):
//
//   START  panel: apagado -> números -> números + gráfica por filas
//   A      sprites encendidos / apagados
//   B      plano B (el fondo) encendido / apagado
//   C      plano A (agujeros y HUD) encendido / apagado
//
// A, B y C sólo valen con el panel abierto. Con el panel abierto el tiempo
// de la partida NO se para al soltar B (decisión del usuario), así que B
// queda para el depurador; con el panel cerrado, B vuelve a ser el del
// tiempo.
//
// Los planos se "apagan" poniendo su línea de paleta al color de fondo (el
// VDP no tiene un interruptor por plano): el fondo usa PAL3 y los agujeros y
// el HUD PAL2, así que se ve exactamente lo que quedaría sin ellos.
//
// Panel (filas 27-29, abajo):
//   SPR n/80 W m      sprites de hardware en la tabla / los que se pedían
//                     (si W > 80, los W-80 últimos no salen)
//   LN s@y C c@y      máximo de sprites (s) y de celdas (c) en una línea, y en
//                     qué línea; con "!" si pasa del límite
//   OV o MK k         líneas por encima de algún límite y líneas con máscara
//   DMA b/8000 q/80   bytes y transferencias en la cola este frame
//   HW                el VDP marcó desbordamiento de sprites (bit 6 de estado)
// Los máximos son de los últimos 8 frames.
//
// Gráfica (columnas 38-39): por cada fila de 8 líneas, el máximo de sprites
// en una línea (0-9, A-K = 10-20, * = más de 20) y una marca si alguna línea
// pasa de 40 celdas (#) o tiene máscara (M).
// ===========================================================================

#ifndef PORT_DEBUG_GFX
#define PORT_DEBUG_GFX 0
#endif

#if PORT_DEBUG_GFX
void GfxDebug_input(u16 pad);
// Después de Spr_flush y antes del vblank.
void GfxDebug_frame(void);
// TRUE con el panel abierto. Decisión del usuario (bloque 2): con el panel
// abierto el tiempo de la partida NO se congela al soltar B, y B sigue
// siendo del depurador (plano B). Ver timeStopped en src/game.c.
bool GfxDebug_panelOpen(void);
#else
#define GfxDebug_input(pad)   ((void)0)
#define GfxDebug_frame()      ((void)0)
#define GfxDebug_panelOpen()  (FALSE)
#endif

#endif
