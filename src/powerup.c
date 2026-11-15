#include "powerup.h"
#include "fastmath.h"
#include "collision.h"
#include "hole.h"
#include "spr.h"
#include "vramspr.h"
#include "resources.h"
#include "gm_art.h"

// Tope de cada tipo, de generaPaquete(): 24 paquetes, 1 bomba, 2 réplicas y
// 1 de rayos.
#define MAX_POW_TOTAL (MAX_PACKAGES + 4)

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
    s16  x;             // origen de la instancia
    s16  y;
    MDSpr *sprite;
} Pickup;

static Pickup pows[MAX_POW_TOTAL];
static u16    groundTile[4][GROUND_FRAMES];
static u16    pulseTick;
// Con qué cámara se colocaron los sprites la última vez (ver Pow_updateAll).
static s16    placedShakeX, placedShakeY;

static const u8 animOf[4] = { GROUND_ANIM_PACK, GROUND_ANIM_BOOM,
                              GROUND_ANIM_REPLICA, GROUND_ANIM_RAYS };

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

    pulseTick = 0;
}

// El latido, una vez por frame. Todos a la vez, como el original, que los
// dibuja a todos con su propia t pero arrancando de la misma.
void Pow_updateAll(void)
{
    u16 i;

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

        // Collision_oAgujerSuper: lo que cae sobre un agujero muere.
        if (Hole_touching(pows[i].x + PACK_BOX_OX, pows[i].y + PACK_BOX_OY,
                          PACK_BOX_W, PACK_BOX_H))
            return;

        pows[i].alive = TRUE;
        pows[i].frame = 0;
        place(&pows[i]);
        return;
    }
}

void Pow_generate(fix16 x, fix16 y)
{
    // generaPaquete(): hasta 16 paquetes crea paquetes; luego una bomba si no
    // hay; luego hasta 2 réplicas mientras haya menos de 18 paquetes; luego
    // uno de rayos mientras haya menos de 24; si no, otro paquete.
    const u16 packs = Pow_count(POW_PACK);

    if (packs < PAQUETES_16)
    {
        spawn(POW_PACK, x, y);
        return;
    }

    if (Pow_count(POW_BOOM) < 1)
    {
        spawn(POW_BOOM, x, y);
        return;
    }

    if ((Pow_count(POW_REPLICA) < 2) && (packs < PAQUETES_18))
    {
        spawn(POW_REPLICA, x, y);
        return;
    }

    if ((Pow_count(POW_RAYS) < 1) && (packs < PAQUETES_24))
    {
        spawn(POW_RAYS, x, y);
        return;
    }

    spawn(POW_PACK, x, y);
}

bool Pow_collect(s16 px, s16 py, s16 pw, s16 ph, u8 weaponIndex, PowerPickup *out)
{
    // oPlayer/Collision_oPaquete: la munición depende del arma en curso.
    static const u16 ammo[7] = { AMMO_WBLOCK, AMMO_LASER, AMMO_3WAY, AMMO_PIVOT,
                                 AMMO_PARA, AMMO_BOMBA, AMMO_RAY };
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        if (!pows[i].alive) continue;

        if (!Coll_overlap(px, py, pw, ph,
                          pows[i].x + PACK_BOX_OX, pows[i].y + PACK_BOX_OY,
                          PACK_BOX_W, PACK_BOX_H))
            continue;

        out->type = pows[i].type;
        out->x = FIX16(pows[i].x);
        out->y = FIX16(pows[i].y);
        out->ammo = (pows[i].type == POW_PACK) ? ammo[(weaponIndex < 7) ? weaponIndex : 0] : 0;

        pows[i].alive = FALSE;
        sprHide(pows[i].sprite);
        return TRUE;
    }

    return FALSE;
}

void Pow_replaceAll(void)
{
    u16 i;

    placedShakeX = g_shakeX;
    placedShakeY = g_shakeY;

    for (i = 0; i < MAX_POW_TOTAL; i++)
        if (pows[i].alive) place(&pows[i]);
}

void Pow_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_POW_TOTAL; i++)
    {
        pows[i].alive = FALSE;
        sprHide(pows[i].sprite);
    }
}
