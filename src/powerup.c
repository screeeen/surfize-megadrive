#include "powerup.h"
#include "fastmath.h"
#include "collision.h"
#include "hole.h"
#include "spr.h"
#include "vramspr.h"
#include "resources.h"
#include "gm_art.h"
#include "weapon.h"
#include "game.h"

// 24 paquetes (el tope de generaPaquete() del GML; los de MonsterLite
// caducan a los 167 f y no llegan) y hueco para la bomba y los rayos de
// MonsterLite (bloque 7), que no caducan.
#define MAX_POW_TOTAL (MAX_PACKAGES + ML_GROUND_POW_MAX)

// Media caja de cada tipo (PowerType): paquete, bomba, réplica (no se usa),
// rayos.
static const s16 boxHalf[4] = { ML_ITEM_HALF, ML_BOMB_HALF, ML_ITEM_HALF, ML_RAYS_HALF };

// El dibujo va centrado en el origen de la instancia, como en el original.
#define POW_SPR_OFF (-(GROUND_CELL / 2))

// oPaquete/Draw_0: t += 0.05 a 60 Hz y rebota entre 0,8 y 1,4, o sea un ciclo
// de unos 12 frames. Con tres tamaños eso son 4 frames por tamaño.
#define POW_PULSE_EVERY GML_FRAMES(4)

typedef struct
{
    bool alive;
    u8   type;
    u8   frame;
    u8   lifted;        // paquete recogido: sube y se acaba (bloque 4)
    u8   weapon;        // paquete: arma equipada al aparecer (bulletType)
    u16  life;          // paquete: frames que le quedan (0 = no caduca)
    s16  x;             // origen de la instancia
    s16  y;
    fix16 fy;           // paquete que sube: la Y con decimales
    MDSpr *sprite;
} Pickup;

// La moneda de MonsterLite (S/Coin.cs), bloque 4.
typedef struct
{
    bool  alive;
    u8    flying;       // recogida: vuela hacia abajo (moveCoin)
    u8    frame;
    u8    animAcc;      // 12 frames de animación por cada 50 de juego
    u16   life;
    u16   value;
    fix16 x;
    fix16 y;
    MDSpr *sprite;
} CoinItem;

static CoinItem coins[MAX_COINS];
static u16      coinTile[ML_COIN_FRAMES];

#define COIN_SPR_OFF (-8)

static Pickup pows[MAX_POW_TOTAL];
static u16    groundTile[4][GROUND_FRAMES];
static u16    pulseTick;
// Con qué cámara se colocaron los sprites la última vez (ver Pow_updateAll).
static s16    placedShakeX, placedShakeY;

static const u8 animOf[4] = { GROUND_ANIM_PACK, GROUND_ANIM_BOOM,
                              GROUND_ANIM_REPLICA, GROUND_ANIM_RAYS };

static void placeCoin(CoinItem *c)
{
    const s16 x = F2I(c->x);
    const s16 y = F2I(c->y);

    if (MDSPR_OFFSCREEN(x + COIN_SPR_OFF, y + COIN_SPR_OFF))
    {
        sprHide(c->sprite);
        return;
    }

    sprPos(c->sprite, x + COIN_SPR_OFF, y + COIN_SPR_OFF);
    sprAnimFrame(c->sprite, 0, c->frame);
    sprTile(c->sprite, coinTile[c->frame]);
}

static void place(Pickup *p)
{
    sprPos(p->sprite, p->x + POW_SPR_OFF, p->y + POW_SPR_OFF);
    sprAnimFrame(p->sprite, animOf[p->type], p->frame);
    sprTile(p->sprite, groundTile[p->type][p->frame]);
}

void Pow_initAll(void)
{
    u16 i;

    VSpr_load(&groundSprite, &groundTile[0][0], GROUND_FRAMES);

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        pows[i].alive = FALSE;
        pows[i].sprite = VSpr_add(&groundSprite, PAL1, groundTile[0][0]);
    }

    VSpr_load(&coinSprite, coinTile, ML_COIN_FRAMES);
    for (i = 0; i < MAX_COINS; i++)
    {
        coins[i].alive = FALSE;
        coins[i].sprite = VSpr_add(&coinSprite, PAL1, coinTile[0]);
    }

    pulseTick = 0;
}

// Caducidad, subida del paquete recogido y vuelo de la moneda. Va en Update
// en MonsterLite, que sigue corriendo con el tiempo parado: la vida baja
// igual y el paquete sube; la moneda que vuela no (va con deltaTime).
static void tickItems(bool frozen)
{
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        Pickup *p = &pows[i];
        const u16 dec = p->lifted ? (1 + ML_ITEM_LIFT_DEC) : 1;

        if (!p->alive || (p->life == 0)) continue;

        if (p->life <= dec)
        {
            p->alive = FALSE;
            sprHide(p->sprite);
            continue;
        }
        p->life -= dec;

        if (p->lifted)
        {
            p->fy -= ML_PACK_RISE;
            p->y = F2I(p->fy);
            place(p);
        }
    }

    for (i = 0; i < MAX_COINS; i++)
    {
        CoinItem *c = &coins[i];

        if (!c->alive) continue;

        if (c->life <= 1)
        {
            c->alive = FALSE;
            sprHide(c->sprite);
            continue;
        }
        c->life--;

        if (frozen) continue;

        // coinAnim, 12 frames por segundo.
        c->animAcc += ML_COIN_FPS;
        if (c->animAcc >= 50)
        {
            c->animAcc -= 50;
            if (++c->frame >= ML_COIN_FRAMES) c->frame = 0;
        }

        if (c->flying)
        {
            const fix16 tx = FIX16(ML_COIN_TARGET_X);
            const fix16 ty = FIX16(ML_COIN_TARGET_Y);
            const fix16 a = Fast_angle(c->x, c->y, tx, ty);

            c->x += fmul(fcos(a), ML_COIN_FLY_SPEED);
            c->y += fmul(fsin(a), ML_COIN_FLY_SPEED);

            // Fuera de la pantalla por abajo ya no vuelve: se acaba.
            if (F2I(c->y) >= (PLAYFIELD_H + 8))
            {
                c->alive = FALSE;
                sprHide(c->sprite);
                continue;
            }
        }

        placeCoin(c);
    }
}

void Pow_tickFrozen(void)
{
    tickItems(TRUE);
}

// El latido, una vez por frame. Todos a la vez, como el original, que los
// dibuja a todos con su propia t pero arrancando de la misma.
void Pow_updateAll(void)
{
    u16 i;

    tickItems(FALSE);

    // Lo que espera en el suelo no se mueve, así que su sprite se colocaba
    // una sola vez, al nacer... con la cámara de ESE frame. Durante una
    // sacudida los agujeros (plano) y todo lo vivo (sprites recolocados cada
    // frame) se movían y los paquetes se quedaban clavados en la pantalla,
    // deslizándose sobre el suelo. Cuando la cámara cambia se recolocan todos.
    if ((g_shakeX != placedShakeX) || (g_shakeY != placedShakeY))
    {
        placedShakeX = g_shakeX;
        placedShakeY = g_shakeY;

        for (i = 0; i < MAX_POW_TOTAL; i++)
            if (pows[i].alive)
                sprPos(pows[i].sprite, pows[i].x + POW_SPR_OFF, pows[i].y + POW_SPR_OFF);
    }

    if (++pulseTick < POW_PULSE_EVERY) return;
    pulseTick = 0;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        Pickup *p = &pows[i];

        if (!p->alive) continue;

        if (++p->frame >= GROUND_FRAMES) p->frame = 0;
        sprAnimFrame(p->sprite, animOf[p->type], p->frame);
        sprTile(p->sprite, groundTile[p->type][p->frame]);
    }
}

u16 Pow_count(u8 type)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_POW_TOTAL; i++)
        if (pows[i].alive && (pows[i].type == type)) n++;

    return n;
}

static void spawn(u8 type, fix16 x, fix16 y)
{
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        if (pows[i].alive) continue;

        pows[i].type = type;
        pows[i].x = F2I(x);
        pows[i].y = F2I(y);
        pows[i].fy = FIX16(pows[i].y);
        pows[i].lifted = 0;
        pows[i].weapon = (u8)g_weaponIndex;
        // El paquete de MonsterLite caduca; la bomba y los rayos, no. Nada
        // de MonsterLite mira los agujeros.
        pows[i].life = (type == POW_PACK) ? ML_ITEM_LIFE_F : 0;

        pows[i].alive = TRUE;
        pows[i].frame = 0;
        place(&pows[i]);
        return;
    }
}

static void spawnCoin(fix16 x, fix16 y)
{
    static const u16 low[2]  = { 50, 60 };
    static const u16 high[5] = { 50, 60, 150, 180, 100 };
    u16 i;

    for (i = 0; i < MAX_COINS; i++)
    {
        CoinItem *c = &coins[i];

        if (c->alive) continue;

        c->alive = TRUE;
        c->flying = 0;
        c->frame = 0;
        c->animAcc = 0;
        c->life = ML_ITEM_LIFE_F;
        // Coin.getCoinType(), con el nivel de cuando aparece.
        c->value = (g_level > ML_COIN_LEVEL_HIGH) ? high[random() % 5] : low[random() & 1];
        c->x = x;
        c->y = y;
        // Q12: la moneda que nace sobre un agujero o sobre otra moneda ya no
        // sale disparada 900 px hacia arriba (animOffset): se queda donde
        // cae.
        placeCoin(c);
        return;
    }
}

void Pow_drop(fix16 x, fix16 y, bool packOnly)
{
    // leavePow(): Random.Range(0, 2) == 0 -> moneda; si no, paquete.
    if (!packOnly && !(random() & 1)) spawnCoin(x, y);
    else                              spawn(POW_PACK, x, y);
}

bool Pow_spawnML(u8 type, fix16 x, fix16 y)
{
    // Tope del port (ML_GROUND_POW_MAX): MonsterLite no limita.
    if ((Pow_count(POW_BOOM) + Pow_count(POW_RAYS)) >= ML_GROUND_POW_MAX) return FALSE;

    spawn(type, x, y);
    return TRUE;
}

bool Pow_collect(s16 cx, s16 cy, u8 weaponIndex, PowerPickup *out)
{
    // La munición que da el paquete es la del arma que estaba equipada al
    // APARECER (paqueteScr: bulletType = getWeaponType() en Start): los
    // valores del prefab de MonsterLite y, para PIVOT y PARA, los del GML.
    // En el orden de inc/weapon.h. En MonsterLite se suma a ese arma; como el
    // arma no cambia en partida, es siempre la equipada.
    static const u16 ammo[WPN_COUNT] = { ML_AMMO_WBLOCK, ML_AMMO_LASER, ML_AMMO_TWAY,
                                         AMMO_PIVOT, AMMO_PARA,
                                         ML_AMMO_CIRCULAR, ML_AMMO_MOIRE };
    u16 i;

    (void)weaponIndex;

    // La moneda (playerMovement.OnTriggerEnter, tag "coin"): suma su valor y
    // sale volando hacia abajo. La que ya vuela no se vuelve a coger.
    for (i = 0; i < MAX_COINS; i++)
    {
        CoinItem *c = &coins[i];

        if (!c->alive || c->flying) continue;

        if (!Coll_circleBox(cx, cy, ML_PLAYER_R2, ML_PLAYER_REACH,
                            F2I(c->x) - ML_ITEM_HALF, F2I(c->y) - ML_ITEM_HALF,
                            ML_ITEM_BOX, ML_ITEM_BOX))
            continue;

        out->type = POW_COIN;
        out->x = c->x;
        out->y = c->y;
        out->ammo = 0;
        out->value = c->value;
        c->flying = 1;
        return TRUE;
    }

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        Pickup *p = &pows[i];
        const bool pack = (p->type == POW_PACK);

        if (!p->alive || p->lifted) continue;

        // Cada uno con su caja de MonsterLite, centrada.
        {
            const s16 h = boxHalf[p->type];

            if (!Coll_circleBox(cx, cy, ML_PLAYER_R2, ML_PLAYER_REACH,
                                p->x - h, p->y - h, 2 * h, 2 * h))
                continue;
        }

        out->type = p->type;
        out->x = FIX16(p->x);
        out->y = FIX16(p->y);
        out->value = 0;
        out->ammo = pack ? ammo[(p->weapon < WPN_COUNT) ? p->weapon : 0] : 0;

        if (pack)
        {
            // triggerAnim(): sube y se acaba (ver tickItems).
            p->lifted = 1;
        }
        else
        {
            p->alive = FALSE;
            sprHide(p->sprite);
        }
        return TRUE;
    }

    return FALSE;
}

s32 Pow_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s)
{
    s32 best = -1;
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        s32 d;

        if (!pows[i].alive) continue;

        // La caja como círculo de su media diagonal, como la del paquete
        // (ML_RAY_R_PACK_X10 es la de 20 px).
        d = Coll_rayCircle(pows[i].x - ox, pows[i].y - oy, c, s,
                           (pows[i].type == POW_BOOM)
                               ? ML_X10_TO_F16((ML_RAY_R_PACK_X10 * ML_BOMB_HALF) / ML_ITEM_HALF)
                               : ML_X10_TO_F16(ML_RAY_R_PACK_X10));
        if ((d >= 0) && ((best < 0) || (d < best))) best = d;
    }

    // Las monedas también son triggers de 1 u.
    for (i = 0; i < MAX_COINS; i++)
    {
        s32 d;

        if (!coins[i].alive) continue;

        d = Coll_rayCircle(F2I(coins[i].x) - ox, F2I(coins[i].y) - oy,
                           c, s, ML_X10_TO_F16(ML_RAY_R_PACK_X10));
        if ((d >= 0) && ((best < 0) || (d < best))) best = d;
    }

    return best;
}

void Pow_replaceAll(void)
{
    u16 i;

    placedShakeX = g_shakeX;
    placedShakeY = g_shakeY;

    for (i = 0; i < MAX_POW_TOTAL; i++)
        if (pows[i].alive) place(&pows[i]);

    for (i = 0; i < MAX_COINS; i++)
        if (coins[i].alive) placeCoin(&coins[i]);
}

void Pow_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        pows[i].alive = FALSE;
        sprHide(pows[i].sprite);
    }

    for (i = 0; i < MAX_COINS; i++)
    {
        coins[i].alive = FALSE;
        sprHide(coins[i].sprite);
    }
}
