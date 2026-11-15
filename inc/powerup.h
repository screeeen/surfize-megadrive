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
    POW_BOOM,           // oBoomPowUp: mata las arañas en pantalla
    POW_REPLICA,        // oNaveReplicaPowUp: crea una nave réplica
    POW_RAYS            // oRayosPowUp: lanza un rayo
} PowerType;

typedef struct
{
    u8    type;         // PowerType del recogido, sólo válido si TRUE
    fix16 x;
    fix16 y;
    u16   ammo;         // munición si era un paquete
} PowerPickup;

void Pow_initAll(void);

// El latido de oPaquete y compañía. Una vez por frame.
void Pow_updateAll(void);

// generaPaquete(): escala según lo que haya en pantalla. Se llama desde
// checkMorte(), con la posición de la cabeza que acaba de morir.
void Pow_generate(fix16 x, fix16 y);

// Recoge lo que toque la caja del jugador. TRUE si ha cogido algo.
bool Pow_collect(s16 px, s16 py, s16 pw, s16 ph, u8 weaponIndex, PowerPickup *out);

u16 Pow_count(u8 type);

void Pow_clearAll(void);

// Lo que espera en el suelo NO se borra al morir ni al empezar partida:
// mataPlayer() y startGame() sólo destruyen oPlayer, oMalo y oMenu, y los
// paquetes, la bomba, las réplicas y los rayos del suelo no son ninguno de
// ellos. Esto sólo los vuelve a colocar con la cámara recentrada.
void Pow_replaceAll(void);

#endif
