#include "debris.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "resources.h"
#include "gm_art.h"
#include "enemy.h"
#include "snake.h"
#include "effect.h"
#include "shake.h"

typedef struct
{
    u8    live;
    fix16 x, y;
    fix16 dx, dy;
    MDSpr *sprite;
} Shard;

static Shard  shards[MAX_DEBRIS];
static u16    nextSlot;                     // la ráfaga nueva recicla la vieja
static u16    debrisTile[DEBRIS_COLORS][DEBRIS_SIZES];

// sprDebri mide 8x8 y la celda es de 16: el dibujo va centrado en el origen.
#define DEBRIS_SPR_OFF  (-(DEBRIS_CELL / 2))

void Debris_initAll(void)
{
    u16 i;

    VSpr_load(&debrisSprite, &debrisTile[0][0], DEBRIS_SIZES);

    for (i = 0; i < MAX_DEBRIS; i++)
    {
        shards[i].live = FALSE;
        shards[i].sprite = VSpr_add(&debrisSprite, PAL1, debrisTile[0][0]);
    }

    nextSlot = 0;
}

void Debris_burst(fix16 x, fix16 y)
{
    u16 deg = random() % 360;       // la ráfaga tampoco empieza siempre igual
    u16 k;

    // oEstallido/Create_0: stShake(5, 15, 1)
    Shake_estallido();

    for (k = 0; k < DEBRIS_PER_BURST; k++)
    {
        Shard *s = &shards[nextSlot];
        const fix16 speed = DEBRIS_SPEED_MIN +
                            (fix16)(random() % (u16)(DEBRIS_SPEED_MAX - DEBRIS_SPEED_MIN));

        if (++nextSlot >= MAX_DEBRIS) nextSlot = 0;

        s->live = TRUE;
        s->x = x;
        s->y = y;
        s->dx = fmul(fcosDeg(deg % 360), speed);
        s->dy = fmul(fsinDeg(deg % 360), speed);

        // "for (i = 0; i < 360; i += random(60))": el abanico se cierra
        // cuando se completa la vuelta, no al llegar a un número fijo.
        deg += DEBRIS_STEP_MIN + (random() % DEBRIS_STEP_RAND);
        if (deg >= 360) break;
    }
}

u16 Debris_updateAll(u16 *snakeKills)
{
    u16 i;
    u16 kills = 0;
    // El tamaño y el color los sortea el original en CADA frame y para CADA
    // astilla (random(8) y choose(c_dkgray, c_black)); aquí sale un número por
    // frame y cada astilla usa un trozo distinto, que da el mismo parpadeo sin
    // doce llamadas a random().
    u16 roll = random();

    for (i = 0; i < MAX_DEBRIS; i++)
    {
        Shard *s = &shards[i];
        s16 px, py;
        bool killed, body;

        if (!s->live) continue;

        s->x += s->dx;
        s->y += s->dy;
        px = F2I(s->x);
        py = F2I(s->y);

        // Other_40 ("fuera de la vista 0"): muere al salir, sin wrap.
        if ((px < 0) || (px >= PLAYFIELD_W) || (py < 0) || (py >= PLAYFIELD_H))
        {
            s->live = FALSE;
            sprHide(s->sprite);
            continue;
        }

        // oSpider/oCabeza*/Collision_oDestro: "hp -= 1" + explosión + destruye
        // la astilla. Los eslabones se la quedan sin recibir daño, igual que
        // con las balas.
        if (Enemy_hitPoint(px, py, DMG_DEBRIS, &killed))
        {
            Effect_spawn(s->x, s->y);
            s->live = FALSE;
            sprHide(s->sprite);
            if (killed) kills++;
            continue;
        }

        if (Snake_hitPoint(px, py, &killed, &body))
        {
            Effect_spawn(s->x, s->y);
            s->live = FALSE;
            sprHide(s->sprite);
            if (killed) { kills++; (*snakeKills)++; }
            continue;
        }

        sprPos(s->sprite, px + DEBRIS_SPR_OFF, py + DEBRIS_SPR_OFF);
        sprAnimFrame(s->sprite, roll & 1, (roll >> 1) & (DEBRIS_SIZES - 1));
        sprTile(s->sprite, debrisTile[roll & 1][(roll >> 1) & (DEBRIS_SIZES - 1)]);
        roll = (roll >> 3) | (roll << 13);
    }

    return kills;
}

void Debris_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_DEBRIS; i++)
    {
        shards[i].live = FALSE;
        sprHide(shards[i].sprite);
    }

    nextSlot = 0;
}
