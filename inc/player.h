#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>
#include "arena.h"
#include "entity.h"

// ===========================================================================
// Jugador (oPlayer)
//
// Movimiento directo sin inercia: acc = 6 px/frame sumado por eje mientras
// se pulsa la dirección, sin aceleración ni fricción, y con wrap toroidal en
// los cuatro bordes con margen offSet = 15 (stPlayerMovement). En diagonal el
// original no normaliza: suma acc a los dos ejes, así que se mueve un 41% más
// rápido en diagonal. Portado tal cual.
//
// La posición es el ORIGEN del sprite, igual que en GameMaker, no la esquina
// superior izquierda: el dibujado resta el origen y la colisión usa la bbox
// de la máscara (ver PLAYER_BOX_* en arena.h).
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
    s16    controlTime;  // cadencia de disparo (controlTime del GML)
} Player;

void Player_init(Player *p);

// Lee el pad y mueve el jugador. El original lee flechas o joystick con zona
// muerta de 0.3 (oControles/Step_1); el pad de Mega Drive es digital, así que
// el D-Pad equivale a las cuatro teclas.
void Player_update(Player *p, u16 pad);

// direction = point_direction(x, y, target.x, target.y): el apuntado es
// instantáneo, no gradual (el giro gradual es cosa de los enemigos).
void Player_aim(Player *p, fix16 tx, fix16 ty);

// Descuenta la cadencia y devuelve TRUE el frame en que toca disparar.
bool Player_tickFire(Player *p);

// Frame de rotación más cercano al rumbo real, redondeando a múltiplos de
// 11,25 grados (32 direcciones). El original rota el sprite de forma continua
// (image_angle = direction - 90); el VDP no rota sprites, así que se eligen
// frames pre-rotados, que viven en ROM y se suben por DMA al cambiar (ver
// VStream en inc/vramspr.h).
u8 Player_facing(const Player *p);

#endif
