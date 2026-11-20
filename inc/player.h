#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>
#include "arena.h"
#include "entity.h"

// ===========================================================================
// Jugador (oPlayer del GML, playerMovement de MonsterLite)
//
// Movimiento del GML (decisión del usuario, 2026-11-16): acc = 6 por eje
// (1,44 px/f), sin inercia y sin normalizar la diagonal; wrap toroidal con
// margen offSet = 15 (3 px). Ver PLAYER_ACC en arena.h. Nace en el centro
// de la pantalla, como en el GML (decisión del usuario al cerrar el bloque 2,
// que sustituye a Q6). Choca con un círculo de 3,6 px (bloque 2 de
// MonsterLite, ML_PLAYER_R2 en arena.h).
//
// La posición es el ORIGEN del sprite, igual que en GameMaker, no la esquina
// superior izquierda: el dibujado resta el origen. La colisión es el círculo
// alrededor de ese punto (ver Coll_circleBox).
// ===========================================================================

// La celda mide 24x24 con el origen del sprite en su centro: la rotación del
// original es alrededor del origen, que en sprite173 está en (19,19) de
// 38x58, así que la cola barre un radio de 42 px del room (8.4 en Mega
// Drive) y hace falta una celda de 24 para que no se recorte.
#define PLAYER_SPR_OFF_X (-12)
#define PLAYER_SPR_OFF_Y (-12)

typedef struct
{
    Entity e;
    u8     facing;       // frame 0-31: ver Player_facing()
    s16    controlTime;  // cadencia de disparo (shootTime de MonsterLite, ver src/game.c)
} Player;

void Player_init(Player *p);

// Lee el pad y mueve el jugador. MonsterLite lee las flechas del teclado
// (InputHelper, rama UNITY_EDITOR || UNITY_ANDROID); el D-Pad equivale a las
// cuatro teclas.
void Player_update(Player *p, u16 pad);

// Lo que se desplaza la nave este frame con el pad dado: PLAYER_ACC en cada
// eje pulsado. Lo usa también el
// pivote del GML, que se mueve con el mismo input que la nave.
void Player_padDelta(u16 pad, fix16 *dx, fix16 *dy);

// direction = point_direction(x, y, target.x, target.y): el apuntado es
// instantáneo, no gradual (el giro gradual es cosa de los enemigos).
void Player_aim(Player *p, fix16 tx, fix16 ty);

// Frame de rotación más cercano al rumbo real, redondeando a múltiplos de
// 11,25 grados (32 direcciones). El original rota el sprite de forma continua
// (image_angle = direction - 90); el VDP no rota sprites, así que se eligen
// frames pre-rotados, que viven en ROM y se suben por DMA al cambiar (ver
// VStream en inc/vramspr.h).
u8 Player_facing(const Player *p);

#endif
