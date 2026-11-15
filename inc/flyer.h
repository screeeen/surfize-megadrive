#ifndef _FLYER_H_
#define _FLYER_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// oPuntoMorte: el número que sale volando hacia el HUD
//
// Create_0: speed = 0.1, acc = 3, alarm = 30, type = 0.
// Step_0:   speed += acc
//           type 1 -> move_towards_point(game_width - 140, 0, speed)
//           type 0 -> move_towards_point(140, 0, speed)
// Draw_0:   dibuja global.kills (type 1) o global.playerBulletsCounter
//           (type 0) a escala 0,4.
//
// Sale de donde ha pasado algo y se va acelerando hacia su esquina del HUD:
// es lo que ata el marcador a la acción. Lo crea puntoToHUD(), desde
// morteGusano() con type 1 y desde las dos mitades de la recogida de paquete
// con type 0 (el manejador del paquete y el del jugador, los dos).
// ===========================================================================

void Flyer_initAll(void);

// puntoToHUD(type): 0 = contador de balas, 1 = contador de kills.
void Flyer_spawn(fix16 x, fix16 y, u8 type, u16 value);

void Flyer_updateAll(void);
void Flyer_clearAll(void);

#endif
