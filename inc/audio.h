#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <genesis.h>

// ===========================================================================
// Sonido (audio_play_sound del original)
//
// Los 22 sonidos del proyecto son muestras Ogg Vorbis. En Mega Drive se
// reproducen como PCM de 8 bits a 13300 Hz con el driver XGM2, que tiene 3
// canales: el canal 1 queda para la música del menú y los efectos se reparten
// entre el 2 y el 3.
//
// Diferencia con el original: GameMaker mezcla tantas voces como haga falta,
// así que allí los disparos se solapan sin límite. Aquí, con dos canales de
// efectos, un sonido nuevo corta al más antiguo.
//
// El sistema FMOD original (global.*Snd) está comentado en el GML desde hace
// tiempo; lo que se porta es la capa audio_play_sound que quedó en uso.
// ===========================================================================

typedef enum
{
    SFX_SHOT = 0,       // oBulletThin, oBulletPara
    SFX_WBLOCK,         // oWBlock
    SFX_LASER,          // oLaser, oBomba
    SFX_THREEWAY,       // o3way
    SFX_CIRCULAR,
    SFX_BOMBA,          // Collision_oBoomPowUp
    SFX_RAYOS,          // Collision_oRayosPowUp
    SFX_BOOM,
    SFX_POWUP,          // oPaquete/Collision_oPlayer
    SFX_REPETEA,        // réplica y pivote
    SFX_RISA,           // Collision_oBoomPowUp
    SFX_ARANAMORTE,     // oExplosion, morteGusano
    SFX_MORTE,          // oMuerte
    SFX_DUST,
    SFX_COUNT
} SfxId;

// Sonido y música apagables desde la tienda (bloque 5). Con el sonido
// apagado no suena ningún efecto, voz ni tick; con la música apagada no
// suena la del menú.
extern bool g_soundOn;
extern bool g_musicOn;

void Audio_init(void);
void Audio_sfx(u8 id);

// menu(): audio_play_sound(menuSong, 1, false)
void Audio_menuMusic(bool on);

// triggerSpeech(weapon_index): la voz que anuncia el arma elegida.
void Audio_speech(u16 weaponIndex);

// Tick de la cuenta del menú (PSG, ver src/audio.c) y su paso por frame.
void Audio_tick(void);
void Audio_update(void);

#endif
