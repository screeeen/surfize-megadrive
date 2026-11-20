#ifndef _SNAKE_H_
#define _SNAKE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Gusanos (oCabeza1 y oCabeza2b)
//
// Cabeza + cadena de seguidores. Los dos gusanos del GML con su longitud
// (Q2), pero con todo lo demás de MonsterLite (bloque 8): la cabeza corre la
// IA de enemyMov con los valores del prefab culobicho (maxSpeed 4,
// turningSpeedMax 10, hp 4) y los eslabones siguen las migas de headScr:
// cada vez que la cabeza se aleja 8 px de la última miga se pone otra y cada
// eslabón salta a la siguiente (ver crumbStep en src/snake.c). En el GML cada
// eslabón iba stepdelay frames por detrás del anterior.
//
// La cadena del original es cabeza -> oCabeza2 -> oCorpo[9..1] -> oCu1 ->
// oCu2 (12 seguidores en el tipo 1, 10 en el tipo 2). Dos vueltas del bucle
// de Step_1 repiten pareja sin efecto visible (variables que persisten entre
// iteraciones), así que la cadena efectiva son esos 12 y 10 eslabones.
//
// Cabeza y eslabones chocan como esferas de 9,6 px (ML_SNAKE_RADIUS_X10).
//
// Lo que sigue es el comportamiento de los eslabones frente a las armas del
// GML (PIVOT, PARA, metralla), que se queda. Las balas de MonsterLite los
// atraviesan (Snake_hitHead).
//
// Los seguidores son un ESCUDO: no se pueden matar, pero se quedan la bala.
// Un impacto contra un eslabón dispara dos manejadores del original, uno
// detrás del otro: el del eslabón, heredado de oMalo
// (Collision_oBulletS: "instance_create(x, y, oExplosion)", que no destruye la
// bala) y el de la propia bala (Collision_oMalo: "with other if hp !=
// undefined hp -= 1" e "instance_destroy()"). Los eslabones tienen hp = 10 y
// ese hp no se comprueba en ninguna parte, así que el daño no lleva a nada y
// la bala se pierde. Y también matan al tocarlos.
//
// Las cabezas no pasan por ese camino: oCabeza1 y oCabeza2b definen su propio
// Collision_oBulletS, que SUSTITUYE al de oMalo y destruye la bala antes de
// que corra el manejador de ella (ver la nota de orden de eventos del
// informe), así que de una cabeza no sale metralla.
//
// Consecuencia en juego: un gusano enroscado tapa a su propia cabeza. Hay que
// buscarle el ángulo, que es justo lo que le da sentido a tener 12 eslabones.
// ===========================================================================

// Celdas con el origen del sprite en el centro, como el resto: 24x24 para
// las partes que rotan (cabezas y los dos eslabones de cola) y 16x16 para
// los oCorpo, que en el original no rotan.
#define SNAKE_HEAD_SPR_OFF_X (-12)
#define SNAKE_HEAD_SPR_OFF_Y (-12)
#define SNAKE_ROT_SPR_OFF    (-12)
#define SNAKE_BODY_SPR_OFF   (-8)

// Reparto de sprites de la cadena del original, por índice de eslabón:
//   1            oCabeza2 -> spr_rtype_outslay_tail, rotación fija
//   2..parts-3   oCorpo   -> spr_rtype_outslay_body, sin rotar
//   parts-2      oCu1     -> spr_rtype_outslay_tail, mira a oCu2
//   parts-1      oCu2     -> spr_rtype_outslay_head, mira a oCu1

#define SNAKE_FOLLOWERS_1   (SNAKE1_PARTS - 1)  // 12
#define SNAKE_FOLLOWERS_2   (SNAKE2_PARTS - 1)  // 10
#define SNAKE_MAX_FOLLOWERS SNAKE_FOLLOWERS_1

typedef enum
{
    SNAKE_TYPE_1 = 0,   // oCabeza1
    SNAKE_TYPE_2        // oCabeza2b
} SnakeType;

void Snake_initAll(void);

// Crea un gusano del tipo dado en un agujero, si queda hueco en el pool.
// TRUE si se ha creado.
bool Snake_spawn(u8 type);

// Un frame de los gusanos: IA (como Enemy_updateAll), migas y dibujo.
void Snake_updateAll(bool hasTarget, fix16 tx, fix16 ty);

// Con el tiempo parado: sólo el temporizador de enemyMov (ver
// Enemy_tickFrozen).
void Snake_tickFrozen(void);

// Cabeza o seguidor más cercano a (x, y): en el original instance_nearest
// busca sobre oMalo, del que oCorpo también desciende, así que el apuntado
// del jugador cuenta los eslabones invulnerables.
bool Snake_closest(s16 x, s16 y, s16 *outX, s16 *outY, s32 *best);

// Impacto de bala. TRUE si se consume la bala: sólo las cabezas reciben daño,
// pero los eslabones también se la quedan (ver arriba). *body dice si lo que
// se la ha quedado era un eslabón, que es el único caso en el que corre el
// manejador del proyectil y, con el WBLOCK o un satélite, sale metralla.
bool Snake_hit(s16 x, s16 y, s16 w, s16 h, bool *killed, bool *body);

// Igual que Enemy_damageNoKill: el haz del PARA resta a la cabeza sin pasar
// por checkMorte, y contra un eslabón sólo se consume él. Ver inc/enemy.h.
bool Snake_damageNoKill(s16 x, s16 y, s16 w, s16 h, s16 dmg);

// Impacto de las balas y bolas de MonsterLite (bloque 3): sólo las cabezas,
// porque los eslabones (snakeBody) no les afectan y las balas los
// atraviesan. Regla de MonsterLite: si hp < 0 muere; si no, resta dmg y la
// cabeza queda marcada en *ignore (un bit por ranura), que la bala gorda
// guarda para no volver a tocarla mientras la atraviesa. TRUE si ha tocado
// una cabeza.
bool Snake_hitHead(s16 x, s16 y, s16 w, s16 h, s16 dmg, u8 *ignore, bool *killed);

// Lo mismo con la esfera de la bola CIRCULAR: centro (cx, cy) y r2/reach de
// la suma de radios (ML_BALL_SNAKE_*).
bool Snake_hitHeadCircle(s16 cx, s16 cy, s16 r2, s16 reach, s16 dmg, u8 *ignore, bool *killed);

// El rayo de LASER y MOIRE: la cabeza o el eslabón más cercano a lo largo
// del rayo (en MonsterLite los eslabones también lo paran). Distancia en
// 1/64 px o -1; *slot es el gusano y *head dice si es la cabeza.
s32 Snake_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s, u16 *slot, bool *head);

// La cabeza más cercana, como enemyController.getClosest de MonsterLite, que
// recorre arañas y cabezas, no eslabones. La usan el apuntado de la nave y
// las bolas CIRCULAR.
bool Snake_closestHead(s16 x, s16 y, s16 *outX, s16 *outY, s32 *best);

// Mata el gusano sin mirar el hp (el rayo destruye la cabeza). Deja la
// posición en Snake_lastKillX/Y, como un impacto.
void Snake_killSlot(u16 slot);

// Versión de punto de Snake_hit, para las astillas (ver inc/enemy.h).
bool Snake_hitPoint(s16 x, s16 y, bool *killed, bool *body);

// Posición de la última cabeza muerta: checkMorte() llama a generaPaquete()
// desde la instancia que muere, así que el paquete sale de ahí.
fix16 Snake_lastKillX(void);
fix16 Snake_lastKillY(void);

// Colisión de la nave (esfera de 3,6 px de centro (cx, cy)) contra la
// cabeza o cualquier eslabón (9,6 px).
bool Snake_collides(s16 cx, s16 cy);

u16 Snake_countType(u8 type);

void Snake_clearAll(void);

// Bloque 7. TRUE si la cabeza de algún gusano toca el círculo de centro
// (cx, cy), con r2/reach de la suma de radios (la nave holo,
// ML_HOLO_SNAKE_*; collideAndDie sólo mira el tag "snake", la cabeza).
bool Snake_headTouchCircle(s16 cx, s16 cy, s16 r2, s16 reach);

// clearEnemies() de MonsterLite: todos los gusanos vivos revientan
// (destroyParts). Devuelve cuántas cabezas.
u16 Snake_explodeAll(void);

// Posición de la cabeza del gusano de la ranura slot (0..MAX_SNAKES-1).
bool Snake_headPos(u16 slot, s16 *x, s16 *y);

#endif
