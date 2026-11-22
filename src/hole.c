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
// El anillo de agujeros del port
//
// rooms/_1/_1.yy: las instancias de oAgujerSuper del room de 1600x1200. La
// primera es la reconstruida; las otras cinco, en el orden del room. Ver la
// nota de inc/hole.h.
//
// En el bloque 2 se cambiaron por el hexágono de MonsterLite centrado (Q6);
// el usuario decidió al cerrar el bloque 2 volver a este anillo
// (docs/MONSTERLITE_DECISIONES.md). Divergencia deliberada de MonsterLite.
//
// MonsterLite elige el agujero al azar, así que el orden no le importa, y
// desde el bloque 8 al port tampoco (la IA del GML saltaba al 0 y vagaba
// hacia los del medio).
// ---------------------------------------------------------------------------
static const s16 holeX[HOLE_COUNT] = { GML_POS(558), GML_POS(820), GML_POS(558), GML_POS(832), GML_POS(1088), GML_POS(1094) };
static const s16 holeY[HOLE_COUNT] = { GML_POS(754), GML_POS(272), GML_POS(446), GML_POS(888), GML_POS(424),  GML_POS(736)  };

// Dónde se pinta cada bloque, en celdas del plano: el plano A sólo se mueve
// de 8 en 8 píxeles, así que el dibujo cae en la celda más cercana (hasta
// 4 px respecto al original). La posición lógica (spawn, IA, colisión) es la
// exacta.
#define HOLE_CELL_OF(v)  (((v) - HOLE_ART_CENTER + 4) / 8)
static const s16 holeCol[HOLE_COUNT] =
{
    HOLE_CELL_OF(GML_POS(558)), HOLE_CELL_OF(GML_POS(820)),  HOLE_CELL_OF(GML_POS(558)),
    HOLE_CELL_OF(GML_POS(832)), HOLE_CELL_OF(GML_POS(1088)), HOLE_CELL_OF(GML_POS(1094))
};
static const s16 holeRow[HOLE_COUNT] =
{
    HOLE_CELL_OF(GML_POS(754)), HOLE_CELL_OF(GML_POS(272)), HOLE_CELL_OF(GML_POS(446)),
    HOLE_CELL_OF(GML_POS(888)), HOLE_CELL_OF(GML_POS(424)), HOLE_CELL_OF(GML_POS(736))
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

// ---------------------------------------------------------------------------
// Agujeros en verde (petición del autor, 2026-11-16; invención estética)
//
// Los agujeros comparten PAL2 con el logo, el HUD y los iconos de arma, y
// usan sus índices 1 (negro), 2 (gris muy oscuro) y 3 (gris oscuro, 2
// píxeles). Cambiar esos colores teñiría también el resto. Así que al
// cargarlos esos píxeles pasan a los índices 14 y 15, que no usaba nadie
// (gm2md.py los deja a 0x0000), y ahí van los verdes. El reborde rojo
// (índices 4 y 5) no se toca.
// ---------------------------------------------------------------------------
#define HOLE_IDX_FILL  14
#define HOLE_IDX_RIM   15
#define HOLE_COL_FILL  0x0020   // relleno: verde muy oscuro (antes negro)
#define HOLE_COL_RIM   0x0042   // borde: verde oscuro (antes gris 0x222/0x444)

static u16 holePal[16];

// Copia un tile del agujero pasando los índices 1, 2 y 3 a los verdes.
//
// Los 8 píxeles de cada fila a la vez (bloque 9): el bucle de antes, píxel a
// píxel con desplazamientos variables, costaba unos 6000 ciclos por tile, y
// ahora se llama al componer cada letra del HUD encima de un agujero. Por
// nibble: b0..b3 son sus bits; "es 1" = b0 sin b1, b2 ni b3; "es 2 o 3" =
// b1 sin b2 ni b3. x * 15 se hace con (x << 4) - x (cada nibble tiene como
// mucho su bit 0, así que no hay acarreo entre nibbles), sin la
// multiplicación de 32 bits, que en el 68000 es una llamada a libgcc.
static void greenTile(const u32 *src, u32 *dst)
{
    u16 w;

    for (w = 0; w < 8; w++)
    {
        const u32 v  = src[w];
        const u32 b0 = v & 0x11111111;
        const u32 b1 = (v >> 1) & 0x11111111;
        const u32 hi = ((v >> 2) | (v >> 3)) & 0x11111111;
        const u32 is1  = b0 & ~b1 & ~hi;
        const u32 is23 = b1 & ~hi;
        const u32 sel  = is1 | is23;

        // 1 -> 14 (HOLE_IDX_FILL), 2 y 3 -> 15 (HOLE_IDX_RIM)
        dst[w] = (v & ~((sel << 4) - sel)) | ((is1 << 4) - (is1 << 1)) | ((is23 << 4) - is23);
    }
}

#if (HOLE_IDX_FILL != 14) || (HOLE_IDX_RIM != 15)
#error "greenTile da los índices 14 y 15 a mano"
#endif

// Las celdas con dibujo, ya en verde, de las dos variantes (0 normal, 1
// roja), por su índice compacto. Se quedan en RAM (2,75 KB con las 43 de
// ahora) porque componer encima (el texto del HUD, las celdas compartidas)
// las necesita y convertirlas otra vez costaba ~2800 ciclos por celda
// (medido en el bloque 9).
#define HOLE_MAX_LOADED 48
static u32 holeGreen[2][HOLE_MAX_LOADED][8];
// Celdas que tienen dibujo en una variante pero no en la otra (el reborde
// rojo sobresale): en la variante en que están vacías una letra encima no
// necesita tile compuesto.
static u8  holeBlank[2][HOLE_MAX_LOADED];

// El tile de VRAM de cada celda en la variante roja (bloque 9). 14 de las 43
// celdas con dibujo son iguales en las dos variantes (el relleno de dentro,
// lejos del reborde): esas usan el tile de la normal y no se suben dos
// veces. Son 14 tiles de VRAM sin cambiar nada de lo que se ve; pagan las
// letras C, O y R de la fuente grande (SCORE, ver src/hud.c).
static u16 holeRedTile[HOLE_MAX_LOADED];
static u8  holeRedRev[HOLE_MAX_LOADED];     // tile rojo propio -> índice compacto
static u16 holeRedCount;

// Convierte las celdas con dibujo de 'ts' a verde y las guarda en
// holeGreen[v]. Sólo al arrancar.
static void loadCells(const TileSet *ts, u16 v)
{
    u16 t;

    for (t = 0; t < HOLE_CELLS; t++)
    {
        if (holeCell[t] == HOLE_EMPTY) continue;

        greenTile(ts->tiles + (t * 8), holeGreen[v][holeCell[t]]);
        holeBlank[v][holeCell[t]] = !tileUsed(ts, t);
    }
}

// Las celdas con dibujo de cada fila del plano, de todos los agujeros, en
// el orden de la tabla: columna, agujero e índice compacto. Hole_rowTiles
// las recorría agujero a agujero y celda a celda, ~5000 ciclos por fila
// (medido en el bloque 9), y ahora se llama cada vez que se escribe un texto
// encima de los agujeros.
#define HOLE_ROWS       (PLAYFIELD_H / 8)
#define HOLE_ROW_CELLS  (HOLE_COUNT * HOLE_MAX_LOADED)
static u16 rowFirst[HOLE_ROWS + 1];
static u8  rcCol[HOLE_ROW_CELLS];
static u8  rcHole[HOLE_ROW_CELLS];
static u8  rcIdx[HOLE_ROW_CELLS];

static void buildRowCells(void)
{
    s16 row;
    u16 n = 0;

    for (row = 0; row < HOLE_ROWS; row++)
    {
        u16 i;

        rowFirst[row] = n;

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
                if ((cx < 0) || (cx >= (PLAYFIELD_W / 8))) continue;

                rcCol[n] = (u8)cx;
                rcHole[n] = (u8)i;
                rcIdx[n] = holeCell[t];
                n++;
            }
        }
    }

    rowFirst[HOLE_ROWS] = n;
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
    const u16 c = holeCell[t];

    return TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                          holeActive[i] ? holeRedTile[c] : (holeTileBase + c));
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
// encima). Se recompone cuando cambia el reborde rojo de alguno. Nunca más
// de MAX_SHARED (findShared las cuenta al arrancar).
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
        const u32 m = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

        buf[w] = (buf[w] & ~((m << 4) - m)) | v;
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

        overlay(buf, holeGreen[holeActive[i] ? 1 : 0][holeCell[t]]);
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
// Le puede pasar a PRESS START en el splash, al aviso "HOLD A/B/C" de la
// partida y a los textos del HUD del bloque 9 (UPGRADE AT, "ARENA!" y el
// mensaje de nivel).
//
// Las letras que caen sobre dibujo de agujero se componen en un tile propio:
// los agujeros debajo y la letra encima, en blanco de la paleta de los
// agujeros (la fuente del sistema sólo usa el índice 15, blanco, y el texto
// se ve blanco).
//
// Bloque 9: hasta el bloque 8 había un solo texto así a la vez (PRESS START
// en el menú, HOLD en partida) y compartían 10 tiles. Ahora en partida
// pueden coincidir UPGRADE AT (fila 4), el mensaje de nivel (filas 9 y 10) y
// HOLD (fila 20), así que:
//   - cada fila con texto encima de los agujeros se apunta en una tabla
//     (TEXT_ROWS) con su texto entero, de 40 columnas;
//   - los tiles compuestos salen de una reserva de HOLE_TEXT_TILES y cada
//     columna se queda con el suyo mientras su letra siga encima de un
//     agujero;
//   - cuando un agujero cambia de variante (el reborde rojo de cuando sale un
//     gusano, drawOne), las celdas suyas que tienen letra encima no se pisan:
//     la fila se vuelve a componer.
//
// Y no se compone en el acto, porque cuesta. Se midió la primera versión,
// que leía la letra y el agujero de la VRAM y subía el tile por la CPU:
// ~5000 ciclos por letra (el puerto del VDP en pleno dibujo va a ~50 ciclos
// por palabra leída y ~120 por palabra escrita, y SYS_disableInts y
// SYS_enableInts son otros ~750), y una subida de nivel, con 21 letras
// encima de agujeros, se comía 2,5 frames. Ahora:
//   - la silueta de la fuente está en RAM (fontMask, 2 KB) y los agujeros ya
//     verdes también (holeGreen): componer una letra son 8 AND/OR por fila;
//   - cada columna recuerda qué letra y qué agujero tiene compuestos: si no
//     cambian, no se vuelve a componer (una fila entera vuelve a empezar
//     cuando cambia un agujero o el número del mensaje);
//   - las filas que se ocultan (Hole_hideTextRow) guardan sus tiles por si
//     el texto vuelve igual: el mensaje de nivel siguiente sólo recompone
//     las cifras. Si la reserva se llena, se desaloja una fila oculta;
//   - Hole_textUpdate, una vez por frame y sólo si hay algo pendiente,
//     compone lo que haya, encola la subida de los tiles por DMA y la del
//     tramo de fila que cambia, y las dos llegan en el mismo vblank: no se
//     ve nunca una fila apuntando a tiles a medio subir.
// Repartirlo en varios frames (3 o 6 letras por frame) se probó y salía
// peor en el arnés de estrés (500-505 vueltas frente a 504 de una vez): con
// la arena llena cada frame va al límite y lo que cuenta es en cuántos hay
// trabajo de más. Ver docs/PORT_AUDIT_GML.md, bloque 9.
//
// Las filas se apuntan con Hole_drawTextRow y se borran con
// Hole_clearTextRow; Hole_forgetText las olvida todas sin pintar (el menú
// repone el plano entero al salir).
//
// Peor caso medido en partida (ver docs/PORT_AUDIT_GML.md, bloque 9): 28
// letras sobre agujero a la vez. Si la reserva se acabara, las letras que
// sobran salen con el tile de la fuente, cortando el agujero, como antes.
// ---------------------------------------------------------------------------
#define HOLE_TEXT_TILES 32
#define TEXT_ROWS       6
#define TEXT_COLS       (PLAYFIELD_W / 8)
#define TEXT_NONE       0xFF
#define FONT_FIRST      32      // ' '
#define FONT_COUNT      64      // hasta '_': mayúsculas, cifras y signos

enum { TEXT_IDLE = 0, TEXT_DIRTY };

static u16 textBase;
static u32 textWhite;       // el blanco de la paleta en los 8 píxeles de una fila
static u32 textFree;        // un bit por tile de la reserva: 1 = libre
static u32 textUpload;      // tiles compuestos este frame, a subir en el vblank
// La silueta de cada letra: 0xF en sus píxeles, 0 en el resto.
static u32 fontMask[FONT_COUNT][8];
// TILE_ATTR de la fuente del sistema (VDP_getTextPalette y
// VDP_getTextPriority cuestan ~180 ciclos cada llamada; no cambian).
static u16 fontBase;
static u32 textBuf[HOLE_TEXT_TILES][8];

static s8  textRow[TEXT_ROWS];                  // fila del plano, o -1
static u8  textState[TEXT_ROWS];
// Fila oculta: el plano sólo enseña los agujeros, pero se guardan su texto y
// sus tiles compuestos por si vuelve igual (el mensaje de nivel, ARENA!,
// HOLD). Si la reserva se llena, se desaloja (evictHidden).
static u8  textHidden[TEXT_ROWS];
// Columnas que hay que recorrer al componer: las del texto y las del texto
// anterior, que pueden tener tiles que soltar. Fuera de ellas sólo hay
// agujeros.
static u8  textLo[TEXT_ROWS], textHi[TEXT_ROWS];
static char textChars[TEXT_ROWS][TEXT_COLS];    // ' ' = nada
static u8  textTile[TEXT_ROWS][TEXT_COLS];      // tile de la reserva, o TEXT_NONE
static u16 textMap[TEXT_ROWS][TEXT_COLS];       // la fila tal como se escribirá
static u16 textShown[TEXT_ROWS][TEXT_COLS];     // la fila tal como está en el plano
// Los agujeros de cada fila apuntada (Hole_rowTiles), guardados: volver a
// sacarlos son ~3400 ciclos y la fila se recompone cada vez que cambia su
// texto. Sólo hay que rehacerlos cuando algún agujero cambia de variante
// (holeUnderGen, que sube Hole_setActive) o la fila es nueva.
static u16 textUnder[TEXT_ROWS][TEXT_COLS];
static u16 textUnderGen[TEXT_ROWS];
static u16 holeUnderGen = 1;
// Lo que hay compuesto en el tile de cada columna: la letra y el atributo
// del agujero de debajo. Si no cambian, no se vuelve a componer (bloque 9:
// medido en el arnés de estrés, la mayoría de las recomposiciones eran de
// letras que no habían cambiado: la fila entera vuelve a empezar cuando
// cambia un agujero o el número del mensaje).
static char textDoneCh[TEXT_ROWS][TEXT_COLS];
static u16  textDoneUnder[TEXT_ROWS][TEXT_COLS];
// Tiles que la fila ya no usa pero que el plano todavía enseña hasta que se
// escriba la fila nueva: no vuelven a la reserva hasta entonces, para que
// otra fila no los pinte encima en el frame de en medio.
static u32 textPending[TEXT_ROWS];

static s16 textSlot(s16 row)
{
    u16 k;

    for (k = 0; k < TEXT_ROWS; k++)
        if (textRow[k] == row) return k;

    return -1;
}

static void freeTile(u16 k, u16 c)
{
    if (textTile[k][c] == TEXT_NONE) return;

    textPending[k] |= 1UL << textTile[k][c];
    textTile[k][c] = TEXT_NONE;
}

// La fila k deja de estar apuntada (y su fila del plano se repinta en el
// acto, o ya está repintada): todos sus tiles vuelven a la reserva.
static void releaseSlot(u16 k)
{
    u16 c;

    for (c = 0; c < TEXT_COLS; c++) freeTile(k, c);
    textFree |= textPending[k];
    textPending[k] = 0;
    textRow[k] = -1;
    textState[k] = TEXT_IDLE;
    textHidden[k] = FALSE;
}

// Suelta una fila oculta que no sea 'keep' (la que tenga tiles, si hay).
// Devuelve FALSE si no había ninguna.
static bool evictHidden(s16 keep)
{
    u16 k;

    for (k = 0; k < TEXT_ROWS; k++)
    {
        if ((k == keep) || (textRow[k] < 0) || !textHidden[k]) continue;

        releaseSlot(k);
        return TRUE;
    }

    return FALSE;
}

static u16 fontAttr(char ch)
{
    return fontBase | (TILE_FONT_INDEX + (u8)(ch - 32));
}

// Los píxeles del tile de agujero 'attr' (uno de los que pinta
// Hole_rowTiles): la variante normal o la roja, de holeGreen, o, si es una
// celda compartida, la composición de los agujeros que la tocan.
static const u32 *underPixels(u16 attr, s16 col, s16 row, u32 *tmp)
{
    const u16 idx = attr & TILE_INDEX_MASK;

    if ((u16)(idx - holeTileBase) < holeLoaded)   return holeGreen[0][idx - holeTileBase];
    if ((u16)(idx - holeRedBase) < holeRedCount)  return holeGreen[1][holeRedRev[idx - holeRedBase]];

    Hole_cellPixels(col, row, tmp);
    return tmp;
}

// ---------------------------------------------------------------------------
// Sombra (bloque 9, decisión del usuario del 2026-11-20: todas las fuentes
// llevan sombra, pintada en los propios tiles)
//
// MonsterLite pinta los textos dos veces: en negro unos píxeles más abajo y
// encima en color (S/HUD.cs: el mensaje de nivel, ARENA! y el aviso, a
// offsetLetterShadow = 4 px de 768, 1,25 px aquí). Los glifos de la fuente
// del sistema ocupan las filas 1-6 y las columnas 1-6 de su celda, así que
// cabe una sombra de 1 px hacia abajo dentro del mismo tile: al arrancar se
// pinta en los 96 glifos de la fuente, en su sitio (TEXT_SHADOW_IDX de PAL0,
// negro). 0 tiles, ninguna escritura por frame, y todo el texto de la fuente
// del sistema la lleva: HUD, mensajes, menú, game over y tienda.
//
// Las letras que caen sobre un agujero la llevan en su tile compuesto (en el
// negro de la paleta de los agujeros, índice 1).
// ---------------------------------------------------------------------------

// La sombra del glifo g (máscara 0xF por píxel): la silueta una fila más
// abajo, donde no tapa a la letra.
static u32 shadowRow(u16 g, u16 w)
{
    return w ? (fontMask[g][w - 1] & ~fontMask[g][w]) : 0;
}

// Pinta la sombra en los 96 glifos de la fuente del sistema, en la VRAM.
static void shadowFont(void)
{
    u16 g, w;

    for (g = 0; g < 96; g++)
    {
        u32 t[8], m[8];

        VTile_read(TILE_FONT_INDEX + g, t);
        for (w = 0; w < 8; w++)
        {
            const u32 v = t[w];
            const u32 b = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

            m[w] = (b << 4) - b;
        }

        // Los que llegan a la fila 7 (los descendentes: , ; g j p q y, y |)
        // no tendrían sitio para la sombra y saldría cortada (lo vio el
        // verificador en la tienda: "ways", "energy"). Si la fila 0 está
        // libre, el glifo sube 1 px; si no (|, de arriba abajo), pierde su
        // última fila. 0 tiles.
        if (m[7])
        {
            if (!m[0])
            {
                for (w = 0; w < 7; w++) { t[w] = t[w + 1]; m[w] = m[w + 1]; }
            }
            t[7] = 0;
            m[7] = 0;

            // La silueta para componer encima de los agujeros, igual.
            if (g < FONT_COUNT)
                for (w = 0; w < 8; w++) fontMask[g][w] = m[w];
        }

        for (w = 0; w < 8; w++)
        {
            const u32 sh = w ? (m[w - 1] & ~m[w]) : 0;

            t[w] |= sh & (TEXT_SHADOW_IDX * 0x11111111UL);
        }
        VDP_loadTileData(t, TILE_FONT_INDEX + g, 1, CPU);
    }
}

// TRUE si el tile de agujero 'attr' no tiene ni un píxel (la celda sólo
// tiene dibujo en la otra variante).
static bool underBlank(u16 attr)
{
    const u16 idx = attr & TILE_INDEX_MASK;

    if ((u16)(idx - holeTileBase) < holeLoaded)   return holeBlank[0][idx - holeTileBase];
    if ((u16)(idx - holeRedBase) < holeRedCount)  return holeBlank[1][holeRedRev[idx - holeRedBase]];

    return FALSE;
}

// La celda c de la fila apuntada k, encima de 'under' (el atributo del
// agujero que va en esa celda, o 0 si no hay). Deja en textMap el atributo
// que hay que escribir y devuelve TRUE si ha compuesto un tile (lo que
// cuenta para el presupuesto del frame).
static bool textCell(u16 k, u16 c, u16 under)
{
    const char ch = textChars[k][c];
    u32 *cell;
    u16 w, t;

    if (ch == ' ')
    {
        freeTile(k, c);
        textMap[k][c] = under;
        return FALSE;
    }

    if (!under || ((u8)(ch - FONT_FIRST) >= FONT_COUNT) || underBlank(under))
    {
        freeTile(k, c);
        textMap[k][c] = fontAttr(ch);
        return FALSE;
    }

    t = textTile[k][c];
    // Una celda compartida cambia de dibujo sin cambiar de tile (se
    // recompone en su sitio): no vale la caché.
    if ((t != TEXT_NONE) && (textDoneCh[k][c] == ch) && (textDoneUnder[k][c] == under)
        && ((u16)((under & TILE_INDEX_MASK) - sharedBase) >= sharedCount))
    {
        textMap[k][c] = (fontBase & ~TILE_ATTR_PALETTE_MASK) | TILE_ATTR(PAL2, FALSE, FALSE, FALSE)
                        | (textBase + t);
        return FALSE;
    }

    if (t == TEXT_NONE)
    {
        while (!textFree)
        {
            if (!evictHidden(k))
            {
                textMap[k][c] = fontAttr(ch);
                return FALSE;
            }
        }

        // El primer bit libre, de 16 en 16, de 8 en 8... (un desplazamiento
        // variable por bit costaba ~1000 ciclos con la reserva medio llena).
        {
            u32 f = textFree;

            t = 0;
            if (!(f & 0xFFFF)) { f >>= 16; t += 16; }
            if (!(f & 0xFF))   { f >>= 8;  t += 8; }
            if (!(f & 0xF))    { f >>= 4;  t += 4; }
            while (!(f & 1))   { f >>= 1;  t++; }
        }
        textFree &= ~(1UL << t);
        textTile[k][c] = (u8)t;
    }

    // El agujero debajo y la letra encima, en el blanco de la paleta de los
    // agujeros (sus píxeles, índice 15, pasan al índice del blanco).
    {
        u32 tmp[8];
        const u32 *under8 = underPixels(under, c, textRow[k], tmp);
        const u32 *mask = fontMask[(u8)(ch - FONT_FIRST)];

        const u16 g = (u8)(ch - FONT_FIRST);

        // y la sombra, en el negro de la paleta de los agujeros (índice 1)
        cell = textBuf[t];
        for (w = 0; w < 8; w++)
        {
            const u32 m = mask[w];
            const u32 sh = shadowRow(g, w);

            cell[w] = (under8[w] & ~(m | sh)) | (textWhite & m) | (0x11111111UL & sh);
        }
    }

    textDoneCh[k][c] = ch;
    textDoneUnder[k][c] = under;
    textUpload |= 1UL << t;
    textMap[k][c] = (fontBase & ~TILE_ATTR_PALETTE_MASK) | TILE_ATTR(PAL2, FALSE, FALSE, FALSE)
                    | (textBase + t);
    return TRUE;
}

// Vuelve a componer la fila apuntada k desde el principio.
static bool textAnyDirty;   // alguna fila por componer: Hole_textUpdate sale enseguida si no

static void textRestart(u16 k)
{
    textState[k] = TEXT_DIRTY;
    textAnyDirty = TRUE;
}

void Hole_textUpdate(void)
{
    u16 k;

    if (!textAnyDirty) return;
    textAnyDirty = FALSE;

    for (k = 0; k < TEXT_ROWS; k++)
    {
        u16 c, lo, hi;

        if (textState[k] != TEXT_DIRTY) continue;

        // Los agujeros que pasan por la fila, como están ahora, y encima las
        // letras (sólo las columnas del texto, el de ahora y el anterior).
        if (textUnderGen[k] != holeUnderGen)
        {
            Hole_rowTiles(textRow[k], textUnder[k], TEXT_COLS);
            textUnderGen[k] = holeUnderGen;
        }
        memcpy(textMap[k], textUnder[k], sizeof(textUnder[k]));
        for (c = textLo[k]; c < textHi[k]; c++) textCell(k, c, textMap[k][c]);

        // Las columnas del texto anterior ya están soltadas.
        lo = 0;
        hi = TEXT_COLS;
        while ((lo < TEXT_COLS) && (textChars[k][lo] == ' ')) lo++;
        while ((hi > lo) && (textChars[k][hi - 1] == ' ')) hi--;
        textLo[k] = (u8)lo;
        textHi[k] = (u8)hi;

        // Al plano, sólo el tramo que ha cambiado, por la cola de DMA: llega
        // en el mismo vblank que los tiles (abajo), así que nunca se ve una
        // fila apuntando a tiles a medio subir. Y no hay espera: por la CPU,
        // con el VDP dibujando, cada palabra son ~120 ciclos.
        lo = 0;
        hi = TEXT_COLS;
        while ((lo < TEXT_COLS) && (textMap[k][lo] == textShown[k][lo])) lo++;
        while ((hi > lo) && (textMap[k][hi - 1] == textShown[k][hi - 1])) hi--;
        if (lo < hi)
        {
            VDP_setTileMapDataRow(BG_A, textMap[k] + lo, textRow[k], lo, hi - lo, DMA_QUEUE_COPY);
            memcpy(textShown[k] + lo, textMap[k] + lo, (hi - lo) * sizeof(u16));
        }

        // Los tiles que la fila ha dejado de usar se pueden dar ya: su fila
        // nueva llega en el mismo vblank que lo que otra fila pinte en ellos.
        textFree |= textPending[k];
        textPending[k] = 0;
        textState[k] = TEXT_IDLE;
    }

    // Los tiles compuestos, en un solo DMA en el vblank (del primero al
    // último que han cambiado; los de en medio van igual, sin cambios).
    if (textUpload)
    {
        u16 lo = 0, hi = HOLE_TEXT_TILES - 1;

        while (!(textUpload & (1UL << lo))) lo++;
        while (!(textUpload & (1UL << hi))) hi--;
        DMA_queueDma(DMA_VRAM, textBuf[lo], (textBase + lo) * 32, (hi - lo + 1) * 16, 2);
        textUpload = 0;
    }
}

void Hole_drawTextRow(const char *s, u16 col, s16 row)
{
    s16 k = textSlot(row);
    u16 c;

    if (k < 0)
    {
        k = textSlot(-1);
        if ((k < 0) && evictHidden(-1)) k = textSlot(-1);

        // Sin fila libre (no pasa: como mucho hay 5 a la vez en partida y 1
        // en el menú), el texto sale con la fuente, sin componer, y sin
        // apuntar.
        if (k < 0)
        {
            VDP_drawText(s, col, row);
            return;
        }

        textRow[k] = (s8)row;
        textUnderGen[k] = 0;
        // No se sabe qué hay en el plano en esa fila: la primera vez se
        // escribe entera.
        for (c = 0; c < TEXT_COLS; c++)
        {
            textTile[k][c] = TEXT_NONE;
            textShown[k][c] = 0xFFFF;
        }
        textLo[k] = TEXT_COLS;
        textHi[k] = 0;
    }

    textHidden[k] = FALSE;
    for (c = 0; c < TEXT_COLS; c++) textChars[k][c] = ' ';
    {
        const u16 lo = col;

        for (c = col; *s && (c < TEXT_COLS); c++) textChars[k][c] = *s++;
        if (lo < textLo[k]) textLo[k] = (u8)lo;
        if (c > textHi[k])  textHi[k] = (u8)c;
    }

    textRestart(k);
}

void Hole_clearTextRow(s16 row)
{
    u16 buf[TEXT_COLS];
    const s16 k = textSlot(row);

    if (k >= 0) releaseSlot(k);

    Hole_rowTiles(row, buf, TEXT_COLS);
    VDP_setTileMapDataRow(BG_A, buf, row, 0, TEXT_COLS, CPU);
}

// TRUE si alguna celda compartida (dos agujeros) cae en la fila.
static bool rowShared(s16 row)
{
    u16 k;

    for (k = 0; k < sharedCount; k++)
        if (sharedRow[k] == row) return TRUE;

    return FALSE;
}

static void hideRow(s16 row, TransferMethod tm)
{
    static const u16 zeros[TEXT_COLS];
    const s16 k = textSlot(row);

    // Sin texto apuntado ni agujeros en la fila (las de arriba del HUD): la
    // fila vacía, de un búfer fijo y sin componer nada.
    if ((k < 0) && (rowFirst[row] == rowFirst[row + 1]) && !rowShared(row))
    {
        VDP_setTileMapDataRow(BG_A, zeros, row, 0, TEXT_COLS, (tm == CPU) ? CPU : DMA_QUEUE);
        return;
    }

    if (k < 0)
    {
        u16 buf[TEXT_COLS];

        Hole_rowTiles(row, buf, TEXT_COLS);
        VDP_setTileMapDataRow(BG_A, buf, row, 0, TEXT_COLS, tm);
        return;
    }

    textHidden[k] = TRUE;
    textState[k] = TEXT_IDLE;
    Hole_rowTiles(row, textShown[k], TEXT_COLS);
    VDP_setTileMapDataRow(BG_A, textShown[k], row, 0, TEXT_COLS, tm);
}

void Hole_hideTextRow(s16 row)
{
    hideRow(row, CPU);
}

void Hole_hideTextRowQueued(s16 row)
{
    hideRow(row, DMA_QUEUE_COPY);
}

void Hole_textTouched(void)
{
    u16 k, c;

    // Alguien ha escrito por encima de las filas sin pasar por aquí (el
    // depurador de gráficos): lo que hay en el plano ya no es textShown. Las
    // filas a la vista se vuelven a escribir enteras (con los tiles que ya
    // tienen compuestos, sin recomponer).
    for (k = 0; k < TEXT_ROWS; k++)
    {
        if (textRow[k] < 0) continue;

        for (c = 0; c < TEXT_COLS; c++) textShown[k][c] = 0xFFFF;
        if (!textHidden[k]) textRestart(k);
    }
}

void Hole_forgetText(void)
{
    u16 k, c;

    // El plano se ha repuesto entero (sólo agujeros): las filas pasan a
    // ocultas, con sus tiles, y sin saber qué hay en el plano (la próxima
    // vez se escriben enteras).
    for (k = 0; k < TEXT_ROWS; k++)
    {
        if (textRow[k] < 0) continue;

        textHidden[k] = TRUE;
        textState[k] = TEXT_IDLE;
        for (c = 0; c < TEXT_COLS; c++) textShown[k][c] = 0xFFFF;
    }
    textUpload = 0;
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

    memcpy(holePal, holePalette, sizeof(holePal));
    holePal[HOLE_IDX_FILL] = HOLE_COL_FILL;
    holePal[HOLE_IDX_RIM]  = HOLE_COL_RIM;
    PAL_setPalette(PAL2, holePal, CPU);

    holeLoaded = 0;
    for (t = 0; t < HOLE_CELLS; t++)
    {
        if (tileUsed(&holeTiles, t) || tileUsed(&holeRedTiles, t))
            holeCell[t] = (u8)holeLoaded++;
        else
            holeCell[t] = HOLE_EMPTY;
    }

    if (holeLoaded > HOLE_MAX_LOADED) SYS_die("HOLE_MAX_LOADED");

    loadCells(&holeTiles, 0);
    loadCells(&holeRedTiles, 1);
    holeTileBase = VTile_alloc(holeLoaded);
    VDP_loadTileData(holeGreen[0][0], holeTileBase, holeLoaded, CPU);

    // La variante roja: sólo las celdas que cambian (ver holeRedTile).
    {
        u16 c, k = 0;

        for (c = 0; c < holeLoaded; c++)
            if (memcmp(holeGreen[0][c], holeGreen[1][c], 32)) k++;

        holeRedCount = k;
        holeRedBase = VTile_alloc(k);
        k = 0;
        for (c = 0; c < holeLoaded; c++)
        {
            if (memcmp(holeGreen[0][c], holeGreen[1][c], 32))
            {
                holeRedTile[c] = holeRedBase + k;
                holeRedRev[k] = (u8)c;
                VDP_loadTileData(holeGreen[1][c], holeRedBase + k, 1, CPU);
                k++;
            }
            else
            {
                holeRedTile[c] = holeTileBase + c;
            }
        }
    }
    findShared();
    buildRowCells();

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
    {
        u16 k;

        for (k = 0; k < TEXT_ROWS; k++) releaseSlot(k);
        textFree = 0xFFFFFFFFUL;
    }

    // La silueta de la fuente del sistema, de la VRAM a la RAM, una vez (en
    // la ROM va comprimida).
    fontBase = TILE_ATTR(VDP_getTextPalette(), VDP_getTextPriority(), FALSE, FALSE);
    {
        u16 g, w;

        for (g = 0; g < FONT_COUNT; g++)
        {
            VTile_read(TILE_FONT_INDEX + g, fontMask[g]);
            for (w = 0; w < 8; w++)
            {
                const u32 v = fontMask[g][w];
                const u32 m = (v | (v >> 1) | (v >> 2) | (v >> 3)) & 0x11111111;

                fontMask[g][w] = (m << 4) - m;
            }
        }
    }

    // Y la sombra, en los tiles de la fuente (ver "Sombra"). Después de
    // sacar las siluetas: fontMask es sólo la letra.
    shadowFont();
}

// Pinta el bloque de un agujero, con su variante. Las celdas sin dibujo no
// se tocan: son del vecino, si lo hay, o se quedan vacías.
//
// Si la fila tiene texto apuntado encima (ver "Texto encima de los
// agujeros"), las celdas con letra no se pisan y la fila se recompone.
static void drawOne(u16 i)
{
    u16 tx, ty;

    for (ty = 0; ty < HOLE_TILES_H; ty++)
    {
        const s16 cy = holeRow[i] + ty;
        const s16 text = textSlot(cy);

        for (tx = 0; tx < HOLE_TILES_W; tx++)
        {
            const u16 t = (ty * HOLE_TILES_W) + tx;
            const s16 cx = holeCol[i] + tx;

            if (holeCell[t] == HOLE_EMPTY) continue;

            if ((cx < 0) || (cx >= (PLAYFIELD_W / 8)) || (cy < 0) || (cy >= (PLAYFIELD_H / 8)))
                continue;

            {
                const s16 k = sharedIndex(cx, cy);
                const u16 attr = (k >= 0) ? composeShared(k) : cellAttr(i, t);

                // Una letra encima: no se pisa; la fila se vuelve a componer
                // con la variante nueva (ver Hole_textUpdate).
                if ((text >= 0) && !textHidden[text] && (textChars[text][cx] != ' ')) textRestart(text);
                else
                {
                    VDP_setTileMapXY(BG_A, attr, cx, cy);
                    if (text >= 0) textShown[text][cx] = attr;
                }
            }
        }
    }
}

void Hole_rowTiles(s16 row, u16 *buf, u16 cols)
{
    u16 c, e;

    for (c = 0; c < cols; c++) buf[c] = 0;

    if ((row < 0) || (row >= HOLE_ROWS)) return;

    // Las celdas con dibujo de la fila, precalculadas (ver buildRowCells), en
    // el orden de los agujeros: el que va después, encima.
    for (e = rowFirst[row]; e < rowFirst[row + 1]; e++)
    {
        const u16 cx = rcCol[e];

        if (cx >= cols) continue;

        buf[cx] = TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                                 holeActive[rcHole[e]] ? holeRedTile[rcIdx[e]] : (holeTileBase + rcIdx[e]));
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
    // Los agujeros guardados de las filas de texto ya no valen (0 es "sin
    // guardar").
    if (!++holeUnderGen) holeUnderGen = 1;
    if (!holdDraw) drawOne(i);
}

void Hole_draw(void)
{
    u16 i;

    for (i = 0; i < HOLE_COUNT; i++) drawOne(i);
}

s32 Hole_rayFirst(s16 ox, s16 oy, fix16 c, fix16 s)
{
    s32 best = -1;
    u16 i;

    for (i = 0; i < HOLE_COUNT; i++)
    {
        const s32 d = Coll_rayCircle(holeX[i] - ox, holeY[i] - oy, c, s,
                                     ML_X10_TO_F16(ML_RAY_R_HOLE_X10));

        if ((d >= 0) && ((best < 0) || (d < best))) best = d;
    }

    return best;
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

