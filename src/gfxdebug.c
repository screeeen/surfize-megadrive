#include "gfxdebug.h"

#if PORT_DEBUG_GFX

#include "arena.h"
#include "spr.h"
#include "beam.h"
#include "vramspr.h"
#include "bg.h"
#include "hud.h"
#include "gm_art.h"

#define LINES        PLAYFIELD_H
#define ROWS         (PLAYFIELD_H / 8)
#define SPR_LIMIT    20             // sprites por línea en H40
#define CELL_LIMIT   40             // celdas (320 px) por línea en H40
#define PANEL_ROW    27
#define GRAPH_COL    38
#define WINDOW       8              // frames que acumulan los máximos

extern MDSpr sprPool[];
extern u16   sprPoolCount;

static u8  mode;                    // 0 apagado, 1 panel, 2 panel + gráfica
static bool sprOff, planeBOff, planeAOff;
static u16 prevPad;

static u8  lineSpr[LINES];
static u8  lineCell[LINES];
static u8  lineMask[LINES];

// Máximos de la ventana
static u8  rowMax[ROWS];
static u8  rowFlag[ROWS];
static u16 wSat, wWant, wSprMax, wSprAt, wCellMax, wCellAt, wOver, wMask;
static u16 wDmaBytes, wDmaQ;
static bool wHw;
static u16 tick;

static void setPal(u16 line, bool off)
{
    static const u16 backdrop[16] = {
        ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR,
        ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR,
        ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR,
        ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR, ROOM_BG_COLOR };
    static u16 saved[4][16];

    if (off)
    {
        PAL_getColors(line * 16, saved[line], 16);
        PAL_setColors(line * 16, backdrop, 16, DMA_QUEUE);
    }
    else
    {
        PAL_setColors(line * 16, saved[line], 16, DMA_QUEUE);
    }
}

static void clearAll(void)
{
    u16 r;

    for (r = PANEL_ROW; r < ROWS; r++) VDP_clearTextLine(r);
    for (r = 0; r < ROWS; r++) VDP_clearText(GRAPH_COL, r, 2);
    // La gráfica pisa la cola de KILLS en las filas del HUD.
    HUD_repaint();
    HUD_update();
}

void GfxDebug_input(u16 pad)
{
    const u16 pressed = pad & ~prevPad;

    prevPad = pad;

    if (pressed & BUTTON_START)
    {
        mode = (mode + 1) % 3;
        clearAll();
        tick = 0;
    }

    if (!mode) return;

    if (pressed & BUTTON_A) sprOff = !sprOff;
    if (pressed & BUTTON_B) { planeBOff = !planeBOff; setPal(BG_PAL, planeBOff); }
    if (pressed & BUTTON_C) { planeAOff = !planeAOff; setPal(PAL2, planeAOff); }
}

bool GfxDebug_panelOpen(void)
{
    return mode != 0;
}

// Recorre la tabla que acaba de montar Spr_flush, que es la que va al VDP.
static void analyse(void)
{
    const u16 ng = Spr_satGame();
    const u16 n = Spr_satUsed();
    u16 nl;
    const VDPSprite *lines = Beam_lineEntries(&nl);
    u16 i, l;
    u16 want = 0;

    memset(lineSpr, 0, sizeof(lineSpr));
    memset(lineCell, 0, sizeof(lineCell));
    memset(lineMask, 0, sizeof(lineMask));

    // Las del juego (vdpSpriteCache) y detrás las de las líneas a las holo,
    // que tienen su propio trozo de la tabla (src/beam.c).
    for (i = 0; i < n; i++)
    {
        const VDPSprite *v = (i < ng) ? &vdpSpriteCache[i] : &lines[i - ng];
        const s16 top = (s16)(v->y & 0x3FF) - 0x80;
        const u16 h = (((v->size_link >> 8) & 3) + 1) * 8;
        const u16 w = ((v->size_link >> 10) & 3) + 1;
        const bool mask = ((v->x & 0x1FF) == 0);

        for (l = 0; l < h; l++)
        {
            const s16 y = top + (s16)l;

            if ((y < 0) || (y >= LINES)) continue;
            if (lineSpr[y] < 255) lineSpr[y]++;
            lineCell[y] += w;
            if (mask) lineMask[y] = 1;
        }
    }

    // Lo que se pedía: ranuras no aparcadas del pool (con sus partes).
    for (i = 0; i < sprPoolCount; i++)
        if (!MDSPR_IS_PARKED(&sprPool[i]))
            want += sprPool[i].parts ? sprPool[i].parts : 1;

    if (n > wSat) wSat = n;
    if (want > wWant) wWant = want;

    for (l = 0; l < LINES; l++)
    {
        const u16 r = l / 8;
        u8 f = 0;

        if (lineSpr[l] > wSprMax) { wSprMax = lineSpr[l]; wSprAt = l; }
        if (lineCell[l] > wCellMax) { wCellMax = lineCell[l]; wCellAt = l; }
        if ((lineSpr[l] > SPR_LIMIT) || (lineCell[l] > CELL_LIMIT)) wOver++;
        if (lineMask[l]) { wMask++; f |= 2; }
        if (lineCell[l] > CELL_LIMIT) f |= 1;

        if (lineSpr[l] > rowMax[r]) rowMax[r] = lineSpr[l];
        rowFlag[r] |= f;
    }

    {
        const u16 q = DMA_getQueueSize();
        const u16 b = DMA_getQueueTransferSize();

        if (b > wDmaBytes) wDmaBytes = b;
        if (q > wDmaQ) wDmaQ = q;
    }

    // Bit 6 del registro de estado: el VDP vio más sprites de los que caben en
    // alguna línea. Se borra al leerlo.
    if (GET_VDP_STATUS(VDP_SPROVERFLOW_FLAG)) wHw = TRUE;
}

static void draw(void)
{
    char s[44];

    sprintf(s, "SPR %2u/80 W %3u%s VRAM %4u   ", wSat, wWant,
            (wWant > 80) ? "!" : " ", VTile_used());
    VDP_drawText(s, 0, PANEL_ROW);
    sprintf(s, "LN %2u%s@%3u C %2u%s@%3u CPU %3u%%  ", wSprMax,
            (wSprMax > SPR_LIMIT) ? "!" : " ", wSprAt, wCellMax,
            (wCellMax > CELL_LIMIT) ? "!" : " ", wCellAt, SYS_getCPULoad());
    VDP_drawText(s, 0, PANEL_ROW + 1);
    sprintf(s, "OV %3u MK %3u DMA %4u/%u q%2u %s %c%c%c  ", wOver, wMask,
            wDmaBytes, DMA_getMaxTransferSize(), wDmaQ, wHw ? "HW" : "  ",
            sprOff ? 's' : 'S', planeBOff ? 'b' : 'B', planeAOff ? 'a' : 'A');
    VDP_drawText(s, 0, PANEL_ROW + 2);

    if (mode == 2)
    {
        u16 r;

        for (r = 0; r < ROWS; r++)
        {
            char g[3];
            const u8 m = rowMax[r];

            g[0] = (m > SPR_LIMIT) ? '*' : ((m < 10) ? ('0' + m) : ('A' + (m - 10)));
            g[1] = (rowFlag[r] & 1) ? '#' : ((rowFlag[r] & 2) ? 'M' : ' ');
            g[2] = 0;
            if (r < PANEL_ROW) VDP_drawText(g, GRAPH_COL, r);
        }
    }
}

static void resetWindow(void)
{
    memset(rowMax, 0, sizeof(rowMax));
    memset(rowFlag, 0, sizeof(rowFlag));
    wSat = wWant = wSprMax = wSprAt = wCellMax = wCellAt = wOver = wMask = 0;
    wDmaBytes = wDmaQ = 0;
    wHw = FALSE;
}

void GfxDebug_frame(void)
{
    if (sprOff)
    {
        // Una sola entrada, fuera de la pantalla: la cola la sube DESPUÉS de
        // la tabla que acaba de encolar Spr_flush y la pisa.
        vdpSpriteCache[0].y = 0;
        vdpSpriteCache[0].size_link = 0;
        vdpSpriteCache[0].attribut = 0;
        vdpSpriteCache[0].x = 1;
        VDP_updateSprites(1, DMA_QUEUE);
    }

    if (!mode) return;

    if (!sprOff) analyse();

    if (++tick >= WINDOW)
    {
        tick = 0;
        draw();
        resetWindow();
    }
}

#endif
