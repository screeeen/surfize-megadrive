#include "hole.h"
#include "collision.h"
#include "resources.h"
#include "gm_art.h"
#include "vramspr.h"

// Índice del primer tile de cada variante, repartido al cargarla (ver
// vramspr.h): HOLE_LOADED tiles cada una, ver "Sólo las celdas con dibujo".
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

#define HOLE_TILES_W 8          // el bloque de hole.png mide 64x64
#define HOLE_TILES_H 8
#define HOLE_ART_CENTER 32      // el origen del sprite está en el centro del bloque

// ---------------------------------------------------------------------------
// El hexágono de MonsterLite (bloque 2, Q6)
//
// AgujerosPadre.prefab: seis hijos "Agujero" (escala 1.1 cada uno, la del
// padre es 1) en posiciones locales de unidades de mundo, en el orden de
// m_Children del padre: (-2, 1), (0, -2), (0, 2), (2, -1), (-2, -1), (2, 1).
// Una unidad son 20 px y la Y de Unity va hacia arriba, así que en pantalla
// son (-40, -20), (0, +40), (0, -40), (+40, +20), (-40, +20), (+40, -20)
// desde el centro del hexágono, que va en el centro de la pantalla.
//
// MonsterLite elige el agujero al azar (FindGameObjectsWithTag), así que el
// orden no le importa; aquí sí, mientras dure la IA del GML (bloque 8): el 0
// es al que salta el enemigo que vaga y los del medio son los destinos de
// vagar (ver inc/hole.h). Las arañas nacen en uno al azar, como en MonsterLite
// (Hole_spiderSpawn). Se toma el orden del prefab.
// ---------------------------------------------------------------------------
#define HOLE_X0 (ML_HOLES_CX - (2 * ML_UNIT_PX))
#define HOLE_Y0 (ML_HOLES_CY - ML_UNIT_PX)
#define HOLE_X1 (ML_HOLES_CX)
#define HOLE_Y1 (ML_HOLES_CY + (2 * ML_UNIT_PX))
#define HOLE_X2 (ML_HOLES_CX)
#define HOLE_Y2 (ML_HOLES_CY - (2 * ML_UNIT_PX))
#define HOLE_X3 (ML_HOLES_CX + (2 * ML_UNIT_PX))
#define HOLE_Y3 (ML_HOLES_CY + ML_UNIT_PX)
#define HOLE_X4 (ML_HOLES_CX - (2 * ML_UNIT_PX))
#define HOLE_Y4 (ML_HOLES_CY + ML_UNIT_PX)
#define HOLE_X5 (ML_HOLES_CX + (2 * ML_UNIT_PX))
#define HOLE_Y5 (ML_HOLES_CY - ML_UNIT_PX)

static const s16 holeX[HOLE_COUNT] = { HOLE_X0, HOLE_X1, HOLE_X2, HOLE_X3, HOLE_X4, HOLE_X5 };
static const s16 holeY[HOLE_COUNT] = { HOLE_Y0, HOLE_Y1, HOLE_Y2, HOLE_Y3, HOLE_Y4, HOLE_Y5 };

// Dónde se pinta cada bloque, en celdas del plano. El plano A sólo se mueve
// de 8 en 8 píxeles, así que el dibujo cae en la rejilla: en X los tres
// valores (120, 160, 200) son exactos, pero en Y los cuatro agujeros de los
// lados (100 y 140) caen a 4 px de una fila. Se redondean HACIA FUERA del
// centro del hexágono (se pintan a 96 y 144), que deja el hexágono simétrico
// y es lo que menos lo deforma: hacia dentro los dos agujeros de cada lado
// se montaban 19 px (mirado en una maqueta). La posición lógica (spawn, IA,
// colisión) es la exacta.
#define HOLE_COL_OF(x)  (((x) - HOLE_ART_CENTER + 4) >> 3)
#define HOLE_ROW_OF(y)  (((y) < ML_HOLES_CY) ? (((y) - HOLE_ART_CENTER) >> 3) \
                                             : (((y) - HOLE_ART_CENTER + 7) >> 3))
static const s16 holeCol[HOLE_COUNT] =
{
    HOLE_COL_OF(HOLE_X0), HOLE_COL_OF(HOLE_X1), HOLE_COL_OF(HOLE_X2),
    HOLE_COL_OF(HOLE_X3), HOLE_COL_OF(HOLE_X4), HOLE_COL_OF(HOLE_X5)
};
static const s16 holeRow[HOLE_COUNT] =
{
    HOLE_ROW_OF(HOLE_Y0), HOLE_ROW_OF(HOLE_Y1), HOLE_ROW_OF(HOLE_Y2),
    HOLE_ROW_OF(HOLE_Y3), HOLE_ROW_OF(HOLE_Y4), HOLE_ROW_OF(HOLE_Y5)
};

// ---------------------------------------------------------------------------
// Sólo las celdas con dibujo
//
// El bloque de 8x8 celdas lleva el agujero (51 px) centrado, así que las
// esquinas están vacías: 21 de las 64 celdas no tienen ni un píxel en
// ninguna de las dos variantes. Antes se subían enteras (128 tiles) y se
// pintaban enteras, con lo que una esquina vacía de un agujero tapaba el
// dibujo del vecino. Ahora sólo se suben y se pintan las celdas con algún
// píxel en la normal o en la roja (las mismas en las dos, para que cambiar
// de variante pinte siempre las mismas celdas): 43 por variante.
//
// holeCell[t] es el índice compacto de la celda t del bloque, o HOLE_EMPTY.
// ---------------------------------------------------------------------------
#define HOLE_CELLS  (HOLE_TILES_W * HOLE_TILES_H)
#define HOLE_EMPTY  0xFF

static u8  holeCell[HOLE_CELLS];
static u16 holeLoaded;

static bool tileUsed(const TileSet *ts, u16 t)
{
    const u32 *src = ts->tiles + (t * 8);
    u16 w;

    for (w = 0; w < 8; w++)
        if (src[w]) return TRUE;

    return FALSE;
}

// Sube las celdas con dibujo de 'ts', seguidas, desde 'base'. Las tiras de
// celdas seguidas van en una sola transferencia.
static void loadCells(const TileSet *ts, u16 base)
{
    u16 t = 0;

    while (t < HOLE_CELLS)
    {
        u16 n = 0;

        if (holeCell[t] == HOLE_EMPTY) { t++; continue; }

        while (((t + n) < HOLE_CELLS) && (holeCell[t + n] != HOLE_EMPTY)) n++;

        VDP_loadTileData(ts->tiles + (t * 8), base + holeCell[t], n, DMA);
        t += n;
    }
}

// Celda (tx, ty) del bloque del agujero i, o HOLE_EMPTY si cae fuera de él.
static u8 cellOf(u16 i, s16 col, s16 row)
{
    const u16 tx = (u16)(col - holeCol[i]);
    const u16 ty = (u16)(row - holeRow[i]);

    if ((tx >= HOLE_TILES_W) || (ty >= HOLE_TILES_H)) return HOLE_EMPTY;

    return (u8)((ty * HOLE_TILES_W) + tx);
}

// Atributo de plano de la celda t del agujero i, con su variante.
static u16 cellAttr(u16 i, u16 t)
{
    const u16 base = holeActive[i] ? holeRedBase : holeTileBase;

    return TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, base + holeCell[t]);
}

// ---------------------------------------------------------------------------
// Celdas compartidas
//
// Donde dos agujeros tienen dibujo en la misma celda del plano, la celda sólo
// puede tener UN tile: el agujero que se pintaba después se comía el dibujo
// del otro, con un recorte cuadrado. En el original no pasa porque son
// sprites, que se componen píxel a píxel.
//
// Aquí se hace lo mismo a mano: para cada celda con dibujo de dos o más
// agujeros hay un tile propio en VRAM con los agujeros que la tocan
// compuestos uno encima de otro, en el orden de la tabla (el que va después,
// encima). Se recompone cuando cambia el reborde rojo de alguno. Con el
// hexágono son 30 celdas, nunca con más de dos agujeros.
// ---------------------------------------------------------------------------
#define MAX_SHARED 48

static u8  sharedCol[MAX_SHARED];
static u8  sharedRow[MAX_SHARED];
static u8  sharedCount;
static u16 sharedBase;

static s16 sharedIndex(s16 col, s16 row)
{
    u16 k;

    for (k = 0; k < sharedCount; k++)
        if ((sharedCol[k] == col) && (sharedRow[k] == row)) return k;

    return -1;
}

// Compone la celda k y la sube. Devuelve el atributo de plano.
// Píxel a píxel (4 bits cada uno): donde 'src' no es el color 0, manda
// 'src'; donde lo es, se queda lo que hubiera en 'buf'. Es componer una capa
// encima de otra con el color 0 transparente.
static void overlay(u32 *buf, const u32 *src)
{
    u16 w;

    for (w = 0; w < 8; w++)
    {
        const u32 v = src[w];
        u32 m = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

        m *= 0xF;
        buf[w] = (buf[w] & ~m) | v;
    }
}

bool Hole_cellPixels(s16 col, s16 row, u32 *buf)
{
    u16 i;
    bool any = FALSE;

    memset(buf, 0, 8 * sizeof(u32));

    for (i = 0; i < HOLE_COUNT; i++)
    {
        const u8 t = cellOf(i, col, row);

        if ((t == HOLE_EMPTY) || (holeCell[t] == HOLE_EMPTY)) continue;

        overlay(buf, (holeActive[i] ? &holeRedTiles : &holeTiles)->tiles + (t * 8));
        any = TRUE;
    }

    return any;
}

static u16 composeShared(u16 k)
{
    u32 buf[8];

    Hole_cellPixels(sharedCol[k], sharedRow[k], buf);
    VDP_loadTileData(buf, sharedBase + k, 1, CPU);
    return TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, sharedBase + k);
}

// ---------------------------------------------------------------------------
// Texto encima de los agujeros
//
// Una letra de la fuente del sistema escrita en el plano A ocupa la celda
// entera: donde la letra no tiene píxel se ve el plano B (el fondo), no el
// agujero que había en esa celda, y el agujero sale cortado en rectángulos.
// Con el hexágono en el centro de la pantalla les pasa a la fila de
// CREDITS del menú y al aviso "HOLD B" de la partida (y les pasará a los
// mensajes del bloque 9).
//
// Las letras que caen sobre dibujo de agujero se componen en un tile propio:
// los agujeros debajo y la letra encima, en blanco de la paleta de los
// agujeros (la fuente del sistema sólo usa el índice 15, blanco, y el texto
// se ve blanco). La letra se lee de la VRAM, donde SGDK dejó la fuente: en la
// ROM va comprimida.
//
// HOLE_TEXT_TILES tiles, compartidos: sólo hay un texto así en pantalla a la
// vez (CREDITS en el menú, HOLD B en partida). Si un texto pide más, las
// letras que sobran salen con el tile de la fuente, como antes.
// ---------------------------------------------------------------------------
#define HOLE_TEXT_TILES 10

static u16 textBase;
static u32 textWhite;       // el blanco de la paleta en los 8 píxeles de una fila

static void readFontTile(u8 c, u32 *dst)
{
    vu32 *ctrl = (vu32 *)VDP_CTRL_PORT;
    vu32 *data = (vu32 *)VDP_DATA_PORT;
    u16 w;

    SYS_disableInts();
    *ctrl = VDP_READ_VRAM_ADDR((u32)(TILE_FONT_INDEX + (u8)(c - 32)) * 32);
    for (w = 0; w < 8; w++) dst[w] = *data;
    SYS_enableInts();
}

void Hole_textRow(const char *s, u16 col, s16 row, u16 *buf, u16 cols)
{
    const u16 font = TILE_ATTR(VDP_getTextPalette(), VDP_getTextPriority(), FALSE, FALSE);
    u16 used = 0;

    while (*s && (col < cols))
    {
        const char c = *s++;

        if (c != ' ')
        {
            u32 cell[8];

            if ((used < HOLE_TEXT_TILES) && Hole_cellPixels(col, row, cell))
            {
                u32 glyph[8];
                u16 w;

                readFontTile((u8)c, glyph);
                // La letra en blanco: sus píxeles (índice 15) pasan al índice
                // del blanco de la paleta de los agujeros.
                for (w = 0; w < 8; w++)
                {
                    const u32 v = glyph[w];
                    const u32 m = ((v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111) * 0xF;

                    glyph[w] = textWhite & m;
                }
                overlay(cell, glyph);

                VDP_loadTileData(cell, textBase + used, 1, CPU);
                buf[col] = TILE_ATTR_FULL(PAL2, VDP_getTextPriority(), FALSE, FALSE, textBase + used);
                used++;
            }
            else
            {
                buf[col] = font | (TILE_FONT_INDEX + (u8)(c - 32));
            }
        }
        col++;
    }
}

void Hole_drawTextRow(const char *s, u16 col, s16 row)
{
    u16 buf[PLAYFIELD_W / 8];

    Hole_rowTiles(row, buf, PLAYFIELD_W / 8);
    Hole_textRow(s, col, row, buf, PLAYFIELD_W / 8);
    VDP_setTileMapDataRow(BG_A, buf, row, 0, PLAYFIELD_W / 8, CPU);
}

void Hole_clearTextRow(s16 row)
{
    u16 buf[PLAYFIELD_W / 8];

    Hole_rowTiles(row, buf, PLAYFIELD_W / 8);
    VDP_setTileMapDataRow(BG_A, buf, row, 0, PLAYFIELD_W / 8, CPU);
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
            {
                const u8 t = cellOf(i, col, row);

                if ((t != HOLE_EMPTY) && (holeCell[t] != HOLE_EMPTY)) n++;
            }

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
    u16 t;

    PAL_setPalette(PAL2, holePalette, DMA);

    holeLoaded = 0;
    for (t = 0; t < HOLE_CELLS; t++)
    {
        if (tileUsed(&holeTiles, t) || tileUsed(&holeRedTiles, t))
            holeCell[t] = (u8)holeLoaded++;
        else
            holeCell[t] = HOLE_EMPTY;
    }

    holeTileBase = VTile_alloc(holeLoaded);
    loadCells(&holeTiles, holeTileBase);
    holeRedBase = VTile_alloc(holeLoaded);
    loadCells(&holeRedTiles, holeRedBase);
    findShared();

    // El color más claro de la paleta (0x0EEE, el blanco): el de las letras
    // compuestas encima de los agujeros.
    {
        u16 i, best = 0, bestSum = 0;

        for (i = 1; i < 16; i++)
        {
            const u16 c = holePalette[i];
            const u16 sum = (c & 0xF) + ((c >> 4) & 0xF) + ((c >> 8) & 0xF);

            if (sum > bestSum) { bestSum = sum; best = i; }
        }
        textWhite = (u32)best * 0x11111111;
    }
    textBase = VTile_alloc(HOLE_TEXT_TILES);
}

// Pinta el bloque de un agujero, con su variante. Las celdas sin dibujo no
// se tocan: son del vecino, si lo hay, o se quedan vacías.
static void drawOne(u16 i)
{
    u16 tx, ty;

    for (ty = 0; ty < HOLE_TILES_H; ty++)
    {
        for (tx = 0; tx < HOLE_TILES_W; tx++)
        {
            const u16 t = (ty * HOLE_TILES_W) + tx;
            const s16 cx = holeCol[i] + tx;
            const s16 cy = holeRow[i] + ty;

            if (holeCell[t] == HOLE_EMPTY) continue;

            if ((cx < 0) || (cx >= (PLAYFIELD_W / 8)) || (cy < 0) || (cy >= (PLAYFIELD_H / 8)))
                continue;

            {
                const s16 k = sharedIndex(cx, cy);

                VDP_setTileMapXY(BG_A, (k >= 0) ? composeShared(k) : cellAttr(i, t), cx, cy);
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
        const s16 ty = row - holeRow[i];
        u16 tx;

        if ((ty < 0) || (ty >= HOLE_TILES_H)) continue;

        for (tx = 0; tx < HOLE_TILES_W; tx++)
        {
            const u16 t = (ty * HOLE_TILES_W) + tx;
            const s16 cx = holeCol[i] + tx;

            if (holeCell[t] == HOLE_EMPTY) continue;
            if ((cx < 0) || (cx >= (s16)cols)) continue;

            buf[cx] = cellAttr(i, t);
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
    u16 i;

    for (i = 0; i < HOLE_COUNT; i++) drawOne(i);
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
    // MonsterLite: createSnake() hace posHoles[Random.Range(0, posHoles.Count)]
    // (S/enemyController.cs:150). El GML daba lo mismo: oCabeza1/Create_0,
    // trunc((N-1) - random(N-1)), o sea 0 .. N-1.
    return random() % HOLE_COUNT;
}

u16 Hole_spiderSpawn(void)
{
    // MonsterLite: createSpider() hace posHoles[Random.Range(0, posHoles.Count)]
    // (S/enemyController.cs:124), un agujero al azar, igual que el gusano.
    // (El GML las sacaba siempre del último: el bucle de oSpider/Create_0.)
    return random() % HOLE_COUNT;
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
