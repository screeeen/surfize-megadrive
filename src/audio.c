#include "audio.h"
#include "resources.h"

// ---------------------------------------------------------------------------
// Muestras, prioridades y reparto de canales
//
// MonsterLite lo toca todo por un solo AudioSource con PlayOneShot, que
// superpone tantas voces como haga falta. XGM2 tiene 3 canales PCM: el 1 es
// de la música del menú y del game over (en partida no hay música: Q21
// revocada por el usuario el 2026-11-21) y los efectos y las voces se
// reparten el 2 y el 3 (pickChannel):
//
//   1. el primer canal libre, en el orden 3, 2;
//   2. si no hay ninguno y el nuevo es de lo que se repite (disparo o
//      impacto: tiene grupo), el canal que tenga otro de su grupo (el más
//      viejo);
//   3. si no, el que tenga el sonido de MENOS prioridad, siempre que sea
//      menor o igual que la del nuevo; entre iguales, el más viejo;
//   4. si los dos tienen más prioridad que el nuevo, el nuevo no suena.
//
// El canal 1 no se usa para efectos aunque esté libre en partida: medido con
// tools/measure.sh, un tercer canal sonando cuesta ~14 vueltas de 600 en el
// caso peor (cada canal es una lectura de la ROM por muestra desde el Z80,
// que para al 68000), y además es el que necesita la música.
//
// No se usa SOUND_PCM_CH_AUTO del driver: mira el canal 3 antes que el 2
// aunque el 2 tenga algo menos importante, así que con la bomba (cuatro
// sonidos en el mismo frame, hasta la quinta entrega: ahora es una sola
// muestra) la voz "EXPLOSIONS" cortaba a "BOOM" teniendo el estallido, de
// menos prioridad, en el otro canal. Y el estado del driver
// no ve los sonidos pedidos en el mismo frame (el Z80 aún no los ha
// arrancado): dos en el mismo frame iban al mismo canal. Aquí cada canal
// guarda en RAM cuándo acaba lo suyo (la longitud a 266 muestras por frame) y
// con qué prioridad suena, sin leer el estado del Z80.
//
// Las prioridades no salen de MonsterLite (todo tiene la 128 del AudioSource)
// ni del GML: son del port (invención, docs/PORT_AUDIT_GML.md, bloque 12).
// El criterio: lo que suena una vez y explica lo que pasa (voces, bomba,
// rayos) no lo corta lo que se repite cada pocos frames (disparos, impactos,
// el ding), y lo que se repite se reparte los dos canales.
//
//   15  música del menú (canal 1): nada la corta
//   13  voces: ARENA, AWESOME, CARALLUDO, repetea, las de arma de la
//       tienda, y las mezclas de la bomba (BOOM + estallido + EXPLOSIONS) y
//       de los rayos (estallido + RAYS), que desde la quinta entrega van
//       cada una en una sola muestra y un solo canal (antes, con tres voces
//       en dos canales, el estallido de los rayos se quedaba sin canal)
//    9  recoger paquete o moneda, botones
//    8  el ding de la cuenta (uno por frame)
//    5  disparos (grupo de los disparos) y explosiones de enemigo (bombaXPL;
//       aranamorte y dust del GML: grupo de los impactos)
//
// Disparos e impactos con la misma prioridad, y cada uno cortando antes a
// uno de su grupo (pickChannel): con dos canales libres de voces, uno se
// queda con los disparos y el otro con los impactos; con uno solo (la voz
// ARENA ocupa el otro los 5 primeros segundos), se turnan. Con los disparos
// a 3 por debajo de los impactos, el impacto de bala (1,42 s) ocupaba los dos
// canales y en combate sonaba 1 disparo de cada 18; con los disparos por
// encima, se perdían los impactos (3 de 18). En MonsterLite se oyen todos.
// ---------------------------------------------------------------------------
#define PRIO_MUSIC      15
#define PRIO_VOICE      13
#define PRIO_PICKUP     9
#define PRIO_SHOT       5
#define PRIO_XPL        5

// Grupos de lo que se repite cada pocos frames: un sonido de un grupo corta
// antes a otro de su grupo que a uno distinto (pickChannel).
#define GRP_NONE        0
#define GRP_SHOT        1
#define GRP_XPL         2
#define PRIO_DING       8

// Muestras por frame de 50 Hz a 13300 Hz. Medido en BlastEm (PAL): ARENA,
// 69 376 muestras, suena 261 frames, 265,8 por frame. La duración se
// redondea hacia arriba (truncar dejaba el canal libre un frame antes de que
// acabara) y se suma uno por el frame que tarda el Z80 en arrancarla.
#define SAMPLES_PER_FRAME   266
#define PCM_FRAMES(len)     ((((len) + SAMPLES_PER_FRAME - 1) / SAMPLES_PER_FRAME) + 1)

typedef struct
{
    const u8 *data;
    u32       len;
    u16       frames;   // PCM_FRAMES(len), resuelto al compilar
    u8        prio;
    u8        group;    // GRP_*
} Sample;

#define S(res, p) { res, sizeof(res), PCM_FRAMES(sizeof(res)), p, GRP_NONE }
#define SG(res, p, g) { res, sizeof(res), PCM_FRAMES(sizeof(res)), p, g }

static const Sample samples[SFX_COUNT] =
{
    // Del GML (tools/gm2md_audio.py)
    [SFX_SHOT]         = SG(sfxShot,       PRIO_SHOT, GRP_SHOT),
    [SFX_LASER]        = SG(sfxLaser,      PRIO_SHOT, GRP_SHOT),
    [SFX_ARANAMORTE]   = SG(sfxAranaMorte, PRIO_XPL, GRP_XPL),
    [SFX_DUST]         = SG(sfxDust,       PRIO_XPL, GRP_XPL),
    // De MonsterLite (tools/ml2md_audio.py)
    [SFX_ML_SHOT]      = SG(mlShot,        PRIO_SHOT, GRP_SHOT),
    [SFX_ML_WBLOCK]    = SG(mlWBlock,      PRIO_SHOT, GRP_SHOT),
    [SFX_ML_LASER]     = SG(mlLaser,       PRIO_SHOT, GRP_SHOT),
    [SFX_ML_TWAY]      = SG(mlTway,        PRIO_SHOT, GRP_SHOT),
    [SFX_ML_CIRCULAR]  = SG(mlCircular,    PRIO_SHOT, GRP_SHOT),
    [SFX_ML_XPL]       = SG(mlXpl,         PRIO_XPL, GRP_XPL),
    [SFX_ML_POWUP]     = S(mlPowUp,       PRIO_PICKUP),
    [SFX_ML_REPETEA]   = S(mlRepetea,     PRIO_VOICE),
    [SFX_ML_ARENA]     = S(mlArena,       PRIO_VOICE),
    // La bomba y los rayos, cada uno en una sola muestra (las voces BOOM y
    // EXPLOSIONS o RAYS con el estallido de bombaAmarilla): con la prioridad
    // de las voces, que es lo que llevan dentro.
    [SFX_ML_BOMB_MIX]  = S(mlBombMix,     PRIO_VOICE),
    [SFX_ML_RAYS_MIX]  = S(mlRaysMix,     PRIO_VOICE),
    [SFX_ML_AWESOME]   = S(mlAwesome,     PRIO_VOICE),
    [SFX_ML_CARALLUDO] = S(mlCaralludo,   PRIO_VOICE),
    [SFX_ML_DING]      = S(mlDing,        PRIO_DING),
    [SFX_ML_BTN_SHORT] = S(mlBtnShort,    PRIO_PICKUP),
    [SFX_ML_BTN_LONG]  = S(mlBtnLong,     PRIO_PICKUP),
};

#undef S

// Interruptores de la tienda (AudioToggle y musicToggle de MonsterLite,
// S/globales.cs:457-494). Encendidos al arrancar; duran hasta apagar (sin
// SRAM).
bool g_soundOn = TRUE;
bool g_musicOn = TRUE;


static u32 chEnd[3];        // vtimer en que acaba lo que suena (0: libre)
static u8  chPrio[3];
static u16 chSeq[3];        // orden de llegada, para desempatar
static u8  chGroup[3];      // GRP_* de lo que suena
static u16 seq;

void Audio_init(void)
{
    u16 i;

    XGM2_loadDriver(TRUE);

    for (i = 0; i < 3; i++) { chEnd[i] = 0; chPrio[i] = 0; chSeq[i] = 0; chGroup[i] = GRP_NONE; }
    seq = 0;
}

static s16 pickChannel(u8 prio, u8 group)
{
    static const u8 order[2] = { SOUND_PCM_CH3, SOUND_PCM_CH2 };
    const u32 t = vtimer;
    s16 best = -1;
    u16 i;

    for (i = 0; i < 2; i++)
        if (chEnd[order[i]] <= t) return order[i];

    // Lo que se repite (disparos e impactos) corta primero a otro de su
    // grupo, el más viejo: así en combate un canal se queda con los disparos y
    // el otro con los impactos, y ninguno de los dos se come al otro.
    if (group != GRP_NONE)
    {
        for (i = 0; i < 2; i++)
        {
            const u8 ch = order[i];

            if ((chGroup[ch] == group) &&
                ((best < 0) || ((u16)(seq - chSeq[ch]) > (u16)(seq - chSeq[best]))))
                best = ch;
        }
        if (best >= 0) return best;
    }

    for (i = 0; i < 2; i++)
    {
        const u8 ch = order[i];

        if (chPrio[ch] > prio) continue;
        if ((best < 0) || (chPrio[ch] < chPrio[best]) ||
            ((chPrio[ch] == chPrio[best]) &&
             ((u16)(seq - chSeq[ch]) > (u16)(seq - chSeq[best]))))
            best = ch;
    }

    return best;
}

static void play(const u8 *data, u32 len, u16 frames, u8 prio, u8 group)
{
    const s16 ch = pickChannel(prio, group);

    if (ch < 0) return;

    if (XGM2_playPCMEx(data, len, (SoundPCMChannel)ch, prio, FALSE, FALSE))
    {
        chEnd[ch] = vtimer + frames;
        chPrio[ch] = prio;
        chGroup[ch] = group;
        chSeq[ch] = ++seq;
    }
}

void Audio_sfx(u8 id)
{
    if (!g_soundOn || (id >= SFX_COUNT)) return;

    play(samples[id].data, samples[id].len, samples[id].frames, samples[id].prio,
         samples[id].group);
}

void Audio_speech(u16 weaponIndex)
{
    static const u8 *const voice[5] = { spk0, spk1, spk2, spk3, spk4 };
    static const u32 voiceLen[5] = { sizeof(spk0), sizeof(spk1), sizeof(spk2),
                                     sizeof(spk3), sizeof(spk4) };

    if (!g_soundOn || (weaponIndex >= 5)) return;

    static const u16 voiceFrames[5] = { PCM_FRAMES(sizeof(spk0)), PCM_FRAMES(sizeof(spk1)),
                                        PCM_FRAMES(sizeof(spk2)), PCM_FRAMES(sizeof(spk3)),
                                        PCM_FRAMES(sizeof(spk4)) };

    play(voice[weaponIndex], voiceLen[weaponIndex], voiceFrames[weaponIndex], PRIO_VOICE, GRP_NONE);
}

void Audio_menuMusic(bool on)
{
    if (on && g_musicOn)
    {
        // El original la lanza una vez al entrar en el menú, sin bucle. Va
        // siempre al canal 1, con la prioridad máxima: ningún efecto la corta.
        XGM2_playPCMEx(musMenu, sizeof(musMenu), SOUND_PCM_CH1, PRIO_MUSIC, FALSE, FALSE);
        chEnd[SOUND_PCM_CH1] = vtimer + PCM_FRAMES(sizeof(musMenu));
        chPrio[SOUND_PCM_CH1] = PRIO_MUSIC;
        chGroup[SOUND_PCM_CH1] = GRP_NONE;
        chSeq[SOUND_PCM_CH1] = ++seq;
    }
    else
    {
        XGM2_stopPCM(SOUND_PCM_CH1);
        chEnd[SOUND_PCM_CH1] = 0;
        chPrio[SOUND_PCM_CH1] = 0;
    }
}
