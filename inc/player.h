#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>

typedef struct
{
    s16 x;      // pixel position, top-left of the sprite box
    s16 y;
} Player;

void Player_init(Player *p);

// Reads the joypad state and moves the player at a constant speed.
// Boundary behavior matches playerMovement.move() in the original Unity
// game: leaving one edge of the arena wraps the ship to the opposite edge.
void Player_update(Player *p, u16 state);

#endif
