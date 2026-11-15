#include "hole.h"
#include "collision.h"
#include "resources.h"
#include "gm_art.h"
#include "vramspr.h"

// Índice del primer tile del agujero, repartido al cargarlo (ver vramspr.h).
static u16 holeTileBase;
static u16 holeRedBase;
static u8  holeActive[HOLE_COUNT];
// Repintar un agujero son 64 escrituras al plano, o sea un acceso al puerto
// del VDP por tile. Las cabezas de gusano van justo hacia los agujeros, así
// que el estado oscilaba en el borde y repintaba casi todos los frames. Con
// un rebote de unos cuantos frames el estado sólo cambia cuando es de verdad.
#define HOLE_DEBOUNCE 6
static u8  holePending[HOLE_COUNT];
static u8  holeTimer[HOLE_COUNT];

// rooms/_1/_1.yy: las instancias de oAgujerSuper del room de 1600x1200. La
// primera es la reconstruida; las otras cinco, en el orden del room. Ver la
// nota de inc/hole.h.
static const s16 holeX[HOLE_COUNT] = { GML_POS(558), GML_POS(820), GML_POS(558), GML_POS(832), GML_POS(1088), GML_POS(1094) };
static const s16 holeY[HOLE_COUNT] = { GML_POS(754), GML_POS(272), GML_POS(446), GML_POS(888), GML_POS(424),  GML_POS(736)  };

#define HOLE_TILES_W 8          // el bloque de hole.png mide 64x64
#define HOLE_TILES_H 8
#define HOLE_ART_CENTER 32      // el origen del sprite está en el centro del bloque

// ---------------------------------------------------------------------------
// Celdas compartidas
//
// Cada agujero es un bloque de 8x8 tiles en el plano A, y donde dos agujeros
// están cerca sus bloques comparten celdas. Una celda del plano sólo tiene UN
// tile, así que el agujero que se pintaba después ponía el suyo (casi todo
// transparente: es la esquina de su bloque) y se comía los goterones del
// otro: un recorte cuadrado con el borde del bloque. En el original no pasa
// porque son sprites, que se componen píxel a píxel.
//
// Aquí se hace lo mismo a mano: para cada celda compartida hay un tile propio
// en VRAM con los agujeros que la tocan compuestos uno encima de otro, en el
// orden del room (el que va después, encima). Se recompone cuando cambia el
// reborde rojo de alguno. Con las posiciones del room son 33 celdas, nunca
// más de dos agujeros por celda.
// ---------------------------------------------------------------------------
#define MAX_SHARED 48

static u8  sharedCol[MAX_SHARED];
static u8  sharedRow[MAX_SHARED];
static u8  sharedCount;
static u16 sharedBase;

static s16 blockCol(u16 i) { return (holeX[i] - HOLE_ART_CENTER + 4) / 8; }
static s16 blockRow(u16 i) { return (holeY[i] - HOLE_ART_CENTER + 4) / 8; }

static bool covers(u16 i, s16 col, s16 row)
{
    return ((u16)(col - blockCol(i)) < HOLE_TILES_W) && ((u16)(row - blockRow(i)) < HOLE_TILES_H);
}

static s16 sharedIndex(s16 col, s16 row)
{
    u16 k;

    for (k = 0; k < sharedCount; k++)
        if ((sharedCol[k] == col) && (sharedRow[k] == row)) return k;

    return -1;
}

// Compone la celda k y la sube. Devuelve el atributo de plano.
static u16 composeShared(u16 k)
{
    u32 buf[8];
    const s16 col = sharedCol[k];
    const s16 row = sharedRow[k];
    u16 i, w;

    memset(buf, 0, sizeof(buf));

    for (i = 0; i < HOLE_COUNT; i++)
    {
        const TileSet *ts;
        const u32 *src;

        if (!covers(i, col, row)) continue;

        ts = holeActive[i] ? &holeRedTiles : &holeTiles;
        src = ts->tiles + ((((row - blockRow(i)) * HOLE_TILES_W) + (col - blockCol(i))) * 8);

        // Píxel a píxel (4 bits cada uno): donde el de encima no es el color
        // 0, manda el de encima.
        for (w = 0; w < 8; w++)
        {
            const u32 v = src[w];
            u32 m = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

            m *= 0xF;
            buf[w] = (buf[w] & ~m) | v;
        }
    }

    VDP_loadTileData(buf, sharedBase + k, 1, CPU);
    return TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, sharedBase + k);
}

static void findShared(void)
{
    s16 col, row;

    sharedCount = 0;

    for (row = 0; row < (PLAYFIELD_H / 8); row++)
    {
        for (col = 0; col < (PLAYFIELD_W / 8); col++)
        {
            u16 i, n = 0;

            for (i = 0; i < HOLE_COUNT; i++)
                if (covers(i, col, row)) n++;

            if ((n > 1) && (sharedCount < MAX_SHARED))
            {
                sharedCol[sharedCount] = (u8)col;
                sharedRow[sharedCount] = (u8)row;
                sharedCount++;
            }
        }
    }

    sharedBase = VTile_alloc(sharedCount);
}

void Hole_initGraphics(void)
{
    PAL_setPalette(PAL2, holePalette, DMA);
    holeTileBase = VTile_alloc(holeTiles.numTile);
    VDP_loadTileSet(&holeTiles, holeTileBase, DMA);
    holeRedBase = VTile_alloc(holeRedTiles.numTile);
    VDP_loadTileSet(&holeRedTiles, holeRedBase, DMA);
    findShared();
}

// Pinta el bloque de un agujero, con su variante.
static void drawOne(u16 i)
{
    const u16 base = holeActive[i] ? holeRedBase : holeTileBase;
    u16 tx, ty;
    const s16 col = (holeX[i] - HOLE_ART_CENTER + 4) / 8;
    const s16 row = (holeY[i] - HOLE_ART_CENTER + 4) / 8;

    for (ty = 0; ty < HOLE_TILES_H; ty++)
    {
        for (tx = 0; tx < HOLE_TILES_W; tx++)
        {
            const s16 cx = col + tx;
            const s16 cy = row + ty;

            if ((cx < 0) || (cx >= (PLAYFIELD_W / 8)) || (cy < 0) || (cy >= (PLAYFIELD_H / 8)))
                continue;

            {
                const s16 k = sharedIndex(cx, cy);

                VDP_setTileMapXY(BG_A,
                                 (k >= 0) ? composeShared(k)
                                 : TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                                                  base + (ty * HOLE_TILES_W) + tx),
                                 cx, cy);
            }
        }
    }
}

void Hole_rowTiles(s16 row, u16 *buf, u16 cols)
{
    u16 i, c;

    for (c = 0; c < cols; c++) buf[c] = 0;

    for (i = 0; i < HOLE_COUNT; i++)
    {
        const u16 base = holeActive[i] ? holeRedBase : holeTileBase;
        const s16 col = (holeX[i] - HOLE_ART_CENTER + 4) / 8;
        const s16 ty = row - ((holeY[i] - HOLE_ART_CENTER + 4) / 8);
        u16 tx;

        if ((ty < 0) || (ty >= HOLE_TILES_H)) continue;

        for (tx = 0; tx < HOLE_TILES_W; tx++)
        {
            const s16 cx = col + tx;

            if ((cx < 0) || (cx >= (s16)cols)) continue;

            buf[cx] = TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                                     base + (ty * HOLE_TILES_W) + tx);
        }
    }

    // Las celdas compartidas, recompuestas: durante el menú el reborde rojo
    // pudo cambiar sin repintarse (ver Hole_holdDraw).
    {
        u16 k;

        for (k = 0; k < sharedCount; k++)
            if ((sharedRow[k] == row) && (sharedCol[k] < cols)) buf[sharedCol[k]] = composeShared(k);
    }
}

static bool holdDraw;

void Hole_holdDraw(bool hold)
{
    holdDraw = hold;
}

void Hole_setActive(u16 i, bool active)
{
    if (holeActive[i] == (u8)active)
    {
        holePending[i] = (u8)active;
        return;
    }

    if (holePending[i] != (u8)active)
    {
        holePending[i] = (u8)active;
        holeTimer[i] = HOLE_DEBOUNCE;
        return;
    }

    if (holeTimer[i] && --holeTimer[i]) return;

    holeActive[i] = (u8)active;
    if (!holdDraw) drawOne(i);
}

void Hole_draw(void)
{
    u16 i, tx, ty;

    (void)tx; (void)ty;

    for (i = 0; i < HOLE_COUNT; i++)
    {
        drawOne(i);
    }
}

fix16 Hole_x(u16 i)
{
    return FIX16(holeX[i % HOLE_COUNT]);
}

fix16 Hole_y(u16 i)
{
    return FIX16(holeY[i % HOLE_COUNT]);
}

u16 Hole_snakeSpawn(void)
{
    // oCabeza1/Create_0: trunc((N-1) - random(N-1)), o sea 0 .. N-1.
    return random() % HOLE_COUNT;
}

u16 Hole_spiderSpawn(void)
{
    // el bucle del original acaba siempre en el último agujero
    return HOLE_COUNT - 1;
}

u16 Hole_wanderTarget(void)
{
    // stWalkingA1W: trunc((N-1) - random(N-2)), o sea de 1 a N-2. Ni el
    // primero ni el último entran.
    return 1 + (random() % (HOLE_COUNT - 2));
}

bool Hole_touching(s16 x, s16 y, s16 w, s16 h)
{
    u16 i;

    for (i = 0; i < HOLE_COUNT; i++)
    {
        if (Coll_overlap(x, y, w, h,
                         holeX[i] + HOLE_BOX_OX, holeY[i] + HOLE_BOX_OY,
                         HOLE_BOX_W, HOLE_BOX_H))
            return TRUE;
    }

    return FALSE;
}
