#include "save.h"
#include "game.h"
#include "audio.h"
#include "weapon.h"

#define SAVE_SIG        0x5346      // "SF"
#define SAVE_VERSION    1
#define SAVE_SLOT_SIZE  64          // bytes reservados por copia
#define SAVE_SLOTS      2

// Lo que se guarda. Cambiar algo aquí es subir SAVE_VERSION: una copia de
// otra versión no se carga (valores por defecto).
typedef struct
{
    u16 sig;
    u16 version;
    u16 seq;            // la copia con el número mayor es la más reciente
    u16 bought;         // armas compradas, un bit por WeaponId
    u32 best;           // récord de puntuación (savedMaxKills)
    u32 coins;          // saldo
    u32 games;          // partidas jugadas (numberOfGames)
    u16 maxKills;       // récord de bajas: desbloquea PIVOT y PARA (GML)
    u8  weapon;         // arma equipada (currentWeapon)
    u8  flags;          // bit 0 sonido, bit 1 música
    u32 achievements;   // logros (bloque 10): reservado
    u16 counters[8];    // contadores de los logros (bloque 10): reservado
    u16 sum;            // suma de control de todo lo anterior
} SaveData;

u32 g_gamesPlayed;

static u16 slotSeq;     // secuencia de la última copia cargada o escrita
static u8  slotLast;    // copia que se escribió la última

static u16 checksum(const SaveData *d)
{
    const u8 *p = (const u8 *)d;
    u16 a = 0x1234, b = 0;
    u16 i;

    // Fletcher de 16 bits sobre todo menos el propio campo de la suma. El
    // módulo 255 con restas: se calcula con cada moneda recogida, y dos
    // divisiones por byte eran ~12 000 ciclos.
    for (i = 0; i < (sizeof(SaveData) - sizeof(u16)); i++)
    {
        a += p[i];
        if (a >= 255) a -= 255;
        b += a;
        if (b >= 255) b -= 255;
    }
    return (u16)((b << 8) | a);
}

static void readSlot(u16 slot, SaveData *d)
{
    u8 *p = (u8 *)d;
    u16 i;

    for (i = 0; i < sizeof(SaveData); i++)
        p[i] = SRAM_readByte((slot * SAVE_SLOT_SIZE) + i);
}

static bool valid(const SaveData *d)
{
    return (d->sig == SAVE_SIG) && (d->version == SAVE_VERSION) &&
           (d->sum == checksum(d)) && (d->weapon < WPN_COUNT);
}

void Save_load(void)
{
    SaveData d[SAVE_SLOTS];
    s16 best = -1;
    u16 i;

    SRAM_enableRO();
    for (i = 0; i < SAVE_SLOTS; i++) readSlot(i, &d[i]);
    SRAM_disable();

    for (i = 0; i < SAVE_SLOTS; i++)
    {
        if (!valid(&d[i])) continue;
        // La más reciente; la secuencia da la vuelta a los 65536 guardados,
        // así que se compara con resta con signo.
        if ((best < 0) || ((s16)(d[i].seq - d[best].seq) > 0)) best = i;
    }

    slotSeq = 0;
    slotLast = 1;           // la primera escritura va a la copia 0
    if (best < 0) return;

    slotSeq = d[best].seq;
    slotLast = (u8)best;

    g_lastScore = d[best].best;
    g_coins = d[best].coins;
    g_gamesPlayed = d[best].games;
    g_maxKills = d[best].maxKills;
    // WBLOCK está siempre comprada.
    g_weaponsBought = d[best].bought | (1 << WPN_WBLOCK);
    g_weaponIndex = d[best].weapon;
    g_soundOn = (d[best].flags & 1) != 0;
    g_musicOn = (d[best].flags & 2) != 0;
}

void Save_write(void)
{
    SaveData d;
    const u16 slot = slotLast ^ 1;
    const u8 *p = (const u8 *)&d;
    u16 i;

    memset(&d, 0, sizeof(d));
    d.sig = SAVE_SIG;
    d.version = SAVE_VERSION;
    d.seq = ++slotSeq;
    d.bought = g_weaponsBought;
    d.best = g_lastScore;
    d.coins = g_coins;
    d.games = g_gamesPlayed;
    d.maxKills = g_maxKills;
    d.weapon = (u8)g_weaponIndex;
    d.flags = (g_soundOn ? 1 : 0) | (g_musicOn ? 2 : 0);
    d.sum = checksum(&d);

    SRAM_enable();
    for (i = 0; i < sizeof(SaveData); i++)
        SRAM_writeByte((slot * SAVE_SLOT_SIZE) + i, p[i]);
    SRAM_disable();

    slotLast = (u8)slot;
}
