#include "achieve.h"
#include "game.h"

// ===========================================================================
// Logros (bloque 10). Todo es invención: ver inc/achieve.h y
// docs/PORT_AUDIT_GML.md, "Bloque 10".
// ===========================================================================

u32 g_achMask;
u32 g_coinsCollected;
u32 g_achNextScore;
u8  g_achQueued;

// Nombre (el del aviso y el de la lista, con la fuente del sistema; como
// mucho 20 letras para que el aviso quepa en una fila, ver src/hud.c) y
// monedas de recompensa, decididas por el usuario el 2026-11-21.
typedef struct
{
    const char *name;
    u16 reward;
} AchieveDef;

static const AchieveDef defs[ACH_COUNT] =
{
    { "FIRST GAME",            50 },
    { "REACH LEVEL 5",        100 },
    { "REACH LEVEL 10",       500 },
    { "REACH LEVEL 15",      2000 },
    { "SCORE 100",            100 },
    { "SCORE 1000",           300 },
    { "SCORE 10000",         1000 },
    { "BUY LASER",            600 },
    { "BUY TWAY",            1200 },
    { "BUY CIRCULAR",        2400 },
    { "BUY MOIRE",           4800 },
    { "COLLECT 1000 COINS",   100 },
    { "COLLECT 10000 COINS", 1000 },
    { "USE A BOMB",           100 },
    { "USE THE RAYS",         100 },
    { "ACTIVATE A HOLO SHIP", 100 },
    { "10 KILLS, ONE BOMB",   500 },
};

// Umbrales de puntuación, de nivel y de monedas recogidas, con su logro.
static const u32 scoreAt[3] = { 100, 1000, 10000 };
static const u16 levelAt[3] = { 5, 10, 15 };
static const u32 coinsAt[2] = { 1000, 10000 };

// La cola de avisos, por orden de llegada: cada logro se consigue una sola
// vez, así que no puede haber más de ACH_COUNT pendientes.
static u8 queue[ACH_COUNT];
static u8 qHead;

static void nextScore(void)
{
    u16 i;

    g_achNextScore = 0xFFFFFFFFUL;
    for (i = 0; i < 3; i++)
    {
        if (!Achieve_done(ACH_SCORE_100 + i))
        {
            g_achNextScore = scoreAt[i];
            return;
        }
    }
}

void Achieve_init(void)
{
    g_achMask = 0;
    g_coinsCollected = 0;
    g_achQueued = 0;
    qHead = 0;
    nextScore();
}

void Achieve_unlock(u8 id)
{
    if ((id >= ACH_COUNT) || Achieve_done(id)) return;

    g_achMask |= 1UL << id;
    g_coins += defs[id].reward;

    queue[(qHead + g_achQueued) % ACH_COUNT] = id;
    g_achQueued++;
}

void Achieve_score(u32 score)
{
    u16 i;

    for (i = 0; i < 3; i++)
        if (score >= scoreAt[i]) Achieve_unlock(ACH_SCORE_100 + i);

    nextScore();
}

void Achieve_level(u16 level)
{
    u16 i;

    for (i = 0; i < 3; i++)
        if (level >= levelAt[i]) Achieve_unlock(ACH_LEVEL_5 + i);
}

void Achieve_coins(u16 value)
{
    u16 i;

    g_coinsCollected += value;
    for (i = 0; i < 2; i++)
        if (g_coinsCollected >= coinsAt[i]) Achieve_unlock(ACH_COINS_1000 + i);
}

u8 Achieve_popNotice(void)
{
    u8 id;

    if (!g_achQueued) return ACH_NONE;

    id = queue[qHead];
    qHead = (qHead + 1) % ACH_COUNT;
    g_achQueued--;
    return id;
}

const char *Achieve_name(u8 id)
{
    return (id < ACH_COUNT) ? defs[id].name : "";
}

u16 Achieve_reward(u8 id)
{
    return (id < ACH_COUNT) ? defs[id].reward : 0;
}

u16 Achieve_doneCount(void)
{
    u16 i, n = 0;

    for (i = 0; i < ACH_COUNT; i++)
        if (Achieve_done(i)) n++;

    return n;
}
