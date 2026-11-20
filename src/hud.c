#include "hud.h"
#include "arena.h"
#include "game.h"
#include "vramspr.h"
#include "resources.h"
#include "gm_art.h"

#define HUD_COLS        (PLAYFIELD_W / 8)       // 40
#define HUD_ROW_TOP     0
#define HUD_ROW_LAST    (GML_POS(82) / 8)       // y = 16 -> fila 2

// ---------------------------------------------------------------------------
// oInGameHUD/Draw_0 pinta BULLETS y KILLS con global.myFont, que es sFont2:
// glifos de 80x80 del room, o sea 16 px en pantalla. El port los pintaba con
// la fuente de 8 px del sistema, la mitad de alto, y el HUD del original es
// una de las cosas que más se ven. Aquí van con los glifos de verdad, en
// bloques de 2x2 tiles.
//
// LAST se queda con la fuente del sistema a propósito: el original lo pinta
// con global.myFont2, que es sFont3 (20x20 del room = 4 px), o sea aún más
// pequeño que los 8 px que da el sistema.
// ---------------------------------------------------------------------------
#define HUD_GLYPH_TILES 2
#define HUD_SHEET_COLS  (HUD_FONT_GLYPHS * HUD_GLYPH_TILES)

static u16 hudFontBase;
static u16 shownBullets;
static u32 shownScore;
static u32 shownLast;
static u16 shownLevel;
static u32 shownFactor;
static u32 shownCoins;

// Índice del glifo, o 0xFF si el carácter no está en la hoja (espacio).
static u8 glyphOf(char c)
{
    u16 i;

    for (i = 0; i < HUD_FONT_GLYPHS; i++)
        if (hudFontChars[i] == c) return (u8)i;

    return 0xFF;
}

void HUD_drawBig(const char *s, s16 col, s16 row)
{
    u16 tx, ty;

    while (*s)
    {
        const u8 g = glyphOf(*s++);

        for (ty = 0; ty < HUD_GLYPH_TILES; ty++)
        {
            for (tx = 0; tx < HUD_GLYPH_TILES; tx++)
            {
                const s16 cx = col + tx;

                if ((cx < 0) || (cx >= HUD_COLS)) continue;

                VDP_setTileMapXY(BG_A,
                                 (g == 0xFF) ? 0
                                 : TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE,
                                                  hudFontBase + (ty * HUD_SHEET_COLS)
                                                  + (g * HUD_GLYPH_TILES) + tx),
                                 cx, row + ty);
            }
        }

        col += HUD_GLYPH_TILES;
    }
}

static void clearBig(void)
{
    u16 ty;

    for (ty = 0; ty < HUD_GLYPH_TILES; ty++) VDP_clearTextLine(HUD_ROW_TOP + ty);
}

// Los dos contadores van en CAMPOS DE ANCHO FIJO, rellenados con espacios por
// la izquierda. Así nunca hay que borrar la franja antes de escribir: cada
// cambio toca sólo sus glifos.
//
// Importa más de lo que parece: escribir un tile del plano es un acceso al
// puerto del VDP, y la versión que borraba los 40 tiles de las dos filas y
// repintaba todo se comía un 8 % del frame con la munición cambiando en cada
// disparo. Medido: 45,0 -> 37,5 fps en el caso peor.
#define HUD_BULLETS_COL  0
#define HUD_BULLETS_LEN  11                     // "BULLETS" + 4 cifras
//
// Bloque 1 de MonsterLite: a la derecha ya no van las KILLS del GML sino la
// PUNTUACIÓN (u32, pasa de 65535 en el nivel 14), en 7 cifras grandes. La
// fuente grande no tiene C ni R, así que la palabra SCORE va debajo con la
// del sistema. Es provisional: el HUD definitivo (SCORE, BEST, LEVEL,
// UPGRADE AT, con la fuente que se decida en Q16) es el bloque 9.
#define HUD_SCORE_DIGITS 7
#define HUD_SCORE_MAX    9999999UL
#define HUD_SCORE_COL    (HUD_COLS - (HUD_SCORE_DIGITS * HUD_GLYPH_TILES))

// Fila 2, fuente del sistema, campos de ancho fijo (nunca se borra la fila):
//   "LV nn UP nnnnnnn LAST nnnnnnn"   desde la columna 0
//   "SCORE"                            bajo las cifras, a la derecha
// LV es el nivel y UP el siguiente umbral, factorLevel * (level + 1): el
// "UPGRADE AT" de MonsterLite (S/HUD.cs:323-363).
#define HUD_LV_COL       3
#define HUD_LV_LEN       2
#define HUD_UP_COL       9
#define HUD_LAST_COL     22
#define HUD_NUM_LEN      7

// COINS (bloque 4 de MonsterLite, provisional hasta el bloque 9). En
// MonsterLite va abajo, empezando en el centro de la pantalla (S/HUD.cs:369-
// 384: x = W / 2, en la última línea), con el saldo total. Aquí, en la
// última fila con la fuente del sistema. En NTSC esa fila no se ve (la
// consola sólo muestra 224 líneas, ver PLAYFIELD_H).
#define HUD_ROW_COINS    ((PLAYFIELD_H / 8) - 1)               // fila 29
#define HUD_COINS_COL    (HUD_COLS / 2)                        // columna 20
#define HUD_COINS_MAX    9999999UL

// v en n cifras rellenas con espacios por la izquierda. Sin sprintf: es u32
// y corre sólo cuando el valor cambia.
static void padNum(char *dst, u32 v, u16 n)
{
    u16 i = n;

    do
    {
        dst[--i] = '0' + (char)(v % 10);
        v /= 10;
    } while (v && i);

    while (i) dst[--i] = ' ';
    dst[n] = 0;
}

// Igual, pero alineado a la izquierda y rellenado por la derecha, para que
// "UP 8" no deje un hueco entre la palabra y la cifra.
static void leftNum(char *dst, u32 v, u16 n)
{
    char tmp[12];
    u16 i = 0, j = 0;

    do
    {
        tmp[i++] = '0' + (char)(v % 10);
        v /= 10;
    } while (v && (i < n));

    while (i) dst[j++] = tmp[--i];
    while (j < n) dst[j++] = ' ';
    dst[n] = 0;
}

void HUD_initGraphics(void)
{
    hudFontBase = VTile_alloc(hudFont.numTile);
    VDP_loadTileSet(&hudFont, hudFontBase, DMA);
}

void HUD_init(void)
{
    shownBullets = 0xFFFF;
    shownScore = 0xFFFFFFFFUL;
    shownLast = 0xFFFFFFFFUL;
    shownLevel = 0xFFFF;
    shownFactor = 0;
    shownCoins = 0xFFFFFFFFUL;

    clearBig();
    VDP_clearTextLine(HUD_ROW_LAST);

    // Las palabras no cambian nunca: se pintan una vez y después sólo se
    // tocan las cifras. Con la munición bajando en cada disparo, repintar
    // "BULLETS" y "KILLS" enteros era repintar 20 glifos cada cinco frames
    // para nada.
    HUD_drawBig("BULLETS", HUD_BULLETS_COL, HUD_ROW_TOP);
    VDP_drawText("LV", 0, HUD_ROW_LAST);
    VDP_drawText("UP", HUD_UP_COL - 3, HUD_ROW_LAST);
    VDP_drawText("LAST", HUD_LAST_COL - 5, HUD_ROW_LAST);
    VDP_drawText("SCORE", HUD_COLS - 5, HUD_ROW_LAST);

    VDP_clearTextLine(HUD_ROW_COINS);
    VDP_drawText("COINS", HUD_COINS_COL, HUD_ROW_COINS);
}

void HUD_update(void)
{
    char buf[20];

#if HUD_SHOW_PERF
    // (132, 52) del room -> (26, 10) -> columna 3, fila 1
    VDP_showFPS(FALSE, 3, 1);
    VDP_showCPULoad(10, 1);
#endif

    if (g_playerBullets != shownBullets)
    {
        shownBullets = g_playerBullets;
        padNum(buf, (g_playerBullets > 9999) ? 9999 : g_playerBullets, 4);
        HUD_drawBig(buf, HUD_BULLETS_COL + (7 * HUD_GLYPH_TILES), HUD_ROW_TOP);
    }

    if (g_score != shownScore)
    {
        shownScore = g_score;
        padNum(buf, (g_score > HUD_SCORE_MAX) ? HUD_SCORE_MAX : g_score, HUD_SCORE_DIGITS);
        HUD_drawBig(buf, HUD_SCORE_COL, HUD_ROW_TOP);
    }

    if ((g_level != shownLevel) || (g_factorLevel != shownFactor))
    {
        const u32 up = g_factorLevel * (u32)(g_level + 1);

        shownLevel = g_level;
        shownFactor = g_factorLevel;
        leftNum(buf, (g_level > 99) ? 99 : g_level, HUD_LV_LEN);
        VDP_drawText(buf, HUD_LV_COL, HUD_ROW_LAST);
        leftNum(buf, (up > HUD_SCORE_MAX) ? HUD_SCORE_MAX : up, HUD_NUM_LEN);
        VDP_drawText(buf, HUD_UP_COL, HUD_ROW_LAST);
    }

    if (g_coins != shownCoins)
    {
        shownCoins = g_coins;
        leftNum(buf, (g_coins > HUD_COINS_MAX) ? HUD_COINS_MAX : g_coins, HUD_NUM_LEN);
        VDP_drawText(buf, HUD_COINS_COL + 6, HUD_ROW_COINS);
    }

    if (g_lastScore != shownLast)
    {
        shownLast = g_lastScore;
        leftNum(buf, (g_lastScore > HUD_SCORE_MAX) ? HUD_SCORE_MAX : g_lastScore, HUD_NUM_LEN);
        VDP_drawText(buf, HUD_LAST_COL, HUD_ROW_LAST);
    }
}
