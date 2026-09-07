#include "player.h"
#include "arena.h"

#define SPEED 2

void Player_init(Player *p)
{
    p->x = (ARENA_W - PLAYER_W) / 2;
    p->y = (ARENA_H - PLAYER_H) / 2;
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
