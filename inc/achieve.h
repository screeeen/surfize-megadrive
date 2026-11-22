#ifndef _ACHIEVE_H_
#define _ACHIEVE_H_

#include <genesis.h>

// ===========================================================================
// Logros (bloque 10)
//
// MonsterLite no define ningún logro (docs/MONSTERLITE_GAP.md §2.11): la
// lista, las recompensas, el aviso y la pantalla son INVENCIÓN, decididos por
// el usuario el 2026-11-21 (Q18, docs/MONSTERLITE_DECISIONES.md) y anotados
// en la tabla de divergencias de docs/PORT_AUDIT_GML.md, "Bloque 10".
//
// Cada logro da monedas al saldo (g_coins) en el momento de conseguirlo.
// Todo vive en RAM (una máscara de bits y dos contadores) y se pierde al
// apagar o con RESET, como el resto del progreso (sin SRAM, decisión del
// usuario del 2026-11-20).
//
// No se sondea nada cada frame: cada logro se comprueba en el sitio donde ya
// pasa lo que mide (src/game.c y src/menu.c). Lo único que va en el bucle de
// partida es una comparación de u32 en las bajas (g_achNextScore) y el
// temporizador del aviso (src/hud.c).
// ===========================================================================

typedef enum
{
    // PROGRESO
    ACH_FIRST_GAME = 0,     // terminar una partida                    50
    ACH_LEVEL_5,            // llegar al nivel 5                      100
    ACH_LEVEL_10,           // llegar al nivel 10                     500
    ACH_LEVEL_15,           // llegar al nivel 15                    2000
    ACH_SCORE_100,          // puntuación 100                         100
    ACH_SCORE_1000,         // puntuación 1000                        300
    ACH_SCORE_10000,        // puntuación 10000                      1000
    // TIENDA (en el orden de la tienda: storeWeapon de src/menu.c)
    ACH_BUY_LASER,          //                                        600
    ACH_BUY_TWAY,           //                                       1200
    ACH_BUY_CIRCULAR,       //                                       2400
    ACH_BUY_MOIRE,          //                                       4800
    ACH_COINS_1000,         // 1000 monedas recogidas en partida      100
    ACH_COINS_10000,        // 10000 monedas recogidas en partida    1000
    // POWER-UPS
    ACH_USE_BOMB,           // tocar una bomba                        100
    ACH_USE_RAYS,           // tocar unos rayos                       100
    ACH_HOLO,               // activar una nave holo                  100
    ACH_BOMB_10,            // 10 bajas en una sola detonación        500
    ACH_COUNT
} AchieveId;

#define ACH_NONE            0xFF

// Grupos de la pantalla de lista: el primer logro de cada uno.
#define ACH_GROUP_SHOP      ACH_BUY_LASER
#define ACH_GROUP_POWER     ACH_USE_BOMB

// Bajas de una sola bomba para ACH_BOMB_10. Con los topes del port (8 arañas
// y 2 gusanos hasta el nivel 9, 3 desde el 10) caben 10 u 11 a la vez.
#define ACH_BOMB_KILLS      10

// El aviso en partida (invención, ver src/hud.c): 2 s a 50 Hz.
#define ACH_NOTICE_F        100

// Un bit por AchieveId: conseguido.
extern u32 g_achMask;

// Monedas recogidas en partida, acumuladas entre partidas (no cuenta las de
// recompensa ni el saldo gastado).
extern u32 g_coinsCollected;

// La siguiente puntuación que da un logro (0xFFFFFFFF cuando ya no queda
// ninguno). Quien suma puntuación compara con esto antes de llamar a
// Achieve_score: una comparación por frame con bajas, sin llamada.
extern u32 g_achNextScore;

// Avisos pendientes de enseñar (los saca el HUD con Achieve_popNotice).
extern u8 g_achQueued;

// Todo a cero (al encender; un RESET pasa por aquí igual).
void Achieve_init(void);

// Consigue el logro 'id' si no lo estaba: pone su bit, suma su recompensa a
// g_coins y encola el aviso. Si ya lo estaba, no hace nada.
void Achieve_unlock(u8 id);

// La puntuación ha llegado a g_achNextScore: los logros de puntuación que
// ya se cumplen.
void Achieve_score(u32 score);

// levelUpgrade() ha subido de nivel (puede saltar varios de golpe).
void Achieve_level(u16 level);

// Se ha recogido una moneda de 'value' en partida.
void Achieve_coins(u16 value);

// El siguiente aviso de la cola, o ACH_NONE.
u8 Achieve_popNotice(void);

// Para la pantalla de lista y el aviso.
const char *Achieve_name(u8 id);
u16 Achieve_reward(u8 id);
u16 Achieve_doneCount(void);

static inline bool Achieve_done(u8 id) { return (g_achMask & (1UL << id)) != 0; }

#endif
