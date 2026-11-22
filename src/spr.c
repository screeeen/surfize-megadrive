#include "spr.h"

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
static u16   satUsed;

void Spr_init(void)
{
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
#ifndef PORT_SPR_C
#define PORT_SPR_C 0
#endif

#if PORT_SPR_C
// Implementación de referencia en C del montador, la que había antes de
// src/spr_a.s. No entra en la build normal: está para poder comparar la tabla
// que produce el ensamblador contra la que producía esto, volcándola con el
// comando `vs` del depurador en la misma escena con la semilla fija.
//
//   make EXTRA_FLAGS="... -DPORT_SPR_C=1"
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
// Coste: una pasada por la tabla (unas 60 entradas) sumando en 30 franjas,
// uno de cada 4 frames, y tres enlaces reescritos.
// ---------------------------------------------------------------------------
#define BANDS        32
#define BAND_SPR_MAX 20
#define BAND_CEL_MAX 40

static bool overflowing(u16 n)
{
    u8 spr[BANDS];
    u8 cel[BANDS];
    const VDPSprite *v = vdpSpriteCache;
    u16 i;

    memset(spr, 0, sizeof(spr));
    memset(cel, 0, sizeof(cel));

    for (i = 0; i < n; i++, v++)
    {
        const s16 top = (s16)(v->y & 0x3FF) - 0x80;
        const u16 sz = v->size_link >> 8;
        const s16 bottom = top + ((((sz & 3) + 1) << 3) - 1);
        const u8 w = ((sz >> 2) & 3) + 1;
        s16 b = (top < 0) ? 0 : (top >> 3);
        const s16 be = (bottom >= (BANDS * 8)) ? (BANDS - 1) : (bottom >> 3);

        for (; b <= be; b++)
        {
            if ((++spr[b] > BAND_SPR_MAX) || ((cel[b] += w) > BAND_CEL_MAX)) return TRUE;
        }
    }

    return FALSE;
}

static u16 rotation;

static void rotate(u16 n, u16 nr)
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

    // 0 -> s -> ... -> nr-1 -> 1 -> ... -> s-1 -> (la cola, sin rotar) -> fin
    vdpSpriteCache[0].link = s;
    vdpSpriteCache[nr - 1].link = 1;
    vdpSpriteCache[s - 1].link = (nr < n) ? nr : 0;
}

void Spr_flush(void)
{
    u16 n;

    // La cola sólo se recorta si de verdad es lo último del pool, y sólo
    // hasta donde diga su dueño que puede haber algo a la vista.
    sprWalkCount = ((tailEnd == sprPoolCount) && (tailWalk < (tailEnd - tailStart)))
                       ? (tailStart + tailWalk) : sprPoolCount;

    // El bucle va en ensamblador (src/spr_a.s): es el mas repetido del juego
    // y el struct esta ordenado para que la entrada se copie de un tiron.
    n = Spr_build();

    if (n)
    {
        // Fin de la lista.
        vdpSpriteCache[n - 1].link = 0;

        // La comprobación cuesta (medido: con el caso peor, que va justo al
        // borde del frame, mirarla en cada frame lo tiraba: 8 kills en vez
        // de 11 a +300). Se mira uno de cada 4 frames y el resultado vale
        // para los otros tres; un atasco dura mucho más que eso.
        //
        // Los sprites de la cola (las líneas a las holo y los rayos) van al
        // final de la tabla: si una franja se llena, los que se pierden son
        // ellos, sin tocar al resto. Así que no cuentan para decidir si hay
        // que rotar, ni rotan: contarlos hacía rotar (y parpadear) los
        // sprites del juego por culpa de una línea. Si la tabla llegó al
        // tope de 80 no se sabe cuántos de la cola entraron: se mira todo,
        // como antes.
        if (n > 2)
        {
            static u8 check;
            static bool over;
            const u16 nr = ((n < SAT_MAX_SIZE) && (tailEntries < n)) ? (n - tailEntries) : n;

            if ((check++ & 3) == 0) over = overflowing(nr);
            if (over) rotate(n, nr);
        }
    }
    else
    {
        // El VDP siempre lee al menos una entrada: una fuera de pantalla.
        vdpSpriteCache[0].y = 0;
        vdpSpriteCache[0].size_link = 0;
        vdpSpriteCache[0].attribut = 0;
        vdpSpriteCache[0].x = 0;
        n = 1;
    }

    satUsed = n;

    // Sin copia intermedia: la cola se vacia en el vblank y la tabla no se
    // vuelve a tocar hasta el frame siguiente, despues de ese vaciado.
    VDP_updateSprites(n, DMA_QUEUE);
}
