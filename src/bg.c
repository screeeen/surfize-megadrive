#include "bg.h"
#include "arena.h"
#include "resources.h"
#include "vramspr.h"
#include "bg_ruins.h"

#ifndef PORT_NO_BG
#define PORT_NO_BG 0
#endif

// Los fondos que hay en la ROM, en el orden en que se recorren. Todos salen
// de tools/bg2md.py con el mismo presupuesto de tiles, y comparten la región
// de VRAM y la línea de paleta: sólo hay uno subido a la vez.
typedef struct
{
    const TileSet *tiles;
    const u16     *pal;
    const u16     *map;
} BgDef;

// Las ruinas en verde oscuro y con menos brillo (petición del autor; no sale
// del GML ni de MonsterLite). bg_ruins_palette, la que genera bg2md.py, es
// una escala de grises de 8 tonos (índices 1-8: 0x000 .. 0xEEE); ésta deja
// los mismos 8 tonos en el mismo orden, todos distintos, pasados a verde y
// con el más claro en 0x282 en vez de 0xEEE. Formato del VDP: 0x0BGR.
static const u16 bgRuinsGreen[16] =
{
    0x0E0E,                                     // 0: transparente, no se usa
    0x0000, 0x0020, 0x0040, 0x0042,             // 1-4
    0x0060, 0x0062, 0x0262, 0x0282,             // 5-8
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const BgDef bgDefs[BG_COUNT] =
{
    { &bgRuinsTiles,       bgRuinsGreen,           bg_ruins_map       },
};

static const u16 blackPal[16];

static u16  bgBase;
static bool bgReady;
static u16  bgCur;
static u16  bgWanted;
static u8   bgStep;             // 0 quieto; 1 negro en curso; 2 nueva paleta
static s16  coverTop = -1;      // filas que tapa la banda del menú: no se tocan
static s16  coverBottom = -1;

static void drawRow(u16 r)
{
    // El plano mide más que la pantalla (64x32 celdas contra 40x30). Las
    // celdas que sobran se rellenan repitiendo el borde de la imagen: la
    // sacudida de pantalla desplaza el plano unos píxeles y si no se asoma
    // una franja del color de fondo.
    const u16 *src = &bgDefs[bgCur].map[((r < BG_ROWS) ? r : (BG_ROWS - 1)) * BG_COLS];
    u16 row[64];
    u16 c;

    for (c = 0; c < planeWidth; c++)
    {
        const u16 m = src[(c < BG_COLS) ? c : (BG_COLS - 1)];

        // El mapa ya trae los bits de volteo en su sitio (12 y 11), así
        // que sólo falta la línea de paleta y la base del tileset.
        row[c] = (BG_PAL << 13) | (m & 0x1800) | (bgBase + (m & 0x07FF));
    }

    VDP_setTileMapDataRow(BG_B, row, r, 0, planeWidth, CPU);
}

void Bg_init(void)
{
    u16 i;

    // Cada diccionario tiene que caber en la región reservada (ver
    // inc/vramspr.h). Si alguno no cabe no hay fondo y el campo vuelve al
    // color plano del original: sin esta comprobación los tiles se escriben
    // encima de los mapas de plano y de la fuente, y sale basura por toda la
    // pantalla (medido con un diccionario de 639).
    if (PORT_NO_BG) return;
    for (i = 0; i < BG_COUNT; i++)
        if (bgDefs[i].tiles->numTile > VTILE_SHARED_TILES) return;

    bgBase = VTile_shared();
    bgReady = TRUE;
    bgCur = bgWanted = 0;
    PAL_setPalette(BG_PAL, bgDefs[0].pal, DMA);
    VDP_loadTileSet(bgDefs[0].tiles, bgBase, DMA);
}

void Bg_draw(void)
{
    Bg_drawRows(0, planeHeight - 1);
}

void Bg_drawRows(u16 first, u16 last)
{
    u16 r;

    if (!bgReady) return;

    for (r = first; (r <= last) && (r < planeHeight); r++)
    {
        if (((s16)r >= coverTop) && ((s16)r <= coverBottom)) continue;
        drawRow(r);
    }
}

void Bg_setCover(s16 top, s16 bottom)
{
    coverTop = top;
    coverBottom = bottom;
}

const u16 *Bg_palette(void)
{
    return bgDefs[bgCur].pal;
}

u16 Bg_current(void)
{
    return bgWanted;
}

void Bg_select(u16 index)
{
    if (!bgReady) return;
    bgWanted = index % BG_COUNT;
}

// El cambio va en tres frames para que no se vea nunca un frame a medias:
// los tiles nuevos no caben en la VRAM a la vez que los viejos, y el mapa no
// se puede reescribir en el mismo instante que los tiles. Así que:
//   1. la paleta del fondo pasa a negro (en el vblank);
//   2. con el fondo ya negro se suben los tiles y se reescribe el mapa: da
//      igual que caiga a mitad de frame, no se ve;
//   3. la paleta nueva (en el vblank).
// Son dos frames de campo negro, con los agujeros, el texto y los sprites
// encima: se lee como un corte, no como un fallo.
void Bg_update(void)
{
    if (!bgReady) return;

    switch (bgStep)
    {
        case 0:
            if (bgWanted == bgCur) return;
            PAL_setColors(BG_PAL * 16, blackPal, 16, DMA_QUEUE);
            bgStep = 1;
            break;

        case 1:
            bgCur = bgWanted;
            VDP_loadTileSet(bgDefs[bgCur].tiles, bgBase, DMA);
            Bg_draw();
            bgStep = 2;
            break;

        default:
            PAL_setColors(BG_PAL * 16, bgDefs[bgCur].pal, 16, DMA_QUEUE);
            bgStep = 0;
            break;
    }
}
