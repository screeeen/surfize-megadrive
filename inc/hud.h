#ifndef _HUD_H_
#define _HUD_H_

#include <genesis.h>

// ===========================================================================
// HUD de partida (oInGameHUD/Draw_0)
//
// Disposición del original, en coordenadas de room escaladas x0.2:
//   "BULLETS<n>"  (0, 0)      alineado a la izquierda
//   "<n>KILLS"    (1600, 0)   alineado a la derecha  -> x = 320
//   "LAST  <n>"   (1600, 82)  alineado a la derecha  -> y = 16, fila 2
// El contador de FPS del original (132, 52) era de desarrollo.
//
// Bloque 1 de MonsterLite (provisional hasta el bloque 9): a la derecha va la
// PUNTUACIÓN (u32, 7 cifras grandes) en vez de las KILLS, y la fila 2 lleva
// "LV n UP n LAST n ... SCORE" con la fuente del sistema. LAST es ahora la
// mejor puntuación de la sesión. Ver src/hud.c.
// "TOP" sólo aparece en el menú (oMenu/Draw_0), que es FASE 8.
//
// La fuente de sprites del original (sFont2/sFont3) llega en FASE 6; aquí se
// usa la del sistema. VDP_drawText cuesta demasiado para llamarlo cada
// frame, así que cada línea se redibuja sólo cuando su valor cambia.
// ===========================================================================

// Sube los glifos del HUD a VRAM. Una vez, antes de cualquier HUD_init().
void HUD_initGraphics(void);

void HUD_init(void);
void HUD_update(void);

// Escribe 's' con los glifos grandes de sFont2 (2x2 tiles por letra) en el
// plano A, desde la columna 'col' y la fila 'row'. Lo usa también el menú.
// Los caracteres que no están en la hoja salen en blanco (espacio).
void HUD_drawBig(const char *s, s16 col, s16 row);

// Lectura de desarrollo: FPS y carga de CPU, en el mismo sitio donde el
// original pintaba su contador (oInGameHUD dibuja "FPS = " en 132,52).
// Instrumento de FASE 10; se quita cuando el rendimiento esté validado.
#define HUD_SHOW_PERF 0

#endif
