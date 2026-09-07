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

// Finds the enemy closest to (x, y) by squared distance, same target choice
// as enemyController.getClosest() (used by both playerMovement's auto-aim
// rotation and disparo's auto-fire). Returns FALSE if no enemy is alive.
bool Enemy_closest(s16 x, s16 y, s16 *outX, s16 *outY);

// AABB test of a single box against the enemy pool; deactivates and hides
// the first overlapping enemy and returns TRUE. Spiders die in one hit here
// (enemyLife's HP system isn't ported -- it's only used by "snake").
bool Enemy_hit(s16 x, s16 y, s16 w, s16 h);

// Deactivates every enemy and hides its sprite (enemyController.clearEnemies()).
void Enemy_clearAll(void);

u16 Enemy_countAlive(void);

#endif
