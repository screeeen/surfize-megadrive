#ifndef _ENTITY_H_
#define _ENTITY_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Modelo común de entidad
//
// Sustituye el modelo de instancias de GameMaker (x/y/speed/direction +
// alarm[] + state_machine) por una estructura plana que vive en pools
// estáticos: en Mega Drive no hay instance_create ni recolección de basura.
//
// CONVENCIÓN DE ÁNGULOS. GameMaker mide direction en grados antihorarios con
// la Y hacia arriba (0 = derecha, 90 = arriba) y mueve con
// x += cos(dir)*speed, y -= sin(dir)*speed. SGDK mide en grados horarios con
// la Y hacia abajo (F16_getAngle devuelve 90 para dy > 0) y mueve con
// x += cos(a)*speed, y += sin(a)*speed. Las dos convenciones son espejo una
// de otra (a_SGDK = 360 - dir_GML), así que las trayectorias y los giros
// resultantes son idénticos: sólo cambia el número del ángulo, que nunca se
// muestra. Se usa la de SGDK para poder aprovechar sus tablas.
//
// Precisión: el ángulo se acumula en fix16 (1/64 de grado), pero F16_sin y
// F16_cos resuelven con tabla de 1 grado, así que el rumbo efectivo queda
// cuantizado a grados enteros. El original usa flotante; la desviación
// máxima de rumbo es de medio grado.
// ===========================================================================

// stInit -> stSteadyW -> stAppearA1W <-> stWalkingA1W
typedef enum
{
    ENT_STEADY = 0,     // stSteadyW:     quieto 6 frames del GML
    ENT_ATTACK,         // stAppearA1W:   persigue al jugador 300 frames
    ENT_WANDER          // stWalkingA1W:  va a un agujero 600 frames
} EntityState;

typedef struct
{
    fix16 x;            // posición en píxeles de pantalla, 1/64
    fix16 y;
    fix16 angle;        // rumbo en grados (convención SGDK), 1/64
    fix16 speed;        // píxeles por frame en curso (speed del GML)
    fix16 velo;         // velocidad asignada al aparecer (velo del GML)
    fix16 turnRate;     // grados por frame (el GML pasa velo como giro)
    u16   alarm;        // alarm[0]: frames hasta aparecer
    u16   stateTime;    // state_time de la máquina de estados
    u8    state;        // EntityState
    u8    visible;      // visible del GML: 0 mientras corre alarm[0]
    u8    active;       // la instancia existe (ocupa ranura del pool)
    s8    hp;
} Entity;

// enemyMov.setSpeed() de MonsterLite (S/enemyMov.cs:41-53), sólo la
// velocidad de avance (el giro y la IA son del bloque 8):
//   f = (level % 10) + 0.8;  speed = Random.Range (f, maxSpeed + f / 10)
// en u/s, que a 50 Hz son x 0.4 px/frame. maxMu es maxSpeed en milésimas
// (ML_SPIDER_MAX_MU, ML_SNAKE_MAX_MU). Random.Range de float con el mínimo
// por encima del máximo sortea entre los dos igual, así que se ordenan.
// El sorteo es en milésimas de u/s; el resultado se redondea al 1/64 de
// px/frame del fix16 (error de hasta medio paso, ver la auditoría).
fix16 Entity_mlSpeed(u16 level, u16 maxMu);

// Deja la entidad inactiva y fuera de pantalla.
void Entity_clear(Entity *e);

// Equivalente de instance_create en un agujero: coloca, pone speed a 0 y
// visible a 0, y arranca alarm[0] (ENEMY_APPEAR_DELAY).
void Entity_spawnHidden(Entity *e, fix16 x, fix16 y, fix16 velo, fix16 turnRate, s8 hp);

// alarm[0] del GML: decrementa y, al llegar a 0, hace visible la entidad y le
// aplica velo (oSpider/Alarm_0 y oCabeza1/Alarm_0: "speed = velo; visible = 1").
// Devuelve TRUE si la entidad ya es visible y debe actualizarse.
bool Entity_tickAppear(Entity *e);

// point_direction(x, y, tx, ty)
fix16 Entity_angleTo(const Entity *e, fix16 tx, fix16 ty);

// turn_towards_direction(wdir, turnspeed): gira por el camino corto y encaja
// el rumbo exacto cuando la diferencia cabe en un paso de giro.
void Entity_turnTowards(Entity *e, fix16 targetAngle, fix16 turnRate);

// Avanza speed píxeles en el rumbo actual (el movimiento por speed+direction
// de GameMaker).
void Entity_advance(Entity *e);

// stPlayerMovement: wrap toroidal en los cuatro bordes con margen.
void Entity_wrapPlayfield(Entity *e, s16 margin);

// Máquina de estados de enemigo, compartida por arañas y cabezas de gusano:
// en el original los dos corren los mismos scripts (stInit -> stSteadyW ->
// stAppearA1W <-> stWalkingA1W) desde su evento Begin Step.
//
// hasPlayer a FALSE reproduce el "else" del original, que persigue un oTri
// (power-up satélite) si no hay jugador: oTri es FASE 7, así que de momento
// sin jugador el enemigo sólo sigue girando hacia su último rumbo.
//
// Devuelve TRUE si en este frame ha llevado la entidad al agujero 0 (el
// teletransporte de stWalkingA1W): la posición da un salto que no es avance,
// y el gusano lo necesita saber para su caja envolvente (ver src/snake.c).
bool Entity_updateAI(Entity *e, bool hasPlayer, fix16 px, fix16 py, s16 boxOX, s16 boxOY, s16 boxW, s16 boxH);

// Índice del frame pre-rotado que corresponde al rumbo actual: 0 = derecha y
// de ahí en sentido horario cada 45 grados, que es como tools/gm2md.py
// genera las hojas de sprites. Redondea al múltiplo de 45 más cercano, el
// equivalente de la rotación continua del original en el VDP.
u8 Entity_frame8(const Entity *e);

// Igual, pero para un ángulo cualquiera (los eslabones de cola del gusano
// apuntan a su vecino, no a su propio rumbo).
u8 Entity_angleFrame8(fix16 angle);

// Lo mismo a 32 direcciones (pasos de 11,25 grados), para las hojas de la
// nave y de las cabezas de gusano, que en el original giran de forma continua
// y se ven de sobra para que los 45 grados canten (ver inc/vramspr.h).
#define ENTITY_ROT_SMOOTH 32
u8 Entity_frame32(const Entity *e);
u8 Entity_angleFrame32(fix16 angle);

// Distancia al cuadrado entre dos puntos fix16, en píxeles al cuadrado.
// Sirve para instance_nearest sin raíces ni divisiones.
s32 Entity_distSq(fix16 ax, fix16 ay, fix16 bx, fix16 by);

#endif
