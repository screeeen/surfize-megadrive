#include "flyer.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "resources.h"
#include "gm_art.h"
#include "spr.h"

typedef struct
{
    u8    live;
    u8    digits;                       // cuántas cifras tiene el número
    fix16 x, y;
    fix16 speed;
    fix16 tx, ty;                       // destino de move_towards_point
    u16   life;
    MDSpr *sprite[FLYER_DIGITS];
} Flyer;

static Flyer flyers[MAX_FLYERS];
static u16   digitTile[10];
static u16   nextSlot;

void Flyer_initAll(void)
{
    u16 i, d;

    VSpr_load(&digitSprite, digitTile, 1);

    for (i = 0; i < MAX_FLYERS; i++)
    {
        flyers[i].live = FALSE;
        for (d = 0; d < FLYER_DIGITS; d++)
            flyers[i].sprite[d] = VSpr_add(&digitSprite, PAL1, digitTile[0]);
    }

    nextSlot = 0;
}

static void hideAll(Flyer *f)
{
    u16 d;

    for (d = 0; d < FLYER_DIGITS; d++) sprHide(f->sprite[d]);
}

void Flyer_spawn(fix16 x, fix16 y, u8 type, u16 value)
{
    Flyer *f = &flyers[nextSlot];
    u16 d, v;

    if (++nextSlot >= MAX_FLYERS) nextSlot = 0;

    f->live   = TRUE;
    f->x      = x;
    f->y      = y;
    f->speed  = FLYER_SPEED0;
    f->life   = FLYER_LIFE;
    f->tx     = FIX16(type ? FLYER_TARGET_X1 : FLYER_TARGET_X0);
    f->ty     = 0;

    // Las cifras, de izquierda a derecha, con el tope del pool.
    if (value > 999) value = 999;
    f->digits = (value >= 100) ? 3 : ((value >= 10) ? 2 : 1);

    v = value;
    for (d = f->digits; d-- > 0; )
    {
        const u16 dig = v % 10;

        v /= 10;
        sprAnim(f->sprite[d], dig);
        sprTile(f->sprite[d], digitTile[dig]);
    }

    for (d = f->digits; d < FLYER_DIGITS; d++) sprHide(f->sprite[d]);
}

void Flyer_updateAll(void)
{
    u16 i, d;

    for (i = 0; i < MAX_FLYERS; i++)
    {
        Flyer *f = &flyers[i];
        s16 px, py;

        if (!f->live) continue;

        if (--f->life == 0)
        {
            f->live = FALSE;
            hideAll(f);
            continue;
        }

        // speed += acc; move_towards_point(tx, ty, speed)
        f->speed += FLYER_ACCEL;

        {
            const fix16 a = Fast_angle(f->x, f->y, f->tx, f->ty);

            f->x += fmul(fcos(a), f->speed);
            f->y += fmul(fsin(a), f->speed);
        }

        px = F2I(f->x);
        py = F2I(f->y);

        // El número se va hacia su esquina del HUD y se sale por arriba; en
        // cuanto deja de verse se aparca, que si no seguiría ocupando una
        // entrada de la tabla del VDP (ver el contrato en inc/spr.h).
        if (MDSPR_OFFSCREEN(px, py))
        {
            f->live = FALSE;
            hideAll(f);
            continue;
        }

        for (d = 0; d < f->digits; d++)
            sprPos(f->sprite[d], px + (s16)(d * DIGIT_CELL), py);
    }
}

void Flyer_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_FLYERS; i++)
    {
        flyers[i].live = FALSE;
        hideAll(&flyers[i]);
    }

    nextSlot = 0;
}
