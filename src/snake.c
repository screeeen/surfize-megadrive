#include "gm_art.h"
#include "snake.h"
#include "spr.h"
#include "vramspr.h"
#include "bench.h"
#include "resources.h"
#include "entity.h"
#include "fastmath.h"
#include "collision.h"
#include "hole.h"
#include "effect.h"
#include "audio.h"
#include "game.h"

typedef struct
{
    Entity e;
    u8     type;
    u8     followers;       // 12 (tipo 1) o 10 (tipo 2)
    u8     stepDelay;       // stepdelay del GML, en frames de 50 Hz
    // El historial se guarda ya en PIXELES enteros: la cabeza convierte una
    // vez por frame y los 12 eslabones lo leen tal cual. Antes guardaba fix16
    // y cada eslabón hacía dos desplazamientos de 6 bits, que en el 68000 son
    // 18 ciclos cada uno: 36 por eslabón y frame, unos 1700 con la arena
    // llena, para un valor que ya se podía haber convertido una sola vez.
    s16    histX[SNAKE_MAX_HISTORY];
    s16    histY[SNAKE_MAX_HISTORY];
    u16    histPos;         // siguiente posición a escribir
    // Posición de cada eslabón en píxeles enteros, calculada una sola vez por
    // frame: el apuntado, las colisiones y el dibujado la leen de aquí en vez
    // de recorrer el historial tres veces. En fix16 no se guarda: nadie la
    // necesitaba con esa precisión y eran dos escrituras más por eslabón y
    // frame, 96 en total.
    s16    partIX[SNAKE_MAX_FOLLOWERS];
    s16    partIY[SNAKE_MAX_FOLLOWERS];
    s16    headIX, headIY;
    // Caja que envuelve cabeza y eslabones, para descartar el gusano entero
    // con cuatro comparaciones: los eslabones son un escudo contra las balas
    // (ver Snake_hit) y comprobar 12 cajas por gusano y por proyectil son
    // ~670 solapes por frame que casi siempre fallan.
    // Medido: partir esta caja en dos mitades para afinar el descarte sale
    // PEOR (41,8 fps frente a 43,0 en el caso peor). Mantener y comprobar dos
    // cajas cuesta más de lo que ahorra saltarse seis eslabones, porque el
    // barrido de 12 sólo corre cuando algo cae dentro de verdad.
    s16    bbX0, bbY0, bbX1, bbY1;
    u8     bbTick;
    // El salto al agujero 0 (ver SNAKE_BB_MARGIN): frames desde que la
    // cabeza saltó (jumpSpan = ninguno en el cuerpo), cuántos tarda el salto
    // en llegar al último eslabón, y si el frame anterior también saltó.
    u8     jumpAge;
    u8     jumpSpan;
    u8     snapped;
    MDSpr *headSprite;
    MDSpr *segSprite[SNAKE_MAX_FOLLOWERS];
    // La cabeza gira a 32 direcciones con los frames en ROM, subidos por DMA
    // al cambiar (ver VStream en inc/vramspr.h): su hueco de VRAM y su hoja.
    VStream headStream;
    const SpriteDefinition *headDef;
    // oCu1 y oCu2 (los dos últimos eslabones) giran a 8 direcciones: van por
    // VStream como la cabeza, cada uno con su hueco. Precargadas, sus dos
    // hojas eran 64 tiles; así son 8 por gusano.
    VStream cu1Stream, cu2Stream;
} SnakeSlot;

// Envolvente de las máscaras de cabeza y de eslabón, en desplazamientos desde
// el punto de la parte: el margen con el que se crece la caja del gusano.
#define MIN2(a, b)      (((a) < (b)) ? (a) : (b))
#define MAX2(a, b)      (((a) > (b)) ? (a) : (b))
#define SNAKE_BB_OX     MIN2(SNAKE_HEAD_BOX_OX, SNAKE_BODY_BOX_OX)
#define SNAKE_BB_OY     MIN2(SNAKE_HEAD_BOX_OY, SNAKE_BODY_BOX_OY)
#define SNAKE_BB_X1     MAX2(SNAKE_HEAD_BOX_OX + SNAKE_HEAD_BOX_W, SNAKE_BODY_BOX_OX + SNAKE_BODY_BOX_W)
#define SNAKE_BB_Y1     MAX2(SNAKE_HEAD_BOX_OY + SNAKE_HEAD_BOX_H, SNAKE_BODY_BOX_OY + SNAKE_BODY_BOX_H)

// La caja envolvente no hace falta exacta: sólo sirve para descartar deprisa.
// Recalcularla cada frame son cuatro comparaciones por eslabón (unos 3800
// ciclos con la arena llena), así que se recalcula cada 4 frames y se infla
// con lo más que puede moverse el gusano en ese tiempo, redondeado hacia
// arriba.
//
// Con la velocidad del GML eran 1,875 px/f x 4 -> 8. Con la de MonsterLite
// por nivel (bloque 1) el máximo es el del nivel 9: f = 9.8 u/s -> 3,92 px/f
// (251/64 en fix16), x 4 = 15,7 -> 16.
//
// El margen sólo cubre el AVANCE. La cabeza también puede SALTAR: en
// stWalkingA1W, al tocar un agujero, la IA la lleva al agujero 0 (ver
// Entity_updateAI): desde otro agujero, a 30 px o más de donde estaba, y al
// llegar al propio agujero 0, hasta unos 12. Con la caja de hasta 3
// frames antes, la cabeza y luego cada eslabón que pasa por el salto
// quedaban fuera de la caja: hasta 3 frames sin chocar con la nave ni con las
// balas, y la nave sin poder apuntarles (lo midió el verificador del bloque
// 1: partes a 48 px fuera). Por eso, desde el frame del salto y hasta que el
// salto ha recorrido el cuerpo entero (eslabones x stepdelay: 72 y 50
// frames), la caja se recalcula CADA frame, exacta.
//
// Sólo cuenta el PRIMER salto de una racha. Mientras vaga encima del agujero
// 0, la cabeza lo sigue tocando y la IA la vuelve a poner en el centro cada
// frame, pero esos saltos son de menos de un avance (todas esas posiciones
// caben en +-4 px del agujero), y el margen los cubre: recalcular cada frame
// durante toda la racha, que dura lo que le queda del estado de vagar (hasta
// 500 frames), sería pagar las 4 comparaciones por eslabón sin necesidad.
#define SNAKE_BB_EVERY  4
#define SNAKE_BB_MARGIN 16

static SnakeSlot snakes[MAX_SNAKES];
static fix16 lastKillX, lastKillY;

// Tiles de las siluetas de los eslabones, precargados en VRAM (ver
// inc/vramspr.h): 8 rotaciones cada una menos el cuerpo, que en el original
// no rota. Las cabezas no están aquí: van por VStream (32 direcciones).
static u16 tail0Tile;               // oCabeza2: la cola, siempre el frame 0
static u16 bodyTile;

static void hideSprites(SnakeSlot *s)
{
    u16 i;

    sprHide(s->headSprite);
    for (i = 0; i < SNAKE_MAX_FOLLOWERS; i++)
        sprHide(s->segSprite[i]);
}

// Cada ranura del pool queda asignada a un tipo de gusano desde el arranque:
// la definición de sprite de SGDK se fija al crearlo, y el reparto de
// siluetas de la cadena depende de la longitud, que es distinta en cada tipo.
// Con los topes del port (3 de tipo 1, 1 de tipo 2) sale exacto.
static u8 slotType(u16 slot)
{
    return (slot < MAX_CABEZA_1) ? SNAKE_TYPE_1 : SNAKE_TYPE_2;
}

static u8 slotParts(u16 slot)
{
    return (slotType(slot) == SNAKE_TYPE_1) ? SNAKE1_PARTS : SNAKE2_PARTS;
}

void Snake_initAll(void)
{
    u16 i, j;

    tail0Tile = VSpr_loadFrame(&snakeTail, 0, 0);
    bodyTile  = VSpr_loadFrame(&snakeBody, 0, 0);

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];
        const u8 parts = slotParts(i);

        Entity_clear(&s->e);
        s->type = slotType(i);
        s->followers = parts - 1;
        s->stepDelay = (s->type == SNAKE_TYPE_1) ? SNAKE1_STEPDELAY : SNAKE2_STEPDELAY;
        // El eslabón n va n*stepdelay - 1 entradas por detrás de la cabeza
        // (ver cacheParts): el último pasa por el salto a los
        // followers*stepdelay - 1 frames.
        s->jumpSpan = (u8)(s->followers * s->stepDelay);
        s->jumpAge = s->jumpSpan;
        s->snapped = FALSE;

        {
            const bool t1 = (s->type == SNAKE_TYPE_1);

            s->headDef = t1 ? &snakeHead1 : &snakeHead2;
            s->headSprite = VStream_add(&s->headStream, s->headDef, PAL1);
        }

        for (j = 0; j < SNAKE_MAX_FOLLOWERS; j++)
        {
            const u16 n = j + 1;            // índice de eslabón
            if (n >= parts)                 s->segSprite[j] = VSpr_add(&snakeBody, PAL1, bodyTile);
            else if (n == 1)                s->segSprite[j] = VSpr_add(&snakeTail, PAL1, tail0Tile);
            else if (n == (u16)(parts - 2)) s->segSprite[j] = VStream_add(&s->cu1Stream, &snakeTail, PAL1);
            else if (n == (u16)(parts - 1)) s->segSprite[j] = VStream_add(&s->cu2Stream, &snakeCu2, PAL1);
            else                            s->segSprite[j] = VSpr_add(&snakeBody, PAL1, bodyTile);
        }
    }
}

// Posición del eslabón n (1 = el que sigue a la cabeza), leída del historial.
// Cache de todos los eslabones de un gusano, una vez por frame.
//
// El eslabón n va n*stepdelay posiciones por detrás en el historial. En vez
// de multiplicar por cada eslabón (12 muls.w por gusano y frame), el índice
// se arrastra restando stepdelay en cada vuelta.
static void cacheParts(SnakeSlot *s)
{
    const u16 delay = s->stepDelay;
    const u16 count = s->followers;
    const s16 *hx = s->histX;
    const s16 *hy = s->histY;
    u16 idx = s->histPos;
    u16 n;

    s->headIX = F2I(s->e.x);
    s->headIY = F2I(s->e.y);

    if (s->bbTick == 0)
    {
        s->bbX0 = s->headIX;
        s->bbY0 = s->headIY;
        s->bbX1 = s->headIX;
        s->bbY1 = s->headIY;
    }

    for (n = 0; n < count; n++)
    {
        idx = (idx - delay) & SNAKE_HIST_MASK;

        {
            const s16 ix = hx[idx];
            const s16 iy = hy[idx];

            s->partIX[n] = ix;
            s->partIY[n] = iy;

            if (s->bbTick == 0)
            {
                if (ix < s->bbX0) s->bbX0 = ix;
                if (ix > s->bbX1) s->bbX1 = ix;
                if (iy < s->bbY0) s->bbY0 = iy;
                if (iy > s->bbY1) s->bbY1 = iy;
            }
        }
    }

    if (s->bbTick) { s->bbTick--; return; }
    s->bbTick = SNAKE_BB_EVERY - 1;

    // La caja se guarda ya crecida con la envolvente de las dos máscaras de
    // parte (cabeza y eslabón), para que el descarte no se coma ningún
    // contacto de verdad.
    s->bbX0 += SNAKE_BB_OX - SNAKE_BB_MARGIN;
    s->bbY0 += SNAKE_BB_OY - SNAKE_BB_MARGIN;
    s->bbX1 += SNAKE_BB_X1 + SNAKE_BB_MARGIN;
    s->bbY1 += SNAKE_BB_Y1 + SNAKE_BB_MARGIN;
}

// TRUE si la caja (x, y, w, h) puede tocar algo de este gusano.
static FORCE_INLINE bool nearSnake(const SnakeSlot *s, s16 x, s16 y, s16 w, s16 h)
{
    return !(((x + w) < s->bbX0) || (x > s->bbX1) ||
             ((y + h) < s->bbY0) || (y > s->bbY1));
}

bool Snake_spawn(u8 type)
{
    u16 i, k;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];
        const u16 h = Hole_snakeSpawn();
        fix16 velo, turn;

        if (s->e.active || (s->type != type)) continue;

        if (type == SNAKE_TYPE_1)
        {
            // oCabeza1/Create_0: velo = random_range(5, 7.8), hp = 4,
            // bodylength = 14, stepdelay = 7.
            //
            // velo es UN solo sorteo que el original usa a la vez como
            // px/frame y como grados de giro por frame, así que aquí se
            // sortea un índice y se interpola con él en las dos escalas: dos
            // sorteos independientes darían gusanos que giran a un ritmo que
            // no corresponde a su velocidad.
            //
            // Bloque 1 de MonsterLite: la VELOCIDAD de avance pasa a ser la
            // de setSpeed por nivel (maxSpeed 4 del prefab culobicho, la
            // misma para los dos tipos); el giro sigue siendo el sorteo del
            // GML hasta el bloque 8, que trae el Slerp.
            const u16 k = random() % 45;

            turn = SNAKE1_TURN_MIN  + (fix16)(((s32)(SNAKE1_TURN_MAX  - SNAKE1_TURN_MIN)  * k) / 44);
            velo = Entity_mlSpeed(g_level, ML_SNAKE_MAX_MU);
            Entity_spawnHidden(&s->e, Hole_x(h), Hole_y(h), velo, turn, SNAKE1_HP);
        }
        else
        {
            // oCabeza2b/Create_0: velo = 6.2, hp = 2, bodylength = 12,
            // stepdelay = 6. Velocidad de MonsterLite, como el tipo 1.
            velo = Entity_mlSpeed(g_level, ML_SNAKE_MAX_MU);
            Entity_spawnHidden(&s->e, Hole_x(h), Hole_y(h), velo, SNAKE2_TURN, SNAKE2_HP);
        }

        // Las colas del original empiezan vacías, así que los eslabones se
        // quedan en el punto de creación hasta que se llenan.
        {
            const s16 hx0 = F2I(s->e.x);
            const s16 hy0 = F2I(s->e.y);

            for (k = 0; k < SNAKE_MAX_HISTORY; k++)
            {
                s->histX[k] = hx0;
                s->histY[k] = hy0;
            }
        }
        s->histPos = 0;
        s->bbTick = 0;
        s->jumpAge = s->jumpSpan;
        s->snapped = FALSE;
        cacheParts(s);

        hideSprites(s);
        return TRUE;
    }

    return FALSE;
}

void Snake_updateAll(bool hasPlayer, fix16 px, fix16 py)
{
    u16 i, n;
    u16 h;
    u8  active[HOLE_COUNT];

    // oAgujerSuper/Draw_0: "if place_meeting(x,y,oCabeza1) gus = 1 else gus = 0".
    // El agujero por el que está saliendo un gusano se pinta con el reborde
    // rojo. Se recalcula entero cada frame y Hole_setActive sólo repinta los
    // que cambian.
    for (h = 0; h < HOLE_COUNT; h++) active[h] = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];

        if (!s->e.active) continue;

        // Igual que la araña: la máquina de estados y el historial corren
        // también durante alarm[0], con la cabeza quieta por speed = 0.
        const bool shown = Entity_tickAppear(&s->e);

        PROF_START(PROF_S_AI);
        {
            const bool snap = Entity_updateAI(&s->e, hasPlayer, px, py,
                                              SNAKE_HEAD_BOX_OX, SNAKE_HEAD_BOX_OY,
                                              SNAKE_HEAD_BOX_W, SNAKE_HEAD_BOX_H);

            // El salto al agujero 0 abre la ventana de caja exacta (ver
            // SNAKE_BB_MARGIN); los siguientes de la misma racha, no.
            if (snap && !s->snapped) s->jumpAge = 0;
            s->snapped = snap;
        }
        Entity_advance(&s->e);
        PROF_STOP(PROF_S_AI);

        // Historial de la cabeza: una entrada por frame.
        PROF_START(PROF_S_CACHE);
        s->histX[s->histPos] = F2I(s->e.x);
        s->histY[s->histPos] = F2I(s->e.y);
        s->histPos = (s->histPos + 1) & SNAKE_HIST_MASK;
        if (s->jumpAge < s->jumpSpan)
        {
            s->bbTick = 0;
            s->jumpAge++;
        }
        cacheParts(s);
        PROF_STOP(PROF_S_CACHE);

        // La cabeza marca el agujero que tiene debajo, se vea o no: durante
        // los 75 frames de aparición es justo cuando el original lo pinta.
        for (h = 0; h < HOLE_COUNT; h++)
        {
            if (((u16)(s->headIX - F2I(Hole_x(h)) + 8) < 16) &&
                ((u16)(s->headIY - F2I(Hole_y(h)) + 8) < 16))
            {
                active[h] = TRUE;
                break;
            }
        }

        if (!shown)
        {
            hideSprites(s);
            continue;
        }

        // oCabeza1/Step_1 y oCabeza2b: image_angle = direction
        PROF_START(PROF_S_HEAD);
        {
            const u16 rot = Entity_frame32(&s->e);

            // La cabeza de tipo 1 va recortada a 16x16 con su esquina por
            // frame (ver trim en tools/gm2md.py); la de tipo 2 llega a 17 px
            // en algunas rotaciones y se queda en 24x24, centrada.
            if (s->headDef == &snakeHead1)
                sprPos(s->headSprite, s->headIX + snakeHead1OffX[rot], s->headIY + snakeHead1OffY[rot]);
            else
                sprPos(s->headSprite, s->headIX + SNAKE_HEAD_SPR_OFF_X, s->headIY + SNAKE_HEAD_SPR_OFF_Y);

            VStream_frame(&s->headStream, s->headSprite, s->headDef->animations[rot]->frames[0]);
        }
        PROF_STOP(PROF_S_HEAD);

        PROF_START(PROF_S_PARTS);

        // Qué eslabón es cuál lo decide la longitud, que no cambia: el 1 es
        // oCabeza2, el penúltimo oCu1 y el último oCu2, y todo lo de en medio
        // son oCorpo sin rotación. Antes el bucle comprobaba las tres
        // condiciones en cada vuelta; ahora los tres casos especiales salen
        // fuera y el cuerpo es un bucle recto de dos escrituras.
        {
            const u16 last = s->followers - 1;      // oCu2
            const u16 prev = s->followers - 2;      // oCu1
            const s16 *pix = s->partIX;
            const s16 *piy = s->partIY;

            // Eslabón 1: oCabeza2, con el lookdir que recibió al nacer.
            // (arte de la cola, frame 0, recortado: ver trim en gm2md.py)
            sprPos(s->segSprite[0], pix[0] + snakeTailOffX[0], piy[0] + snakeTailOffY[0]);

            // El cuerpo: el tramo más largo del juego, con el sesgo de
            // pantalla sacado fuera (ver sprPosBias en inc/spr.h).
            {
                const s16 bx = sprBiasX() + SNAKE_BODY_SPR_OFF;
                const s16 by = sprBiasY() + SNAKE_BODY_SPR_OFF;

                for (n = 1; n < prev; n++)
                    sprPosBias(s->segSprite[n], pix[n] + bx, piy[n] + by);
            }

            // oCu1 mira a oCu2 y oCu2 mira a oCu1.
            {
                const u8 f1 = Entity_angleFrame8(Fast_angle(I2F(pix[prev]), I2F(piy[prev]),
                                                            I2F(pix[last]), I2F(piy[last])));
                const u8 f2 = Entity_angleFrame8(Fast_angle(I2F(pix[last]), I2F(piy[last]),
                                                            I2F(pix[prev]), I2F(piy[prev])));

                sprPos(s->segSprite[prev], pix[prev] + snakeTailOffX[f1], piy[prev] + snakeTailOffY[f1]);
                VStream_frame(&s->cu1Stream, s->segSprite[prev], snakeTail.animations[f1]->frames[0]);

                sprPos(s->segSprite[last], pix[last] + snakeCu2OffX[f2], piy[last] + snakeCu2OffY[f2]);
                VStream_frame(&s->cu2Stream, s->segSprite[last], snakeCu2.animations[f2]->frames[0]);
            }
        }
        PROF_STOP(PROF_S_PARTS);
    }

    for (h = 0; h < HOLE_COUNT; h++) Hole_setActive(h, active[h]);
}

bool Snake_closest(s16 x, s16 y, s16 *outX, s16 *outY, s32 *bestIO)
{
    u16 i, n;
    bool found = FALSE;
    s32 best = *bestIO;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *s = &snakes[i];

        // Sin mirar 'visible' a propósito: el gusano que espera en su agujero
        // (alarm[0] = 90) ya es un oMalo, y el oPlayer/Step_0 del original
        // apunta con instance_nearest(x, y, oMalo), que no mira la
        // visibilidad. Por eso al empezar la partida la nave se gira hacia el
        // agujero del primer gusano y le dispara; mirando 'visible' disparaba
        // hacia arriba, a la nada, hasta que salía.
        if (!s->e.active) continue;

        // Si el punto más cercano de la caja del gusano ya está más lejos que
        // el mejor que llevamos, sus 13 eslabones no pueden mejorarlo: 52
        // distancias (con sus dos muls) se cambian por cuatro restas. El
        // "mejor que llevamos" entra ya con el de las arañas, que se recorren
        // antes y suelen estar cerca: así el descarte vale desde el primer
        // gusano y no sólo a partir del segundo.
        {
            const s16 cdx = (x < s->bbX0) ? (s16)(s->bbX0 - x) : ((x > s->bbX1) ? (s16)(x - s->bbX1) : 0);
            const s16 cdy = (y < s->bbY0) ? (s16)(s->bbY0 - y) : ((y > s->bbY1) ? (s16)(y - s->bbY1) : 0);

            if (((s32)(cdx * cdx) + (s32)(cdy * cdy)) >= best) continue;
        }

        for (n = 0; n <= s->followers; n++)
        {
            const s16 ex = (n == 0) ? s->headIX : s->partIX[n - 1];
            const s16 ey = (n == 0) ? s->headIY : s->partIY[n - 1];
            const s16 dx = ex - x;
            const s16 dy = ey - y;
            // Multiplicación de 16x16 bits: la de 32 llama a __mulsi3.
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

// morteGusano(): el gusano no desaparece, REVIENTA. El original recorre el
// cuerpo entero y por cada eslabón crea un oExplosion desplazado al azar
// (offsetExplosions() = choose(random(5), random(-5))), destruyendo la parte,
// y al acabar suena aranamorteSnd una sola vez.
//
// Aquí el pool de explosiones es de MAX_EFFECTS, así que la cascada se ve
// recortada a las últimas: se recorre igual y la ranura más vieja se recicla,
// que deja la explosión repartida a lo largo del bicho.
static void blowUp(SnakeSlot *s)
{
    u16 n;

    Effect_spawnQuiet(s->e.x, s->e.y);

    for (n = 0; n < s->followers; n++)
    {
        // offsetExplosions(): +-5 px del room -> +-1 px de pantalla.
        const fix16 j = (fix16)((random() & 0x7F) - 64);

        Effect_spawnQuiet(I2F(s->partIX[n]) + j, I2F(s->partIY[n]) - j);
    }

    // morteGusano(): audio_play_sound(aranamorteSnd, .7, false)
    Audio_sfx(SFX_ARANAMORTE);
}

bool Snake_hit(s16 x, s16 y, s16 w, s16 h, bool *killed, bool *body)
{
    u16 i;

    *killed = FALSE;
    *body = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];
        s16 hx, hy;

        if (!s->e.active || !s->e.visible) continue;

        hx = s->headIX + SNAKE_HEAD_BOX_OX;
        hy = s->headIY + SNAKE_HEAD_BOX_OY;

        if (!Coll_overlap(x, y, w, h, hx, hy, SNAKE_HEAD_BOX_W, SNAKE_HEAD_BOX_H)) continue;

        // oCabeza1/Collision_oBulletS: "hp = hp - 1" + checkMorte(), que
        // destruye el gusano entero cuando hp < 0.
        s->e.hp -= DMG_TO_SNAKE;

        if (s->e.hp < 0)
        {
            lastKillX = s->e.x;
            lastKillY = s->e.y;
            blowUp(s);
            Entity_clear(&s->e);
            hideSprites(s);
            *killed = TRUE;
        }

        return TRUE;
    }

    // Los eslabones son un escudo. En el original el impacto contra un eslabón
    // dispara DOS manejadores: el del eslabón, heredado de oMalo
    // ("if (global.explosions) instance_create(x, y, oExplosion)", que no
    // destruye la bala), y a continuación el de la propia bala
    // (oBulletThin/oWBlock/o3way/oLaser/Collision_oMalo: "with other if hp !=
    // undefined hp -= 1" e "instance_destroy()"). Los eslabones tienen hp = 10
    // (oCorpo/oCu1/oCu2/oCabeza2 Create_0) y NADIE comprueba nunca ese hp, así
    // que no se pueden matar: se quedan la bala y siguen. Las cabezas no pasan
    // por aquí porque oSpider/oCabeza1/oCabeza2b definen su propio
    // Collision_oBulletS, que destruye la bala antes de que corra el de ella.
    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *s = &snakes[i];
        u16 n;

        if (!s->e.active || !s->e.visible) continue;

        if (!nearSnake(s, x, y, w, h)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_overlap(x, y, w, h,
                             s->partIX[n] + SNAKE_BODY_BOX_OX,
                             s->partIY[n] + SNAKE_BODY_BOX_OY,
                             SNAKE_BODY_BOX_W, SNAKE_BODY_BOX_H))
            {
                // Es lo que dispara la metralla del WBLOCK y del PIVOT: su
                // Collision_oMalo sólo corre contra un eslabón.
                *body = TRUE;
                return TRUE;
            }
    }

    return FALSE;
}

bool Snake_hitPoint(s16 x, s16 y, bool *killed, bool *body)
{
    u16 i, n;

    *killed = FALSE;
    *body = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];

        if (!s->e.active || !s->e.visible) continue;

        if (((u16)(x - (s->headIX + SNAKE_HEAD_BOX_OX)) < (u16)SNAKE_HEAD_BOX_W) &&
            ((u16)(y - (s->headIY + SNAKE_HEAD_BOX_OY)) < (u16)SNAKE_HEAD_BOX_H))
        {
            s->e.hp -= DMG_TO_SNAKE;

            if (s->e.hp < 0)
            {
                lastKillX = s->e.x;
                lastKillY = s->e.y;
                blowUp(s);
                Entity_clear(&s->e);
                hideSprites(s);
                *killed = TRUE;
            }

            return TRUE;
        }

        if ((x < s->bbX0) || (x > s->bbX1) || (y < s->bbY0) || (y > s->bbY1)) continue;

        for (n = 0; n < s->followers; n++)
            if (((u16)(x - (s->partIX[n] + SNAKE_BODY_BOX_OX)) < (u16)SNAKE_BODY_BOX_W) &&
                ((u16)(y - (s->partIY[n] + SNAKE_BODY_BOX_OY)) < (u16)SNAKE_BODY_BOX_H))
            {
                *body = TRUE;
                return TRUE;
            }
    }

    return FALSE;
}

bool Snake_damageNoKill(s16 x, s16 y, s16 w, s16 h, s16 dmg)
{
    u16 i, n;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];

        if (!s->e.active || !s->e.visible) continue;

        if (Coll_overlap(x, y, w, h,
                         s->headIX + SNAKE_HEAD_BOX_OX, s->headIY + SNAKE_HEAD_BOX_OY,
                         SNAKE_HEAD_BOX_W, SNAKE_HEAD_BOX_H))
        {
            s->e.hp -= dmg;
            return TRUE;
        }

        if (!nearSnake(s, x, y, w, h)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_overlap(x, y, w, h,
                             s->partIX[n] + SNAKE_BODY_BOX_OX,
                             s->partIY[n] + SNAKE_BODY_BOX_OY,
                             SNAKE_BODY_BOX_W, SNAKE_BODY_BOX_H))
                return TRUE;
    }

    return FALSE;
}

bool Snake_collides(s16 cx, s16 cy)
{
    u16 i, n;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *s = &snakes[i];

        if (!s->e.active || !s->e.visible) continue;

        if (Coll_circleBox(cx, cy, ML_PLAYER_R2, ML_PLAYER_REACH,
                           s->headIX + SNAKE_HEAD_BOX_OX,
                           s->headIY + SNAKE_HEAD_BOX_OY,
                           SNAKE_HEAD_BOX_W, SNAKE_HEAD_BOX_H))
            return TRUE;

        // El descarte por la caja del gusano, con el cuadrado que envuelve
        // el círculo: si no la toca, ningún eslabón puede tocar el círculo.
        if (!nearSnake(s, cx - ML_PLAYER_REACH, cy - ML_PLAYER_REACH,
                       2 * ML_PLAYER_REACH, 2 * ML_PLAYER_REACH)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_circleBox(cx, cy, ML_PLAYER_R2, ML_PLAYER_REACH,
                               s->partIX[n] + SNAKE_BODY_BOX_OX,
                               s->partIY[n] + SNAKE_BODY_BOX_OY,
                               SNAKE_BODY_BOX_W, SNAKE_BODY_BOX_H))
                return TRUE;
    }

    return FALSE;
}

fix16 Snake_lastKillX(void) { return lastKillX; }
fix16 Snake_lastKillY(void) { return lastKillY; }

u16 Snake_countType(u8 type)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_SNAKES; i++)
        if (snakes[i].e.active && (snakes[i].type == type)) n++;

    return n;
}

void Snake_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        Entity_clear(&snakes[i].e);
        hideSprites(&snakes[i]);
    }
}
