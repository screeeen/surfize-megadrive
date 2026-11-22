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
    GAME_MENU,          // global.isMenu: interludio tras morir
    GAME_DYING          // bloque 12: la cámara lenta de la muerte y su sacudida
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

// La munición del arma equipada (bullets[currentWeapon] de MonsterLite) y el
// arma (currentWeapon, ver inc/weapon.h). Mientras se juega, la munición
// vive en g_playerBullets; g_ammo guarda la de cada arma entre partidas
// (Q10: hasta apagar la consola).
extern u16 g_playerBullets;
extern u16 g_weaponIndex;
extern u16 g_ammo[];

// Monedas (CoinsManager de MonsterLite, bloque 4): el saldo, que se gasta en
// la tienda (bloque 5) y dura hasta apagar la consola (sin SRAM), y las de
// esta partida (earnedCoins), que enseña el game over.
extern u32 g_coins;
extern u32 g_coinsEarned;

// Armas que se pueden elegir en el menú, un bit por WeaponId (ver
// checkWeapon en src/game.c), y kills de récord para la siguiente que va por
// récord (nextWeapon del GML).
extern u16 g_weaponsAvail;
extern u32 g_nextWeapon;   // kills para la siguiente arma (ver checkWeapon)
#define NEXT_WEAPON_DONE 0xFFFFFFFFUL   // todas desbloqueadas: el menú pinta "re"

// Armas compradas en la tienda, un bit por WeaponId (WBLOCK siempre), y la
// mejor puntuación al empezar la partida (para el NEW RECORD! del game over).
extern u16 g_weaponsBought;
extern u32 g_bestAtStart;

// Recalcula g_weaponsAvail: las compradas y las del GML que se desbloquean
// por récord. La llama la tienda al comprar.
void Game_checkWeapons(void);

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
