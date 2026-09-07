#ifndef _BULLET_H_
#define _BULLET_H_

#include <genesis.h>

// Ported from disparo.dispara()'s default case (the plain "bulletThin" shot;
// WBLOCK/LASER/other weapons from WeaponsController aren't ported) and
// Bullet.cs's straight-line travel.
#define MAX_BULLETS 8
#define BULLET_W    8
#define BULLET_H    8

void Bullet_initAll(void);

// Fires one bullet from (x, y) toward (targetX, targetY) at a fixed speed.
// Direction is locked in at fire time (it doesn't home), same as the
// original: disparo aims once per shot at whatever's currently closest.
void Bullet_fire(s16 x, s16 y, s16 targetX, s16 targetY);

// Advances every active bullet and deactivates any that leave the arena.
void Bullet_updateAll(void);

// Tests every active bullet against the enemy pool (Enemy_hit); each hit
// removes both the bullet and the enemy. Returns the number of kills this
// frame, to be added to the score (globales.kills in the original).
u16 Bullet_checkHits(void);

void Bullet_clearAll(void);

#endif
