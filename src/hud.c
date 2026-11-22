#include "hud.h"
#include "arena.h"
#include "game.h"
#include "hole.h"
#include "weapon.h"
#include "vramspr.h"
#include "resources.h"
#include "gm_art.h"
#include "achieve.h"

// ===========================================================================
// HUD de partida, bloque 9 de MonsterLite (S/HUD.cs). Ver inc/hud.h para la
// disposición y docs/PORT_AUDIT_GML.md, "Bloque 9", para las medidas.
// ===========================================================================

#define HUD_COLS        (PLAYFIELD_W / 8)       // 40

// ---------------------------------------------------------------------------
// Los glifos grandes
//
// oInGameHUD/Draw_0 pinta BULLETS con global.myFont, que es sFont2: glifos de
// 80x80 del room, o sea 16 px en pantalla. Es también el tamaño del HUD de
// MonsterLite en apaisado (bulletsSt y killsSt, W/18 = 17,8 px a 320). Van
// en bloques de 2x2 tiles.
//
// La hoja trae 25 glifos ("BULETSKIA0123456789OPNXre", de cuando el HUD y el
// menú del GML escribían KILLS, TOP y NEXT en grande), pero el HUD sólo usa
// las letras de BULLETS y las cifras: se suben esas 16 y no las 25. Son 36
// tiles menos, que pagan los tiles nuevos de este bloque (ver
// HUD_initGraphics).
// ---------------------------------------------------------------------------
#define HUD_GLYPH_TILES 2
#define HUD_SHEET_COLS  (HUD_FONT_GLYPHS * HUD_GLYPH_TILES)
static const char bigChars[] = "BULETS0123456789COR";
#define HUD_BIG_GLYPHS  (sizeof(bigChars) - 1)

static u16 bigBase;
// El atributo del tile de arriba a la izquierda de cada glifo grande, por
// carácter (los otros tres son +1, +2 y +3): sin buscar en bigChars cada
// vez que cambia una cifra. 256 entradas y sin "& 0x7F" en el índice: con
// la máscara, GCC 16 a -O3 leía el carácter con un and.l de 32 bits en una
// dirección impar (shownS + 5) y el 68000 paraba con ADDRESS ERROR.
static u16 bigAttr[256];


// C y R no están en sFont2 (la hoja del GML trae BULETSKIA0123456789OPNXre).
// Dibujadas con su trazo: 4 px de grueso y 2 de alto por paso, la C con la
// barra de la E y el lado de la O, la R con la cabeza de la P y la pierna
// en escalones de 2 px como la K. Una fila de 16 px por u16, el bit 15 a la
// izquierda; la sombra se les pone como a las demás. Invención de arte
// (sólo forma), anotada en docs/PORT_AUDIT_GML.md.
static const u16 bigGlyphC[16] = { 0x3FFC, 0x3FFC, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0xF000, 0x3FFC, 0x3FFC, 0x0000, 0x0000 };
static const u16 bigGlyphR[16] = { 0xFFF0, 0xFFF0, 0xF03C, 0xF03C, 0xF03C, 0xF03C, 0xFFF0, 0xFFF0, 0xF3C0, 0xF3C0, 0xF0F0, 0xF0F0, 0xF03C, 0xF03C, 0x0000, 0x0000 };


// Las cifras en amarillo (BEST y COINS: "<color=yellow>" en S/HUD.cs:329,
// 374) y la admiración en rojo de "!!!" ("<color=red>", 505). La fuente del
// sistema sólo pinta el índice 15 de PAL0, que es el blanco; las copias van
// con los índices 14 y 13 de PAL0, que no usa nadie más.
#define HUD_IDX_YELLOW  14
#define HUD_IDX_RED     13
#define HUD_COL_YELLOW  0x00EE      // Color.yellow (1, 0.92, 0.016)
#define HUD_COL_RED     0x000E      // Color.red
static u16 yellowBase;              // '0'..'9'
static u16 redBang;                 // '!'

// El icono del arma, a 16x16 (ver buildIcon), y de qué arma es el que hay
// subido.
#define HUD_ICON_TILES  2
static u16 iconBase;
static u16 iconWeapon = 0xFFFF;

// ---------------------------------------------------------------------------
// Disposición (S/HUD.cs:284-398, en apaisado)
// ---------------------------------------------------------------------------
#define HUD_ROW_TOP      0

// "BULLETS<n>" arriba a la izquierda, en grande: ver drawTopLine.
#define HUD_BULLETS_MAX  9999

// "<n>SCORE " a la derecha (killsSt, alineado a la derecha): ver
// drawTopLine. Hasta 6 cifras: el nivel 17 empieza en 1179648 (§2.3) y hace
// falta matar a decenas de miles de bichos para llegar.
#define HUD_SCORE_MAX    999999UL

// Debajo de BULLETS, el icono del arma (currentWeaponSt, W/20 = 16 px) y su
// nombre (WeaponsController.currentWeapon.ToString()) a 1,4 iconos del borde:
// en S/HUD.cs:307-311 el rectángulo del nombre empieza en 5 + 16 x 1,4 =
// 27 px de 1024 -> 24 px, la columna 3.
#define HUD_ROW_WEAPON   2
#define HUD_NAME_COL     3

// La columna de la derecha, alineada a la derecha (bestkillsSt).
#define HUD_ROW_BEST     2
#define HUD_ROW_LEVEL    3
#define HUD_ROW_UPGRADE  4
#define HUD_RIGHT_W      11                     // "nnnnnnnBEST", "LEVEL nnnnn"
#define HUD_UPGRADE_W    19                     // "UPGRADE AT: nnnnnnn"
#define HUD_NUM_MAX      9999999UL

// "COINS <saldo>": coinsSt (alineado al centro) en un rectángulo que empieza
// en x = W/2 y mide bulletsSt.fontSize = W/18 de ancho, o sea centrado en
// W/2 + W/36 = 169 px; y a H - W/18 - 8 de alto con el mismo alto, o sea
// centrado en H - 8 - W/36 (en 1024x768: 732 de 768). Con el alto que se ve:
// 228,8 px en PAL (fila 28) y 213,5 en NTSC (fila 26).
#define HUD_COINS_CX     169
#define HUD_COINS_COL0   13
#define HUD_COINS_W      17
#define HUD_COINS_DIG    7

// Mensajes. Todos centrados en x (los estilos son MiddleCenter en
// rectángulos centrados en W/2), a la altura de su rectángulo:
//   "ARENA!"   y = H - H/1.2 = 40 px más medio alto de la línea -> fila 5
//   nivel      y = H/3 = 80 px, dos líneas centradas en ~83 px -> filas 9-10
//   " !!!"     ver HUD_ROW_ALERT
//   HOLD       y = H - H/2.9 = 157 px (el de antes, bloque 2) -> fila 20
#define HUD_ROW_ARENA    5
#define HUD_ROW_LVMSG    9
#define HUD_ROW_HOLD     20
// " !!!" se dibuja en un rectángulo que empieza en (W/2, H - H/1.9) y mide
// la pantalla entera, con readySt, que es MiddleCenter: el texto queda
// centrado en (W, 0,97 H), en la esquina de abajo a la derecha y casi todo
// fuera de la pantalla. Decisión del usuario (Q22, 2026-11-20): centrado y
// algo más arriba de y = H - H/1.9 = 114 px, para que no lo tape la nave
// cuando nace en (160, 120) (su sprite ocupa y = 108-131). Fila 12 (96-103
// px): no pasa por ningún agujero en sus columnas y deja una fila libre con
// el mensaje de nivel (filas 9-10).
#define HUD_ROW_ALERT    12
#define HUD_ALERT_COL    18                     // " !!!" centrado: 18-21

// El aviso de logro (bloque 10, invención: MonsterLite no tiene logros).
// Una fila, dos por encima de COINS: la 26 en PAL (208-215 px), que no pasa
// por ningún agujero (el más bajo, el de (166, 178), llega a la fila 25) ni
// por otro mensaje (ARENA! 5, nivel 9-10, " !!!" 12, HOLD 20, COINS 28) ni
// por el panel del depurador de gráficos (filas 27-29). Va escrita
// directamente, sin la reserva de letras encima de los agujeros (src/hole.c);
// en NTSC (fila 24) cortaría el agujero de abajo mientras se ve, pero el port
// es PAL (decisión 1). "ACHIEVEMENT! <nombre> +<monedas>", con las monedas en
// amarillo como COINS; como mucho 13 + 20 + 2 + 4 = 39 columnas.
#define HUD_ACH_PREFIX   "ACHIEVEMENT! "

#define HUD_ARENA_TEXT   "ARENA!"
// Q8: el tiempo corre con A, B o C. Si cambia ML_TIME_BUTTON, cambia esto.
#define HUD_HOLD_TEXT    "HOLD A/B/C"

// MonsterLite, WeaponsController.WEAPONS.ToString(); PIVOT y PARA, los del
// GML con el nombre que ya tienen en la tienda.
static const char *const weaponName[WPN_COUNT] = {
    "WBLOCK", "LASER", "TWAY", "PIVOT", "PARA", "CIRCULAR", "MOIRE"
};

// ---------------------------------------------------------------------------
// Estado: lo que hay pintado
// ---------------------------------------------------------------------------
static bool staticDrawn;    // palabras e icono
static u16  shownBullets;
static u32  shownScore;
static u32  shownBest;
static u16  shownLevel;
static u32  shownFactor;
static u32  shownCoins;
static u16  shownCoinsRow;

// Los mensajes: lo que se quiere (want) y lo que hay pintado (on).
static bool arenaWant, arenaOn;
static bool holdWant, holdOn;
static bool alertWant, alertOn;
static bool levelWant, levelOn;
static u16  levelShownLv;
static u32  levelShownFactor;

// " !!!": oleadaT.
static u8   alertLeft;

// El mensaje de nivel: cada subida lanza su corrutina ChangeLevelWait, y la
// primera que acaba lo quita aunque haya otras en marcha (globales.
// showNewLevel es un solo bool). Se guardan los frames que le quedan a cada
// una; 8 de sobra: hacen falta 8 subidas en 155 f.
#define HUD_LEVEL_WAITS 8
static u8   levelWaits;
static u8   levelLeft[HUD_LEVEL_WAITS];

// ---------------------------------------------------------------------------
// El HUD baja deslizándose (decisión del usuario del 2026-11-20)
//
// S/HUD.cs:156-206: en cada Update, en partida, run() acerca la posición del
// grupo a (0, 0) con MoveTowards(..., Time.deltaTime * 1000) y, fuera de
// partida, runOut() la lleva a (0, -SCREENH) a 300; empieza arriba
// (setSizes: transform.position = startPos). Lineal, en píxeles de la
// pantalla del aparato. Con la resolución por defecto del proyecto, 768 de
// alto: 240 / 768 de la pantalla por cada píxel, o sea 1000 -> 312,5 px/s =
// 6,25 px/f y 300 -> 1,875 px/f. Entra en 38,4 f y sale en 128. Como va con
// Time.deltaTime, con el tiempo parado (Q8) no se mueve.
//
// Dentro del grupo (GUI.BeginGroup, líneas 213-366) van BULLETS, el arma,
// SCORE, BEST, LEVEL, UPGRADE AT y el mensaje de nivel; COINS, ARENA!, el
// aviso y "!!!" no.
//
// En el port el grupo son las filas 0-4 del plano A, que comparte filas con
// los agujeros y el texto de partida: no se puede desplazar el plano (los
// agujeros irían con él), ni por columnas en vertical, y la ventana no está
// libre (su mapa guarda los tiles del menú, ver VTile_allocSpare). Así que el
// grupo se vuelve a escribir una fila más abajo (o más arriba) cada vez que
// la posición cruza una fila de 8 px: 5 escrituras al entrar (entre los
// frames 34 y 39 de la partida) y 5 al salir. Escalones de 8 px en vez de un
// deslizamiento suave.
// ---------------------------------------------------------------------------
#define HUD_GROUP_ROWS   5
#define HUD_SLIDE_TOP    (-240 * 8)             // en octavos de píxel
#define HUD_SLIDE_IN     50                     // 6,25 px/f
#define HUD_SLIDE_OUT    15                     // 1,875 px/f
#define HUD_OFF_UNKNOWN  0x7F

static s16 slideY = HUD_SLIDE_TOP;
static s16 shownOff = HUD_OFF_UNKNOWN;          // filas que está desplazado lo pintado

// La fila r del grupo, donde está pintada ahora (negativa: fuera).
static s16 gRow(s16 r)
{
    return r + shownOff;
}

// El salto de COINS: frames que le quedan (t de S/HUD.cs:389-396), y si
// acaba de terminar y hay que volver a pintar la línea.
static u8   coinsHide;
static bool coinsBack;

// El aviso de logro: el que se quiere ver, el que hay pintado (ACH_NONE:
// nada), los frames que le quedan y si está parado porque la tienda o la
// lista de logros tapan la pantalla (sigue al empezar la partida siguiente).
static u8   achWant = ACH_NONE, achOn = ACH_NONE;
static u8   achLeft;
static bool achBlocked;

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

// v en decimal, sin ceros a la izquierda, en dst. Devuelve la longitud.
// v no pasa de 9 999 999 (los que llaman lo recortan). Sin sprintf ni
// divisiones de 32 bits (una llamada a libgcc de cientos de ciclos por
// cifra): una divu de 32/16 parte el número en dos mitades de 16 bits y
// cada cifra sale de un divu.w de 16 bits, que el compilador pone en línea.
static u16 fmtNum(char *dst, u32 v)
{
    char tmp[10];
    u16 n = 0, i = 0, hi = 0, lo, k;

    if (v >= 10000)
    {
        hi = divu(v, 10000);
        lo = (u16)(v - ((u32)hi * 10000));
        for (k = 0; k < 4; k++) { tmp[n++] = '0' + (char)(lo % 10); lo /= 10; }
        do { tmp[n++] = '0' + (char)(hi % 10); hi /= 10; } while (hi);
    }
    else
    {
        lo = (u16)v;
        do { tmp[n++] = '0' + (char)(lo % 10); lo /= 10; } while (lo);
    }

    while (n) dst[i++] = tmp[--n];
    dst[i] = 0;
    return i;
}

static u16 strCopy(char *dst, const char *s)
{
    u16 i = 0;

    while (s[i]) { dst[i] = s[i]; i++; }
    dst[i] = 0;
    return i;
}

// TILE_ATTR de la fuente del sistema: VDP_getTextPalette y
// VDP_getTextPriority cuestan ~180 ciclos cada llamada y no cambian.
static u16 textAttr;

static u16 textTile(char c)
{
    return textAttr | (TILE_FONT_INDEX + (u8)(c - 32));
}

static u16 yellowTile(char c)
{
    if ((c < '0') || (c > '9')) return textTile(c);

    return (textAttr & ~TILE_ATTR_PALETTE_MASK) | TILE_ATTR(PAL0, FALSE, FALSE, FALSE)
           | (yellowBase + (c - '0'));
}


// ---------------------------------------------------------------------------
// Glifos grandes
// ---------------------------------------------------------------------------
static u8 bigSlot(char c)
{
    u16 i;

    for (i = 0; i < HUD_BIG_GLYPHS; i++)
        if (bigChars[i] == c) return (u8)i;

    return 0xFF;
}

// Las dos filas de tiles de la cadena se escriben de una vez cada una, por
// la cola de DMA: con VDP_setTileMapXY tile a tile eran ~1000 ciclos por
// tile (medido en el bloque 9), 16 000 por cada cambio de BULLETS, o sea en
// cada disparo; y por la CPU, con el VDP dibujando, cada palabra espera
// ~120 ciclos a que el FIFO tenga sitio. Todo lo que el HUD escribe en
// partida va por la cola (DMA_QUEUE_COPY copia los datos): nadie más
// escribe en sus celdas, así que no hay orden que respetar con la CPU.
void HUD_drawBig(const char *s, s16 col, s16 row)
{
    u16 top[HUD_COLS], bottom[HUD_COLS];
    u16 n = 0;

    while (*s && ((col + n + 1) < HUD_COLS))
    {
        const u8 g = bigSlot(*s++);

        if (g == 0xFF)
        {
            top[n] = top[n + 1] = bottom[n] = bottom[n + 1] = 0;
        }
        else
        {
            const u16 t = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, bigBase + (g * 4));

            top[n] = t;
            top[n + 1] = t + 1;
            bottom[n] = t + 2;
            bottom[n + 1] = t + 3;
        }
        n += HUD_GLYPH_TILES;
    }

    if (!n || (col < 0)) return;

    // Las filas por encima de la pantalla (el HUD entrando) no se escriben.
    if (row >= 0)       VDP_setTileMapDataRow(BG_A, top, row, col, n, DMA_QUEUE_COPY);
    if ((row + 1) >= 0) VDP_setTileMapDataRow(BG_A, bottom, row + 1, col, n, DMA_QUEUE_COPY);
}

// ---------------------------------------------------------------------------
// El icono del arma
//
// MonsterLite lo pinta a W/20 = 16 px (currentWeaponSt en apaisado). Los
// iconos del port son los del menú, de 32x32 (oMenu del GML; CIRCULAR y
// MOIRE de MonsterLite, Q11), que ya están en VRAM, pero a ese tamaño
// taparían 4x4 celdas del campo. Se reducen a la mitad al empezar cada
// partida, en 4 tiles propios: de cada 2x2 píxeles se queda el más claro de
// la paleta, para que el marco y las rayas de un píxel (el láser) no se
// pierdan. Mirado en una maqueta: los siete se reconocen.
// ---------------------------------------------------------------------------
// Brillo de cada índice de la paleta de los agujeros (0, el transparente,
// siempre pierde).
static u8 lumTab[16];

static void buildIcon(u16 weapon)
{
    const bool ml = (weapon >= WPN_CIRCULAR);
    const TileSet *ts = ml ? &weaponMlTiles : &weaponTiles;
    const u16 perRow = (ml ? 2 : 5) * 4;
    const u16 k = ml ? (weapon - WPN_CIRCULAR) : weapon;
    u32 tiles[HUD_ICON_TILES * HUD_ICON_TILES][8];
    u16 y, tx, p;

    // Sólo cuando cambia el arma. La primera versión, píxel a píxel con
    // índices calculados, costaba ~440 000 ciclos (3 frames al empezar la
    // partida); así, fila a fila con desplazamientos fijos, no llega a un
    // frame.
    if (weapon == iconWeapon) return;
    iconWeapon = weapon;

    for (y = 0; y < 16; y++)
    {
        const u16 sy = y * 2;                   // par: sy y sy + 1, en el mismo tile
        u32 out0 = 0, out1 = 0;                 // las dos mitades de la fila de salida

        for (tx = 0; tx < 4; tx++)              // los 4 tiles de ancho del icono
        {
            const u32 *src = ts->tiles + (((((sy >> 3) * perRow) + (k * 4) + tx) * 8) + (sy & 7));
            u32 r0 = src[0], r1 = src[1];
            u32 half = 0;

            for (p = 0; p < 4; p++)             // 8 píxeles de origen -> 4
            {
                u8 best = (u8)(r0 >> 28);
                u8 c = (u8)((r0 >> 24) & 0xF);

                if (lumTab[c] > lumTab[best]) best = c;
                c = (u8)(r1 >> 28);
                if (lumTab[c] > lumTab[best]) best = c;
                c = (u8)((r1 >> 24) & 0xF);
                if (lumTab[c] > lumTab[best]) best = c;

                half = (half << 4) | best;
                r0 <<= 8;
                r1 <<= 8;
            }

            if (tx < 2) out0 = (out0 << 16) | half;
            else        out1 = (out1 << 16) | half;
        }

        tiles[((y >> 3) * HUD_ICON_TILES)][y & 7] = out0;
        tiles[((y >> 3) * HUD_ICON_TILES) + 1][y & 7] = out1;
    }

    VDP_loadTileData(&tiles[0][0], iconBase, HUD_ICON_TILES * HUD_ICON_TILES, CPU);
}

// Copia de un glifo de la fuente del sistema con su color en el índice idx.
static void recolor(char c, u16 idx, u16 dst)
{
    u32 t[8];
    u16 w;

    // La fuente ya lleva su sombra (índice TEXT_SHADOW_IDX, ver src/hole.c):
    // la letra (índice 15) pasa a 'idx' y la sombra se queda.
    VTile_read(TILE_FONT_INDEX + (u8)(c - 32), t);
    for (w = 0; w < 8; w++)
    {
        const u32 v = t[w];
        const u32 m = (v & (v >> 1) & (v >> 2) & (v >> 3)) & 0x11111111;   // == 15

        t[w] = (v & ~((m << 4) - m)) | (m * idx);
    }
    VDP_loadTileData(t, dst, 1, CPU);
}

void HUD_initGraphics(void)
{
    u16 i, ty, tx;

    textAttr = TILE_ATTR(VDP_getTextPalette(), VDP_getTextPriority(), FALSE, FALSE);
    for (i = 1; i < 16; i++)
    {
        const u16 c = holePalette[i];

        lumTab[i] = 1 + (c & 0xE) + ((c >> 4) & 0xE) + ((c >> 8) & 0xE);
    }

    // Los 16 glifos grandes que se usan, en bloques de 4 tiles seguidos, con
    // la sombra de MonsterLite (decisión del usuario del 2026-11-20):
    // BULLETS y SCORE se pintan en negro offsetLetterShadow * 2 = 8 px de
    // 768 más abajo (S/HUD.cs:286-290, 350-351), 2,5 px aquí -> 2. La
    // letra (índice 8, blanco) llega a la fila 13 de los 16 de la celda, así
    // que la sombra cabe dentro: 0 tiles más. Sustituye a la sombra de 1 px
    // en diagonal que traía sFont2 del GML (índice 1). La sombra va en el
    // índice 1 de PAL2, negro.
    bigBase = VTile_alloc(HUD_BIG_GLYPHS * 4);
    for (i = 0; i < HUD_BIG_GLYPHS; i++)
    {
        u16 g = 0, y;
        u32 white[HUD_GLYPH_TILES][16];

        while ((g < HUD_FONT_GLYPHS) && (hudFontChars[g] != bigChars[i])) g++;

        for (tx = 0; tx < HUD_GLYPH_TILES; tx++)
            for (y = 0; y < 16; y++)
            {
                u32 m = 0;

                if (g < HUD_FONT_GLYPHS)
                {
                    const u32 v = hudFont.tiles[((((y >> 3) * HUD_SHEET_COLS) + (g * HUD_GLYPH_TILES) + tx) * 8)
                                                + (y & 7)];
                    // nibble == 8: bit 3 sí y los otros no
                    m = ((v >> 3) & ~(v >> 2) & ~(v >> 1) & ~v) & 0x11111111;
                }
                else
                {
                    // C o R: los 8 bits de esta mitad, un nibble por píxel
                    const u16 row = ((bigChars[i] == 'C') ? bigGlyphC : bigGlyphR)[y];
                    const u16 half = (row >> (tx ? 0 : 8)) & 0xFF;
                    u16 x;

                    for (x = 0; x < 8; x++)
                        if (half & (0x80 >> x)) m |= 1UL << (28 - (x * 4));
                }

                white[tx][y] = (m << 4) - m;
            }

        for (ty = 0; ty < HUD_GLYPH_TILES; ty++)
            for (tx = 0; tx < HUD_GLYPH_TILES; tx++)
            {
                u32 t[8];
                u16 w;

                for (w = 0; w < 8; w++)
                {
                    const u16 yy = (ty * 8) + w;
                    const u32 sh = (yy >= 2) ? (white[tx][yy - 2] & ~white[tx][yy]) : 0;

                    t[w] = (white[tx][yy] & 0x88888888UL) | (sh & 0x11111111UL);
                }
                VDP_loadTileData(t, bigBase + (i * 4) + (ty * 2) + tx, 1, CPU);
            }
    }

    for (i = 0; i < HUD_BIG_GLYPHS; i++)
        bigAttr[(u8)bigChars[i]] = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, bigBase + (i * 4));

    // El negro de la sombra de la fuente del sistema (ver src/hole.c).
    PAL_setColor(TEXT_SHADOW_IDX, 0x0000);

    // 10 cifras amarillas, la admiración roja y el icono: 15 tiles.
    yellowBase = VTile_alloc(10 + 1 + (HUD_ICON_TILES * HUD_ICON_TILES));
    redBang = yellowBase + 10;
    iconBase = redBang + 1;
    for (i = 0; i < 10; i++) recolor('0' + i, HUD_IDX_YELLOW, yellowBase + i);
    recolor('!', HUD_IDX_RED, redBang);
    PAL_setColor(HUD_IDX_YELLOW, HUD_COL_YELLOW);
    PAL_setColor(HUD_IDX_RED, HUD_COL_RED);
}

// ---------------------------------------------------------------------------
// Las filas
// ---------------------------------------------------------------------------

// La fila de COINS con el alto que se ve (ver HUD_COINS_*).
static u16 coinsRow(void)
{
    // El centro de la línea: H - 8 px de 768 (2,5 -> 3) - W/36.
    const u16 h = IS_PAL_SYSTEM ? 240 : 224;

    return (h - 3 - (PLAYFIELD_W / 36)) / 8;
}

// Las filas de los mensajes y de COINS fuera, sin llevarse los agujeros que
// pasan por ellas. Las del grupo (0-4) no: salen deslizándose (HUD_menuTick).
static void clearMessageRows(void)
{
    Hole_hideTextRow(HUD_ROW_ARENA);
    Hole_hideTextRow(HUD_ROW_LVMSG);
    Hole_hideTextRow(HUD_ROW_LVMSG + 1);
    Hole_hideTextRow(HUD_ROW_HOLD);
    Hole_hideTextRow(shownCoinsRow);
    // La fila de " !!!" no pasa por ningún agujero en las columnas del
    // texto: se borran sólo esas.
    VDP_clearTileMapRect(BG_A, HUD_ALERT_COL, HUD_ROW_ALERT, 4, 1);
}

// Las filas del grupo, a sólo agujeros, por la cola: lo que se pinte después
// en el mismo frame (también por la cola) llega detrás. Sólo las que tienen
// algo pintado: con el grupo desplazado 'shownOff' filas, las 0 .. 4 +
// shownOff (todas si no se sabe).
static void clearGroupRows(void)
{
    s16 r, last = HUD_GROUP_ROWS - 1;

    if (shownOff != HUD_OFF_UNKNOWN) last += shownOff;

    for (r = 0; r <= last; r++) Hole_hideTextRowQueued(r);
}

// Una línea alineada a la derecha de la pantalla, en un campo de w columnas
// (lo de la izquierda, en blanco: así un número más corto no deja restos).
static void drawRight(s16 row, u16 w, const u16 *tiles, u16 n)
{
    u16 buf[HUD_COLS];
    u16 i;

    if (row < 0) return;

    for (i = 0; i < w - n; i++) buf[i] = textTile(' ');
    for (i = 0; i < n; i++) buf[w - n + i] = tiles[i];
    VDP_setTileMapDataRow(BG_A, buf, row, HUD_COLS - w, w, DMA_QUEUE_COPY);
}

// ---------------------------------------------------------------------------
// La línea de arriba: "BULLETS<n>" a la izquierda y "<n>SCORE" a la derecha,
// todo con los glifos grandes (S/HUD.cs:290, 356: bulletsSt y killsSt, los
// dos a W/18), la cifra de las balas pegada a la palabra y la de la
// puntuación pegada a SCORE. Caben 20 glifos de 16 px: BULLETS y SCORE son
// 12, y para las cifras quedan 8, con un glifo de hueco siempre entre las
// dos (sin él, "BULLETS35123456SCORE" se lee como un solo número). Si no
// caben 7 (balas de 2 cifras o más con 6 de puntuación, de 3 con 5...),
// manda la puntuación y las balas se quedan en las cifras que quepan, todas
// a 9 (pregunta Q23 al usuario). Se escribe entera, sus dos filas por la cola
// de DMA, cada vez que cambia una de las dos cifras.
// ---------------------------------------------------------------------------
#define HUD_LINE_GLYPHS  (HUD_COLS / HUD_GLYPH_TILES)          // 20
#define HUD_DIGIT_GLYPHS (HUD_LINE_GLYPHS - 7 - 5)             // 8

static void putBig(u16 *top, u16 *bottom, u16 pos, char c)
{
    const u16 t = bigAttr[(u8)c];
    const u16 x = pos * HUD_GLYPH_TILES;

    top[x] = t;
    top[x + 1] = t + 1;
    bottom[x] = t + 2;
    bottom[x + 1] = t + 3;
}

// Las cifras pintadas de cada número y cuántas son (0xFF: nada). Mientras
// no cambie cuántas, la línea no se mueve: basta formatear y escribir el
// número que ha cambiado.
static char shownB[12], shownS[12];
static u8 shownBd = 0xFF, shownSd = 0xFF;

// Escribe en el plano los glifos 'n' de 's' desde el glifo 'pos'.
static void writeDigits(s16 row, u16 pos, const char *s, u16 n)
{
    u16 top[HUD_COLS], bottom[HUD_COLS];
    u16 i;

    if (!n) return;

    for (i = 0; i < n; i++) putBig(top, bottom, i, s[i]);
    if (row >= 0) VDP_setTileMapDataRow(BG_A, top, row, pos * HUD_GLYPH_TILES, n * HUD_GLYPH_TILES, DMA_QUEUE_COPY);
    VDP_setTileMapDataRow(BG_A, bottom, row + 1, pos * HUD_GLYPH_TILES, n * HUD_GLYPH_TILES, DMA_QUEUE_COPY);
}

// Las balas, con el tope que deja la puntuación (ver arriba).
static u16 fmtBullets(char *dst, u16 sd)
{
    u16 bd = fmtNum(dst, (g_playerBullets > HUD_BULLETS_MAX) ? HUD_BULLETS_MAX : g_playerBullets);
    u16 i;

    if ((bd + sd) > (HUD_DIGIT_GLYPHS - 1))
    {
        bd = HUD_DIGIT_GLYPHS - 1 - sd;
        for (i = 0; i < bd; i++) dst[i] = '9';
    }
    return bd;
}

// 'full': la línea entera (al pintar el HUD); si no, sólo lo que ha cambiado.
static void drawTopLine(bool full)
{
    static const char bullets[] = "BULLETS";
    static const char score[] = "SCORE";
    const s16 row = gRow(HUD_ROW_TOP);
    u16 bd = shownBd, sd = shownSd, i;
    bool layout = full;

    if ((row + 1) < 0) return;

    if (full || (g_score != shownScore))
    {
        sd = fmtNum(shownS, (g_score > HUD_SCORE_MAX) ? HUD_SCORE_MAX : g_score);
        if (sd != shownSd) layout = TRUE;
    }
    if (full || layout || (g_playerBullets != shownBullets))
    {
        bd = fmtBullets(shownB, sd);
        if (bd != shownBd) layout = TRUE;
    }

    if (layout)
    {
        // Cambia el reparto: se escribe de la primera cifra de las balas
        // hasta SCORE (o la línea entera).
        u16 top[HUD_COLS], bottom[HUD_COLS];
        const u16 s0 = HUD_LINE_GLYPHS - 5 - sd;
        const u16 c0 = full ? 0 : (7 * HUD_GLYPH_TILES);
        const u16 c1 = full ? HUD_COLS : ((HUD_LINE_GLYPHS - 5) * HUD_GLYPH_TILES);

        for (i = 0; i < 7; i++)  putBig(top, bottom, i, bullets[i]);
        for (i = 0; i < bd; i++) putBig(top, bottom, 7 + i, shownB[i]);
        for (i = (7 + bd) * HUD_GLYPH_TILES; i < s0 * HUD_GLYPH_TILES; i++) top[i] = bottom[i] = 0;
        for (i = 0; i < sd; i++) putBig(top, bottom, s0 + i, shownS[i]);
        for (i = 0; i < 5; i++)  putBig(top, bottom, (HUD_LINE_GLYPHS - 5) + i, score[i]);

        if (row >= 0) VDP_setTileMapDataRow(BG_A, top + c0, row, c0, c1 - c0, DMA_QUEUE_COPY);
        VDP_setTileMapDataRow(BG_A, bottom + c0, row + 1, c0, c1 - c0, DMA_QUEUE_COPY);
    }
    else
    {
        if (g_playerBullets != shownBullets) writeDigits(row, 7, shownB, bd);
        if (g_score != shownScore)           writeDigits(row, HUD_LINE_GLYPHS - 5 - sd, shownS, sd);
    }

    shownBd = (u8)bd;
    shownSd = (u8)sd;
    shownBullets = g_playerBullets;
    shownScore = g_score;
}

static void drawStatic(void)
{
    char buf[12];
    u16 tiles[HUD_COLS];
    u16 i;

    // El icono, 2x2, y el nombre (sin sombra: las dos líneas que la pintaban
    // están comentadas en S/HUD.cs:303-304).
    buildIcon(g_weaponIndex);
    for (i = 0; i < 2; i++)
    {
        const s16 r = gRow(HUD_ROW_WEAPON + i);

        if (r < 0) continue;
        tiles[0] = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, iconBase + (i * 2));
        tiles[1] = tiles[0] + 1;
        VDP_setTileMapDataRow(BG_A, tiles, r, 0, 2, DMA_QUEUE_COPY);
    }
    // En un campo de 8, el nombre más largo (CIRCULAR), para que no quede
    // nada del de la partida anterior.
    if (gRow(HUD_ROW_WEAPON) >= 0)
    {
        const char *name = weaponName[g_weaponIndex % WPN_COUNT];

        for (i = 0; i < 8; i++) tiles[i] = textTile(*name ? *name++ : ' ');
        VDP_setTileMapDataRow(BG_A, tiles, gRow(HUD_ROW_WEAPON), HUD_NAME_COL, 8, DMA_QUEUE_COPY);
    }

    // "<best>BEST": globales.maxKills1, que en MonsterLite sólo se actualiza
    // al morir (setScores, S/gameControl.cs:720, 797-800): durante la
    // partida es el récord de antes de empezarla, g_bestAtStart. Sin sombra
    // (S/HUD.cs:352, comentada).
    {
        u16 t[HUD_RIGHT_W];
        const u16 n = fmtNum(buf, (g_bestAtStart > HUD_NUM_MAX) ? HUD_NUM_MAX : g_bestAtStart);

        for (i = 0; i < n; i++) t[i] = yellowTile(buf[i]);
        t[n] = textTile('B'); t[n + 1] = textTile('E');
        t[n + 2] = textTile('S'); t[n + 3] = textTile('T');
        drawRight(gRow(HUD_ROW_BEST), HUD_RIGHT_W, t, n + 4);
        shownBest = g_bestAtStart;
    }
}

static void drawLevel(void)
{
    char buf[HUD_COLS + 1];
    u16 t[HUD_RIGHT_W];
    u16 n, i;
    const u32 up = g_factorLevel * (u32)(g_level + 1);

    n = strCopy(buf, "LEVEL ");
    n += fmtNum(buf + n, g_level);
    for (i = 0; i < n; i++) t[i] = textTile(buf[i]);
    drawRight(gRow(HUD_ROW_LEVEL), HUD_RIGHT_W, t, n);

    // "UPGRADE AT: " es el siguiente umbral, factorLevel * (level + 1), con
    // el factor ya duplicado (§2.3). Con 5 cifras o más llega a las columnas
    // del agujero de arriba (18-23): la fila va encima de los agujeros, sin
    // cortarlos (ver src/hole.c).
    if (gRow(HUD_ROW_UPGRADE) < 0) return;
    n = strCopy(buf, "UPGRADE AT: ");
    n += fmtNum(buf + n, (up > HUD_NUM_MAX) ? HUD_NUM_MAX : up);
    Hole_drawTextRow(buf, HUD_COLS - n, gRow(HUD_ROW_UPGRADE));
}

static void drawCoins(bool show)
{
    u16 buf[HUD_COINS_W];
    char num[12];
    u16 i;

    for (i = 0; i < HUD_COINS_W; i++) buf[i] = textTile(' ');

    if (show)
    {
        const u32 v = (g_coins > HUD_NUM_MAX) ? HUD_NUM_MAX : g_coins;
        const u16 nd = fmtNum(num, v);
        const u16 len = 6 + nd;
        // Centrado en HUD_COINS_CX: primera columna (cx - 4 * len) / 8.
        const u16 col = ((HUD_COINS_CX - (4 * len)) / 8) - HUD_COINS_COL0;
        static const char label[] = "COINS ";

        for (i = 0; i < 6; i++) buf[col + i] = textTile(label[i]);
        for (i = 0; i < nd; i++) buf[col + 6 + i] = yellowTile(num[i]);
    }

    VDP_setTileMapDataRow(BG_A, buf, shownCoinsRow, HUD_COINS_COL0, HUD_COINS_W, DMA_QUEUE_COPY);
}

static void drawLevelMsg(void)
{
    char buf[HUD_COLS + 1];
    u16 n;
    const u32 up = g_factorLevel * (u32)(g_level + 1);

    // "LEVEL n\n<size=16>NEXT UPGRADE: SCORE x!</size>" (S/HUD.cs:241-243),
    // con los valores de ahora: si vuelve a subir con el mensaje puesto,
    // cambia.
    n = strCopy(buf, "LEVEL ");
    n += fmtNum(buf + n, g_level);
    Hole_drawTextRow(buf, (HUD_COLS - n) / 2, HUD_ROW_LVMSG);

    n = strCopy(buf, "NEXT UPGRADE: SCORE ");
    n += fmtNum(buf + n, (up > HUD_NUM_MAX) ? HUD_NUM_MAX : up);
    buf[n++] = '!';
    buf[n] = 0;
    Hole_drawTextRow(buf, (HUD_COLS - n) / 2, HUD_ROW_LVMSG + 1);

    levelShownLv = g_level;
    levelShownFactor = g_factorLevel;
}

static void drawAlert(bool on)
{
    u16 buf[4];
    u16 i;

    buf[0] = 0;
    for (i = 1; i < 4; i++)
        buf[i] = on ? ((textAttr & ~TILE_ATTR_PALETTE_MASK) | TILE_ATTR(PAL0, FALSE, FALSE, FALSE) | redBang) : 0;
    VDP_setTileMapDataRow(BG_A, buf, HUD_ROW_ALERT, HUD_ALERT_COL, 4, DMA_QUEUE_COPY);
}

static u16 achRow(void)
{
    return coinsRow() - 2;
}

// La fila del aviso, entera de una vez por la cola: los agujeros que pasen
// por ella (ninguno en PAL) y el texto centrado encima.
static void drawAchieve(u8 id)
{
    u16 buf[HUD_COLS];
    const u16 row = achRow();

    Hole_rowTiles(row, buf, HUD_COLS);

    if (id != ACH_NONE)
    {
        char s[HUD_COLS + 8];
        u16 n, d0, col, i;

        n = strCopy(s, HUD_ACH_PREFIX);
        n += strCopy(s + n, Achieve_name(id));
        s[n++] = ' ';
        s[n++] = '+';
        d0 = n;
        n += fmtNum(s + n, Achieve_reward(id));
        if (n > HUD_COLS) n = HUD_COLS;

        col = (HUD_COLS - n) / 2;
        for (i = 0; i < n; i++) buf[col + i] = (i >= d0) ? yellowTile(s[i]) : textTile(s[i]);
    }

    VDP_setTileMapDataRow(BG_A, buf, row, 0, HUD_COLS, DMA_QUEUE_COPY);
}

// El temporizador del aviso: cuando se acaba uno, el siguiente de la cola
// (los que se consiguen en el mismo frame salen uno detrás de otro). Cuenta
// en los frames de partida, también con el tiempo parado (como "!!!",
// es un texto del port y no de la física), salvo en el hitstop (playUpdate
// sale antes de llamar a HUD_tick), y en el game over.
static void achTick(void)
{
    if (achBlocked) return;

    if (achLeft && (--achLeft == 0)) achWant = ACH_NONE;

    if (!achLeft && g_achQueued)
    {
        achWant = Achieve_popNotice();
        achLeft = ACH_NOTICE_F;
    }
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// El grupo deslizante (BULLETS, arma, SCORE, BEST, LEVEL, UPGRADE AT): si la
// posición ha cruzado una fila, se borra y se pinta entero en la nueva; si
// no, sólo lo que ha cambiado.
static NO_INLINE void drawGroupWork(s16 off);

static FORCE_INLINE void drawGroup(void)
{
    s16 off = slideY >> 6;                  // filas de 8 px, hacia abajo

    if (off < -HUD_GROUP_ROWS) off = -HUD_GROUP_ROWS;

    // Lo normal: nada que hacer. Se mira aquí, sin llamar a nada: con todo
    // el dibujo dentro, la función abría un marco de 264 bytes y guardaba 10
    // registros cada frame (medido: 2-3 líneas de VDP por frame en reposo).
    if ((off == shownOff) && staticDrawn && (g_playerBullets == shownBullets)
        && (g_score == shownScore) && (g_level == shownLevel) && (g_factorLevel == shownFactor))
        return;
    if ((off == shownOff) && (off <= -HUD_GROUP_ROWS)) return;

    drawGroupWork(off);
}

static NO_INLINE void drawGroupWork(s16 off)
{

    if (off != shownOff)
    {
        clearGroupRows();
        shownOff = off;
        staticDrawn = FALSE;
        shownBullets = 0xFFFF;
        shownScore = 0xFFFFFFFFUL;
        shownLevel = 0xFFFF;
    }

    if (shownOff <= -HUD_GROUP_ROWS) return;    // todo fuera

    if (!staticDrawn)
    {
        staticDrawn = TRUE;
        drawStatic();
        drawTopLine(TRUE);
    }
    else if ((g_playerBullets != shownBullets) || (g_score != shownScore))
    {
        drawTopLine(FALSE);
    }

    if ((g_level != shownLevel) || (g_factorLevel != shownFactor))
    {
        shownLevel = g_level;
        shownFactor = g_factorLevel;
        drawLevel();
    }
}

void HUD_init(void)
{
    // Se repinta todo en el próximo HUD_update, con el grupo donde vaya su
    // deslizamiento. No hace falta borrar el resto: al empezar la partida el
    // HUD ya está fuera (HUD_clear al entrar en el menú) y cada línea se
    // escribe en un campo de ancho fijo.
    shownCoinsRow = coinsRow();
    // Las filas encima de los agujeros (UPGRADE AT y los mensajes) creen
    // saber lo que hay en el plano: que lo vuelvan a escribir.
    Hole_textTouched();

    // Al empezar la partida no hay nada del grupo pintado (Menu_leave ha
    // repuesto el plano): no hace falta borrar sus filas.
    shownOff = -HUD_GROUP_ROWS;
    staticDrawn = FALSE;
    shownBullets = 0xFFFF;
    shownScore = 0xFFFFFFFFUL;
    shownBest = 0xFFFFFFFFUL;
    shownLevel = 0xFFFF;
    shownFactor = 0;
    shownCoins = 0xFFFFFFFFUL;

    arenaOn = holdOn = alertOn = levelOn = FALSE;
    // El aviso de logro también se vuelve a pintar, si lo hay.
    achOn = ACH_NONE;
}

void HUD_repaint(void)
{
    HUD_init();
    // Lo que haya en las filas del grupo no se sabe: se borran todas.
    shownOff = HUD_OFF_UNKNOWN;
}

void HUD_newGame(void)
{
    // La partida anterior acabó con los mensajes fuera (HUD_clear al entrar
    // en el menú). El grupo sigue desde donde lo dejó runOut.
    arenaWant = TRUE;
    holdWant = alertWant = levelWant = FALSE;
    alertLeft = 0;
    levelWaits = 0;
    coinsHide = 0;
    coinsBack = FALSE;
    // Los avisos que esperaban detrás de la tienda siguen aquí.
    achBlocked = FALSE;
}

void HUD_clear(void)
{
    // La primera vez (Menu_init, al arrancar) todavía no hay fila de COINS.
    if (!shownCoinsRow) shownCoinsRow = coinsRow();
    clearMessageRows();

    arenaWant = arenaOn = FALSE;
    holdWant = holdOn = FALSE;
    alertWant = alertOn = FALSE;
    levelWant = levelOn = FALSE;
    levelWaits = 0;
    alertLeft = 0;
    coinsHide = 0;
    coinsBack = FALSE;

    // El aviso de logro se queda (el de "FIRST GAME" sale en el game over):
    // sólo se borra lo pintado, y HUD_menuTick lo vuelve a poner.
    if (achOn != ACH_NONE) drawAchieve(ACH_NONE);
    achOn = ACH_NONE;
}

void HUD_menuTick(void)
{
    // runOut(): fuera de partida el grupo sube a 300 px/s del aparato.
    if (slideY > HUD_SLIDE_TOP)
    {
        slideY -= HUD_SLIDE_OUT;
        if (slideY < HUD_SLIDE_TOP) slideY = HUD_SLIDE_TOP;
    }
    drawGroup();

    // El aviso de logro sigue en el game over, en su fila (la banda va de la
    // 12 a la 19 y la marquesina en la 29). En la tienda y en la lista de
    // logros, no: tapan esa fila (HUD_hideNow lo para).
    achTick();
    if (!achBlocked && (achWant != achOn))
    {
        achOn = achWant;
        drawAchieve(achOn);
    }
}

void HUD_hideNow(void)
{
    // La tienda borra el plano entero y escribe encima: el grupo pasa a estar
    // fuera ya (en MonsterLite seguiría subiendo detrás de la tienda).
    slideY = HUD_SLIDE_TOP;
    shownOff = -HUD_GROUP_ROWS;
    // Y el aviso de logro, parado hasta la partida siguiente (HUD_newGame):
    // la tienda pinta en su fila.
    achBlocked = TRUE;
    achOn = ACH_NONE;
}

void HUD_setArena(bool on) { arenaWant = on; }
void HUD_setHold(bool on)  { holdWant = on; }

void HUD_levelUp(void)
{
    levelWant = TRUE;
    if (levelWaits < HUD_LEVEL_WAITS) levelLeft[levelWaits++] = ML_LEVEL_MSG_F;
}

void HUD_waveAlert(void)
{
    // globales.OLEADA = "!!!"; oleadaT sólo vuelve a 30 cuando se acaba.
    if (alertWant) return;
    alertWant = TRUE;
    alertLeft = ML_ALERT_F;
}

void HUD_tick(bool timeRuns)
{
    if (alertLeft && (--alertLeft == 0)) alertWant = FALSE;

    if (coinsHide && (--coinsHide == 0)) coinsBack = TRUE;

    if (achLeft || g_achQueued) achTick();

    // run(): MoveTowards con Time.deltaTime, que con el tiempo parado es 0.
    if (timeRuns && (slideY < 0))
    {
        slideY += HUD_SLIDE_IN;
        if (slideY > 0) slideY = 0;
    }

    if (timeRuns && levelWaits)
    {
        u16 i, j = 0;

        for (i = 0; i < levelWaits; i++)
        {
            if (--levelLeft[i] == 0) levelWant = FALSE;     // showNewLevel = false
            else levelLeft[j++] = levelLeft[i];
        }
        levelWaits = (u8)j;
    }
}

static NO_INLINE void updateWork(void);

void HUD_update(void)
{
    // Lo normal es que no haya cambiado nada: se mira sin llamar a nada ni
    // abrir marco de pila (cada frame cuenta en el arnés de estrés).
    if ((g_coins == shownCoins) && !coinsBack
        && (arenaWant == arenaOn) && (holdWant == holdOn)
        && (alertWant == alertOn) && (levelWant == levelOn)
        && (g_playerBullets == shownBullets) && (g_score == shownScore)
        && (g_level == shownLevel) && (g_factorLevel == shownFactor)
        && (achWant == achOn)
        && staticDrawn && ((slideY >> 6) == shownOff))
        return;

    updateWork();
}

static NO_INLINE void updateWork(void)
{
#if HUD_SHOW_PERF
    // (132, 52) del room -> (26, 10) -> columna 3, fila 1
    VDP_showFPS(FALSE, 3, 1);
    VDP_showCPULoad(10, 1);
#endif

    drawGroup();

    // El saldo. Al recoger una moneda la línea se va por debajo de la
    // pantalla unos frames (ver HUD_tick y docs/PORT_AUDIT_GML.md).
    if (g_coins != shownCoins)
    {
        if ((shownCoins != 0xFFFFFFFFUL) && (g_coins > shownCoins) && !coinsHide)
        {
            coinsHide = ML_COINS_BUMP_F;
            drawCoins(FALSE);
        }
        shownCoins = g_coins;
        if (!coinsHide) drawCoins(TRUE);
    }

    // Se acabó el salto: vuelve a su sitio con el saldo de ahora.
    if (coinsBack)
    {
        coinsBack = FALSE;
        drawCoins(TRUE);
    }

    if (achWant != achOn)
    {
        achOn = achWant;
        drawAchieve(achOn);
    }

    // Los mensajes. ARENA!, el de nivel y el aviso llevan la sombra de
    // MonsterLite (S/HUD.cs:240, 476, 495). "!!!" no la lleva en MonsterLite
    // (505), pero aquí sí, por decisión del usuario: recolor() copia el glifo
    // de la fuente con su sombra (ver arriba).
    if (arenaWant != arenaOn)
    {
        arenaOn = arenaWant;
        if (arenaOn) Hole_drawTextRow(HUD_ARENA_TEXT, (HUD_COLS - (sizeof(HUD_ARENA_TEXT) - 1)) / 2,
                                        HUD_ROW_ARENA);
        else         Hole_hideTextRow(HUD_ROW_ARENA);
    }

    if (holdWant != holdOn)
    {
        holdOn = holdWant;
        if (holdOn) Hole_drawTextRow(HUD_HOLD_TEXT, (HUD_COLS - (sizeof(HUD_HOLD_TEXT) - 1)) / 2,
                                       HUD_ROW_HOLD);
        else        Hole_hideTextRow(HUD_ROW_HOLD);
    }

    if (alertWant != alertOn)
    {
        alertOn = alertWant;
        drawAlert(alertOn);
    }

    if (levelWant != levelOn)
    {
        levelOn = levelWant;
        if (levelOn) drawLevelMsg();
        else
        {
            Hole_hideTextRow(HUD_ROW_LVMSG);
            Hole_hideTextRow(HUD_ROW_LVMSG + 1);
        }
    }
    else if (levelOn && ((g_level != levelShownLv) || (g_factorLevel != levelShownFactor)))
    {
        drawLevelMsg();
    }
}
