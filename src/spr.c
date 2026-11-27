#include "spr.h"
#include "beam.h"

// Una ranura de más al final: el SUMIDERO. Si alguna vez se agota el pool,
// Spr_alloc devuelve esa en vez de NULL. Devolver NULL sería un fallo de los
// malos en una build publicada: los módulos guardan el puntero y escriben la
// posición sin mirar, así que un NULL acaba escribiendo en la memoria baja
// del 68000. Con el sumidero lo peor que pasa es que ese sprite no se vea,
// porque el montador sólo recorre hasta poolCount.
MDSpr sprPool[MDSPR_POOL + 1];
u16   sprPoolCount;
// Las que recorre el montador este frame: todas, o todas menos la COLA si la
// cola está entera aparcada (ver Spr_tailBegin).
u16   sprWalkCount;
static u16   sprWanted;     // las pedidas, quepan o no
static u16   tailStart, tailEnd;
static u16   tailWalk = 0xFFFF;    // ranuras de la cola a recorrer
static u16   tailEntries;          // sprites de la cola a la vista
static u16   satUsed;          // entradas en la lista: las del juego y las de las líneas
static u16   satGame;          // las del juego

static void bandInit(void);

void Spr_init(void)
{
    bandInit();
    sprPoolCount = 0;
    sprWanted = 0;
    satUsed = 0;

    // Suelta cualquier reserva que hubiera hecho SGDK y deja la tabla limpia.
    VDP_resetSprites();
    VDP_updateSprites(1, DMA);
}

MDSpr *Spr_alloc(const SpriteDefinition *def, u16 attr)
{
    MDSpr *s;

    sprWanted++;
    s = (sprPoolCount >= MDSPR_POOL) ? &sprPool[MDSPR_POOL] : &sprPool[sprPoolCount++];

    s->def   = def;
    s->attr  = attr;
    sprSetFrame(s, def->animations[0]->frames[0]);
    sprHide(s);

    return s;
}

u16 Spr_satUsed(void)   { return satUsed; }
u16 Spr_satGame(void)   { return satGame; }
u16 Spr_allocated(void) { return sprPoolCount; }

void Spr_tailBegin(void)          { tailStart = sprPoolCount; }
void Spr_tailEnd(void)            { tailEnd = sprPoolCount; }
void Spr_tailVisible(u16 count)    { tailWalk = count; }
void Spr_tailEntries(u16 count)    { tailEntries = count; }

void Spr_check(void)
{
    if (sprWanted > MDSPR_POOL)
    {
        char msg[40];

        sprintf(msg, "SPRITES: %u de %u ranuras", sprWanted, (u16)MDSPR_POOL);
        SYS_die(msg);
    }
}

// ---------------------------------------------------------------------------
// El montador
//
// Por cada ranura: descartar si no toca la pantalla y, si toca, volcar sus
// sprites de hardware. El desplazamiento y el tamaño de cada subsprite los da
// rescomp en el frame; el índice de tile avanza con numTile, igual que hace
// SGDK en updateSpriteTableAll.
//
// La lista del VDP se recorre por el campo 'link': cada entrada apunta a la
// siguiente y la última lleva 0. Por eso el enlace se escribe como el índice
// del que viene detrás.
// ---------------------------------------------------------------------------
#ifndef PORT_NO_ASM
#define PORT_NO_ASM 0
#endif

#if PORT_NO_ASM
// Implementación en C del montador, la que había antes de src/spr_a.s y la
// que se usa con PORT_NO_ASM=1 (portable; hasta la quinta entrega del bloque
// 12 se llamaba PORT_SPR_C). Hace lo mismo que el ensamblador: se puede
// comparar la tabla volcándola con el comando `vs` del depurador.
//
//   make EXTRA_FLAGS="... -DPORT_NO_ASM=1"
u16 Spr_build(void)
{
    VDPSprite *vs = vdpSpriteCache;
    const MDSpr *s = sprPool;
    u16 left = sprWalkCount;
    u16 n = 0;

    while (left--)
    {
        const s16 y = s->y;

        if (y != MDSPR_PARKED)
        {
            const s16 x = s->x;
            u16 parts = s->parts;

            if (parts == 0)
            {
                vs->y = y;
                vs->size_link = s->sizeLink | (n + 1);
                vs->attribut = s->attr;
                vs->x = x;
                vs++;

                if (++n >= SAT_MAX_SIZE) return n;
            }
            else
            {
                const FrameVDPSprite *p = s->frame->frameVDPSprites;
                u16 attr = s->attr;

                do
                {
                    vs->y = y + p->offsetY;
                    vs->size_link = (((u16)p->size) << 8) | (n + 1);
                    vs->attribut = attr;
                    vs->x = x + p->offsetX;

                    attr += p->numTile;
                    p++;
                    vs++;

                    if (++n >= SAT_MAX_SIZE) return n;
                }
                while (--parts);
            }
        }

        s++;
    }

    return n;
}
#endif

// ---------------------------------------------------------------------------
// Rotación de la tabla cuando una línea se desborda
//
// El VDP pinta como mucho 20 sprites y 320 px de sprite por línea (H40). Lo
// que no cabe es lo que va DETRÁS en la lista, y como la lista sale siempre en
// el mismo orden (el de reparto), los que sobran eran siempre los mismos: un
// trozo de gusano o una araña que desaparecían del todo mientras duraba el
// atasco. Es lo que hacen los juegos comerciales: si hay desbordamiento, cada
// frame la lista empieza por un sitio distinto, así que lo que sobra va
// cambiando y en vez de desaparecer, parpadea.
//
// Sólo se rota si hace falta, porque rotar cambia qué sprite queda encima de
// cuál, y sin desbordamiento eso sería un parpadeo gratuito donde dos objetos
// se solapan. La comprobación es por franjas de 8 líneas (un sprite cuenta en
// todas las franjas que toca), así que se pasa un poco de prudente: alguna
// vez rota sin que una línea concreta llegue al límite. La entrada 0 no rota:
// es la nave, que va siempre encima.
//
// Coste: desde el 2026-11-26, un cuarto de la tabla por frame sumando en sus
// franjas (src/spr_a.s, ~2 000 ciclos con la tabla llena), una mirada a las
// 32 franjas cada cuatro, y tres enlaces reescritos. Antes era la tabla
// entera en C uno de cada cuatro frames: ~35 000 ciclos de golpe.
// ---------------------------------------------------------------------------
// Las franjas llenas, en src/spr_a.s: sprBand, una palabra larga por franja
// de 8 líneas (sprites << 16 | celdas), y sprBandTab, por tamaño del VDP y
// fila de arranque (y & 7), lo que suma un sprite a cada franja que toca y
// cuántas toca menos una (8 bytes por entrada, ver Spr_bandAdd).
u32 sprBand[SPR_BANDS];
typedef struct { u32 inc; u16 nb1; u16 pad; } SprBandRow;
SprBandRow sprBandTab[16 * 8];
_Static_assert(SAT_MAX_SIZE == VDP_SAT_ENTRIES, "inc/hwlimits.h y SGDK, de acuerdo");
_Static_assert(sizeof(VDPSprite) == VDP_SPRITE_BYTES, "una entrada de la tabla");
_Static_assert(SPR_BANDS == 72, "sprBand");
_Static_assert(sizeof(SprBandRow) == 8, "SprBandRow: 8 bytes (src/spr_a.s, Spr_bandAdd)");
// El contador de sprites de cada franja arranca en 0x8000 - 21: al pasar de
// 20 sprites se pone el bit de signo y Spr_bandAdd lo ve con un bmi.
#define SPR_BAND_ZERO   (((u32)(0x8000 - (VDP_LINE_SPRITES + 1))) << 16)

bool Spr_bandAdd(const VDPSprite *v, u16 count);
bool Spr_bandOver(void);

#if PORT_NO_ASM
// Las dos de src/spr_a.s, en C (PORT_NO_ASM=1): la misma cuenta.
bool Spr_bandAdd(const VDPSprite *v, u16 count)
{
    bool hit = FALSE;

    while (count--)
    {
        const u16 y = (u16)v->y & 0x3FF;

        if (y < VDP_SPRITE_Y_RANGE)
        {
            const SprBandRow *t = &sprBandTab[((v->size_link >> 8) << 3) | (y & 7)];
            u32 *b = &sprBand[y >> 3];
            u16 k = t->nb1 + 1;

            while (k--) { *b += t->inc; if (*b & 0x80000000UL) hit = TRUE; b++; }
        }
        v++;
    }
    return hit;
}

bool Spr_bandOver(void)
{
    bool over = FALSE;
    u16 i;

    for (i = 16; i < 48; i++)
        if ((sprBand[i] & 0x80000000UL) || ((sprBand[i] & 0xFFFF) > VDP_LINE_CELLS)) over = TRUE;
    for (i = 0; i < SPR_BANDS; i++) sprBand[i] = SPR_BAND_ZERO;
    return over;
}
#endif

static void bandInit(void)
{
    u16 sz, r;

    for (sz = 0; sz < 16; sz++)
        for (r = 0; r < 8; r++)
        {
            SprBandRow *t = &sprBandTab[(sz << 3) | r];
            const u16 h = ((sz & 3) + 1) * 8;

            t->inc = 0x10000UL | (((sz >> 2) & 3) + 1);
            t->nb1 = (u16)((r + h - 1) >> 3);
            t->pad = 0;
        }
    for (r = 0; r < SPR_BANDS; r++) sprBand[r] = SPR_BAND_ZERO;
}

static u16 rotation;

// after: la entrada a la que enlaza la última del juego (la primera de las
// líneas, o 0 si no hay).
static void rotate(u16 n, u16 nr, u16 after)
{
    const u16 m = nr - 1;           // entradas que rotan: de la 1 a la nr-1
    u16 s;

    if (m < 2) return;

    // Un paso que no divida al número de entradas, para que el arranque
    // recorra toda la lista y no se quede saltando entre dos sitios.
    rotation += 7;
    if (rotation >= m) rotation %= m;
    if (rotation == 0) return;

    s = 1 + rotation;

    // 0 -> s -> ... -> nr-1 -> 1 -> ... -> s-1 -> (la cola, sin rotar) ->
    // (las líneas) -> fin
    vdpSpriteCache[0].link = s;
    vdpSpriteCache[nr - 1].link = 1;
    vdpSpriteCache[s - 1].link = (nr < n) ? nr : after;
}

void Spr_flush(void)
{
    u16 n, k, after, len;

    // La cola sólo se recorta si de verdad es lo último del pool, y sólo
    // hasta donde diga su dueño que puede haber algo a la vista.
    sprWalkCount = ((tailEnd == sprPoolCount) && (tailWalk < (tailEnd - tailStart)))
                       ? (tailStart + tailWalk) : sprPoolCount;

    // El bucle va en ensamblador (src/spr_a.s): es el mas repetido del juego
    // y el struct esta ordenado para que la entrada se copie de un tiron.
    n = Spr_build();

    if (n == 0)
    {
        // El VDP siempre lee al menos una entrada: una fuera de pantalla.
        vdpSpriteCache[0].y = 0;
        vdpSpriteCache[0].size_link = 0;
        vdpSpriteCache[0].attribut = 0;
        vdpSpriteCache[0].x = 0;
        n = 1;
    }

    // Las líneas a las holo, con lo que haya dejado libre el juego: van
    // DETRÁS de todo en la lista (entran por la entrada 79, ver src/beam.c),
    // así que ni le quitan sitio en la tabla al juego ni, si una línea de
    // pantalla se llena, le quitan a él los sprites de esa línea: lo que se
    // pierde es de ellas. Su trozo de la tabla ya está montado y sólo se
    // sube (en el mismo DMA) lo que cambia.
    k = Beam_linesFlush(n, &len);
    after = k ? (SAT_MAX_SIZE - 1) : 0;

    // Fin de la lista del juego: sigue por las líneas, o se acaba.
    vdpSpriteCache[n - 1].link = after;

    // La comprobación cuesta (medido: con el caso peor, que va justo al
    // borde del frame, mirarla en cada frame lo tiraba: 8 kills en vez
    // de 11 a +300). Se suma un cuarto de la tabla por frame y se mira al
    // cuarto; el resultado vale para los cuatro siguientes: un atasco dura
    // mucho más que eso.
    //
    // Los sprites de la cola (los rayos) van al final de los del juego: si
    // una franja se llena, los que se pierden son ellos (y antes, las
    // líneas). Así que no cuentan para decidir si hay que rotar, ni rotan.
    // Si la tabla llegó al tope de 80 no se sabe cuántos de la cola
    // entraron: se mira todo, como antes. Las líneas no cuentan nunca.
    if (n > 2)
    {
        static u8 check;
        static bool over, early;
        static u16 nrCycle;
        const u16 nr = ((n < SAT_MAX_SIZE) && (tailEntries < n)) ? (n - tailEntries) : n;
        const u16 q = check++ & 3;
        u16 a, b;

        // Los cuartos salen del nr del primer frame de la vuelta (y no del
        // de cada frame, que cambia: se contaban entradas dos veces o
        // ninguna), recortados a las que haya este frame.
        if (q == 0) { nrCycle = nr; early = FALSE; }
        a = (u16)((nrCycle * q) >> 2);
        b = (u16)((nrCycle * (q + 1)) >> 2);
        if (b > nr) b = nr;
        if (a > b) a = b;

        // Un cuarto de las entradas por frame, y al cuarto frame se mira
        // (src/spr_a.s): la cuenta junta cuatro frames seguidos, que en un
        // atasco apenas cambian, y el coste no cae entero en uno solo.
        // Spr_bandAdd devuelve TRUE en cuanto una franja pasa de 20 sprites
        // con lo sumado hasta ahora (el signo del contador, sin coste):
        // entonces se rota ya, sin esperar al cuarto frame. Las celdas se
        // miran al cuarto, como antes.
        if (Spr_bandAdd(&vdpSpriteCache[a], b - a)) { early = TRUE; over = TRUE; }
        if (q == 3) over = Spr_bandOver() || early;
        if (over) rotate(n, nr, after);
    }

    satGame = n;
    satUsed = n + k;

    // Sin copia intermedia: la cola se vacia en el vblank y la tabla no se
    // vuelve a tocar hasta el frame siguiente, despues de ese vaciado.
    // Directo a la cola de DMA: VDP_updateSprites pasa por DMA_transfer y
    // DMA_queueDma (que mira si la fuente cruza un banco de 128 KB, cosa que
    // la RAM no hace) y costaba ~1600 ciclos por frame (medido, 2026-11-26).
    // Si la cola de DMA está llena no se sube nada: las líneas no pueden
    // darse por subidas (Beam_linesDmaLost).
    if (!DMA_queueDmaFast(DMA_VRAM, vdpSpriteCache, VDP_SPRITE_TABLE, len * (sizeof(VDPSprite) / 2), 2))
        Beam_linesDmaLost();
}
