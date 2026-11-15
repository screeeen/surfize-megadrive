#ifndef _MENU_H_
#define _MENU_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Splash, menú e interludio (oMenu) y estados de aplicación
//
// El original es de máquina recreativa: arranca en una pantalla de splash con
// una marquesina de texto y un contador de créditos, y no deja empezar sin
// meter uno (hasCredits() en oMenu/Step_0). Al morir vuelve al menú, que
// escribe KILLS / TOP / NEXT letra a letra, pinta una barra gris que crece y
// deja elegir arma entre las desbloqueadas con izquierda y derecha.
//
// Controles del original: cursores para mover, X (o el botón 1 del joystick)
// para empezar, C para meter crédito. En Mega Drive: D-Pad, A/B/C para
// empezar y START para meter crédito.
// ===========================================================================

// Posiciones de oMenu/Create_0 escaladas x0.2 (ver docs/PORT_AUDIT_GML.md)
#define MENU_STEP           GML_LEN(12)         // STEP: crecimiento de la barra
#define MENU_FIRST_STEP     GML_LEN(300)        // FIRST_STEP: tope de la barra
#define MENU_TYPE_DELAY     GML_FRAMES(4)       // alarm[0] = 4 por letra
#define MENU_SCROLL_SPEED   2                   // scroll -= 2 por frame

void Menu_init(void);

// Sube los tiles del menú (a la VRAM sobrante, ver VTile_allocSpare).
void Menu_loadTiles(void);

// Dibuja y actualiza el splash o el interludio. Devuelve TRUE cuando hay que
// empezar partida (botón con crédito disponible).
bool Menu_update(u16 pad, bool firstTime);

// Entra en el menú tras morir (script menu()).
void Menu_enter(void);

void Menu_leave(void);

#endif
