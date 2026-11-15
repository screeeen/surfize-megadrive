#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Araña (oSpider)
//
// Nace en un agujero (siempre el 4 en el original), invisible e inmóvil
// durante 90 frames, y luego corre la misma máquina de 3 estados que las
// cabezas de gusano con velo = 3: 0.72 px/frame de avance y 3.6 grados/frame
// de giro. hp = 0 y las balas restan 4, así que muere de un impacto.
//
// Tocarla mata al jugador (oMalo/Collision_oPlayer).
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

// Crea una araña en su agujero, invisible y con alarm[0] en marcha.
void Enemy_spawn(void);

void Enemy_updateAll(bool hasPlayer, fix16 px, fix16 py);

bool Enemy_collides(s16 px, s16 py, s16 pw, s16 ph);

// Posición (origen) de la araña viva más cercana a (x, y) en píxeles.
// *best es la distancia AL CUADRADO del mejor candidato que llevamos, en
// píxeles. Entra con la de otro módulo y sale con la del mejor de todos, para
// que el que corra después pueda descartar por caja (ver Snake_closest).
bool Enemy_closest(s16 x, s16 y, s16 *outX, s16 *outY, s32 *best);

// Resta DMG_TO_SPIDER a la araña que solape la caja dada. TRUE si ha habido
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

// oBombaExp: hace daño a todo lo que solape su caja. Devuelve cuántas
// arañas ha matado.
u16 Enemy_damageArea(s16 x, s16 y, s16 w, s16 h);

// El power-up de la bomba no mataba arañas sumando kills, como creía este
// port: lo que limpia la pantalla es el Animation End de oExplosionCruz, que
// destruye todo sin pasar por checkMorte(). Por eso aquí basta con
// Enemy_clearAll(). Ver src/game.c (POW_BOOM) y el informe.

void Enemy_clearAll(void);

u16 Enemy_countAlive(void);

#endif
