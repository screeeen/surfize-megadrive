#ifndef _GAME_H_
#define _GAME_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Estado de partida y variables globales
//
// Porta las globales de scrInit() y los scripts startGame() / mataPlayer().
// En GameMaker son global.*, aquí variables estáticas del programa: no hay
// asignación dinámica en ninguna parte del port.
//
// Los estados de aplicación del original son global.isMenu + global.started
// (oMenu existe o no existe). El menú en sí es FASE 8; hasta entonces la
// muerte reinicia la partida directamente.
// ===========================================================================

typedef enum
{
    GAME_SPLASH = 0,    // global.started sin partida jugada: pantalla inicial
    GAME_PLAY,
    GAME_MENU           // global.isMenu: interludio tras morir
} GameState;

// Puntuación y niveles de MonsterLite (bloque 1, docs/MONSTERLITE_GAP.md
// §2.3). globales.kills de MonsterLite se llama kills pero es la PUNTUACIÓN:
// cada baja suma el nivel. Pasa de 65535 dentro del nivel 14, así que u32.
//   g_score        globales.kills
//   g_level        globales.level        (empieza en 1)
//   g_factorLevel  gameControl.factorLevel (empieza en 1, se duplica)
// El siguiente umbral ("UPGRADE AT" del HUD) es g_factorLevel * (g_level + 1).
extern u32 g_score;
extern u16 g_level;
extern u32 g_factorLevel;

// Cuenta de BAJAS (global.kills del GML), que no es la puntuación: la siguen
// usando el desbloqueo de armas por récord (checkWeapon, umbrales del GML) y
// el interludio KILLS/TOP hasta que lleguen los bloques 3 y 5.
// g_lastScore es ahora la mejor PUNTUACIÓN de la sesión (el LAST del HUD).
extern u16 g_kills;
extern u32 g_lastScore;
extern u16 g_maxKills;

// global.playerBulletsCounter, global.weapon_index
extern u16 g_playerBullets;
extern u16 g_weaponIndex;

// Lista de armas desbloqueadas (ds_list weapons) y umbral siguiente
// (nextWeapon), que el menú de FASE 8 usará para elegir arma.
extern u16 g_weaponsUnlocked;
extern u32 g_nextWeapon;   // kills para la siguiente arma (ver checkWeapon)
#define NEXT_WEAPON_DONE 0xFFFFFFFFUL   // todas desbloqueadas: el menú pinta "re"

// global.credits: el original es de recreativa y no deja empezar sin crédito
extern u16 g_credits;

extern u8 g_state;

void Game_init(void);

// toGame() + startGame() de MonsterLite: limpia enemigos, puntuación 0,
// nivel 1, factor 1. La nave aparece a los 40 f, el primer gusano a los 90 f
// y desde ahí una oleada cada 20 f (ver src/game.c).
void Game_start(void);

// levelUpgrade() de MonsterLite (S/gameControl.cs:781-795), tal cual:
//   if (kills / factorLevel > level) { level = kills / factorLevel; factorLevel *= 2; }
// Puede saltar varios niveles de golpe y el factor se duplica UNA vez.
// Devuelve TRUE si ha subido.
bool Game_levelUpgrade(void);

// mataPlayer(): tocar un enemigo termina la partida.
void Game_killPlayer(void);

// Un frame de juego.
void Game_update(void);

#endif
