#include "audio.h"
#include "resources.h"

typedef struct
{
    const u8 *data;
    u32       len;
} Sample;

static Sample samples[SFX_COUNT];
static u8 nextChannel;

// Interruptores de la tienda (AudioToggle y musicToggle de MonsterLite,
// S/globales.cs:457-494). Encendidos al arrancar; se guardarán en SRAM en el
// bloque 6.
bool g_soundOn = TRUE;
bool g_musicOn = TRUE;

void Audio_init(void)
{
    XGM2_loadDriver(TRUE);

    samples[SFX_SHOT]       = (Sample){ sfxShot,       sizeof(sfxShot) };
    samples[SFX_WBLOCK]     = (Sample){ sfxWBlock,     sizeof(sfxWBlock) };
    samples[SFX_LASER]      = (Sample){ sfxLaser,      sizeof(sfxLaser) };
    samples[SFX_THREEWAY]   = (Sample){ sfxThreeWay,   sizeof(sfxThreeWay) };
    samples[SFX_CIRCULAR]   = (Sample){ sfxCircular,   sizeof(sfxCircular) };
    samples[SFX_BOMBA]      = (Sample){ sfxBomba,      sizeof(sfxBomba) };
    samples[SFX_RAYOS]      = (Sample){ sfxRayos,      sizeof(sfxRayos) };
    samples[SFX_BOOM]       = (Sample){ sfxBoom,       sizeof(sfxBoom) };
    samples[SFX_POWUP]      = (Sample){ sfxPowUp,      sizeof(sfxPowUp) };
    samples[SFX_REPETEA]    = (Sample){ sfxRepetea,    sizeof(sfxRepetea) };
    samples[SFX_RISA]       = (Sample){ sfxRisa,       sizeof(sfxRisa) };
    samples[SFX_ARANAMORTE] = (Sample){ sfxAranaMorte, sizeof(sfxAranaMorte) };
    samples[SFX_MORTE]      = (Sample){ sfxMorte,      sizeof(sfxMorte) };
    samples[SFX_DUST]       = (Sample){ sfxDust,       sizeof(sfxDust) };

    nextChannel = 0;
}

void Audio_sfx(u8 id)
{
    if (!g_soundOn || (id >= SFX_COUNT)) return;

    // Canales 2 y 3 en alternancia: el 1 es para la música del menú.
    XGM2_playPCM(samples[id].data, samples[id].len,
                 nextChannel ? SOUND_PCM_CH3 : SOUND_PCM_CH2);
    nextChannel ^= 1;
}

void Audio_speech(u16 weaponIndex)
{
    static const u8 *const voice[5] = { spk0, spk1, spk2, spk3, spk4 };
    static const u32 voiceLen[5] = { sizeof(spk0), sizeof(spk1), sizeof(spk2),
                                     sizeof(spk3), sizeof(spk4) };

    if (!g_soundOn || (weaponIndex >= 5)) return;

    // La voz va al canal 3 para no cortar la música del menú (canal 1).
    XGM2_playPCM(voice[weaponIndex], voiceLen[weaponIndex], SOUND_PCM_CH3);
}

void Audio_menuMusic(bool on)
{
    if (on && g_musicOn)
    {
        // El original la lanza una vez al entrar en el menú, sin bucle.
        XGM2_playPCMEx(musMenu, sizeof(musMenu), SOUND_PCM_CH1, 6, FALSE, FALSE);
    }
    else
    {
        XGM2_stopPCM(SOUND_PCM_CH1);
    }
}

// ---------------------------------------------------------------------------
// El tick de la cuenta del menú. No sale del original (la cuenta tampoco): lo
// pidió el autor. Es un golpe de ruido blanco del PSG de 2 frames, flojo y
// bajando: suena a "tsk". El PSG está libre, porque toda la música y los
// efectos del juego son PCM por el driver XGM2, que sin música XGM no lo
// toca; así no hace falta ningún sample nuevo ni se roba un canal PCM.
// ---------------------------------------------------------------------------
#define TICK_CH 3                       // el canal de ruido

static const u8 tickEnv[] = { 9, 12 };  // atenuación por frame (15 = mudo)
static u8 tickStep;

void Audio_tick(void)
{
    if (!g_soundOn) return;

    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_CLOCK2);
    PSG_setEnvelope(TICK_CH, tickEnv[0]);
    tickStep = 1;
}

void Audio_update(void)
{
    if (!tickStep) return;

    if (tickStep < sizeof(tickEnv))
    {
        PSG_setEnvelope(TICK_CH, tickEnv[tickStep]);
        tickStep++;
    }
    else
    {
        PSG_setEnvelope(TICK_CH, PSG_ENVELOPE_MIN);
        tickStep = 0;
    }
}
