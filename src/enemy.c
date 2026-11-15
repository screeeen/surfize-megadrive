#include "gm_art.h"
#include "enemy.h"
#include "spr.h"
#include "vramspr.h"
#include "resources.h"
#include "entity.h"
#include "fastmath.h"
#include "collision.h"
#include "hole.h"
#include "game.h"

typedef struct
{
    Entity  e;
    fix16   animIndex;      // image_index del original
    MDSpr *sprite;
    VStream stream;         // su hueco de VRAM: el frame se sube al cambiar
    u8      shownF;         // frame (rot * 8 + anim) que hay subido en el hueco
} EnemySlot;

static EnemySlot enemies[MAX_SPIDERS];

// Las arañas van por VStream (ver inc/vramspr.h): cada una tiene su hueco de
// 4 tiles y el frame se sube por DMA al cambiar. Precargadas eran 256 tiles
// (8 rotaciones x 8 frames); así son 32, y los 224 que sobran se los lleva
// el fondo (VTILE_SHARED_TILES). El peor caso son 8 arañas cambiando a la
// vez: 8 x 128 bytes por frame, y medido en el caso peor eso costaba un 9 %
// de frames perdidos. Así que cada frame sólo pueden subir las arañas de una
// ventana de SPIDER_DMA_WINDOW, que va rotando: cada una se actualiza como
// mucho cada 3 frames (la animación avanza 1,2 frames por frame, no se nota).
#define SPIDER_DMA_WINDOW 3
static u16 spiderWindow;

// ---------------------------------------------------------------------------
// Caché de las arañas vivas, compactada, en coordenadas enteras.
//
// Todo lo que consulta a las arañas (14 proyectiles por frame, el jugador, el
// apuntado, las bombas) necesita la caja entera, y antes cada consulta la
// recalculaba desde el fix16: dos llamadas a F16_toInt por araña y consulta,
// más la comprobación de ranura viva. Medido, el barrido de proyectiles solo
// se comía el 79 % de un frame PAL.
//
// Ahora se calcula una vez por frame, al final de Enemy_updateAll, y el resto
// del frame recorre sólo lo que está vivo. Quien mata una araña la saca de la
// lista en el acto (liveKill) para que el frame siga siendo coherente.
// ---------------------------------------------------------------------------
static s16 liveX[MAX_SPIDERS];      // caja ya desplazada por SPIDER_BOX_O*
static s16 liveY[MAX_SPIDERS];
static u8  liveSlot[MAX_SPIDERS];   // ranura de enemies[] de cada entrada
static u16 liveCount;

static void liveRebuild(void)
{
    u16 i;

    liveCount = 0;

    for (i = 0; i < MAX_SPIDERS; i++)
    {
        const Entity *e = &enemies[i].e;

        if (!e->active || !e->visible) continue;

        liveX[liveCount] = F2I(e->x) + SPIDER_BOX_OX;
        liveY[liveCount] = F2I(e->y) + SPIDER_BOX_OY;
        liveSlot[liveCount] = (u8)i;
        liveCount++;
    }
}

static void liveKill(u16 n)
{
    liveCount--;
    liveX[n] = liveX[liveCount];
    liveY[n] = liveY[liveCount];
    liveSlot[n] = liveSlot[liveCount];
}

static void hide(EnemySlot *s)
{
    sprHide(s->sprite);
}

void Enemy_initAll(void)
{
    u16 i;

    for (i = 0; i < MAX_SPIDERS; i++)
    {
        Entity_clear(&enemies[i].e);
        enemies[i].animIndex = 0;
        enemies[i].sprite = VStream_add(&enemies[i].stream, &spiderSprite, PAL1);
        enemies[i].shownF = 0xFF;
        // La animación la lleva el port (image_speed del original), no el
        // temporizador propio de SGDK.
    }
}

void Enemy_spawn(void)
{
    u16 i;

    for (i = 0; i < MAX_SPIDERS; i++)
    {
        if (enemies[i].e.active) continue;

        {
            const u16 h = Hole_spiderSpawn();

            // oSpider/Create_0: hp = 0, speed = 0, alarm[0] = 90, visible = 0,
            // y las coordenadas del agujero. La velocidad ya no es el velo = 3
            // del GML sino la de MonsterLite por nivel (setSpeed, maxSpeed 1
            // del prefab de la araña); el giro sigue siendo el del GML hasta
            // el bloque 8.
            Entity_spawnHidden(&enemies[i].e, Hole_x(h), Hole_y(h),
                               Entity_mlSpeed(g_level, ML_SPIDER_MAX_MU),
                               SPIDER_TURN, SPIDER_HP);
            enemies[i].animIndex = 0;
            hide(&enemies[i]);
        }
        return;
    }
}

void Enemy_updateAll(bool hasPlayer, fix16 px, fix16 py)
{
    u16 i;

    spiderWindow += SPIDER_DMA_WINDOW;
    if (spiderWindow >= MAX_SPIDERS) spiderWindow -= MAX_SPIDERS;

    for (i = 0; i < MAX_SPIDERS; i++)
    {
        EnemySlot *s = &enemies[i];

        if (!s->e.active) continue;

        // alarm[0]: el original sigue ejecutando la máquina de estados
        // mientras la araña está invisible (Step_1 corre siempre); lo único
        // que la frena es speed = 0 hasta que salta la alarma. Así que
        // aparece ya girada hacia el jugador y con su reloj de ataque
        // avanzado 75 frames de los 250.
        const bool shown = Entity_tickAppear(&s->e);

        Entity_updateAI(&s->e, hasPlayer, px, py,
                        SPIDER_BOX_OX, SPIDER_BOX_OY, SPIDER_BOX_W, SPIDER_BOX_H);
        Entity_advance(&s->e);

        if (!shown)
        {
            hide(s);
            continue;
        }

        // image_speed por defecto del sprite: 1 frame de animación por frame
        // de juego a 60 Hz, que a 50 Hz son 1.2 para que el ciclo dure lo
        // mismo en tiempo real.
        s->animIndex += GML_SPEED_ANIM;
        if (s->animIndex >= FIX16(ENEMY_ANIM_FRAMES)) s->animIndex -= FIX16(ENEMY_ANIM_FRAMES);

        // Fila de la hoja = rotación (animación de rescomp), columna = frame
        // de animación. SPR_setAnimAndFrame pone las dos sin que el cambio de
        // rotación reinicie la animación.
        {
            const u16 rot = Entity_frame8(&s->e);
            const u16 fr  = (u16)F2I(s->animIndex);
            u16 f = (rot * ENEMY_ANIM_FRAMES) + fr;
            // ¿le toca subir este frame? (la ventana da la vuelta al pool)
            const u16 k = (i + MAX_SPIDERS - spiderWindow) % MAX_SPIDERS;

            if ((f != s->shownF) && ((k < SPIDER_DMA_WINDOW) || (s->shownF == 0xFF)))
            {
                VStream_frame(&s->stream, s->sprite, spiderSprite.animations[rot]->frames[fr]);
                s->shownF = (u8)f;
            }
            // Si no le toca, sigue el frame de antes, con SU desplazamiento.
            f = s->shownF;

            // Los frames van recortados a 16x16 alrededor del dibujo (ver
            // trim en tools/gm2md.py): cada uno trae dónde cae su esquina
            // respecto al centro de la araña.
            sprPos(s->sprite, F2I(s->e.x) + spiderOffX[f], F2I(s->e.y) + spiderOffY[f]);
        }
    }

    liveRebuild();
}

bool Enemy_collides(s16 px, s16 py, s16 pw, s16 ph)
{
    u16 n;

    for (n = 0; n < liveCount; n++)
        if (Coll_overlap(px, py, pw, ph, liveX[n], liveY[n], SPIDER_BOX_W, SPIDER_BOX_H))
            return TRUE;

    return FALSE;
}

bool Enemy_closest(s16 x, s16 y, s16 *outX, s16 *outY, s32 *bestIO)
{
    u16 n;
    bool found = FALSE;
    s32 best = *bestIO;

    for (n = 0; n < liveCount; n++)
    {
        // La caché guarda la caja; el centro que compara el original es la
        // posición de la instancia, de ahí el desplazamiento de vuelta.
        const s16 ex = liveX[n] - SPIDER_BOX_OX;
        const s16 ey = liveY[n] - SPIDER_BOX_OY;
        const s16 dx = ex - x;
        const s16 dy = ey - y;
        const s32 d = (s32)(dx * dx) + (s32)(dy * dy);

        if (d < best)
        {
            found = TRUE;
            best = d;
            *outX = ex;
            *outY = ey;
        }
    }

    // Las que todavía están ocultas en su agujero (alarm[0]) no entran en la
    // lista de vivas, que es la de colisión, pero SÍ cuentan para apuntar:
    // en el original son oMalo desde el Create_0 y instance_nearest no mira
    // la visibilidad.
    for (n = 0; n < MAX_SPIDERS; n++)
    {
        const Entity *e = &enemies[n].e;

        if (!e->active || e->visible) continue;

        {
            const s16 ex = F2I(e->x);
            const s16 ey = F2I(e->y);
            const s16 dx = ex - x;
            const s16 dy = ey - y;
            const s32 d = (s32)(dx * dx) + (s32)(dy * dy);

            if (d < best)
            {
                found = TRUE;
                best = d;
                *outX = ex;
                *outY = ey;
            }
        }
    }

    *bestIO = best;
    return found;
}

bool Enemy_hit(s16 x, s16 y, s16 w, s16 h, s16 dmg, bool *killed)
{
    u16 n;

    *killed = FALSE;

    for (n = 0; n < liveCount; n++)
    {
        EnemySlot *s;

        if (!Coll_overlap(x, y, w, h, liveX[n], liveY[n], SPIDER_BOX_W, SPIDER_BOX_H)) continue;

        // oSpider/Collision_oBulletS: "hp = hp - 4; if hp < 0 { ... }"
        // oSpider/Collision_oDestro: "hp = hp - 1".
        s = &enemies[liveSlot[n]];
        s->e.hp -= dmg;

        if (s->e.hp < 0)
        {
            Entity_clear(&s->e);
            hide(s);
            liveKill(n);
            *killed = TRUE;
        }

        return TRUE;
    }

    return FALSE;
}

bool Enemy_hitPoint(s16 x, s16 y, s16 dmg, bool *killed)
{
    u16 n;

    *killed = FALSE;

    for (n = 0; n < liveCount; n++)
    {
        EnemySlot *s;

        // (u16)(x - caja) < ancho hace el "dentro del rango" con UNA
        // comparación: si x queda por debajo de la caja, la resta se envuelve
        // a un número enorme y falla igual.
        if ((u16)(x - liveX[n]) >= (u16)SPIDER_BOX_W) continue;
        if ((u16)(y - liveY[n]) >= (u16)SPIDER_BOX_H) continue;

        s = &enemies[liveSlot[n]];
        s->e.hp -= dmg;

        if (s->e.hp < 0)
        {
            Entity_clear(&s->e);
            hide(s);
            liveKill(n);
            *killed = TRUE;
        }

        return TRUE;
    }

    return FALSE;
}

bool Enemy_damageNoKill(s16 x, s16 y, s16 w, s16 h, s16 dmg)
{
    u16 n;

    for (n = 0; n < liveCount; n++)
    {
        if (!Coll_overlap(x, y, w, h, liveX[n], liveY[n], SPIDER_BOX_W, SPIDER_BOX_H)) continue;

        enemies[liveSlot[n]].e.hp -= dmg;
        return TRUE;
    }

    return FALSE;
}

u16 Enemy_damageArea(s16 x, s16 y, s16 w, s16 h)
{
    u16 n = liveCount, kills = 0;

    // Hacia atrás: liveKill() mueve la última entrada al hueco.
    while (n--)
    {
        EnemySlot *s;

        if (!Coll_overlap(x, y, w, h, liveX[n], liveY[n], SPIDER_BOX_W, SPIDER_BOX_H)) continue;

        s = &enemies[liveSlot[n]];
        s->e.hp -= DMG_TO_SPIDER;

        if (s->e.hp < 0)
        {
            Entity_clear(&s->e);
            hide(s);
            liveKill(n);
            kills++;
        }
    }

    return kills;
}

void Enemy_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_SPIDERS; i++)
    {
        Entity_clear(&enemies[i].e);
        hide(&enemies[i]);
    }

    liveCount = 0;
}

u16 Enemy_countAlive(void)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_SPIDERS; i++)
        if (enemies[i].e.active) n++;

    return n;
}
