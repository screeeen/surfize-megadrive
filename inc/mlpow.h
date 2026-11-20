#ifndef _MLPOW_H_
#define _MLPOW_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// La bomba y los rayos de MonsterLite (bloque 7)
//
// Lo que hacen al tocarlos. Lo que espera en el suelo vive en src/powerup.c y
// la nave holo en src/projectile.c.
// ===========================================================================

// Un rayo por enemigo como mucho.
#define MLPOW_MAX_RAYS  (MAX_SPIDERS + MAX_SNAKES)

typedef struct
{
    u8  kind;           // 0 libre, 1 araña, 2 gusano, 3 objetivo ya muerto
    u8  slot;           // ranura del enemigo en su pool
    s16 tx, ty;         // la última posición conocida del objetivo
} MLRay;

// enemyController.clearEnemies(): todas las arañas y gusanos vivos explotan,
// con el sonido de la bomba. Devuelve cuántos han muerto; quien llama suma
// level por cada uno (globales.kills += 1 * globales.level).
u16 MLPow_clearEnemies(void);

// triggerSplashBomb.rayobomba(): un rayo de (px, py) a cada araña y a cada
// cabeza. Sin enemigos no crea ninguno.
void MLPow_startRays(fix16 px, fix16 py);

// Un frame de los rayos (RayoScr.Update, que sigue con el tiempo parado):
// siguen a su objetivo y, al acabarse el temporizador, clearEnemies().
// Devuelve las muertes de este frame.
u16 MLPow_tickRays(void);

// Los rayos vivos, para dibujarlos (src/beam.c): NULL si no hay.
const MLRay *MLPow_rays(u16 *count);

void MLPow_clear(void);

#endif
