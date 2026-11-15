#include "vramspr.h"

// Tilesets ya subidos, para no duplicar: rescomp comparte un mismo tileset
// entre animaciones cuando los frames salen idénticos (y lo hace mucho con
// las rotaciones de los proyectiles).
#define MAX_LOADED 160

static const TileSet *loadedSet[MAX_LOADED];
static u16            loadedIdx[MAX_LOADED];
static u16            loadedCount;
static u16            nextTile = TILE_USER_INDEX;

u16 VTile_alloc(u16 count)
{
    const u16 base = nextTile;

    nextTile += count;
    return base;
}

u16 VTile_allocSpare(u16 count)
{
    // [siguiente libre, fin) de cada trozo, en tiles. Se calculan la primera
    // vez a partir de las direcciones que tiene puestas el VDP.
    static u16 next[2], end[2];
    u16 i;

    if (!end[0])
    {
        next[0] = VDP_getWindowAddress() / 32;
        end[0]  = next[0] + ((64 * 32 * 2) / 32);
        next[1] = (VDP_getSpriteListAddress() + (80 * 8)) / 32;
        end[1]  = 0x10000 / 32;
    }

    for (i = 0; i < 2; i++)
    {
        if ((end[i] - next[i]) >= count)
        {
            const u16 base = next[i];

            next[i] += count;
            return base;
        }
    }

    return 0;
}

void VTile_check(void)
{
    if (VTile_used() > TILE_USER_LENGTH)
    {
        char msg[40];

        sprintf(msg, "VRAM: %u de %u tiles", VTile_used(), (u16)TILE_USER_LENGTH);
        SYS_die(msg);
    }
}

u16 VTile_used(void)
{
    return nextTile - TILE_USER_INDEX;
}

u16 VTile_shared(void)
{
    static u16 sharedBase;

    if (!sharedBase) sharedBase = VTile_alloc(VTILE_SHARED_TILES);

    return sharedBase;
}

static u16 upload(const TileSet *ts)
{
    u16 i;

    for (i = 0; i < loadedCount; i++)
        if (loadedSet[i] == ts) return loadedIdx[i];

    {
        // Los tilesets de los sprites precargados son pequeños (un frame) y
        // caben en los huecos de VRAM sobrante que deja el menú (ver
        // VTile_allocSpare). Van ahí primero; lo que no quepa, al espacio
        // normal. El menú reparte lo suyo antes (Menu_init va antes que los
        // *_initAll en Game_init), así que nunca se queda sin sitio.
        u16 idx = VTile_allocSpare(ts->numTile);

        if (!idx) idx = VTile_alloc(ts->numTile);

        VDP_loadTileSet(ts, idx, DMA);

        if (loadedCount < MAX_LOADED)
        {
            loadedSet[loadedCount] = ts;
            loadedIdx[loadedCount] = idx;
            loadedCount++;
        }

        return idx;
    }
}

void VSpr_load(const SpriteDefinition *def, u16 *table, u16 stride)
{
    u16 a, f;

    for (a = 0; a < def->numAnimation; a++)
    {
        const Animation *an = def->animations[a];

        for (f = 0; f < an->numFrame; f++)
            table[(a * stride) + f] = upload(an->frames[f]->tileset);
    }
}

u16 VSpr_loadFrame(const SpriteDefinition *def, u16 anim, u16 frame)
{
    return upload(def->animations[anim]->frames[frame]->tileset);
}

// Los sprites van con prioridad BAJA a proposito. En el VDP el orden es
// sprite alto > plano A alto > plano B alto > sprite bajo > plano A bajo >
// plano B bajo: un sprite bajo sigue pasando por encima de los agujeros y del
// fondo (planos de prioridad baja), pero queda DEBAJO de los tiles de
// prioridad alta, que aqui son el HUD y el texto del menu. Es lo que hace el
// original, donde oInGameHUD vive en la capa mas alta del room
// (Instances_1, depth -2147482300) y nada se le pone encima. Con todos los
// sprites en prioridad alta, los paquetes y las balas tapaban las cifras.
MDSpr *VSpr_add(const SpriteDefinition *def, u16 pal, u16 tileIndex)
{
    return Spr_alloc(def, TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE, tileIndex));
}

MDSpr *VStream_add(VStream *st, const SpriteDefinition *def, u16 pal)
{
    u16 a, f, cap = 0;

    for (a = 0; a < def->numAnimation; a++)
    {
        const Animation *an = def->animations[a];

        for (f = 0; f < an->numFrame; f++)
        {
            const u16 n = an->frames[f]->tileset->numTile;

            if (n > cap) cap = n;
        }
    }

    st->cap  = cap;
    st->base = VTile_alloc(cap);

    {
        MDSpr *s = VSpr_add(def, pal, st->base);

        // Nada subido todavía: el primer VStream_frame tiene que subir.
        VStream_invalidate(s);
        return s;
    }
}

void VStream_frame(VStream *st, MDSpr *s, const AnimationFrame *f)
{
    if (s->frame == f) return;

    sprSetFrame(s, f);

    {
        const TileSet *ts = f->tileset;

        // Dirección VRAM = tile * 32 bytes; longitud en palabras = tiles * 16.
        DMA_queueDma(DMA_VRAM, ts->tiles, st->base * 32, ts->numTile * 16, 2);
    }
}
