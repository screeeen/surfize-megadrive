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
// cuantizado a grados enteros (se trunca). El original usa flotante; la
// desviación máxima de rumbo es de un grado.
// ===========================================================================

// enemyMov.enemyState de MonsterLite (S/enemyMov.cs:24-28). Sustituye desde
// el bloque 8 a la máquina de 3 estados del GML (stSteadyW, stAppearA1W,
// stWalkingA1W).
typedef enum
{
    ENT_ATTACK = 0,     // va hacia el jugador (o una nave holo si no lo hay)
    ENT_WANDER          // va hacia un punto al azar de la pantalla
} EntityState;

typedef struct
{
    fix16 x;            // posición en píxeles de pantalla, 1/64
    fix16 y;
    fix16 angle;        // rumbo en grados (convención SGDK), 1/64
    fix16 speed;        // píxeles por frame (setSpeed de MonsterLite)
    fix16 tx, ty;       // target de enemyMov: hacia dónde gira
    fix16 rx, ry;       // randomPos de enemyMov: el destino de WANDER
    s16   timer;        // timer de enemyMov, en quintos (ML_AI_TIMER_*)
    u16   turnK;        // fracción del Slerp por frame, Q12 (ML_TURN_K_*)
    u8    state;        // EntityState
    u8    visible;      // los enemigos nacen visibles (bloque 8); la nave también
    u8    active;       // la instancia existe (ocupa ranura del pool)
    s8    hp;
} Entity;

// enemyMov.setSpeed() de MonsterLite (S/enemyMov.cs:41-53), la velocidad de
// avance:
//   f = (level % 10) + 0.8;  speed = Random.Range (f, maxSpeed + f / 10)
// en u/s, que a 50 Hz son x 0.4 px/frame. maxMu es maxSpeed en milésimas
// (ML_SPIDER_MAX_MU, ML_SNAKE_MAX_MU). Random.Range de float con el mínimo
// por encima del máximo sortea entre los dos igual, así que se ordenan.
// El sorteo es en milésimas de u/s; el resultado se redondea al 1/64 de
// px/frame del fix16 (error de hasta medio paso, ver la auditoría).
fix16 Entity_mlSpeed(u16 level, u16 maxMu);

// globales.getRandomPos(): un punto entre el 10 y el 90 % de la pantalla en
// cada eje (x 32-287, y 24-215).
void Entity_mlRandomPos(fix16 *x, fix16 *y);

// Deja la entidad inactiva y fuera de pantalla.
void Entity_clear(Entity *e);

// enemyMov.Start() (S/enemyMov.cs:32-39): crea el enemigo en (x, y) ya
// visible y en marcha, sin la espera de 75 f en el agujero del GML. Rumbo
// al azar de 0 a 360, randomPos, setSpeed (speed ya sorteada por quien
// llama, con Entity_mlSpeed) y turningSpeed = Random.Range(1,
// turningSpeedMax / 1.4) como fracción Q12 hasta turnKMax
// (ML_SPIDER_TURN_K_MAX, ML_SNAKE_TURN_K_MAX), y el estado al azar.
void Entity_spawnML(Entity *e, fix16 x, fix16 y, fix16 speed, u16 turnKMax, s8 hp);

// point_direction(x, y, tx, ty)
fix16 Entity_angleTo(const Entity *e, fix16 tx, fix16 ty);

// Avanza speed píxeles en el rumbo actual (el movimiento por speed+direction
// de GameMaker).
void Entity_advance(Entity *e);

// stPlayerMovement: wrap toroidal en los cuatro bordes con margen.
void Entity_wrapPlayfield(Entity *e, s16 margin);

// enemyMov.move() (S/enemyMov.cs:80-115), el tick de FixedUpdate: elige el
// destino según el estado, avanza speed en el rumbo que llevaba
// (Translate(right * step)) y gira hacia el destino una fracción turnK del
// ángulo que falta (Slerp). En WANDER el temporizador baja además level %
// 100 por tick.
//
// hasTarget/(tx, ty) es el destino de ATTACK: el jugador o, si no lo hay,
// una nave holo (lo elige quien llama). Sin ninguno de los dos, el enemigo
// sigue girando hacia el último destino que tuvo, como target en Unity.
void Entity_moveML(Entity *e, bool hasTarget, fix16 tx, fix16 ty);

// enemyMov.Update() (S/enemyMov.cs:55-69), a 60 Hz: el temporizador baja 1,2
// por frame de 50 Hz y, al pasar de 0, se re-sortean el estado y randomPos
// y vuelve a 30 (25,8 f). Corre también con el tiempo parado (Update no se
// para con timeScale 0), pero no durante la congelación de la bomba y los
// rayos (Thread.Sleep).
void Entity_tickML(Entity *e);

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
