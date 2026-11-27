#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <genesis.h>

// ===========================================================================
// Sonido
//
// Todo son muestras PCM de 8 bits a 13300 Hz por el driver XGM2, que tiene 3
// canales PCM. Hay dos juegos de muestras:
//
//   - las del GML (tools/gm2md_audio.py, desde los Ogg de surfize), para lo
//     que sólo existe en el GML: las armas PARA y BOMBA, la metralla, la
//     muerte del gusano y las voces de arma de la tienda (triggerSpeech);
//   - las de MonsterLite (tools/ml2md_audio.py, desde los WAV de
//     Assets/Sound), para todo lo que MonsterLite define (bloque 12): la
//     biblioteca de SoundManager, con el clip que le pone la escena.
//
// MonsterLite toca todo por UN AudioSource con PlayOneShot, que superpone sin
// límite. Aquí el canal 1 es de la música y los efectos y las voces tienen
// el 2 y el 3; cada sonido lleva una prioridad (0-15): va a un canal libre o,
// si no hay, corta al de menos prioridad siempre que no supere la suya; si
// no, no suena. El reparto y la tabla están en src/audio.c y en
// docs/PORT_AUDIT_GML.md, bloque 12.
// ===========================================================================

typedef enum
{
    // --- del GML -----------------------------------------------------------
    SFX_SHOT = 0,       // oBulletPara (shotSnd)
    SFX_LASER,          // oBomba del GML (laserSnd)
    SFX_ARANAMORTE,     // oExplosion: explosiones de lo que sólo es del GML
    SFX_DUST,           // oca: con aranamorte
    // --- de MonsterLite (bloque 12), campo de SoundManager ------------------
    SFX_ML_SHOT,        // nBulletSnd: bala fina
    SFX_ML_WBLOCK,      // wBulletSnd
    SFX_ML_LASER,       // laserBulletSnd / moireBulletSnd
    SFX_ML_TWAY,        // triBulletSnd
    SFX_ML_CIRCULAR,    // circularBulletSnd
    SFX_ML_XPL,         // bombaXPLSnd: impacto de bala y clearEnemies
    SFX_ML_POWUP,       // pillaPaqueteGameSnd: paquete y moneda
    SFX_ML_REPETEA,     // repeteaSpeechSnd: nave holo activada
    SFX_ML_ARENA,       // arenaWomanSnd: menú
    // Las mezclas (tools/ml2md_audio.py, decisión del usuario del 2026-11-26):
    // lo que MonsterLite toca a la vez en un evento, en una sola muestra.
    SFX_ML_BOMB_MIX,    // bomba: boomSpeechSnd + bombaAmarillaXPLSnd + explosionWomanSnd
    SFX_ML_RAYS_MIX,    // rayos: bombaAmarillaXPLSnd + raysWomanSnd
    SFX_ML_AWESOME,     // awesomeWomanSnd: NEW RECORD!
    SFX_ML_CARALLUDO,   // caralludoSpeechSnd (Fafarre.aiff, tono 2)
    SFX_ML_DING,        // dingSnd (tono 3): la cuenta del game over
    SFX_ML_BTN_SHORT,   // buttonShortSnd
    SFX_ML_BTN_LONG,    // buttonLongSnd
    SFX_COUNT
} SfxId;

// Sonido y música apagables desde la tienda (bloque 5). Con el sonido
// apagado no suena ningún efecto ni voz; con la música apagada no suena la
// del menú.
extern bool g_soundOn;
extern bool g_musicOn;

void Audio_init(void);

// Un efecto o una voz, con su prioridad (src/audio.c).
void Audio_sfx(u8 id);

// menu(): audio_play_sound(menuSong, 1, false)
void Audio_menuMusic(bool on);

// triggerSpeech(weapon_index) del GML: la voz que anuncia el arma elegida en
// la tienda.
void Audio_speech(u16 weaponIndex);

#endif
