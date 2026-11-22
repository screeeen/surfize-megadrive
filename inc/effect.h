#ifndef _EFFECT_H_
#define _EFFECT_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Explosiones (oExplosion / oEstallido / oExplosionCruz)
//
// El original crea una en cada impacto. Aquí hay un pool pequeño: si se
// llenan, la nueva sustituye a la más antigua, que es lo que cabe en el
// presupuesto de sprites del VDP.
//
// El polvo (oca) SÍ está portado, encadenado detrás del fogonazo en la misma
// ranura (ver src/effect.c): es lo que de verdad se ve de una muerte.
//
// Lo que falta son las astillas. Y no son las de oDebrie, que en el GML están
// comentadas y no se crean nunca, sino las de oDestro: oEstallido lanza 12 por
// impacto de oWBlock o de un satélite del PIVOT contra un eslabón de gusano, y
// cada una resta 1 de hp a arañas y cabezas. Ver docs/PORT_AUDIT_GML.md.
// ===========================================================================

void Effect_initAll(void);
void Effect_spawn(fix16 x, fix16 y);

// Igual, pero sin sonido: morteGusano() crea una explosión por eslabón y el
// original lanza aranamorteSnd una sola vez al final del bucle.
void Effect_spawnQuiet(fix16 x, fix16 y);

// oAutoDestruccion: el impacto del arma PARA. Vive 80 frames y revienta en
// metralla.
void Effect_autoDest(fix16 x, fix16 y);

// oMuerte: el fogonazo de la muerte del jugador.
void Effect_death(fix16 x, fix16 y);

// TRUE mientras dura ese fogonazo (el original no deja jugar: destruye la
// nave y abre el menú).
bool Effect_deathActive(void);

void Effect_updateAll(void);
// Las dos mitades de Effect_updateAll, para la cámara lenta de la muerte
// (bloque 12): el fogonazo de oMuerte, que es del GML y va a su ritmo, y el
// resto (explosiones, humareda y el blancazo de la bomba), que se frena con
// el mundo.
void Effect_updateDeath(void);
void Effect_updateWorld(void);
void Effect_clearAll(void);

// oExplosionCruz: la pantalla entera a medio camino del blanco durante 60
// frames del original (la paleta, ver src/effect.c).
void Effect_bombFlash(void);
void Effect_bombCancel(void);

#endif
