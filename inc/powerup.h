#ifndef _POWERUP_H_
#define _POWERUP_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Lo que espera en el suelo: paquetes de munición (oPaquete) y los tres
// power-ups (oBoomPowUp, oNaveReplicaPowUp, oRayosPowUp), más generaPaquete()
//
// En el original son instancias estáticas: no se mueven, sólo laten o giran
// al dibujarse. Como no se mueven, aquí se pintan como bloques de tiles del
// plano A y no gastan ni un sprite de hardware, que es lo que permite tener
// los 24 del tope original en pantalla. Coste: la posición se ajusta a la
// rejilla de 8 px y se pierde el latido.
//
// Todos mueren al tocar un agujero (Collision_oAgujerSuper).
// ===========================================================================

typedef enum
{
    POW_PACK = 0,       // oPaquete: munición
    POW_BOOM,           // Bomba de MonsterLite, con el arte de oBoomPowUp
    POW_REPLICA,        // oNaveReplicaPowUp: ya no sale (la holo es un proyectil)
    POW_RAYS,           // RayosBomba de MonsterLite, con el arte de oRayosPowUp
    POW_COIN            // Coin de MonsterLite (bloque 4): sólo en PowerPickup
} PowerType;

typedef struct
{
    u8    type;         // PowerType del recogido, sólo válido si TRUE
    fix16 x;
    fix16 y;
    u16   ammo;         // munición si era un paquete
    u16   value;        // monedas si era una moneda
} PowerPickup;

void Pow_initAll(void);

// El latido de oPaquete y compañía, la caducidad del paquete y de la moneda
// (167 f), la subida del paquete recogido y el vuelo de la moneda. Una vez
// por frame, también fuera de la partida (en MonsterLite van en Update).
void Pow_updateAll(void);

// Con el tiempo parado (Q8): sólo lo que en MonsterLite va en Update sin
// deltaTime, la caducidad y la subida del paquete.
void Pow_tickFrozen(void);

// leavePow() de MonsterLite (bloque 4): lo que suelta un enemigo muerto por
// un disparo, 50 % moneda y 50 % paquete; packOnly para la bola CIRCULAR
// contra una araña, que siempre suelta paquete. Sustituye a generaPaquete()
// del GML, que ya no se usa (los power-ups vendrán con las oleadas, bloque 7).
void Pow_drop(fix16 x, fix16 y, bool packOnly);

// Recoge lo que toque la nave: en MonsterLite todo lo que recoge el jugador
// lo toca su esfera de 3,6 px (OnTriggerEnter del prefab player), así que es
// el círculo de centro (cx, cy) contra la caja del objeto. TRUE si ha cogido
// algo.
// Las oleadas 3 y 4 de MonsterLite (bloque 7): la bomba o los rayos en
// (x, y). No caducan. FALSE si ya hay ML_GROUND_POW_MAX de los dos.
bool Pow_spawnML(u8 type, fix16 x, fix16 y);

bool Pow_collect(s16 cx, s16 cy, u8 weaponIndex, PowerPickup *out);

u16 Pow_count(u8 type);

// El rayo de LASER y MOIRE choca con lo que hay en el suelo (en MonsterLite
// paquetes, bombas, rayos y naves holo son triggers, y el rayo los ve):
// distancia a lo largo del rayo, en 1/64 px, del primero que corta, o -1.
s32 Pow_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s);

void Pow_clearAll(void);

// Lo que espera en el suelo NO se borra al morir ni al empezar partida:
// mataPlayer() y startGame() sólo destruyen oPlayer, oMalo y oMenu, y los
// paquetes, la bomba, las réplicas y los rayos del suelo no son ninguno de
// ellos. Esto sólo los vuelve a colocar con la cámara recentrada.
void Pow_replaceAll(void);

#endif
