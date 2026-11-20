#include "menu.h"
#include "bg.h"
#include "game.h"
#include "resources.h"
#include "vramspr.h"
#include "hole.h"
#include "audio.h"
#include "hud.h"
#include "weapon.h"
#include "save.h"

#define COLS            (PLAYFIELD_W / 8)
#define ROW_CREDITS     (GML_POS(1200 * 70 / 100) / 8)      // fila 21
#define ROW_MARQUEE     ((PLAYFIELD_H - GML_LEN(20)) / 8)   // fila 29
#define COL_TEXT        (GML_POS(1600 * 25 / 100) / 8)      // WIDTH_OFFSET 0.25 -> col 10
#define WEAPON_GAP      (GML_LEN(128) / 8)                  // 26 px -> 3 tiles
// Game over de MonsterLite (bloque 5, S/gameOverSrc.cs), con la fuente del
// sistema (Q16), dentro de la banda del menú, que ocupa las filas 12-19.
#define GO_ROW_SCORE    12
#define GO_ROW_COINS    13
#define GO_ROW_LEVEL    14
#define GO_ROW_RECORD   15
#define GO_ROW_BEST     16
#define GO_ROW_OPTIONS  18
#define GO_COL          COL_TEXT
#define GO_COL_PLAY     14
#define GO_COL_GEAR     24

// La tienda GEAR (bloque 5, S/StoreMenu.cs:171-618) va en una banda más
// grande, de la fila 3 a la 27.
#define ST_ROW_TOP      3
#define ST_ROW_BOTTOM   27
#define ST_ROW_TITLE    4
#define ST_ROW_ITEMS    6           // dos filas por arma: nombre y precio, descripción
#define ST_ROW_COINS    15
#define ST_ROW_SHELF    17          // los iconos (4 filas) y el cursor debajo
#define ST_ROW_SELECT   22
#define ST_ROW_NEXT     23
#define ST_ROW_AUDIO    25
#define ST_ROW_PLAY     26
#define ST_COL_SOUND    2
#define ST_COL_MUSIC    16

// "TAP TO START" (S/menu.cs) parpadea con "t % 100 < 50", t contado por
// llamada a OnGUI: dos por frame de 60 Hz, o sea 25 frames de Update
// encendido y 25 apagado -> 21 f a 50 Hz.
#define SPLASH_BLINK_F  21
#define HUD_ROW_BOTTOM  2                                   // BULLETS/KILLS (2 filas) y LAST

// Tiles: los agujeros ocupan 64 y el suelo 16 desde TILE_USER_INDEX.
#define WEAPON_TILES     4                                  // 4x4 por icono



// oMenu/Create_0: los textos de marquesina, uno por arma (en el orden de
// inc/weapon.h). Los dos últimos, de las armas de MonsterLite, son sus
// nombres en la tienda (W/GameAssets.cs): "Circles" y "Moire".
static const char *marquee[WPN_COUNT] = {
    " WHITE BLOCK ", " LASER ", " 3 WAY ", " CIRCLE ", " TRIANGLE OF DESTRUCTION ",
    " CIRCLES ", " MOIRE "
};

// oMenu/Draw_0, rama del splash. Literal del original salvo tres cosas:
//   - SPACE y CURSORS, que en Mega Drive no existen, pasan a START y D-PAD
//     (sin créditos desde el bloque 5, Q13: START empieza);
//   - la firma dice SCREEN IMPLOSION, no REDEFINETECLAS: el GML lleva el
//     nombre viejo y el autor ha confirmado cuál es el bueno.
// El "AND AND" es del original y se deja: es su voz.
static const char *splashText =
    " PRESS START ... D-PAD TO MOVE ... SHOOT ALL THE BUGS AND AND SNAKES ..."
    "SURVIVE FOR GLORY... GAME BY SCREEN IMPLOSION 2012 ";

static s16  barHeight;      // t del original, en píxeles
static s16  scroll;         // scroll de la marquesina
static u16  marqueePos;     // primer carácter visible
static bool marqueeDirty;   // hay que escribir la línea aunque no haya avanzado
static u16  marqueeVis;     // columnas del texto ya en pantalla (scroll > 0)
static u16  prevPad;
static bool inMenu;
static bool staticDrawn;    // logo/iconos ya pintados: son tiles, no hace
                            // falta repetirlos cada frame como el original
static u16  shownWeapon;
static s16  shownBar;
static u16  splashBlink;    // parpadeo de PRESS START
static bool splashShown;

// La cuenta del game over (ver showStep).
static u32  counterK;       // lo que enseña SCORE
static u32  counterC;       // lo que enseña COINS COLLECTED
static u16  scoreRunners;   // corrutinas Show en el bucle de SCORE
static u16  coinWait[24];   // corrutinas en el de COINS, por el frame (mod 24) en que despiertan
static u16  updTick;        // frames de Update (60 Hz) desde que se abrió
static u8   updAcc;         // 6 de Update por cada 5 de 50 Hz
static u32  shownK, shownC;
static bool goDrawn;        // las líneas fijas del game over ya están
static u8   goChoice;       // 0 PLAY, 1 GEAR
static u8   shownChoice;
static bool shownPlay;

// El fogonazo rojo del fondo (ver flashStep).
static bool flashOn;
static s16  flashCol[16][3];    // el color de ahora, canal a canal, en 1/16 de nivel

// La tienda.
static bool inStore;
static u8   stItem;         // 0-3 las armas de pago, 4 estante, 5 sonido, 6 música, 7 PLAY
static bool stDirty;

static s16  bandTop = -1;   // filas del plano B que ha pisado la banda
static s16  bandBottom = -1;

static void clearArea(void)
{
    u16 r;

    // oInGameHUD sólo se dibuja "if !instance_exists(oMenu)", así que al
    // entrar en el menú se limpia la franja del HUD. El resto del plano A
    // ya está limpio de texto (lo deja así Menu_leave) y tiene los agujeros,
    // que en el original siguen viéndose detrás del menú: no se tocan.
    for (r = 0; r <= HUD_ROW_BOTTOM; r++) VDP_clearTextLine(r);
    VDP_clearTextLine(ROW_MARQUEE);
}

// Escribe 'text' en la fila 'row' del plano A sobre lo que haya de agujeros:
// las celdas sin letra se quedan con el agujero, no con un hueco, y las
// letras que caen encima de un agujero se componen con él (ver
// Hole_textRow), por si la fila cruza el dibujo de algún agujero. Una sola escritura de fila, así que no hay frame en que
// la fila salga a medias.
static void drawRowOverHoles(const char *text, u16 col, u16 row)
{
    Hole_drawTextRow(text, col, row);
}

// La marquesina va en la fila 29 del plano A, encima del fondo, y se mueve
// con el scroll horizontal POR FILA DE TILES: así sólo se desplaza su fila y
// el fondo (plano B) y los agujeros se quedan quietos. En partida el scroll
// vuelve a ser de plano entero, que es lo que usa la sacudida (src/shake.c).
static s16 rowScroll[PLAYFIELD_H / 8];

static void setMenuScrollMode(bool menu)
{
    u16 r;

    // Toda la tabla a cero ANTES de cambiar de modo: en ese momento no hay
    // sacudida (Shake_stop), así que ningún modo ve un valor distinto de 0
    // y el cambio, aunque caiga a mitad de frame, no se nota.
    for (r = 0; r < (PLAYFIELD_H / 8); r++) rowScroll[r] = 0;
    VDP_setHorizontalScrollTile(BG_A, 0, rowScroll, PLAYFIELD_H / 8, CPU);
    VDP_setHorizontalScrollTile(BG_B, 0, rowScroll, PLAYFIELD_H / 8, CPU);
    VDP_setScrollingMode(menu ? HSCROLL_TILE : HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
}

// Índices repartidos al cargar, no escritos a mano (ver inc/vramspr.h).
static u16 weaponTileBase;
static u16 weaponMlBase;
static u16 logoTileBase;
static u16 barTile_;

// Los tres tilesets del menú van a la VRAM que SGDK no reparte (ver
// VTile_allocSpare): así no pisan el fondo y se suben una sola vez.
void Menu_loadTiles(void)
{
    if (!weaponTileBase)
    {
        weaponTileBase = VTile_allocSpare(weaponTiles.numTile);
        barTile_       = VTile_allocSpare(barTile.numTile);
        logoTileBase   = VTile_allocSpare(logoTiles.numTile);
        // Los dos iconos de MonsterLite (32 tiles) ya no caben en la VRAM
        // sobrante: van a la normal.
        weaponMlBase   = VTile_allocSpare(weaponMlTiles.numTile);
        if (!weaponMlBase) weaponMlBase = VTile_alloc(weaponMlTiles.numTile);
    }

    VDP_loadTileSet(&weaponTiles, weaponTileBase, DMA);
    VDP_loadTileSet(&weaponMlTiles, weaponMlBase, DMA);
    VDP_loadTileSet(&logoTiles, logoTileBase, DMA);
    VDP_loadTileSet(&barTile, barTile_, DMA);
}

void Menu_init(void)
{
    Menu_loadTiles();

    barHeight = 0;
    scroll = PLAYFIELD_W;
    splashBlink = 0;
    splashShown = FALSE;
    marqueePos = 0;
    marqueeDirty = TRUE;
    prevPad = 0;
    inMenu = FALSE;
    staticDrawn = FALSE;
    shownWeapon = 0xFFFF;
    shownBar = -1;
    clearArea();
    setMenuScrollMode(TRUE);
    Hole_holdDraw(TRUE);
}

void Menu_enter(void)
{
    // menu(): el récord se guarda al entrar (oMenu/Create_0)
    if (g_maxKills <= g_kills) g_maxKills = g_kills;

    // Lo que esté pulsado al entrar no cuenta como pulsación nueva (el GML
    // usa keyboard_check_pressed). Sin esto, el B que se mantiene en partida
    // para que corra el tiempo (Q8) contaba como pulsado en el primer frame.
    prevPad = JOY_readJoypad(JOY_1);

    barHeight = 0;
    inMenu = TRUE;
    inStore = FALSE;
    staticDrawn = FALSE;

    // menu(): audio_play_sound(menuSong, 1, false)
    Audio_menuMusic(TRUE);
    shownWeapon = 0xFFFF;
    shownBar = -1;

    // El game over (bloque 5): la cuenta empieza de cero.
    {
        u16 i;

        counterK = counterC = 0;
        scoreRunners = 0;
        for (i = 0; i < 24; i++) coinWait[i] = 0;
        updTick = 0;
        updAcc = 0;
        shownK = shownC = 0xFFFFFFFFUL;
        goDrawn = FALSE;
        goChoice = 0;
        shownChoice = 0xFF;
        shownPlay = FALSE;
    }

    // gameOverSrc.Start: con más de 10 puntos el fondo se pone rojo puro
    // (ver flashStep).
    flashOn = (g_score > 10);
    if (flashOn)
    {
        const u16 *base = Bg_palette();
        u16 i;
        u16 red[16];

        for (i = 0; i < 16; i++)
        {
            red[i] = (i == 0) ? base[0] : 0x000E;
            flashCol[i][0] = 14 * 16;       // rojo
            flashCol[i][1] = 0;
            flashCol[i][2] = 0;
        }
        PAL_setColors(BG_PAL * 16, red, 16, DMA_QUEUE);
    }

    clearArea();

    // El fondo y los agujeros se quedan: en el original el menú se dibuja
    // encima del room, que sigue entero detrás. Sólo la banda pisa las filas
    // del fondo que tapa, y Menu_leave las repinta.
    setMenuScrollMode(TRUE);
    Hole_holdDraw(TRUE);
    scroll = 0;
    marqueeDirty = TRUE;
}

void Menu_leave(void)
{
    u16 r;

    Audio_menuMusic(FALSE);
    inMenu = FALSE;
    inStore = FALSE;
    staticDrawn = FALSE;

    // Si el fogonazo rojo no había acabado, el fondo vuelve a su paleta.
    if (flashOn)
    {
        PAL_setColors(BG_PAL * 16, Bg_palette(), 16, DMA_QUEUE);
        flashOn = FALSE;
    }

    // La marquesina sube por la cola de DMA (ver drawMarquee). Si en la cola
    // queda la del último frame del menú, se escribiría en el vblank, DESPUÉS
    // de limpiar el plano, y el texto se quedaba pintado encima del campo en
    // la partida siguiente. Se vacía ya, y lo que se borra a continuación
    // queda borrado.
    DMA_flushQueue();

    // Fuera el texto del menú, fila a fila y dejando los agujeros que pasan
    // por cada una: una sola escritura por fila, sin un frame en que el
    // agujero falte. Y el fondo, sólo en las filas que pisó la banda.
    for (r = 0; r < (PLAYFIELD_H / 8); r++)
    {
        u16 row[64];

        Hole_rowTiles(r, row, planeWidth);
        VDP_setTileMapDataRow(BG_A, row, r, 0, planeWidth, CPU);
    }

    Bg_setCover(-1, -1);
    if (bandTop >= 0) Bg_drawRows(bandTop, bandBottom);
    bandTop = bandBottom = -1;

    setMenuScrollMode(FALSE);
    Hole_holdDraw(FALSE);
}

static void fillBand(s16 top, s16 bottom);

// La barra gris de draw_line_width_color: una línea horizontal de grosor t
// centrada en FIRST_ROW_HEIGHT, que aquí son filas de tiles.
static void drawBar(void)
{
    const s16 half = barHeight / 2;

    if (barHeight == shownBar) return;
    shownBar = barHeight;

    fillBand((GML_POS(1200 * 53 / 100) - half) / 8, (GML_POS(1200 * 53 / 100) + half) / 8);
}

// Pinta la banda en las filas top..bottom del plano B.
static void fillBand(s16 top, s16 bottom)
{
    s16 r, c;

    // La barra va al PLANO B, no al A, y esto es lo que la arregla.
    //
    // El original pinta una sola línea gruesa (draw_line_width_color) y
    // escribe el texto ENCIMA. Aquí el texto va en tiles del plano A, y en un
    // plano el color 0 no es transparente: pinta el fondo. O sea que cada
    // letra abría un agujero oscuro en la barra, y como además había que
    // saltarse las filas del texto para no taparlo, lo que salía eran franjas
    // grises en vez de una banda.
    //
    // Con la barra en el plano B, que está DETRÁS, el hueco de cada letra
    // deja ver la barra y queda la banda entera con el texto encima, como en
    // el original. La banda pisa las filas del fondo del campo que tapa (que
    // también vive en el plano B); Game_start lo repinta al salir.
    const s16 oldTop = bandTop;
    const s16 oldBottom = bandBottom;
    u16 line[64];

    if (top < 0) top = 0;
    if (bottom >= (PLAYFIELD_H / 8)) bottom = (PLAYFIELD_H / 8) - 1;
    if ((bandTop < 0) || (top < bandTop)) bandTop = top;
    if (bottom > bandBottom) bandBottom = bottom;
    // Un cambio de fondo con el menú abierto no tiene que comerse la banda.
    Bg_setCover(bandTop, bandBottom);

    // Prioridad alta: en el original oMenu va a depth -100, por delante de
    // los objetos del room, y la banda TAPA los paquetes y las réplicas que
    // siguen en el suelo durante el menú. Un plano de prioridad alta queda
    // por encima de los sprites (que van en baja, ver VSpr_add) y el texto,
    // en el plano A alto, encima. Las 64 columnas del plano, no sólo las 40
    // de la pantalla, por si el plano llega desplazado.
    for (c = 0; c < planeWidth; c++) line[c] = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, barTile_);

    // Sólo las filas que no estaban ya pintadas, y cada una de una sola
    // escritura: mientras la banda crecía se repintaban todas, celda a celda
    // (hasta 512 escrituras al VDP por frame), y el game over perdía frames.
    for (r = top; r <= bottom; r++)
    {
        if ((oldTop >= 0) && (r >= oldTop) && (r <= oldBottom)) continue;
        VDP_setTileMapDataRow(BG_B, line, r, 0, planeWidth, CPU);
    }
}

// Marquesina: el texto avanza 2 px por frame y se redibuja al pasar un tile.
// La marquesina va en el plano B y se mueve con el scroll por hardware: el
// texto sólo se reescribe cuando avanza un tile entero, en vez de redibujar
// los 40 caracteres cada frame.
static void drawMarquee(const char *text, u16 len)
{
    scroll -= MENU_SCROLL_SPEED;

    if (scroll <= -8)
    {
        scroll += 8;
        marqueePos = (marqueePos + 1) % len;
        marqueeDirty = TRUE;
    }

    // La primera línea se escribe ya al abrir: en el splash el scroll empieza
    // en PLAYFIELD_W y el texto entra deslizándose por la derecha. Antes no
    // se escribía nada hasta que el scroll llegaba a -8, y la fila se quedaba
    // vacía los primeros 164 frames.
    //
    // Mientras el scroll es positivo la línea va recortada a lo que ya ha
    // entrado en pantalla: el plano mide 64 columnas y da la vuelta, así que
    // las últimas columnas del texto asomarían por la izquierda.
    {
        const u16 vis = (scroll > 0) ? (u16)(((PLAYFIELD_W - scroll) / 8) + 1) : (COLS + 1);

        if (vis != marqueeVis)
        {
            marqueeVis = vis;
            marqueeDirty = TRUE;
        }
    }

    if (marqueeDirty)
    {
        char line[COLS + 2];
        u16 i;

        marqueeDirty = FALSE;

        for (i = 0; i <= COLS; i++)
            line[i] = (i < marqueeVis) ? text[(marqueePos + i) % len] : ' ';
        line[COLS + 1] = 0;

        // Por la cola de DMA, en el MISMO vblank que el scroll de abajo.
        // Con VDP_drawText el texto se reescribía en el acto, a mitad de
        // frame, y el scroll (que va en el vblank) no lo compensaba hasta el
        // frame siguiente: durante un frame el texto saltaba 8 px adelante y
        // volvía. Era el temblor de la marquesina.
        VDP_drawTextEx(BG_A, line,
                       TILE_ATTR(VDP_getTextPalette(), VDP_getTextPriority(), FALSE, FALSE),
                       0, ROW_MARQUEE, DMA_QUEUE);
    }

    // Por la cola de DMA también, para que caiga en el mismo vblank que el
    // texto: escribirlo a mitad de frame partía la fila (ver src/shake.c).
    rowScroll[ROW_MARQUEE] = scroll;
    VDP_setHorizontalScrollTile(BG_A, ROW_MARQUEE, &rowScroll[ROW_MARQUEE], 1, DMA_QUEUE);
}

// El estante de armas de la tienda (antes, el selector del interludio del
// GML): los iconos de las que se pueden elegir en la fila 'row' y el cursor
// debajo del arma equipada.
static void drawWeapons(s16 row)
{
    u16 i, tx, ty;

    if (g_weaponIndex == shownWeapon) return;
    shownWeapon = g_weaponIndex;

    for (i = 0; i < WPN_COUNT; i++)
    {
        const s16 col = COL_TEXT + (i * WEAPON_GAP);
        // weapons.png son 5 iconos en fila y weapons_ml.png 2: la fila ty
        // del icono k empieza en ty * (iconos * 4) + k * 4.
        const bool ml = (i >= 5);
        const u16 base = ml ? weaponMlBase : weaponTileBase;
        const u16 perRow = (ml ? 2 : 5) * WEAPON_TILES;
        const u16 k = ml ? (i - 5) : i;

        // Las que no se pueden elegir no se pintan (PIVOT y PARA hasta su
        // récord). Como sólo se añaden, nunca hay que borrar un icono.
        if (!(g_weaponsAvail & (1 << i))) continue;

        for (ty = 0; ty < WEAPON_TILES; ty++)
        {
            for (tx = 0; tx < WEAPON_TILES; tx++)
            {
                const s16 cx = col + tx;

                if (cx >= COLS) continue;

                VDP_setTileMapXY(BG_A,
                                 TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE,
                                                base + (ty * perRow)
                                                + (k * WEAPON_TILES) + tx),
                                 cx, row + ty);
            }
        }
    }

    // El arma elegida se marca debajo (el original le pinta sprite191 encima).
    VDP_clearTextLine(row + WEAPON_TILES);
    VDP_drawText("^", COL_TEXT + (g_weaponIndex * WEAPON_GAP) + 1,
                 row + WEAPON_TILES);
}

// El logo es el sprite de oMenu (sprite176, 256x256 con el origen en el
// centro) dibujado en la posición de la instancia, que en el room está en
// (832, 608): el centro. Estaba pintado arriba del todo.
#define LOGO_TILES   7
#define LOGO_CX      GML_POS(832)
#define LOGO_CY      GML_POS(608)

// El logo va en el plano A, en las celdas 17-23 x 11-17; las que caen sobre
// dibujo de algún agujero se componen con él. Una celda sólo tiene un tile: con el del
// logo tal cual, donde el logo es transparente se veía el plano B y los seis
// agujeros salían cortados en rectángulo. En el original son sprites, que se
// componen píxel a píxel.
//
// Así que cada celda del logo con agujero debajo se compone (agujeros debajo,
// logo encima: comparten PAL2) y se sube ENCIMA del propio tile del logo: el
// logo sólo se ve en el splash, sus tiles no los usa nadie más y no hace
// falta ni un tile de VRAM nuevo (no queda VRAM sobrante, ver
// VTile_allocSpare). Se compone siempre desde el logo de la ROM, así que
// pintarlo otra vez da lo mismo. La celda entera pasa a prioridad alta, la
// parte de agujero incluida: en el splash no hay sprites encima.
static void drawLogo(void)
{
    u16 tx, ty;
    const s16 col = (LOGO_CX - ((LOGO_TILES * 8) / 2)) / 8;
    const s16 row = (LOGO_CY - ((LOGO_TILES * 8) / 2)) / 8;

    if (staticDrawn) return;
    staticDrawn = TRUE;

    for (ty = 0; ty < LOGO_TILES; ty++)
    {
        for (tx = 0; tx < LOGO_TILES; tx++)
        {
            const u16 t = (ty * LOGO_TILES) + tx;
            u32 cell[8];

            if (Hole_cellPixels(col + tx, row + ty, cell))
            {
                const u32 *logo = logoTiles.tiles + (t * 8);
                u16 w;

                for (w = 0; w < 8; w++)
                {
                    const u32 v = logo[w];
                    u32 m = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

                    m *= 0xF;
                    cell[w] = (cell[w] & ~m) | v;
                }
                VDP_loadTileData(cell, logoTileBase + t, 1, CPU);
            }

            VDP_setTileMapXY(BG_A,
                             TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, logoTileBase + t),
                             col + tx, row + ty);
        }
    }
}

// ---------------------------------------------------------------------------
// La cuenta del game over (S/gameOverSrc.cs, Q15: el mismo ritmo acelerado)
//
// gameOverSrc.Update() lanza en CADA frame una corrutina Show nueva y no
// para las anteriores. Cada una, por su cuenta:
//
//     while (counterK < kills)        { counterK += 1; ding; espera 1 frame }
//     while (counterC < earnedCoins)  { counterC += 1; espera 0,4 s = 24 frames }
//
// Aquí no hay corrutinas: se cuenta cuántas hay en cada bucle. Las del de
// SCORE despiertan todos los frames; las del de COINS, cada 24, así que se
// agrupan por el frame (módulo 24) en que despiertan. Un frame de Update:
//   1. despiertan las de COINS de este grupo: cada una suma 1 mientras
//      quepa; las que ya no caben, acaban;
//   2. nace la corrutina nueva y, con las del bucle de SCORE, cada una suma 1
//      mientras quepa; las que ya no caben pasan al de COINS y dan su primer
//      paso en el acto (y despertarán dentro de 24 frames).
// Simulado contra la tabla de docs/MONSTERLITE_GAP.md §2.8: 100/60 acaban en
// los frames de Update 14/42; 1000/300, 45/117; 5000/500, 100/173; 20000/3000,
// 200/417. Igual.
//
// El "ding" sonaba una vez por corrutina, hasta ~200 superpuestos; Q15: uno
// por frame en que sube SCORE (el tick del PSG, Audio_tick). Lo de no contar
// mientras la cámara se sacude (!globales.shaking) no se porta: en el game
// over del port no hay sacudida.
// ---------------------------------------------------------------------------
static bool showStep(void)
{
    const u32 kills = g_score;
    const u32 earned = g_coinsEarned;
    const u16 b = updTick % 24;
    u32 add, leaving;

    updTick++;

    add = coinWait[b];
    if (add > (earned - counterC)) add = earned - counterC;
    counterC += add;
    coinWait[b] = (u16)add;

    scoreRunners++;
    add = scoreRunners;
    if (add > (kills - counterK)) add = kills - counterK;
    counterK += add;
    leaving = scoreRunners - add;
    scoreRunners = (u16)add;

    add = leaving;
    if (add > (earned - counterC)) add = earned - counterC;
    counterC += add;
    coinWait[b] += (u16)add;

    return (scoreRunners != 0);
}

// El fogonazo rojo: cada corrutina nueva acerca el color del fondo al suyo
// con Lerp(actual, base, deltaTime · counterK), o sea una fracción
// counterK / 60 por frame de Update. Con counterK >= 60 (unos 11 frames) ya
// es el color de base: un fogonazo de ~0,2 s. Q16: el fondo del port es la
// imagen de ruinas, así que el rojo puro la tapa entera y se le vuelve a dar
// su paleta.
static void flashStep(void)
{
    const u16 *base = Bg_palette();
    // f / 60 en 1/1024: una multiplicación de 16 bits por canal en vez de
    // una división de 32.
    const s16 f1024 = (s16)((((counterK > 60) ? 60 : counterK) * 1024) / 60);
    u16 pal[16];
    bool done = TRUE;
    u16 i, k;

    for (i = 0; i < 16; i++)
    {
        for (k = 0; k < 3; k++)
        {
            // Canales del VDP en niveles de 0 a 14 (pares), aquí x16.
            const s16 target = (s16)(((base[i] >> (4 * k)) & 0xE) * 16);
            s16 *c = &flashCol[i][k];

            const s16 d = target - *c;
            s16 step = (s16)(((s32)d * f1024) >> 10);

            // Sin pasos de 0 que dejen el color a un pelo del suyo.
            if ((step == 0) && (d != 0) && (f1024 > 0)) step = (d > 0) ? 1 : -1;
            *c += step;
            if (*c != target) done = FALSE;
        }
        pal[i] = (u16)((((flashCol[i][2] / 16) & 0xE) << 8) |
                       (((flashCol[i][1] / 16) & 0xE) << 4) |
                        ((flashCol[i][0] / 16) & 0xE));
    }
    pal[0] = base[0];

    PAL_setColors(BG_PAL * 16, done ? base : pal, 16, DMA_QUEUE);
    if (done) flashOn = FALSE;
}

// Un u32 en decimal, rápido. El sprintf("%lu") de SGDK divide en 32 bits por
// software para cada cifra, y con dos contadores cambiando cada frame el game
// over perdía frames (medido: la cuenta iba al 37 % de su ritmo). Aquí una
// división de 32 entre 16 del 68000 (divu) parte el número en dos mitades de
// cuatro cifras, y cada mitad se escribe con divisiones de 16 bits. Vale
// hasta 655 359 999. Devuelve el final de la cadena.
static char *fmtU32(char *dst, u32 v)
{
    char tmp[12];
    u16 n = 0;
    u16 hi, lo;

    if (v > 655359999UL) v = 655359999UL;
    hi = (u16)(v / 10000);      // divu: cociente de 16 bits
    lo = (u16)(v - ((u32)hi * 10000));

    do { tmp[n++] = '0' + (lo % 10); lo /= 10; } while (lo && (n < 4));
    if (hi)
    {
        while (n < 4) tmp[n++] = '0';
        do { tmp[n++] = '0' + (hi % 10); hi /= 10; } while (hi);
    }
    while (n) *dst++ = tmp[--n];
    *dst = 0;
    return dst;
}

// Etiqueta seguida de un número.
static void labelNum(char *dst, const char *label, u32 v)
{
    while (*label) *dst++ = *label++;
    fmtU32(dst, v);
}

// Escribe una línea de texto en la fila 'row' del plano A, en prioridad
// alta, con espacios hasta 'width' columnas para borrar lo que hubiera.
static void drawLine(const char *text, u16 col, u16 row, u16 width)
{
    char line[COLS + 1];
    u16 n = strlen(text);
    u16 i;

    if (width > COLS) width = COLS;
    for (i = 0; i < width; i++) line[i] = (i < n) ? text[i] : ' ';
    line[width] = 0;
    VDP_drawText(line, col, row);
}

static void drawGameOver(void)
{
    char buf[COLS + 1];

    // Las líneas fijas: LEVEL, NEW RECORD! y BEST SCORE (gameOverSrc,
    // 200-264). "LAST SCORE" no se dibuja en el original (Q15).
    if (!goDrawn)
    {
        goDrawn = TRUE;
        sprintf(buf, "LEVEL: %u!", g_level);
        drawLine(buf, GO_COL, GO_ROW_LEVEL, 20);
        drawLine((g_score > g_bestAtStart) ? "NEW RECORD!" : "", GO_COL, GO_ROW_RECORD, 20);
        labelNum(buf, "BEST SCORE: ", g_lastScore);
        drawLine(buf, GO_COL, GO_ROW_BEST, 24);
    }

    if (counterK != shownK)
    {
        shownK = counterK;
        labelNum(buf, "SCORE: ", counterK);
        drawLine(buf, GO_COL, GO_ROW_SCORE, 24);
    }

    if (counterC != shownC)
    {
        shownC = counterC;
        labelNum(buf, "COINS COLLECTED: ", counterC);
        drawLine(buf, GO_COL, GO_ROW_COINS, 26);
    }

    // PLAY sólo aparece cuando SCORE ha terminado; GEAR, siempre.
    {
        const bool playOn = (counterK >= g_score);

        if ((playOn != shownPlay) || (goChoice != shownChoice))
        {
            shownPlay = playOn;
            shownChoice = goChoice;
            drawLine(playOn ? ((goChoice == 0) ? ">PLAY" : " PLAY") : "", GO_COL_PLAY, GO_ROW_OPTIONS, 6);
            drawLine((goChoice == 1) ? ">GEAR" : " GEAR", GO_COL_GEAR, GO_ROW_OPTIONS, 6);
        }
    }
}

// ---------------------------------------------------------------------------
// La tienda GEAR (S/StoreMenu.cs, versión gold)
//
// Una fila por arma de pago con su nombre, su descripción y el precio
// ("<n> COINS") o "AVAILABLE!" si ya es tuya; "YOUR COINS"; el estante con
// las armas que se pueden elegir y "SELECT YOUR GEAR: <arma>"; los
// interruptores de sonido y música, y PLAY. Comprar descuenta el saldo y
// EQUIPA el arma (EventHandler, setMeOnly). Con el pad: arriba y abajo
// recorren las opciones, A o START eligen, y en el estante izquierda y
// derecha cambian de arma.
//
// Se descartan el botón del leaderboard y el GEAR, que no hace nada. Los
// iconos de bomba y rayos de la tienda son decorativos y no se portan.
// ---------------------------------------------------------------------------
#define ST_ITEMS        8
#define ST_ITEM_SHELF   4
#define ST_ITEM_SOUND   5
#define ST_ITEM_MUSIC   6
#define ST_ITEM_PLAY    7

// W/GameAssets.cs:133-155: nombre, descripción y precio (Q14), en el orden
// de la tienda.
static const u8   storeWeapon[4] = { WPN_LASER, WPN_TWAY, WPN_CIRCULAR, WPN_MOIRE };
static const char *storeName[4]  = { "Laser", "Tway", "Circles", "Moire" };
static const char *storeDesc[4]  = { "fries it all", "Shoots in 3 ways",
                                     "Intelligent energy balls", "High frecuency laser" };
static const u16  storePrice[4]  = { 6000, 12000, 24000, 48000 };

// Nombres para "SELECT YOUR GEAR", en el orden de inc/weapon.h.
static const char *gearName[WPN_COUNT] = {
    "WBLOCK", "LASER", "TWAY", "PIVOT", "PARA", "CIRCULAR", "MOIRE"
};

static void storeOpen(void)
{
    s16 r;

    inStore = TRUE;
    stItem = 0;
    stDirty = TRUE;

    // Fuera el game over (sus filas vuelven a tener sólo agujeros, que la
    // banda tapa) y la banda grande.
    for (r = 0; r < (PLAYFIELD_H / 8); r++)
    {
        if (r == ROW_MARQUEE) continue;
        Hole_clearTextRow(r);
    }
    fillBand(ST_ROW_TOP, ST_ROW_BOTTOM);
    shownWeapon = 0xFFFF;
}

static void drawStore(void)
{
    char buf[COLS + 1];
    u16 i;

    drawWeapons(ST_ROW_SHELF);

    if (!stDirty) return;
    stDirty = FALSE;

    drawLine("GEAR", (COLS - 4) / 2, ST_ROW_TITLE, 4);

    for (i = 0; i < 4; i++)
    {
        const bool owned = (g_weaponsBought & (1 << storeWeapon[i])) != 0;
        const u16 row = ST_ROW_ITEMS + (2 * i);

        if (owned) sprintf(buf, "%c%-9s AVAILABLE!", (stItem == i) ? '>' : ' ', storeName[i]);
        else       sprintf(buf, "%c%-9s %u COINS", (stItem == i) ? '>' : ' ', storeName[i], storePrice[i]);
        drawLine(buf, 1, row, COLS - 2);
        drawLine(storeDesc[i], 3, row + 1, COLS - 4);
    }

    labelNum(buf, "YOUR COINS: ", g_coins);
    drawLine(buf, 2, ST_ROW_COINS, COLS - 4);

    sprintf(buf, "%cSELECT YOUR GEAR: %s", (stItem == ST_ITEM_SHELF) ? '>' : ' ',
            gearName[(g_weaponIndex < WPN_COUNT) ? g_weaponIndex : 0]);
    drawLine(buf, 1, ST_ROW_SELECT, COLS - 2);

    // PIVOT y PARA siguen yendo por récord de bajas (GML).
    if (g_nextWeapon == NEXT_WEAPON_DONE) buf[0] = 0;
    else sprintf(buf, "NEXT GEAR AT %lu KILLS", g_nextWeapon + 1);
    drawLine(buf, 2, ST_ROW_NEXT, COLS - 4);

    sprintf(buf, "%cSOUND %s", (stItem == ST_ITEM_SOUND) ? '>' : ' ', g_soundOn ? "ON " : "OFF");
    drawLine(buf, ST_COL_SOUND - 1, ST_ROW_AUDIO, 12);
    sprintf(buf, "%cMUSIC %s", (stItem == ST_ITEM_MUSIC) ? '>' : ' ', g_musicOn ? "ON " : "OFF");
    drawLine(buf, ST_COL_MUSIC - 1, ST_ROW_AUDIO, 12);

    drawLine((stItem == ST_ITEM_PLAY) ? ">PLAY" : " PLAY", (COLS - 5) / 2, ST_ROW_PLAY, 5);
}

// Una pulsación en la tienda. TRUE si hay que empezar la partida.
static bool storeInput(u16 pressed)
{
    if (pressed & BUTTON_UP)   { stItem = (stItem + ST_ITEMS - 1) % ST_ITEMS; stDirty = TRUE; }
    if (pressed & BUTTON_DOWN) { stItem = (stItem + 1) % ST_ITEMS; stDirty = TRUE; }

    if (stItem == ST_ITEM_SHELF)
    {
        // El estante: izquierda y derecha cambian el arma equipada entre las
        // que se pueden elegir (setMeOnly), y anuncian su nombre.
        if (pressed & BUTTON_LEFT)
        {
            s16 w = (s16)g_weaponIndex - 1;

            while ((w >= 0) && !(g_weaponsAvail & (1 << w))) w--;
            if (w >= 0) { g_weaponIndex = w; Audio_speech(g_weaponIndex); stDirty = TRUE; }
        }
        if (pressed & BUTTON_RIGHT)
        {
            u16 w = g_weaponIndex + 1;

            while ((w < WPN_COUNT) && !(g_weaponsAvail & (1 << w))) w++;
            if (w < WPN_COUNT) { g_weaponIndex = w; Audio_speech(g_weaponIndex); stDirty = TRUE; }
        }
    }

    if (!(pressed & (BUTTON_A | BUTTON_START | BUTTON_C))) return FALSE;

    stDirty = TRUE;

    if (stItem < 4)
    {
        const u8 w = storeWeapon[stItem];

        if (!(g_weaponsBought & (1 << w)))
        {
            // PurchaseWithVirtualItem: sin saldo, no pasa nada.
            if (g_coins < storePrice[stItem]) return FALSE;
            g_coins -= storePrice[stItem];
            g_weaponsBought |= (u16)(1 << w);
            Game_checkWeapons();
            Audio_sfx(SFX_POWUP);
        }
        // Comprada (o recién comprada): se equipa, y se guarda (Soomla guarda
        // la compra en el acto).
        g_weaponIndex = w;
        Audio_speech(g_weaponIndex);
        Save_write();
        return FALSE;
    }

    if (stItem == ST_ITEM_SOUND) { g_soundOn = !g_soundOn; return FALSE; }

    if (stItem == ST_ITEM_MUSIC)
    {
        g_musicOn = !g_musicOn;
        Audio_menuMusic(g_musicOn);
        return FALSE;
    }

    if (stItem != ST_ITEM_PLAY) return FALSE;

    // Al salir de la tienda se guarda el arma elegida y el sonido y la
    // música. En MonsterLite eso esperaba al game over siguiente (setData);
    // aquí no se pierde si se apaga a mitad de partida.
    Save_write();
    return TRUE;
}

bool Menu_update(u16 pad, bool firstTime)
{
    const u16 pressed = pad & ~prevPad;

    prevPad = pad;
    (void)firstTime;

    if (inStore)
    {
        drawStore();
        drawMarquee(marquee[(g_weaponIndex < WPN_COUNT) ? g_weaponIndex : 0],
                    strlen(marquee[(g_weaponIndex < WPN_COUNT) ? g_weaponIndex : 0]));
        return storeInput(pressed);
    }

    if (inMenu)
    {
        // t += STEP hasta FIRST_STEP
        if (barHeight < MENU_FIRST_STEP) barHeight += MENU_STEP;
        drawBar();

        // La cuenta va a 60 Hz como en MonsterLite: 6 frames de Update por
        // cada 5 de los nuestros.
        {
            u16 steps = 1;
            bool dinged = FALSE;

            if (++updAcc >= 5) { updAcc = 0; steps = 2; }

            while (steps--)
            {
                const u32 before = counterK;

                showStep();
                if (counterK != before) dinged = TRUE;
                if (flashOn) flashStep();
            }
            if (dinged) Audio_tick();
        }

        drawGameOver();
        drawMarquee(marquee[(g_weaponIndex < WPN_COUNT) ? g_weaponIndex : 0],
                    strlen(marquee[(g_weaponIndex < WPN_COUNT) ? g_weaponIndex : 0]));

        if (pressed & (BUTTON_LEFT | BUTTON_RIGHT)) goChoice ^= 1;

        if (pressed & (BUTTON_A | BUTTON_START | BUTTON_C))
        {
            if (goChoice == 1)
            {
                storeOpen();
                return FALSE;
            }
            // PLAY sólo cuando la cuenta de SCORE ha terminado.
            if (counterK >= g_score) return TRUE;
        }
        return FALSE;
    }

    drawLogo();
    drawMarquee(splashText, strlen(splashText));

    // "PRESS START" parpadeando donde el GML ponía los créditos (Q13).
    {
        const bool on = (splashBlink < SPLASH_BLINK_F);

        if (++splashBlink >= (2 * SPLASH_BLINK_F)) splashBlink = 0;
        if (on != splashShown)
        {
            splashShown = on;
            if (on) drawRowOverHoles("PRESS START", (COLS - 11) / 2, ROW_CREDITS);
            else    Hole_clearTextRow(ROW_CREDITS);
        }
    }

    // Selector de fondo PROVISIONAL (no está en el original, que no tiene
    // fondos): arriba y abajo recorren los fondos de la ROM.
    if (BG_COUNT > 1)
    {
        if (pressed & BUTTON_UP)   Bg_select(Bg_current() + BG_COUNT - 1);
        if (pressed & BUTTON_DOWN) Bg_select(Bg_current() + 1);
    }

    // Q13: sin créditos, START empieza la partida. A y C también, como en el
    // resto del menú (decisión del usuario, 2026-11-16).
    return (pressed & (BUTTON_A | BUTTON_START | BUTTON_C)) != 0;
}
