#ifndef _HUD_H_
#define _HUD_H_

#include <genesis.h>

// ===========================================================================
// HUD de partida (bloque 9 de MonsterLite, S/HUD.cs)
//
// Lo que pinta MonsterLite en apaisado, y dónde va aquí (columnas y filas de
// 8 px; las filas de abajo se calculan con el alto que se ve, 240 o 224):
//
//   línea de arriba        "BULLETS<n> <n>SCORE"       filas 0-1, glifos grandes
//                           (balas pegadas a BULLETS, puntuación pegada a
//                           SCORE, alineada a la derecha; ver drawTopLine)
//   debajo, a la izquierda  icono del arma y su nombre  filas 2-3 / fila 2
//   a la derecha            "<best>BEST" (en amarillo)  fila 2
//                           "LEVEL <n>"                 fila 3
//                           "UPGRADE AT: <n>"           fila 4
//   abajo, centrado         "COINS <saldo>" (amarillo)  fila 28 (26 en NTSC)
//   mensajes                "ARENA!"                    fila 5
//                           "LEVEL n" / "NEXT UPGRADE: SCORE x!"  filas 9-10
//                           " !!!" (en rojo)            fila 12
//                           "HOLD A/B/C"                fila 20
//                           "ACHIEVEMENT! <logro> +<n>" fila 26 (24 en NTSC),
//                           bloque 10, invención (ver src/hud.c)
//
// La línea de arriba va con los glifos grandes de sFont2 del GML (16 px, el
// tamaño de MonsterLite en apaisado: W/18), con C y R dibujadas en su
// estilo; lo demás, con la fuente del sistema (Q16). Todo con la sombra de
// MonsterLite pintada en los tiles. Las filas 0-4 bajan deslizándose al
// empezar la partida y suben al acabar, como el GUI.BeginGroup de
// MonsterLite. Medidas, colores y divergencias en docs/PORT_AUDIT_GML.md,
// "Bloque 9".
//
// Cada línea se escribe sólo cuando cambia su valor (escribir en el plano es
// un acceso al puerto del VDP por tile): ver HUD_update.
// ===========================================================================

// Sube los glifos y los tiles del HUD a VRAM. Una vez, antes de HUD_init().
void HUD_initGraphics(void);

// Repinta el HUD entero (palabras, icono del arma y cifras) en el siguiente
// HUD_update. No cambia los mensajes: los que estén puestos se repintan.
void HUD_init(void);

// HUD_init después de que alguien haya escrito encima del HUD (el
// depurador de gráficos): borra y repinta también el grupo de arriba.
void HUD_repaint(void);

// Partida nueva: fuera los mensajes que quedaran y "ARENA!" puesto.
void HUD_newGame(void);

// Quita el HUD y los mensajes de la pantalla (al entrar en el menú; en
// MonsterLite el HUD sale de la pantalla fuera de partida). Deja los
// agujeros que pasan por sus filas.
void HUD_clear(void);

// Escribe lo que ha cambiado. Una vez por frame de partida, también con el
// tiempo parado.
void HUD_update(void);

// Fuera de partida (game over), una vez por frame: el grupo de arriba sale
// deslizándose como en MonsterLite. HUD_hideNow lo da por fuera sin pintar
// (la tienda borra el plano entero).
void HUD_menuTick(void);
void HUD_hideNow(void);

// Los temporizadores de los mensajes, una vez por frame de partida (no en
// los del hitstop, en los que MonsterLite duerme el hilo). 'timeRuns' es
// FALSE con el tiempo parado (Q8): el mensaje de nivel va con el tiempo
// escalado y no cuenta; "!!!" y el salto de COINS van en Update y OnGUI y sí.
void HUD_tick(bool timeRuns);

// "ARENA!" (mientras HUD.counter < 60) y el aviso de tiempo parado (Q8).
void HUD_setArena(bool on);
void HUD_setHold(bool on);

// levelUpgrade() acaba de subir de nivel: "LEVEL n / NEXT UPGRADE..." 155 f.
void HUD_levelUp(void);

// La oleada 27: " !!!" en rojo 26 f.
void HUD_waveAlert(void);

// Escribe 's' con los glifos grandes de sFont2 (2x2 tiles por letra) en el
// plano A, desde la columna 'col' y la fila 'row'. Sólo están en VRAM los
// que usa el HUD ("BULETS", "COR" y las cifras); los demás salen en blanco.
void HUD_drawBig(const char *s, s16 col, s16 row);

// Lectura de desarrollo: FPS y carga de CPU, en el mismo sitio donde el
// original pintaba su contador (oInGameHUD dibuja "FPS = " en 132,52).
// Instrumento de FASE 10; se quita cuando el rendimiento esté validado.
#define HUD_SHOW_PERF 0

#endif
