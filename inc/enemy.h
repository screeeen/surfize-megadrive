#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Araña (oSpider)
//
// Nace en un agujero al azar (createSpider de MonsterLite), ya visible y en
// marcha, y corre la IA de enemyMov (bloque 8, ver inc/entity.h) con los
// valores del prefab de la araña: maxSpeed 1, turningSpeedMax 8. Muere de
// cualquier impacto.
//
// Choca como la esfera de MonsterLite: 8,4 px de radio alrededor de su
// posición (ML_SPIDER_RADIUS_X10). Tocarla mata al jugador.
// ===========================================================================

// Celda de 24x24 con el origen (30,40 de 64x64) en el centro, por lo mismo
// que el jugador: el original rota alrededor del origen.
#define ENEMY_SPR_OFF_X (-12)
#define ENEMY_SPR_OFF_Y (-12)

// sprite185 trae 8 frames de animación, y el port genera 8 rotaciones de
// cada uno: la hoja va en filas de rotación, así que el índice de frame es
// rotacion * 8 + animacion.
#define ENEMY_ANIM_FRAMES 8

void Enemy_initAll(void);

// Crea una araña en un agujero al azar, si queda ranura (tope de 8, Q4).
void Enemy_spawn(void);

// Un frame de las arañas: move() y Update() de enemyMov. (tx, ty) es el
// destino de ATTACK, si hasTarget: el jugador o, sin él, una nave holo.
void Enemy_updateAll(bool hasTarget, fix16 tx, fix16 ty);

// Con el tiempo parado sólo corre el Update de enemyMov: el temporizador
// sigue bajando y re-sorteando (Entity_tickML).
void Enemy_tickFrozen(void);

// TRUE si alguna araña toca la nave: esfera de 3,6 px contra la de 8,4.
bool Enemy_collides(s16 cx, s16 cy);

// Posición (origen) de la araña viva más cercana a (x, y) en píxeles.
// *best es la distancia AL CUADRADO del mejor candidato que llevamos, en
// píxeles. Entra con la de otro módulo y sale con la del mejor de todos, para
// que el que corra después pueda descartar por caja (ver Snake_closest).
bool Enemy_closest(s16 x, s16 y, s16 *outX, s16 *outY, s32 *best);

// Resta dmg a la araña cuya esfera toque la caja dada (Coll_circleBox). TRUE si ha habido
// impacto; *killed a TRUE si ha muerto.
// El daño lo pone QUIEN GOLPEA porque el manejador que corre es el del
// enemigo: oSpider/Collision_oBulletS resta 4 y Collision_oDestro resta 1.
bool Enemy_hit(s16 x, s16 y, s16 w, s16 h, s16 dmg, bool *killed);

// Daño SIN comprobar la muerte. Lo pide object1, el haz del arma PARA: su
// Collision_oMalo hace "with other if hp != undefined hp -= 1" y nada más, y
// el manejador del enemigo NO corre porque object1 no desciende de oBulletS.
// O sea que el haz ablanda pero no mata: el enemigo se queda con el hp en
// negativo hasta que le entra una bala y es ESA la que dispara checkMorte.
bool Enemy_damageNoKill(s16 x, s16 y, s16 w, s16 h, s16 dmg);

// Igual que Enemy_hit pero contra un PUNTO, que es lo que son las astillas de
// oDestro. Con hasta 32 astillas vivas preguntando por las 8 arañas en cada
// frame, el solape de caja contra caja era de lo más caro del juego: así el
// rechazo en X es una sola comparación sin signo.
bool Enemy_hitPoint(s16 x, s16 y, s16 dmg, bool *killed);

// La bola CIRCULAR (esfera de 4 px) contra las arañas: círculo de centro
// (cx, cy) con r2/reach de la suma de radios (ML_BALL_SPIDER_*). La araña
// que toca muere. TRUE si ha tocado alguna.
bool Enemy_hitCircle(s16 cx, s16 cy, s16 r2, s16 reach, bool *killed);

// El rayo de LASER y MOIRE (S/RaycastScr.cs): la araña viva más cercana a
// lo largo del rayo que sale de (ox, oy) con dirección (c, s), con el radio
// de MonsterLite. Devuelve su distancia en 1/64 px (o -1) y en *which su
// entrada para Enemy_killLive, válida hasta que cambie la lista.
s32 Enemy_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s, u16 *which);

// Mata sin mirar el hp (el rayo destruye la araña) y devuelve su posición
// (x e y pueden ser NULL).
void Enemy_killLive(u16 n, fix16 *x, fix16 *y);

// oBombaExp: hace daño a todo lo que solape su caja. Devuelve cuántas
// arañas ha matado.
u16 Enemy_damageArea(s16 x, s16 y, s16 w, s16 h);

// El power-up de la bomba no mataba arañas sumando kills, como creía este
// port: lo que limpia la pantalla es el Animation End de oExplosionCruz, que
// destruye todo sin pasar por checkMorte(). Por eso aquí basta con
// Enemy_clearAll(). Ver src/game.c (POW_BOOM) y el informe.

void Enemy_clearAll(void);

// Bloque 7. TRUE si alguna araña toca el círculo de centro (cx, cy): r2 y
// reach son los de la SUMA de los dos radios (Coll_circleCircle), p. ej. la
// nave holo con ML_HOLO_SPIDER_*.
bool Enemy_touchCircle(s16 cx, s16 cy, s16 r2, s16 reach);

// clearEnemies() de MonsterLite: todas las arañas vivas explotan y
// desaparecen. Devuelve cuántas.
u16 Enemy_explodeAll(void);

// Posición de la araña de la ranura slot (0..MAX_SPIDERS-1), si vive.
bool Enemy_slotPos(u16 slot, s16 *x, s16 *y);

u16 Enemy_countAlive(void);

#endif
