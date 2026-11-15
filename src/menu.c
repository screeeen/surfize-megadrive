#include "menu.h"
#include "bg.h"
#include "game.h"
#include "resources.h"
#include "vramspr.h"
#include "hole.h"
#include "audio.h"
#include "hud.h"

#define COLS            (PLAYFIELD_W / 8)
#define ROW_CREDITS     (GML_POS(1200 * 70 / 100) / 8)      // fila 21
#define ROW_WEAPONS     (GML_POS(1200 * 80 / 100) / 8)      // OFFSET_WEAPON_FRAME -> fila 24
#define ROW_MARQUEE     ((PLAYFIELD_H - GML_LEN(20)) / 8)   // fila 29
#define COL_TEXT        (GML_POS(1600 * 25 / 100) / 8)      // WIDTH_OFFSET 0.25 -> col 10
#define WEAPON_GAP      (GML_LEN(128) / 8)                  // 26 px -> 3 tiles
// KILLS/TOP/NEXT van con global.myFont (sFont2, setFontInterlude), la letra
// grande del HUD: 16 px, dos filas de tiles. setFontInterlude pone
// draw_set_valign(fa_center), así que la 'y' del original es el CENTRO de la
// línea: la fila de arriba es (y - 8) / 8, redondeada.
#define BIG_ROW_KILLS   ((GML_POS(1200 * 45 / 100) - 8 + 4) / 8)   // fila 13
#define BIG_ROW_TOP     ((GML_POS(1200 * 53 / 100) - 8 + 4) / 8)   // fila 15
#define BIG_ROW_NEXT    ((GML_POS(1200 * 61 / 100) - 8 + 4) / 8)   // fila 17

// Cuenta de los números. NO está en el original (que escribe el número ya
// hecho, letra a letra): la pidió el autor. Sube DE UNO EN UNO, un número
// por frame, con un tick en cada paso; sólo si el valor es tan grande que
// tardaría más de COUNT_MAX_FRAMES (3 s) da pasos mayores.
#define COUNT_MAX_FRAMES 150

// NEXT parpadea (petición del autor, tampoco está en el original): una vez
// escrita entera, la línea se enciende y se apaga cada NEXT_BLINK frames.
#define NEXT_BLINK      16
#define HUD_ROW_BOTTOM  2                                   // BULLETS/KILLS (2 filas) y LAST

// Tiles: los agujeros ocupan 64 y el suelo 16 desde TILE_USER_INDEX.
#define WEAPON_TILES     4                                  // 4x4 por icono



// oMenu/Create_0: los textos de marquesina, uno por arma.
static const char *marquee[5] = {
    " WHITE BLOCK ", " LASER ", " 3 WAY ", " CIRCLE ", " TRIANGLE OF DESTRUCTION "
};

// oMenu/Draw_0, rama del splash. Literal del original salvo tres cosas:
//   - SPACE y CURSORS, que en Mega Drive no existen, pasan a A y D-PAD;
//   - la firma dice SCREEN IMPLOSION, no REDEFINETECLAS: el GML lleva el
//     nombre viejo y el autor ha confirmado cuál es el bueno.
// El "AND AND" es del original y se deja: es su voz.
static const char *splashText =
    " PRESS A TO START ... D-PAD TO MOVE ... SHOOT ALL THE BUGS AND AND SNAKES ..."
    "SURVIVE FOR GLORY... GAME BY SCREEN IMPLOSION 2012 ";

static s16  barHeight;      // t del original, en píxeles
static u16  typeTimer;      // alarm[0]
static u16  typeLen;        // iText
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
static u16  shownTypeLen;   // typeLen ya escrito en pantalla
static u16  countKills;     // lo que enseña ahora la cuenta
static u16  countTop;
static u16  shownKills;
static u16  shownTop;
static u16  blinkTimer;
static bool nextOn;         // NEXT encendido en el parpadeo
static u16  shownCredits = 0xFFFF;   // CREDITS ya escrito en pantalla
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
// Hole_textRow): con el hexágono, la fila de CREDITS cruza la parte de abajo
// de tres agujeros. Una sola escritura de fila, así que no hay frame en que
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
    }

    VDP_loadTileSet(&weaponTiles, weaponTileBase, DMA);
    VDP_loadTileSet(&logoTiles, logoTileBase, DMA);
    VDP_loadTileSet(&barTile, barTile_, DMA);
}

void Menu_init(void)
{
    Menu_loadTiles();

    barHeight = 0;
    typeTimer = MENU_TYPE_DELAY;
    typeLen = 1;
    scroll = PLAYFIELD_W;
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
    // para que corra el tiempo (Q8) gastaba un crédito en el primer frame del
    // menú y la partida volvía a empezar sin pasar por el interludio.
    prevPad = JOY_readJoypad(JOY_1);

    barHeight = 0;
    typeTimer = MENU_TYPE_DELAY;
    typeLen = 1;
    inMenu = TRUE;
    staticDrawn = FALSE;

    // menu(): audio_play_sound(menuSong, 1, false)
    Audio_menuMusic(TRUE);
    shownWeapon = 0xFFFF;
    shownBar = -1;
    shownTypeLen = 0xFFFF;
    countKills = countTop = 0;
    shownKills = shownTop = 0xFFFF;
    // Menu_leave borró la fila de CREDITS, y sus letras compuestas comparten
    // tiles con el aviso HOLD B de la partida (ver Hole_textRow): se vuelve
    // a escribir siempre al entrar.
    shownCredits = 0xFFFF;
    blinkTimer = 0;
    nextOn = TRUE;
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
    staticDrawn = FALSE;

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

// La barra gris de draw_line_width_color: una línea horizontal de grosor t
// centrada en FIRST_ROW_HEIGHT, que aquí son filas de tiles.
static void drawBar(void)
{
    const s16 half = barHeight / 2;

    if (barHeight == shownBar) return;
    shownBar = barHeight;

    const s16 top = (GML_POS(1200 * 53 / 100) - half) / 8;
    const s16 bottom = (GML_POS(1200 * 53 / 100) + half) / 8;
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
    if ((bandTop < 0) || (top < bandTop)) bandTop = (top < 0) ? 0 : top;
    if (bottom > bandBottom) bandBottom = (bottom >= (PLAYFIELD_H / 8)) ? ((PLAYFIELD_H / 8) - 1) : bottom;
    // Un cambio de fondo con el menú abierto no tiene que comerse la banda.
    Bg_setCover(bandTop, bandBottom);

    for (r = top; r <= bottom; r++)
    {
        if ((r < 0) || (r >= (PLAYFIELD_H / 8))) continue;

        // Las 64 columnas del plano, no sólo las 40 de la pantalla, por si
        // el plano llega desplazado.
        for (c = 0; c < planeWidth; c++)
            // Prioridad alta: en el original oMenu va a depth -100, por
            // delante de los objetos del room, y la banda TAPA los paquetes y
            // las réplicas que siguen en el suelo durante el menú. Un plano
            // de prioridad alta queda por encima de los sprites (que van en
            // baja, ver VSpr_add) y el texto, en el plano A alto, encima.
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, barTile_), c, r);
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

static void drawWeapons(void)
{
    u16 i, tx, ty;

    if (g_weaponIndex == shownWeapon) return;
    shownWeapon = g_weaponIndex;

    for (i = 0; i < g_weaponsUnlocked; i++)
    {
        const s16 col = COL_TEXT + (i * WEAPON_GAP);

        for (ty = 0; ty < WEAPON_TILES; ty++)
        {
            for (tx = 0; tx < WEAPON_TILES; tx++)
            {
                const s16 cx = col + tx;

                if (cx >= COLS) continue;

                // weapons.png son 5 iconos en fila, así que la fila ty del
                // icono i empieza en ty * (5 * 4) + i * 4.
                VDP_setTileMapXY(BG_A,
                                 TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE,
                                                weaponTileBase + (ty * WEAPON_TILES * 5)
                                                + (i * WEAPON_TILES) + tx),
                                 cx, ROW_WEAPONS + ty);
            }
        }
    }

    // El arma elegida se marca debajo (el original le pinta sprite191 encima).
    VDP_clearTextLine(ROW_WEAPONS + WEAPON_TILES);
    VDP_drawText("^", COL_TEXT + (g_weaponIndex * WEAPON_GAP) + 1,
                 ROW_WEAPONS + WEAPON_TILES);
}

// El logo es el sprite de oMenu (sprite176, 256x256 con el origen en el
// centro) dibujado en la posición de la instancia, que en el room está en
// (832, 608): el centro. Estaba pintado arriba del todo.
#define LOGO_TILES   7
#define LOGO_CX      GML_POS(832)
#define LOGO_CY      GML_POS(608)

// El logo va en el plano A, en las celdas 17-23 x 11-17, y con el hexágono
// de agujeros centrado (bloque 2 de MonsterLite) 40 de sus 49 celdas caen
// sobre dibujo de algún agujero. Una celda sólo tiene un tile: con el del
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

// Un paso de la cuenta: de uno en uno, salvo que el valor no quepa en
// COUNT_MAX_FRAMES; nunca se pasa.
static u16 countUp(u16 shown, u16 target)
{
    const u16 step = (target / COUNT_MAX_FRAMES) + 1;

    if (shown >= target) return target;
    return ((target - shown) > step) ? (shown + step) : target;
}

// Las cifras de una cuenta, detrás de su etiqueta de 6 letras ("KILLS ",
// "TOP   "), recortadas a lo que ya ha escrito la máquina de escribir. Los
// números sólo crecen, así que nunca quedan cifras viejas a la derecha.
static void drawCount(u16 value, s16 row)
{
    char d[6];

    if (typeLen <= 6) return;

    sprintf(d, "%u", value);
    if ((typeLen - 6) < strlen(d)) d[typeLen - 6] = 0;
    HUD_drawBig(d, COL_TEXT + (6 * 2), row);
}

bool Menu_update(u16 pad, bool firstTime)
{
    const u16 pressed = pad & ~prevPad;
    char buf[24];

    prevPad = pad;

    if (inMenu)
    {
        // t += STEP hasta FIRST_STEP
        if (barHeight < MENU_FIRST_STEP) barHeight += MENU_STEP;
        drawBar();

        // Texto escrito letra a letra: iText += 1 cada alarm[0] = 4 frames
        if (typeTimer > 0) typeTimer--;
        if (typeTimer == 0)
        {
            typeLen++;
            typeTimer = MENU_TYPE_DELAY;
        }

        // La cuenta arranca cuando la línea ya ha escrito su etiqueta
        // ("KILLS " son 6 letras) y llega al valor en COUNT_FRAMES frames.
        if (typeLen > 6)
        {
            const u16 k = countUp(countKills, g_kills);
            const u16 t = countUp(countTop, g_maxKills);

            // Un tick por frame en que se mueve alguna de las dos.
            if ((k != countKills) || (t != countTop)) Audio_tick();
            countKills = k;
            countTop   = t;
        }

        // Las líneas sólo se reescriben cuando cambia algo: una letra nueva
        // (cada 4 frames) o una cifra de la cuenta. Reescribirlas en cada
        // frame eran tres sprintf y tres líneas de tiles para nada, y con la
        // arena llena el menú perdía frames.
        // El parpadeo empieza cuando NEXT ya está escrita entera ("NEXT  "
        // más el número, que como mucho son 4 cifras).
        if (typeLen > 10)
        {
            if (++blinkTimer >= NEXT_BLINK)
            {
                blinkTimer = 0;
                nextOn = !nextOn;
                // Sólo la línea de NEXT: con el resto igual, no se repinta.
                // Apagada son espacios, o sea tiles vacíos que dejan ver la
                // banda (que va en el plano B).
                if (nextOn)
                {
                    if (g_nextWeapon == NEXT_WEAPON_DONE) strcpy(buf, "NEXT  re");
                    else sprintf(buf, "NEXT  %lu", g_nextWeapon);
                }
                else strcpy(buf, "          ");
                HUD_drawBig(buf, COL_TEXT, BIG_ROW_NEXT);
            }
        }

        if (typeLen != shownTypeLen)
        {
            // Entra una letra (cada 4 frames): las tres líneas enteras.
            shownTypeLen = typeLen;
            shownKills = countKills;
            shownTop = countTop;

            sprintf(buf, "KILLS %u", countKills);
            if (typeLen < strlen(buf)) buf[typeLen] = 0;
            HUD_drawBig(buf, COL_TEXT, BIG_ROW_KILLS);

            sprintf(buf, "TOP   %u", countTop);
            if (typeLen < strlen(buf)) buf[typeLen] = 0;
            HUD_drawBig(buf, COL_TEXT, BIG_ROW_TOP);

            if (nextOn)
            {
                if (g_nextWeapon == NEXT_WEAPON_DONE) strcpy(buf, "NEXT  re");
                else sprintf(buf, "NEXT  %lu", g_nextWeapon);
                if (typeLen < strlen(buf)) buf[typeLen] = 0;
                HUD_drawBig(buf, COL_TEXT, BIG_ROW_NEXT);
            }
        }
        else
        {
            // Sólo se mueve la cuenta: se reescriben sus cifras y nada más.
            // Repintar las dos líneas enteras en cada paso eran unas 140
            // escrituras al VDP por frame, y con la arena llena el menú no
            // llegaba: la cuenta subía un número cada 2-3 frames.
            if (countKills != shownKills)
            {
                shownKills = countKills;
                drawCount(countKills, BIG_ROW_KILLS);
            }
            if (countTop != shownTop)
            {
                shownTop = countTop;
                drawCount(countTop, BIG_ROW_TOP);
            }
        }

        drawWeapons();
        drawMarquee(marquee[(g_weaponIndex < 5) ? g_weaponIndex : 0],
                    strlen(marquee[(g_weaponIndex < 5) ? g_weaponIndex : 0]));

        // oWeaponBox/Step_0: izquierda y derecha cambian de arma, acotado a
        // las desbloqueadas.
        // oWeaponBox/Step_0: cada cambio llama a triggerSpeech()
        if (pressed & BUTTON_LEFT)
        {
            if (g_weaponIndex > 0) { g_weaponIndex--; Audio_speech(g_weaponIndex); }
        }
        if (pressed & BUTTON_RIGHT)
        {
            if (g_weaponIndex < (g_weaponsUnlocked - 1)) { g_weaponIndex++; Audio_speech(g_weaponIndex); }
        }
    }
    else
    {
        drawLogo();
        drawMarquee(splashText, strlen(splashText));
    }

    {
        if (g_credits != shownCredits)
        {
            shownCredits = g_credits;
            sprintf(buf, "CREDITS %u", g_credits);
            // La fila de créditos cruza dos agujeros: borrarla con
            // VDP_clearTextLine les abría una franja.
            drawRowOverHoles(buf, (COLS - strlen(buf)) / 2, ROW_CREDITS);
        }
    }

    // Selector de fondo PROVISIONAL (no está en el original, que no tiene
    // fondos): arriba y abajo recorren los fondos de la ROM, en el splash y
    // en el menú. El autor concretará más adelante cómo se eligen.
    if (BG_COUNT > 1)
    {
        if (pressed & BUTTON_UP)   Bg_select(Bg_current() + BG_COUNT - 1);
        if (pressed & BUTTON_DOWN) Bg_select(Bg_current() + 1);
    }

    // oMenu/Step_0: C mete crédito, el botón de disparo empieza si hay.
    if (pressed & BUTTON_START) g_credits++;

    if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C))
    {
        if (g_credits > 0)
        {
            g_credits--;
            return TRUE;
        }
    }

    (void)firstTime;
    return FALSE;
}
