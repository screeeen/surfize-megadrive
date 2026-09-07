#ifndef _ARENA_H_
#define _ARENA_H_

// Fixed single-screen playfield: the original is a top-down arena shooter
// with a static camera (no scrolling), so the arena is just the visible
// 320x224 screen.
#define ARENA_W 320
#define ARENA_H 224

// Matches the real nave.png silhouette (traced from its alpha channel),
// nose rotated to face up.
#define PLAYER_W 24
#define PLAYER_H 32

#define ARENA_X_MIN 0
#define ARENA_X_MAX (ARENA_W - PLAYER_W)
#define ARENA_Y_MIN 0
#define ARENA_Y_MAX (ARENA_H - PLAYER_H)

#endif
