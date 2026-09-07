#include "player.h"
#include "arena.h"

#define SPEED 2

void Player_init(Player *p)
{
    p->x = (ARENA_W - PLAYER_W) / 2;
    p->y = (ARENA_H - PLAYER_H) / 2;
    p->facing = 0;
}

void Player_update(Player *p, u16 state)
{
    if (state & BUTTON_LEFT)  p->x -= SPEED;
    if (state & BUTTON_RIGHT) p->x += SPEED;
    if (state & BUTTON_UP)    p->y -= SPEED;
    if (state & BUTTON_DOWN)  p->y += SPEED;

    // Screen wrap, same as globales.min/maxX/Y + playerMovement.move().
    if (p->x < ARENA_X_MIN) p->x = ARENA_X_MAX;
    if (p->x > ARENA_X_MAX) p->x = ARENA_X_MIN;
    if (p->y < ARENA_Y_MIN) p->y = ARENA_Y_MAX;
    if (p->y > ARENA_Y_MAX) p->y = ARENA_Y_MIN;
}

// Unit vectors (x1000) for frames 0-7 of player.png: 0=up, then clockwise
// every 45 degrees (screen Y grows downward, so "up" is -y).
static const s16 dirX[8] = {    0,  707, 1000,  707,    0, -707, -1000, -707 };
static const s16 dirY[8] = {-1000, -707,    0,  707, 1000,  707,     0, -707 };

void Player_faceTarget(Player *p, s16 targetX, s16 targetY)
{
    const s32 dx = targetX - p->x;
    const s32 dy = targetY - p->y;
    s32 best = 0;
    u8 bestFacing = p->facing;
    bool found = FALSE;
    u8 k;

    for (k = 0; k < 8; k++)
    {
        const s32 dot = (dx * dirX[k]) + (dy * dirY[k]);

        if (!found || (dot > best))
        {
            found = TRUE;
            best = dot;
            bestFacing = k;
        }
    }

    p->facing = bestFacing;
}
