#ifndef _PROJECTILE_H_
#define _PROJECTILE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Todo lo que lanza el jugador (oBulletThin, oWBlock, o3way, oLaser,
// oBulletPara, oBomba, oRay)
//
// Comparten un pool porque en el original sólo hay un arma activa a la vez:
// global.weapon_index se elige en el menú y no cambia durante la partida
// (oWeaponBox sólo lee izquierda/derecha "if global.isMenu").
//
// Todos los proyectiles descienden de oBulletS, así que el daño que reciben
// los enemigos es el de SU manejador (-4 la araña, -1 la cabeza de gusano),
// no el del proyectil: ver la nota de orden de eventos en el informe.
//
// Los satélites del PIVOT y las naves réplica salen del mismo pool: con el
// PIVOT equipado el original no dispara balas, así que no compiten.
// ===========================================================================

typedef enum
{
    PROJ_NONE = 0,
    PROJ_THIN,          // oBulletThin: la bala por defecto, infinita
    PROJ_WBLOCK,        // oWBlock
    PROJ_3WAY,          // o3way
    PROJ_LASER,         // oLaser
    PROJ_PARA,          // oBulletPara
    PROJ_BOMB,          // oBomba: mecha de 60 frames y explota
    PROJ_RAY,           // oRay: a los 30 frames barre la pantalla
    PROJ_OPTION,        // oOption: satélite del PIVOT
    PROJ_TRI            // oTri: nave réplica del power-up
} ProjType;

void Proj_initAll(void);

// Lanza el arma que toque desde (x, y) con el rumbo dado. Devuelve TRUE si
// ha disparado algo (para descontar munición).
bool Proj_fireWeapon(u8 weaponIndex, fix16 x, fix16 y, fix16 angle);

// Dispara la bala por defecto (la que sale sin munición).
void Proj_fireThin(fix16 x, fix16 y, fix16 angle);

// Avanza todos los proyectiles, resuelve impactos y mechas. Devuelve las
// muertes de este frame y deja en *snakeKills las que eran cabezas.
u16 Proj_update(u16 *snakeKills);

// TRUE si hay un oBulletPara vivo: el original sólo permite uno
// ("if !instance_exists(oBulletPara)").
bool Proj_paraAlive(void);

// oNaveReplicaPowUp: crea una nave réplica en (x, y), hasta MAX_TRI.
void Proj_spawnTri(fix16 x, fix16 y);

// Posición de las réplicas vivas: oPlayer/Draw_0 traza una línea azul a cada
// una (ver inc/beam.h). Devuelve cuántas ha escrito.
u16 Proj_triPositions(fix16 *xs, fix16 *ys, u16 max);

// TRUE mientras arde la mecha de un oRay: su Draw_0 llena la pantalla de
// descargas hasta que estalla (ver inc/beam.h).
bool Proj_rayBurning(void);

// oPlayer/Collision_oPivot: tocar el pivote destruye satélites y réplicas.
void Proj_clearSatellites(void);

// El pivote y las réplicas necesitan la posición del jugador y su objetivo.
void Proj_setPlayer(fix16 px, fix16 py, u16 pad, bool hasTarget, fix16 tx, fix16 ty);

void Proj_clearAll(void);

// mataPlayer() y startGame(): sólo se destruyen oPlayer, oMalo y oMenu. Las
// naves réplica (oTri) no son ni lo uno ni lo otro, así que sobreviven a la
// muerte y siguen en la partida siguiente. Esto borra todo lo demás (balas en
// vuelo, láser, bomba con la mecha encendida, satélites del PIVOT, que viven
// del jugador) y deja las réplicas colocadas con la cámara de ahora.
void Proj_clearKeepTris(void);

#endif
