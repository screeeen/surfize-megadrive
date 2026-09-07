#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>

typedef struct
{
    s16 x;      // pixel position, top-left of the sprite box
    s16 y;
    u8  facing; // 0-7: sprite frame index, see Player_faceTarget()
} Player;

void Player_init(Player *p);

// Reads the joypad state and moves the player at a constant speed.
// Boundary behavior matches playerMovement.move() in the original Unity
// game: leaving one edge of the arena wraps the ship to the opposite edge.
void Player_update(Player *p, u16 state);

// Turns the ship to face (targetX, targetY), snapped to the nearest of 8
// pre-rendered rotation frames (0=up, clockwise in 45-degree steps -- see
// player.png). Ported from playerMovement.rotateShipDirection(): the
// original computes a continuous angle via atan2 and applies it as a
// transform rotation; there's no continuous rotation on this hardware, so
// this picks whichever of the 8 frames points closest to the target by
// comparing dot products against each frame's direction vector -- exact
// same "closest angle" result as atan2 + rounding would give, just without
// needing an angle or a division. Leaves facing unchanged if the caller has
// no target (same as the original only rotating when getClosest() finds one).
void Player_faceTarget(Player *p, s16 targetX, s16 targetY);

#endif
