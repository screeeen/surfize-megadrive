#ifndef _DEBRIS_H_
#define _DEBRIS_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// oEstallido y su metralla (oDestro)
//
// oEstallido/Create_0:
//     if (global.explosions) {
//         for (i = 0; i < 360; i += 30) {
//             b = instance_create(x, y, oDestro)
//             b.speed = 10
//             b.direction = i
//         }
//     }
//     stShake(5, 15, 1)
//     instance_destroy()
//
// Doce astillas en abanico completo, a velocidad fija, desde un punto. No es
// adorno: oDestro desciende de oBulletS y los enemigos tienen manejador para
// él (oSpider/oCabeza1/oCabeza2b Collision_oDestro: "hp -= 1" + explosión +
// destruye la astilla), así que la metralla MATA. Una ráfaga que caiga en
// medio de un grupo de arañas se lo lleva por delante en cadena.
//
// Quién la lanza, en el original que está en uso:
//   - oWBlock/Collision_oMalo  (el arma por defecto)
//   - oOption/Collision_oMalo  (los satélites del PIVOT)
//   - oAutoDestruccion/Alarm_0 (el impacto del PARA, a los 80 frames)
//   - oExplosionSmall x3       (sólo lo crea la bomba, que no es seleccionable)
//
// Y OJO con cuándo: los dos primeros manejadores sólo corren si el proyectil
// toca un ESLABÓN de gusano. Contra una araña o una cabeza gana el manejador
// del enemigo, que destruye el proyectil antes. Ver inc/snake.h.
//
// Las astillas mueren al salir de la vista (Other_40 -> action_kill_object),
// sin wrap, al revés que todo lo demás del juego.
// ===========================================================================

void Debris_initAll(void);

// oEstallido: la ráfaga de 12, con su sacudida.
void Debris_burst(fix16 x, fix16 y);

// Avanza, resuelve impactos y devuelve las muertes de este frame; deja en
// *snakeKills las que eran cabezas, igual que Proj_update.
u16 Debris_updateAll(u16 *snakeKills);

void Debris_clearAll(void);

#endif
