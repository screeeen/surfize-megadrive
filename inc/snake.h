#ifndef _SNAKE_H_
#define _SNAKE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Gusanos (oCabeza1 y oCabeza2b)
//
// Cabeza + cadena de seguidores. En el original cada eslabón copia la
// posición del anterior con stepdelay frames de retardo (una cola de
// posiciones por eslabón), así que el eslabón i va i*stepdelay frames por
// detrás de la cabeza: aquí se guarda un único historial circular de la
// cabeza y cada parte lo lee con su desfase, que da el mismo resultado con
// una sola cola por gusano.
//
// La cadena del original es cabeza -> oCabeza2 -> oCorpo[9..1] -> oCu1 ->
// oCu2 (12 seguidores en el tipo 1, 10 en el tipo 2). Dos vueltas del bucle
// de Step_1 repiten pareja sin efecto visible (variables que persisten entre
// iteraciones), así que la cadena efectiva son esos 12 y 10 eslabones.
//
// La cabeza corre la misma máquina de 3 estados que la araña, con velo como
// velocidad Y como grados de giro por frame.
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

void Snake_updateAll(bool hasPlayer, fix16 px, fix16 py);

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

// Versión de punto de Snake_hit, para las astillas (ver inc/enemy.h).
bool Snake_hitPoint(s16 x, s16 y, bool *killed, bool *body);

// Posición de la última cabeza muerta: checkMorte() llama a generaPaquete()
// desde la instancia que muere, así que el paquete sale de ahí.
fix16 Snake_lastKillX(void);
fix16 Snake_lastKillY(void);

// Colisión contra cabeza o cualquier seguidor.
bool Snake_collides(s16 px, s16 py, s16 pw, s16 ph);

u16 Snake_countType(u8 type);

void Snake_clearAll(void);

#endif
