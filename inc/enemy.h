#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>

// Ported from enemyController.createSpider() + enemyMov.move(). Only the
// ATTACK case is kept (every enemy always steers straight for the player);
// the WANDER state, the hole-based spawn points and the per-enemy
// turning-speed Quaternion.Slerp smoothing aren't ported. "snake" (the
// segmented enemy in headScr.c) isn't ported either -- this is "spider" only.
#define MAX_ENEMIES 6
#define ENEMY_W     16
#define ENEMY_H     16

// Allocates the sprite pool. Call once at boot, after SPR_init().
void Enemy_initAll(void);

// Activates one free enemy slot at (x, y), if any is free. No-op otherwise.
void Enemy_spawn(s16 x, s16 y);

// Spawns one enemy at a random point along the arena border (stand-in for
// enemyController's posHoles spawn points, which aren't ported).
void Enemy_spawnAtEdge(void);

// Steps every active enemy one frame closer to (targetX, targetY).
void Enemy_updateAll(s16 targetX, s16 targetY);

// AABB test against the player's box; TRUE if any active enemy overlaps it.
// Matches the "arana" case of playerMovement.OnTriggerEnter -> finishGame().
bool Enemy_collides(s16 px, s16 py, s16 pw, s16 ph);

// Deactivates every enemy and hides its sprite (enemyController.clearEnemies()).
void Enemy_clearAll(void);

u16 Enemy_countAlive(void);

#endif
