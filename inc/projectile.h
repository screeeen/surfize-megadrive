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
    PROJ_THIN,          // BulletThin de MonsterLite: la bala por defecto
    PROJ_WBLOCK,        // Bullet de MonsterLite (WBLOCK), con el arte de oWBlock
    PROJ_3WAY,          // Bullet de MonsterLite (TWAY), con el arte de o3way
    PROJ_CIRC,          // circularBullet de MonsterLite (CIRCULAR)
    PROJ_PARA,          // oBulletPara
    PROJ_BOMB,          // oBomba: mecha de 60 frames y explota
    PROJ_RAY,           // oRay del GML: ya no se crea (bloque 7)
    PROJ_OPTION,        // oOption: satélite del PIVOT
    PROJ_TRI            // naveProxy de MonsterLite: la nave holo (bloque 7)
} ProjType;

void Proj_initAll(void);

// Lanza el arma (WeaponId, inc/weapon.h) desde (x, y) con el rumbo dado.
// Devuelve TRUE si ha creado algo; la munición se gasta igual (ver
// src/game.c).
bool Proj_fireWeapon(u8 weaponIndex, fix16 x, fix16 y, fix16 angle);

// Dispara la bala por defecto (la que sale sin munición).
void Proj_fireThin(fix16 x, fix16 y, fix16 angle);

// Avanza todos los proyectiles, resuelve impactos y mechas. Devuelve las
// muertes de este frame y deja en *snakeKills las que eran cabezas.
u16 Proj_update(u16 *snakeKills);

// TRUE si hay un oBulletPara vivo: el original sólo permite uno
// ("if !instance_exists(oBulletPara)").
bool Proj_paraAlive(void);

// Las naves holo de MonsterLite (naveProxy, bloque 7). Las crea la oleada
// en (x, y), inactivas, hasta ML_HOLO_MAX; active sólo lo usa el arnés.
// FALSE si ya hay ML_HOLO_MAX o el pool está lleno.
bool Proj_spawnHolo(fix16 x, fix16 y, bool active);
u16 Proj_holoCount(void);

// Bloque 8. El destino de ATTACK sin jugador (enemyMov.move): una nave holo
// cualquiera, activa o no. FALSE si no hay ninguna.
bool Proj_anyHolo(fix16 *x, fix16 *y);

// Bloque 8. Fuera de la partida (game over, tienda) y en los 40 f del
// arranque no corre Proj_update, pero en MonsterLite la física sí: los
// enemigos que van a por una holo la destruyen al tocarla, activa o no
// (collideAndDie). Sólo eso: el resto de la holo sigue parado.
void Proj_holoContact(void);

// playerMovement.OnTriggerEnter con "NaveProxy": la nave toca una holo y la
// activa (setWorking). TRUE si ha activado alguna que no lo estaba.
bool Proj_holoTouch(s16 px, s16 py);

// Con el tiempo parado: la holo inactiva sigue girando (rotateInactive va en
// Update); la activa no dispara (disparoProxy mira !slowMotion).
void Proj_tickFrozen(void);

// Posición de las holo ACTIVAS: gizmosProxy traza una línea azul del jugador
// a cada una (ver inc/beam.h). Devuelve cuántas ha escrito.
u16 Proj_triPositions(fix16 *xs, fix16 *ys, u16 max);

// oPlayer/Collision_oPivot: tocar el pivote destruye satélites y réplicas.
void Proj_clearSatellites(void);

// El pivote, las réplicas, las bolas CIRCULAR y el rayo de LASER y MOIRE
// necesitan la posición del jugador, su rumbo y su objetivo.
void Proj_setPlayer(fix16 px, fix16 py, fix16 angle, u16 pad, bool hasTarget, fix16 tx, fix16 ty);

void Proj_clearAll(void);

// mataPlayer() y startGame(): sólo se destruyen oPlayer, oMalo y oMenu. Las
// naves holo de MonsterLite, igual que las réplicas del GML, sobreviven a la
// muerte y siguen en la partida siguiente (clearPowerUps no se llama). Esto borra todo lo demás (balas en
// vuelo, láser, bomba con la mecha encendida, satélites del PIVOT, que viven
// del jugador) y deja las réplicas colocadas con la cámara de ahora.
void Proj_clearKeepTris(void);

#endif
