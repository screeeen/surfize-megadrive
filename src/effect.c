#include "effect.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "resources.h"
#include "gm_art.h"
#include "audio.h"
#include "debris.h"
#include "shake.h"
#include "gm_art.h"

// PORT_NO_SMOKE: variante sin las explosiones grises (la humareda del oca y
// el parpadeo de oAutoDestruccion), para ver la partida sin ellas. Sólo se
// quita el dibujo: el fogonazo blanco, la metralla y los tiempos siguen.
#ifndef PORT_NO_SMOKE
#define PORT_NO_SMOKE 0
#endif

// ===========================================================================
// La muerte de un enemigo en el original son DOS cosas, no una:
//
//   oExplosion/Draw_0    t += random(3)
//                        draw_sprite_ext(sprExplosion, ..., t, t, ...)
//                        if t > 6 instance_destroy()
//
//   oExplosion/Create_0  u = instance_create(x, y, oca)      // el polvo
//   oca/Create_0         alarm = 40;  size = random(30*0.1)
//   oca/Draw_0           xf += 3;  yf += choose(-6,6)
//                        draw_sprite_ext(expl_4, ..., xf, yf, size, size, ...)
//                        draw_sprite_ext(expl_1, ..., xf, yf, size, size, ...)
//
// El fogonazo (lo primero) dura 4 frames: 80 ms, un parpadeo. Lo que de
// verdad se ve de una muerte es el polvo, que vive 40 frames (0,67 s) y mide
// hasta 124 px en pantalla. Aquí se portan los dos, encadenados en la misma
// ranura: primero el fogonazo y, cuando muere, la humareda.
//
// Desviaciones (ver docs/PORT_AUDIT_GML.md):
//   - el tamaño del polvo es random(3) en el original; la celda de 64 px topa
//     la escala en 1,55, así que se sortea entre tres tamaños hasta ese tope.
//   - la animación de la humareda avanza cada 2 frames en vez de cada uno,
//     para no pasarse del presupuesto de DMA por frame (64 tiles por cambio).
//   - sin el giro aleatorio por frame (angle = random(30)) ni el alpha 0,8:
//     el VDP no rota ni mezcla. El color sale del propio arte (gris oscuro).
//   - el oca también se mueve por su cuenta (speed/direction/gravity), pero
//     el Draw usa xf/yf, así que ese movimiento no se ve: no se porta.
// ===========================================================================

#define EFFECT_T_MAX    FIX16(6)
#define EFFECT_T_STEP   FIX16R(3.0 * 1.2)   // random(3) a 60 Hz -> 50 Hz
#define EFFECT_SPR_OFF  (-(EXPL_CELL / 2))  // celda de 64x64 centrada

#define DUST_LIFE       GML_FRAMES(40)      // alarm = 40
#define DUST_DRIFT      GML_SPEED(3)        // xf += 3
#define DUST_SHAKE      GML_SPEED(6)        // yf += choose(-6,6)
#define DUST_OFFY       GML_POS(20)         // yf = y + choose(20,-20)
#define DUST_ANIM_EVERY 2

// oAutoDestruccion/Create_0: alarm = 80, y su Draw dibuja expl_4 a escala
// random_range(0.1, 20) con curvas de rayos encima. El VDP no escala ni
// dibuja curvas: lo que queda es el parpadeo de tamaño, con los tres de la
// hoja de humareda, y al agotarse el reloj el oEstallido de su Alarm_0.
#define AUTO_LIFE       GML_FRAMES(80)

enum { EFF_FREE = 0, EFF_FLASH, EFF_DUST, EFF_AUTO };

typedef struct
{
    u8     phase;
    fix16  t;                               // fogonazo
    u16    life;                            // polvo
    fix16  x, y;                            // xf, yf del polvo
    u8     anim;                            // fila de tamaño del polvo
    u8     frame;
    u8     tick;
    MDSpr *sprite;
} Effect;

static Effect effects[MAX_EFFECTS];
static u16 next;

// ---------------------------------------------------------------------------
// oMuerte
//
// Draw_0: "t += 1; draw_sprite_ext(sprExplosion_2, ..., t*8, t*8, 0, c_white,
// 1); if t > 20 instance_destroy()". Empieza a escala 8 (51 px de pantalla) y
// al cuarto frame ya es más grande que el room entero: lo que se ve es un
// fogonazo blanco que se lo come todo.
//
// Aquí los tres primeros tamaños son sprite (hasta la celda de 64 px) y a
// partir de ahí se pone el color de fondo en blanco, que es lo que queda de
// ese sprite gigante. Step_0 además sacude la VISTA cambiándole el tamaño
// (zoom aleatorio de +-1600 sobre 1600): el VDP no escala el plano, así que
// se cambia por la sacudida más fuerte del juego. Documentado en el informe.
// ---------------------------------------------------------------------------
// El color de fondo se cambia por la cola de DMA, en el vblank: PAL_setColor
// escribe la CRAM en el acto, a mitad de frame, y eso deja un frame partido
// (gris arriba, blanco abajo) y en consola de verdad un punto de color en la
// linea donde cayo la escritura.
static const u16 deathWhiteColor = 0x0EEE;
static const u16 roomBgColor     = ROOM_BG_COLOR;

// ---------------------------------------------------------------------------
// oExplosionCruz (la bomba)
//
// Draw_0: "t += 1; draw_sprite_ext(sprExplosion, ..., t*8, t*8, 0, c_white,
// 0.5); if t > 60 instance_destroy()". Igual que oMuerte, el sprite escala a
// t*8: a los tres frames ya es más grande que el room, así que lo que se ve
// es la pantalla entera tapada por un blanco al 50 % durante 60 frames del
// original, y luego se va de golpe. El blancazo.
//
// El VDP no mezcla colores, pero sí se puede cambiar la paleta: las cuatro
// líneas (64 colores) pasan a la mitad de camino hacia el blanco, que es
// exactamente lo que da un blanco al 50 % por encima. Sube y baja por la
// cola de DMA, en el vblank. Desviación: el HUD también se aclara (en el
// original va por encima), porque comparte línea de paleta con los agujeros;
// es blanco y casi no se nota.
// ---------------------------------------------------------------------------
#define BOMB_LIFE   GML_FRAMES(60)

static u16  bombSaved[64];
static u16  bombPal[64];
static u16  bombTimer;

void Effect_bombFlash(void)
{
    u16 i;

    if (bombTimer == 0) PAL_getColors(0, bombSaved, 64);

    for (i = 0; i < 64; i++)
    {
        const u16 c = bombSaved[i];
        // Cada canal (3 bits, en los bits 1-3, 5-7 y 9-11) a medio camino
        // del 7: (c + 7 + 1) / 2.
        const u16 r = ((((c >> 1) & 7) + 8) >> 1);
        const u16 g = ((((c >> 5) & 7) + 8) >> 1);
        const u16 b = ((((c >> 9) & 7) + 8) >> 1);

        bombPal[i] = (b << 9) | (g << 5) | (r << 1);
    }

    PAL_setColors(0, bombPal, 64, DMA_QUEUE);
    bombTimer = BOMB_LIFE;
}

// Devuelve la paleta de antes. Lo llaman el final del blancazo y la muerte
// del jugador, que pone su propio blanco en el color de fondo.
void Effect_bombCancel(void)
{
    if (bombTimer == 0) return;

    bombTimer = 0;
    PAL_setColors(0, bombSaved, 64, DMA_QUEUE);
}

static void bombUpdate(void)
{
    if (bombTimer == 0) return;
    if (--bombTimer == 0) PAL_setColors(0, bombSaved, 64, DMA_QUEUE);
}

static MDSpr *deathSprite;
// El fogonazo de la muerte va por VStream (ver inc/vramspr.h): sólo hay uno y
// son 3 frames en toda la partida, así que precargarlo (109 tiles) no compensa
// frente a un hueco de 64. Libera 45 tiles para el fondo.
static VStream deathStream;
static u16    deathTimer;
static u8     deathWhite;
static fix16  deathX, deathY;

// Fogonazo y humareda, precargados en VRAM (ver inc/vramspr.h): son los
// frames más gordos del juego (hasta 53 tiles) y subirlos por DMA cada vez
// que cambian se comía el presupuesto del vblank entero.
static u16 explTile[(1 + EXPL_DUST_SIZES) * EXPL_DUST_FRAMES];

// NO_INLINE no es un capricho: con este GCC (m68k 16.2.0, -O2/-O3) al
// integrar place() en Effect_spawn se pierden los desplazamientos de los dos
// campos y 'e->x = x; e->y = y;' se compilan como dos escrituras al offset 0
// de la ranura, que machacan 'phase' y dejan x/y sin inicializar: la
// explosión aparecía en una posición cualquiera, casi siempre fuera de la
// pantalla. Comprobado en el desensamblado (move.w 30(%sp),(%a2) dos veces)
// y con tools/check_codegen.py, que revisa el ROM entero tras cada build.
static NO_INLINE void place(Effect *e)
{
    const s16 ix = F2I(e->x) + EFFECT_SPR_OFF;
    const s16 iy = F2I(e->y) + EFFECT_SPR_OFF;

    // La humareda va a la deriva (xf += 3) y puede acabar fuera: lo que no se
    // ve se aparca, que es como el montador descarta (ver inc/spr.h).
    if (MDSPR_OFFSCREEN(ix, iy)) sprHide(e->sprite);
    else                         sprPos(e->sprite, ix, iy);
}

void Effect_initAll(void)
{
    u16 i;

    VSpr_load(&explosionSprite, explTile, EXPL_DUST_FRAMES);


    for (i = 0; i < MAX_EFFECTS; i++)
    {
        effects[i].phase = EFF_FREE;
        effects[i].t = 0;
        effects[i].sprite = VSpr_add(&explosionSprite, PAL1, explTile[0]);
    }

    deathSprite = VStream_add(&deathStream, &muerteSprite, PAL1);
    deathTimer = 0;
    deathWhite = FALSE;

    next = 0;
}

static void spawnAt(fix16 x, fix16 y)
{
    u16 i, slot = next;

    // Primero una ranura libre; si no hay, la más antigua.
    for (i = 0; i < MAX_EFFECTS; i++)
    {
        if (effects[i].phase == EFF_FREE)
        {
            slot = i;
            break;
        }
    }

    effects[slot].phase = EFF_FLASH;
    effects[slot].t = 0;
    effects[slot].x = x;
    effects[slot].y = y;
    // size = random(30*0.1): aquí, uno de los tres tamaños de la hoja.
    effects[slot].anim = EXPL_ANIM_DUST + (random() % EXPL_DUST_SIZES);
    effects[slot].frame = 0;
    effects[slot].tick = 0;

    sprAnimFrame(effects[slot].sprite, EXPL_ANIM_FLASH, 0);
    sprTile(effects[slot].sprite, explTile[EXPL_ANIM_FLASH * EXPL_DUST_FRAMES]);
    place(&effects[slot]);

    next = (slot + 1) % MAX_EFFECTS;
}

void Effect_spawn(fix16 x, fix16 y)
{
    // oExplosion/Create_0: audio_play_sound(aranamorteSnd, .7, false)
    Audio_sfx(SFX_ARANAMORTE);
    // oca/Create_0: audio_play_sound(dustSnd, .5, false)
    Audio_sfx(SFX_DUST);

    spawnAt(x, y);
}

void Effect_spawnQuiet(fix16 x, fix16 y)
{
    spawnAt(x, y);
}

void Effect_autoDest(fix16 x, fix16 y)
{
    u16 i, slot = next;

    for (i = 0; i < MAX_EFFECTS; i++)
    {
        if (effects[i].phase == EFF_FREE)
        {
            slot = i;
            break;
        }
    }

    effects[slot].phase = EFF_AUTO;
    effects[slot].life = AUTO_LIFE;
    effects[slot].x = x;
    effects[slot].y = y;
    effects[slot].anim = EXPL_ANIM_DUST;
    effects[slot].frame = 0;
    effects[slot].tick = 0;
#if PORT_NO_SMOKE
    sprHide(effects[slot].sprite);
#else
    place(&effects[slot]);
#endif

    next = (slot + 1) % MAX_EFFECTS;
}

void Effect_death(fix16 x, fix16 y)
{
    // La paleta vuelve a la de verdad antes de que el fogonazo de la muerte
    // ponga su blanco en el color de fondo.
    Effect_bombCancel();

    deathTimer = MUERTE_LIFE;
    deathWhite = FALSE;
    deathX = x;
    deathY = y;

    VStream_frame(&deathStream, deathSprite, muerteSprite.animations[0]->frames[0]);
    sprPos(deathSprite, F2I(x) - (MUERTE_CELL / 2), F2I(y) - (MUERTE_CELL / 2));

    // oMuerte/Step_0 deforma la vista cada frame, y el port lo imitaba con la
    // sacudida del oExplosionCruz (que Game_killPlayer cortaba en el acto).
    // Desde el bloque 12 la muerte es la de MonsterLite: cámara lenta y, al
    // acabar, la sacudida grande (ver Game_killPlayer).
}

bool Effect_deathActive(void)
{
    return deathTimer != 0;
}

static void deathUpdate(void)
{
    u16 frame;

    if (deathTimer == 0) return;

    deathTimer--;

    if (deathTimer == 0)
    {
        sprHide(deathSprite);
        if (deathWhite)
        {
            PAL_setColors(0, &roomBgColor, 1, DMA_QUEUE);
            deathWhite = FALSE;
        }
        return;
    }

    // Los primeros frames son el sprite creciendo; el resto, la pantalla en
    // blanco que deja ese sprite cuando se pasa del tamaño del room.
    frame = MUERTE_LIFE - deathTimer - 1;

    if (frame < MUERTE_FRAMES)
    {
        VStream_frame(&deathStream, deathSprite, muerteSprite.animations[0]->frames[frame]);
        // Recolocado cada frame: justo aquí arranca la sacudida más fuerte
        // del juego y el fogonazo tiene que moverse con el resto.
        sprPos(deathSprite, F2I(deathX) - (MUERTE_CELL / 2), F2I(deathY) - (MUERTE_CELL / 2));
    }
    else if (!deathWhite)
    {
        sprHide(deathSprite);
        PAL_setColors(0, &deathWhiteColor, 1, DMA_QUEUE);
        deathWhite = TRUE;
    }
}

static void kill(Effect *e);

static void toDust(Effect *e)
{
#if PORT_NO_SMOKE
    kill(e);
    return;
#endif

    e->phase = EFF_DUST;
    e->life = DUST_LIFE;
    e->frame = 0;
    e->tick = 0;
    // oca/Create_0: yf = y + choose(20,-20)
    e->y += (random() & 1) ? DUST_OFFY : -DUST_OFFY;

    sprAnimFrame(e->sprite, e->anim, 0);
    sprTile(e->sprite, explTile[e->anim * EXPL_DUST_FRAMES]);
    place(e);
}

static void kill(Effect *e)
{
    e->phase = EFF_FREE;
    sprHide(e->sprite);
}

void Effect_updateAll(void)
{
    deathUpdate();
    Effect_updateWorld();
}

void Effect_updateDeath(void)
{
    deathUpdate();
}

void Effect_updateWorld(void)
{
    u16 i;

    bombUpdate();

    for (i = 0; i < MAX_EFFECTS; i++)
    {
        Effect *e = &effects[i];

        if (e->phase == EFF_FREE) continue;

        if (e->phase == EFF_FLASH)
        {
            s16 frame;

            e->t += (fix16)(random() % (u16)EFFECT_T_STEP);

            if (e->t > EFFECT_T_MAX)
            {
                toDust(e);
                continue;
            }

            // El frame n está dibujado a escala n+1.
            frame = F2I(e->t);
            if (frame >= EXPL_FLASH_FRAMES) frame = EXPL_FLASH_FRAMES - 1;

            sprAnimFrame(e->sprite, EXPL_ANIM_FLASH, frame);
            sprTile(e->sprite, explTile[(EXPL_ANIM_FLASH * EXPL_DUST_FRAMES) + frame]);
            // Cada frame, no sólo al nacer: la cámara de la sacudida cambia
            // bajo el efecto y el sprite tiene que seguirla (ver sprPos).
            place(e);
            continue;
        }

        if (e->phase == EFF_AUTO)
        {
            // random_range(0.1, 20) cada frame: aquí, uno de los tres tamaños
            // y uno de los cinco frames, sorteados en cada frame.
            const u16 r = random();
            const u16 a = EXPL_ANIM_DUST + (r % EXPL_DUST_SIZES);
            const u16 f = (r >> 4) % EXPL_DUST_FRAMES;

            if (--e->life == 0)
            {
                // Alarm_0: instance_create(x, y, oEstallido)
                Debris_burst(e->x, e->y);
                kill(e);
                continue;
            }

#if PORT_NO_SMOKE
            (void)a; (void)f;
            sprHide(e->sprite);
#else
            sprAnimFrame(e->sprite, a, f);
            sprTile(e->sprite, explTile[(a * EXPL_DUST_FRAMES) + f]);
            place(e);
#endif
            continue;
        }

        // Polvo.
        if (--e->life == 0)
        {
            kill(e);
            continue;
        }

        // oca/Draw_0: xf += 3; yf += choose(-6,6)
        e->x += DUST_DRIFT;
        e->y += (random() & 1) ? DUST_SHAKE : -DUST_SHAKE;
        place(e);

        // La animación avanza cada 2 frames, desfasada por ranura para no
        // juntar los cuatro cambios de frame (64 tiles cada uno) en el mismo.
        if (++e->tick >= DUST_ANIM_EVERY)
        {
            e->tick = 0;
            if (++e->frame >= EXPL_DUST_FRAMES) e->frame = 0;
            sprAnimFrame(e->sprite, e->anim, e->frame);
            sprTile(e->sprite, explTile[(e->anim * EXPL_DUST_FRAMES) + e->frame]);
        }
    }
}

void Effect_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_EFFECTS; i++)
    {
        effects[i].t = 0;
        kill(&effects[i]);
    }

    deathTimer = 0;
    sprHide(deathSprite);
    Effect_bombCancel();

    if (deathWhite)
    {
        PAL_setColors(0, &roomBgColor, 1, DMA_QUEUE);
        deathWhite = FALSE;
    }
}
