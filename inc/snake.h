#ifndef _SNAKE_H_
#define _SNAKE_H_

#include <genesis.h>

// Ported from enemyController.createSnake() + headScr.cs's trailing body
// segments, using headScr's own commented-out alternative implementation
// (a per-frame position history read back with a fixed delay per segment)
// instead of its breadcrumb-list version -- same lag-follow effect, simpler
// on this hardware. The head reuses spider's homing (enemyMov's ATTACK
// case) but takes more hits to kill, standing in for enemyLife's HP.
//
// Body segments have no HP and can't be shot -- Bullet.OnTriggerEnter in
// the original only ever checks the "arana"/"snake" (head) tags, never
// "snakeBody" -- but they do end the run on touch, same as playerMovement's
// "snakeBody" case.
//
// Sprites share PAL2 with spiderSprite: the hardware only has 4 sprite
// palette lines and they're already spoken for by text/player/enemy/bullet,
// so snake_head.png/snake_seg.png are drawn in spider's exact two colors
// and told apart by shape/size instead.
#define MAX_SNAKES      2
#define SNAKE_SEGMENTS  3
#define SNAKE_HEAD_HP   3
#define SNAKE_W         16
#define SNAKE_H         16

void Snake_initAll(void);
void Snake_spawnAtEdge(void);
void Snake_updateAll(s16 targetX, s16 targetY);

// Same target choice as Enemy_closest(), scoped to snake heads only; callers
// combine this with Enemy_closest() to pick the nearest threat overall.
bool Snake_closest(s16 x, s16 y, s16 *outX, s16 *outY);

// Hits only ever land on the head. Returns TRUE if a head was hit at all
// (the bullet should be consumed either way); *killed is set TRUE only if
// this hit brought that head's HP to 0 (which also clears its segments).
bool Snake_hit(s16 x, s16 y, s16 w, s16 h, bool *killed);

// Collision against a head OR any of its body segments.
bool Snake_collides(s16 px, s16 py, s16 pw, s16 ph);

void Snake_clearAll(void);

#endif
