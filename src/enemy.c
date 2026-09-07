#include "enemy.h"
#include "resources.h"
#include "arena.h"

#define ENEMY_SPEED 1

typedef struct
{
    bool alive;
    s16  x;
    s16  y;
    Sprite *sprite;
} EnemySlot;

static EnemySlot enemies[MAX_ENEMIES];

void Enemy_initAll(void)
{
    u16 i;

    PAL_setPalette(PAL2, spiderSprite.palette->data, DMA);

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].alive = FALSE;
        enemies[i].sprite = SPR_addSprite(&spiderSprite, -ENEMY_W, -ENEMY_H, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    }
}

void Enemy_spawn(s16 x, s16 y)
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].alive)
        {
            enemies[i].alive = TRUE;
            enemies[i].x = x;
            enemies[i].y = y;
            SPR_setPosition(enemies[i].sprite, x, y);
            return;
        }
    }
}

void Enemy_spawnAtEdge(void)
{
    s16 x, y;

    switch (random() & 3)
    {
        case 0: x = 0;                    y = random() % (ARENA_H - ENEMY_H); break; // left
        case 1: x = ARENA_W - ENEMY_W;     y = random() % (ARENA_H - ENEMY_H); break; // right
        case 2: x = random() % (ARENA_W - ENEMY_W); y = 0;                    break; // top
        default: x = random() % (ARENA_W - ENEMY_W); y = ARENA_H - ENEMY_H;   break; // bottom
    }

    Enemy_spawn(x, y);
}

// Same always-face-the-target heading as enemyMov's ATTACK state, stepping
// one pixel per axis toward it instead of turning smoothly (no
// Quaternion.Slerp/atan2 on this hardware).
static void stepToward(s16 *pos, s16 target)
{
    if (*pos < target) *pos += ENEMY_SPEED;
    else if (*pos > target) *pos -= ENEMY_SPEED;
}

void Enemy_updateAll(s16 targetX, s16 targetY)
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].alive) continue;

        stepToward(&enemies[i].x, targetX);
        stepToward(&enemies[i].y, targetY);
        SPR_setPosition(enemies[i].sprite, enemies[i].x, enemies[i].y);
    }
}

static bool overlap(s16 ax, s16 ay, s16 aw, s16 ah, s16 bx, s16 by, s16 bw, s16 bh)
{
    return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

bool Enemy_collides(s16 px, s16 py, s16 pw, s16 ph)
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].alive && overlap(px, py, pw, ph, enemies[i].x, enemies[i].y, ENEMY_W, ENEMY_H))
            return TRUE;
    }

    return FALSE;
}

void Enemy_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].alive = FALSE;
        SPR_setPosition(enemies[i].sprite, -ENEMY_W, -ENEMY_H);
    }
}

u16 Enemy_countAlive(void)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive) n++;

    return n;
}
