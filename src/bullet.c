#include "bullet.h"
#include "resources.h"
#include "arena.h"
#include "enemy.h"
#include "snake.h"

#define BULLET_SPEED 4

typedef struct
{
    bool alive;
    s16  x;
    s16  y;
    s8   dx;    // -1, 0 or 1 per axis: same 8-direction simplification
    s8   dy;    // used for enemy homing (no atan2 on this hardware)
    Sprite *sprite;
} BulletSlot;

static BulletSlot bullets[MAX_BULLETS];

static s8 sign(s16 v)
{
    if (v > 0) return 1;
    if (v < 0) return -1;
    return 0;
}

void Bullet_initAll(void)
{
    u16 i;

    PAL_setPalette(PAL3, bulletSprite.palette->data, DMA);

    for (i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].alive = FALSE;
        bullets[i].sprite = SPR_addSprite(&bulletSprite, -BULLET_W, -BULLET_H, TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
    }
}

void Bullet_fire(s16 x, s16 y, s16 targetX, s16 targetY)
{
    u16 i;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].alive)
        {
            bullets[i].alive = TRUE;
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].dx = sign(targetX - x);
            bullets[i].dy = sign(targetY - y);
            SPR_setPosition(bullets[i].sprite, x, y);
            return;
        }
    }
}

void Bullet_updateAll(void)
{
    u16 i;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].alive) continue;

        bullets[i].x += bullets[i].dx * BULLET_SPEED;
        bullets[i].y += bullets[i].dy * BULLET_SPEED;

        if ((bullets[i].x < -BULLET_W) || (bullets[i].x > ARENA_W) ||
            (bullets[i].y < -BULLET_H) || (bullets[i].y > ARENA_H))
        {
            bullets[i].alive = FALSE;
            SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
        }
        else
        {
            SPR_setPosition(bullets[i].sprite, bullets[i].x, bullets[i].y);
        }
    }
}

u16 Bullet_checkHits(void)
{
    u16 i;
    u16 kills = 0;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        bool killed;

        if (!bullets[i].alive) continue;

        if (Enemy_hit(bullets[i].x, bullets[i].y, BULLET_W, BULLET_H))
        {
            bullets[i].alive = FALSE;
            SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
            kills++;
        }
        else if (Snake_hit(bullets[i].x, bullets[i].y, BULLET_W, BULLET_H, &killed))
        {
            bullets[i].alive = FALSE;
            SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
            if (killed) kills++;
        }
    }

    return kills;
}

void Bullet_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].alive = FALSE;
        SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
    }
}
