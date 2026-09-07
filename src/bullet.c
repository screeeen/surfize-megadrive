#include "bullet.h"
#include "resources.h"
#include "arena.h"
#include "enemy.h"
#include "snake.h"

#define BULLET_SPEED 4

typedef struct
{
    bool alive;
    fix16 x;    // sub-pixel position: plain integer speed can't hold an
    fix16 y;    // arbitrary angle without drifting off it frame to frame
    fix16 dx;   // per-frame velocity, fixed at fire time (real angle, not
    fix16 dy;   // the old 8-direction sign() approximation)
    Sprite *sprite;
} BulletSlot;

static BulletSlot bullets[MAX_BULLETS];

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
            // Real angle to the target (SGDK's F16_getAngle is atan2 under
            // the hood, 1-degree table resolution) instead of snapping to
            // one of 8 fixed directions -- disparo.dispara() in the
            // original aims the same way, via transform.rotation.
            const fix16 angle = F16_getAngle(FIX16(x), FIX16(y), FIX16(targetX), FIX16(targetY));

            bullets[i].alive = TRUE;
            bullets[i].x = FIX16(x);
            bullets[i].y = FIX16(y);
            bullets[i].dx = F16_mul(F16_cos(angle), FIX16(BULLET_SPEED));
            bullets[i].dy = F16_mul(F16_sin(angle), FIX16(BULLET_SPEED));
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
        s16 px, py;

        if (!bullets[i].alive) continue;

        bullets[i].x += bullets[i].dx;
        bullets[i].y += bullets[i].dy;
        px = F16_toInt(bullets[i].x);
        py = F16_toInt(bullets[i].y);

        if ((px < -BULLET_W) || (px > ARENA_W) || (py < -BULLET_H) || (py > ARENA_H))
        {
            bullets[i].alive = FALSE;
            SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
        }
        else
        {
            SPR_setPosition(bullets[i].sprite, px, py);
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
        s16 px, py;

        if (!bullets[i].alive) continue;

        px = F16_toInt(bullets[i].x);
        py = F16_toInt(bullets[i].y);

        if (Enemy_hit(px, py, BULLET_W, BULLET_H))
        {
            bullets[i].alive = FALSE;
            SPR_setPosition(bullets[i].sprite, -BULLET_W, -BULLET_H);
            kills++;
        }
        else if (Snake_hit(px, py, BULLET_W, BULLET_H, &killed))
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
