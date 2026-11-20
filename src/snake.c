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
    // Las migas de headScr (bloque 8). breadcrumbs[0] es la última miga, la
    // posición de la cabeza cuando se puso: se guarda exacta, en fix16, para
    // medir cuánto se ha alejado la cabeza. breadcrumbs[1..] son las de los
    // eslabones: el eslabón n está en breadcrumbs[n + 1] (Lerp con t = 0), así
    // que la posición de cada eslabón ES su miga y no hace falta otro
    // historial. Antes eran 128 posiciones por gusano (stepdelay del GML).
    fix16  crumbX, crumbY;
    // Posición de cada eslabón en píxeles enteros: sólo cambia al poner una
    // miga. El apuntado, las colisiones y el dibujado la leen de aquí.
    s16    partIX[SNAKE_MAX_FOLLOWERS];
    s16    partIY[SNAKE_MAX_FOLLOWERS];
    s16    headIX, headIY;
    // Caja que envuelve cabeza y eslabones, para descartar el gusano entero
    // con cuatro comparaciones: los eslabones son un escudo contra las balas
    // del GML (ver Snake_hit) y comprobar 12 eslabones por gusano y por
    // proyectil son cientos de pruebas por frame que casi siempre fallan.
    // Medido: partir esta caja en dos mitades para afinar el descarte sale
    // PEOR (41,8 fps frente a 43,0 en el caso peor). Mantener y comprobar dos
    // cajas cuesta más de lo que ahorra saltarse seis eslabones, porque el
    // barrido de 12 sólo corre cuando algo cae dentro de verdad.
    s16    bbX0, bbY0, bbX1, bbY1;
    u8     bbTick;
    // Los eslabones sólo se mueven al poner una miga (cada 2 a 11 frames,
    // según la velocidad), así que sus sprites sólo se recolocan entonces o
    // cuando cambia la cámara de la sacudida (el sesgo con el que se
    // colocaron). Medido: colocarlos cada frame era el 22 % del gusano.
    u8     partsDirty;
    s16    drawBX, drawBY;
    // Migas puestas desde que nació, hasta followers. Los eslabones nacen
    // todos en la cabeza (headScr.Start) y se van separando de uno en uno:
    // tras k migas, los eslabones 0 .. k-2 están en migas distintas y del
    // k-1 en adelante siguen apilados en el punto de nacimiento. Dibujar la pila
    // eran 12 sprites en las mismas líneas: con dos gusanos naciendo a la
    // misma altura, 31 en una línea (el VDP pinta 20) y se perdían los
    // paquetes del suelo. Así que de la pila sólo se dibuja el primero, que
    // en pantalla es lo mismo.
    u8     spread;
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

// La caja envolvente no hace falta exacta: sólo sirve para descartar deprisa.
// Recalcularla cada frame son cuatro comparaciones por eslabón (unos 3800
// ciclos con la arena llena), así que se recalcula cada 4 frames y se infla
// con lo más que puede moverse el gusano en ese tiempo, redondeado hacia
// arriba, más el radio de la esfera de las partes (9,6 px -> 10).
//
// Lo más rápido que va una cabeza es lo del nivel 9 de MonsterLite: f = 9.8
// u/s -> 3,92 px/f (251/64 en fix16), x 4 = 15,7 -> 16. Los eslabones sólo
// se mueven al poner una miga, y saltan a la de delante, que es una
// posición por la que ya pasó la cabeza: tampoco salen de la caja. Desde el
// bloque 8 la cabeza ya no salta al agujero 0 (era la IA del GML), así que
// no hace falta la ventana de caja exacta tras el salto que llevaba antes.
#define SNAKE_BB_EVERY  4
#define SNAKE_BB_MARGIN 16
#define SNAKE_BB_GROW   (SNAKE_BB_MARGIN + ML_SNAKE_REACH)

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
// Con los topes del port (2 de tipo 1, 1 de tipo 2) sale exacto.
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

// headScr.Update(): "if ((head.position - breadcrumbs[0]).magnitude >=
// segmentSpacing)" quita la última miga y pone una nueva donde está la
// cabeza, y cada eslabón pasa a la miga siguiente. Como la posición de cada
// eslabón es su miga, eso es correr la lista un puesto: el eslabón n se va
// a donde estaba el n - 1, y el primero a la miga que era la última.
//
// En MonsterLite corre en Update (60 Hz) y aquí una vez por frame de 50 Hz:
// como la cabeza va como mucho a 3,92 px/f y la miga es de 8 px, no se
// pierde ninguna, pero la separación real entre migas va de 8 a 11,9 px (en
// MonsterLite, de 8 a 11,3).
static void crumbStep(SnakeSlot *s)
{
    // Diferencia en 1/64 de píxel: cabe en 16 bits (la cabeza no se aleja
    // ni 12 px de su última miga) y el cuadrado sale con dos muls.w.
    const s16 dx = s->e.x - s->crumbX;
    const s16 dy = s->e.y - s->crumbY;
    const s16 lim = ML_SNAKE_CRUMB_PX * 64;

    // Dentro del cuadrado inscrito de 8 / sqrt(2) px no hace falta
    // multiplicar: es lo que pasa casi todos los frames.
    if (((u16)(dx + 362) < 724) && ((u16)(dy + 362) < 724)) return;

    if (((u16)(dx + lim - 1) < (u16)((2 * lim) - 1)) &&
        ((u16)(dy + lim - 1) < (u16)((2 * lim) - 1)) &&
        (((s32)(dx * dx) + (s32)(dy * dy)) < ML_SNAKE_CRUMB_F2))
        return;

    {
        s16 *px = s->partIX;
        s16 *py = s->partIY;
        u16 n = s->followers - 1;

        for (; n > 0; n--)
        {
            px[n] = px[n - 1];
            py[n] = py[n - 1];
        }
        px[0] = F2I(s->crumbX);
        py[0] = F2I(s->crumbY);
    }

    s->partsDirty = TRUE;
    if (s->spread < s->followers) s->spread++;

    s->crumbX = s->e.x;
    s->crumbY = s->e.y;
}

// La caja envolvente, cada SNAKE_BB_EVERY frames.
static void updateBox(SnakeSlot *s)
{
    const s16 *px = s->partIX;
    const s16 *py = s->partIY;
    s16 x0, y0, x1, y1;
    u16 n;

    s->headIX = F2I(s->e.x);
    s->headIY = F2I(s->e.y);

    if (s->bbTick) { s->bbTick--; return; }
    s->bbTick = SNAKE_BB_EVERY - 1;

    x0 = x1 = s->headIX;
    y0 = y1 = s->headIY;

    for (n = 0; n < s->followers; n++)
    {
        const s16 ix = px[n];
        const s16 iy = py[n];

        if (ix < x0) x0 = ix;
        if (ix > x1) x1 = ix;
        if (iy < y0) y0 = iy;
        if (iy > y1) y1 = iy;
    }

    // Crecida con el radio de las partes y el margen de avance, para que el
    // descarte no se coma ningún contacto de verdad.
    s->bbX0 = x0 - SNAKE_BB_GROW;
    s->bbY0 = y0 - SNAKE_BB_GROW;
    s->bbX1 = x1 + SNAKE_BB_GROW;
    s->bbY1 = y1 + SNAKE_BB_GROW;
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

        if (s->e.active || (s->type != type)) continue;

        // createSnake() + enemyMov.Start() (bloque 8): en un agujero al azar,
        // ya visible y en marcha, con rumbo, destino y estado al azar. Los
        // dos tipos del GML (Q2) llevan los valores del prefab culobicho:
        // velocidad por nivel con maxSpeed 4, turningSpeedMax 10 y hp 4.
        Entity_spawnML(&s->e, Hole_x(h), Hole_y(h),
                       Entity_mlSpeed(g_level, ML_SNAKE_MAX_MU),
                       ML_SNAKE_TURN_K_MAX,
                       (type == SNAKE_TYPE_1) ? SNAKE1_HP : SNAKE2_HP);

        // headScr.Start(): los eslabones nacen en la cabeza y las migas son
        // sus posiciones, todas la misma.
        s->crumbX = s->e.x;
        s->crumbY = s->e.y;
        for (k = 0; k < SNAKE_MAX_FOLLOWERS; k++)
        {
            s->partIX[k] = F2I(s->e.x);
            s->partIY[k] = F2I(s->e.y);
        }
        s->bbTick = 0;
        updateBox(s);
        s->partsDirty = TRUE;
        s->spread = 0;

        hideSprites(s);
        return TRUE;
    }

    return FALSE;
}

void Snake_tickFrozen(void)
{
    u16 i;

    // Con el tiempo parado sólo corre el Update de enemyMov; el de headScr
    // también, pero con la cabeza quieta no pone migas.
    for (i = 0; i < MAX_SNAKES; i++)
        if (snakes[i].e.active) Entity_tickML(&snakes[i].e);
}

void Snake_updateAll(bool hasTarget, fix16 tx, fix16 ty)
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

        // move() de enemyMov (FixedUpdate) y su Update, como la araña.
        PROF_START(PROF_S_AI);
        Entity_moveML(&s->e, hasTarget, tx, ty);
        Entity_tickML(&s->e);
        PROF_STOP(PROF_S_AI);

        PROF_START(PROF_S_CACHE);
        crumbStep(s);
        updateBox(s);
        PROF_STOP(PROF_S_CACHE);

        // La cabeza marca el agujero que tiene debajo: el reborde rojo
        // mientras sale de él.
        for (h = 0; h < HOLE_COUNT; h++)
        {
            if (((u16)(s->headIX - F2I(Hole_x(h)) + 8) < 16) &&
                ((u16)(s->headIY - F2I(Hole_y(h)) + 8) < 16))
            {
                active[h] = TRUE;
                break;
            }
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
        if (s->partsDirty || (s->drawBX != sprBiasX()) || (s->drawBY != sprBiasY()))
        {
            const u16 last = s->followers - 1;      // oCu2
            const u16 prev = s->followers - 2;      // oCu1
            const s16 *pix = s->partIX;
            const s16 *piy = s->partIY;

            s->partsDirty = FALSE;
            s->drawBX = sprBiasX();
            s->drawBY = sprBiasY();

            // Eslabón 1: oCabeza2, con el lookdir que recibió al nacer.
            // (arte de la cola, frame 0, recortado: ver trim en gm2md.py)
            sprPos(s->segSprite[0], pix[0] + snakeTailOffX[0], piy[0] + snakeTailOffY[0]);

            // El cuerpo: el tramo más largo del juego, con el sesgo de
            // pantalla sacado fuera (ver sprPosBias en inc/spr.h).
            {
                const s16 bx = sprBiasX() + SNAKE_BODY_SPR_OFF;
                const s16 by = sprBiasY() + SNAKE_BODY_SPR_OFF;

                // Recién nacido sólo se colocan los eslabones 0 .. spread-1
                // (el spread-1 es el primero de la pila, en el punto de
                // nacimiento); los de detrás siguen aparcados desde
                // hideSprites al nacer, así que no hay que tocarlos.
                const u16 end = (s->spread < prev) ? s->spread : prev;

                for (n = 1; n < end; n++)
                    sprPosBias(s->segSprite[n], pix[n] + bx, piy[n] + by);
            }

            // oCu1 mira a oCu2 y oCu2 mira a oCu1.
            {
                const u8 f1 = Entity_angleFrame8(Fast_angle(I2F(pix[prev]), I2F(piy[prev]),
                                                            I2F(pix[last]), I2F(piy[last])));
                const u8 f2 = Entity_angleFrame8(Fast_angle(I2F(pix[last]), I2F(piy[last]),
                                                            I2F(pix[prev]), I2F(piy[prev])));

                if (prev < s->spread)
                {
                    sprPos(s->segSprite[prev], pix[prev] + snakeTailOffX[f1], piy[prev] + snakeTailOffY[f1]);
                    VStream_frame(&s->cu1Stream, s->segSprite[prev], snakeTail.animations[f1]->frames[0]);
                }

                if (last < s->spread)
                {
                    sprPos(s->segSprite[last], pix[last] + snakeCu2OffX[f2], piy[last] + snakeCu2OffY[f2]);
                    VStream_frame(&s->cu2Stream, s->segSprite[last], snakeCu2.animations[f2]->frames[0]);
                }
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

        if (!s->e.active) continue;

        // La esfera de 9,6 px de la cabeza contra la máscara del arma del GML.
        if (!Coll_circleBox(s->headIX, s->headIY, ML_SNAKE_R2, ML_SNAKE_REACH, x, y, w, h)) continue;

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

        if (!s->e.active) continue;

        if (!nearSnake(s, x, y, w, h)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_circleBox(s->partIX[n], s->partIY[n], ML_SNAKE_R2, ML_SNAKE_REACH, x, y, w, h))
            {
                // Es lo que dispara la metralla del WBLOCK y del PIVOT: su
                // Collision_oMalo sólo corre contra un eslabón.
                *body = TRUE;
                return TRUE;
            }
    }

    return FALSE;
}

static void killSnake(SnakeSlot *s)
{
    lastKillX = s->e.x;
    lastKillY = s->e.y;
    blowUp(s);
    Entity_clear(&s->e);
    hideSprites(s);
}

// El impacto de una bala o una bola de MonsterLite contra una cabeza, con
// la caja de la bala (circle = FALSE: x, y, w, h) o la esfera de la bola
// (circle = TRUE: centro x, y; w y h son r2 y reach de la suma de radios).
static FORCE_INLINE bool hitHeadCore(bool circle, s16 x, s16 y, s16 w, s16 h, s16 dmg,
                                     u8 *ignore, bool *killed)
{
    u16 i;

    *killed = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];

        if (!s->e.active) continue;
        if (*ignore & (1 << i)) continue;

        if (circle)
        {
            if (!Coll_circleCircle(x - s->headIX, y - s->headIY, w, h)) continue;
        }
        else if (!Coll_circleBox(s->headIX, s->headIY, ML_SNAKE_R2, ML_SNAKE_REACH, x, y, w, h))
        {
            continue;
        }

        // bulletScript / bulletThinSrc / circularScr, OnTriggerEnter con
        // "snake": primero mira si hp < 0, y sólo entonces mata; si no,
        // resta. En el GML era al revés (resta y luego mira).
        if (s->e.hp < 0)
        {
            killSnake(s);
            *killed = TRUE;
        }
        else
        {
            s->e.hp -= dmg;
            // OnTriggerEnter sólo salta al ENTRAR: la bala gorda que
            // atraviesa no vuelve a tocar esta cabeza aunque siga
            // solapándola los frames siguientes.
            *ignore |= (u8)(1 << i);
        }

        return TRUE;
    }

    return FALSE;
}

bool Snake_hitHead(s16 x, s16 y, s16 w, s16 h, s16 dmg, u8 *ignore, bool *killed)
{
    return hitHeadCore(FALSE, x, y, w, h, dmg, ignore, killed);
}

bool Snake_hitHeadCircle(s16 cx, s16 cy, s16 r2, s16 reach, s16 dmg, u8 *ignore, bool *killed)
{
    return hitHeadCore(TRUE, cx, cy, r2, reach, dmg, ignore, killed);
}

s32 Snake_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s, u16 *slot, bool *head)
{
    const s16 r64 = ML_X10_TO_F16(ML_SNAKE_RADIUS_X10);
    s32 best = -1;
    u16 i, n;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *sn = &snakes[i];
        s32 d;

        if (!sn->e.active) continue;

        d = Coll_rayCircle(sn->headIX - ox, sn->headIY - oy, c, s, r64);
        if ((d >= 0) && ((best < 0) || (d < best))) { best = d; *slot = i; *head = TRUE; }

        for (n = 0; n < sn->followers; n++)
        {
            d = Coll_rayCircle(sn->partIX[n] - ox, sn->partIY[n] - oy, c, s, r64);
            if ((d >= 0) && ((best < 0) || (d < best))) { best = d; *slot = i; *head = FALSE; }
        }
    }

    return best;
}

bool Snake_closestHead(s16 x, s16 y, s16 *outX, s16 *outY, s32 *best)
{
    bool found = FALSE;
    u16 i;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *s = &snakes[i];
        s32 dx, dy, d;

        if (!s->e.active) continue;

        dx = s->headIX - x;
        dy = s->headIY - y;
        d = (dx * dx) + (dy * dy);
        if (d < *best) { *best = d; *outX = s->headIX; *outY = s->headIY; found = TRUE; }
    }

    return found;
}

void Snake_killSlot(u16 slot)
{
    if (snakes[slot].e.active) killSnake(&snakes[slot]);
}

bool Snake_hitPoint(s16 x, s16 y, bool *killed, bool *body)
{
    u16 i, n;

    *killed = FALSE;
    *body = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        SnakeSlot *s = &snakes[i];

        if (!s->e.active) continue;

        if (Coll_circleCircle(x - s->headIX, y - s->headIY, ML_SNAKE_R2, ML_SNAKE_REACH))
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
            if (Coll_circleCircle(x - s->partIX[n], y - s->partIY[n], ML_SNAKE_R2, ML_SNAKE_REACH))
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

        if (!s->e.active) continue;

        if (Coll_circleBox(s->headIX, s->headIY, ML_SNAKE_R2, ML_SNAKE_REACH, x, y, w, h))
        {
            s->e.hp -= dmg;
            return TRUE;
        }

        if (!nearSnake(s, x, y, w, h)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_circleBox(s->partIX[n], s->partIY[n], ML_SNAKE_R2, ML_SNAKE_REACH, x, y, w, h))
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

        if (!s->e.active) continue;

        // Esfera de la nave (3,6 px) contra la de la cabeza y la de cada
        // eslabón (9,6 px): 13,2 px entre centros.
        if (Coll_circleCircle(cx - s->headIX, cy - s->headIY,
                              ML_PLAYER_SNAKE_R2, ML_PLAYER_SNAKE_REACH))
            return TRUE;

        // El descarte por la caja del gusano (ya crecida con el radio de las
        // partes), con el cuadrado que envuelve la nave: si no la toca,
        // ningún eslabón puede tocarla.
        if (!nearSnake(s, cx - ML_PLAYER_REACH, cy - ML_PLAYER_REACH,
                       2 * ML_PLAYER_REACH, 2 * ML_PLAYER_REACH)) continue;

        for (n = 0; n < s->followers; n++)
            if (Coll_circleCircle(cx - s->partIX[n], cy - s->partIY[n],
                                  ML_PLAYER_SNAKE_R2, ML_PLAYER_SNAKE_REACH))
                return TRUE;
    }

    return FALSE;
}

bool Snake_headTouchCircle(s16 cx, s16 cy, s16 r2, s16 reach)
{
    u16 i;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        const SnakeSlot *s = &snakes[i];

        if (!s->e.active) continue;

        if (Coll_circleCircle(cx - s->headIX, cy - s->headIY, r2, reach))
            return TRUE;
    }

    return FALSE;
}

u16 Snake_explodeAll(void)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (!snakes[i].e.active) continue;

        // headScr.destroyParts(): la cabeza y los eslabones revientan.
        killSnake(&snakes[i]);
        n++;
    }

    return n;
}

bool Snake_headPos(u16 slot, s16 *x, s16 *y)
{
    const SnakeSlot *s = &snakes[slot];

    if (!s->e.active) return FALSE;

    *x = s->headIX;
    *y = s->headIY;
    return TRUE;
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
