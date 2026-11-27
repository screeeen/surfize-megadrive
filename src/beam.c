#include "beam.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "resources.h"
#include "gm_art.h"
#include "entity.h"
#include "enemy.h"
#include "snake.h"
#include "projectile.h"
#include "hole.h"
#include "weapon.h"
#include "mlpow.h"
#include "lineseg_tab.h"

#define BEAM_SPR_OFF    (-(BEAM_CELL / 2))
#define DOT_SPR_OFF     (-(DOT_CELL / 2))
#define MAX_LINES       ML_HOLO_MAX
#define MAX_RAY_DOTS    (MLPOW_MAX_RAYS * BEAM_RAY_DOTS)

static MDSpr *segSprite[BEAM_SEGMENTS];
static MDSpr *rayDot[MAX_RAY_DOTS];
static u16    beamTile[8][BEAM_COLORS];
// Cuántos puntos de rayos se enseñaron el frame anterior: sólo hay que
// esconder esos, no el pool entero cada frame.
static u16    rayShown;
static u16    dotTile[DOT_COLORS];

// ---------------------------------------------------------------------------
// Las líneas a las holo: estado
//
// Desde la quinta entrega (2026-11-26) las líneas NO van en el pool de
// sprites: tienen su propio trozo de la tabla del VDP, ya montado, que sólo
// se toca cuando cambia algo. Ver Beam_linesFlush.
//
// Las entradas 40 a 79 de la tabla son de las líneas, al revés: la ranura j
// (0 = la primera en la lista del VDP) es la entrada 79 - j. La lista entra
// por la 79 y baja: cada ranura enlaza con la siguiente (78 - j) y la última
// a la vista con 0. Así los enlaces no dependen de cuántas ranuras quepan
// este frame (las que deja libres el juego, que va en las entradas 0 a
// n - 1): sólo hay que poner a 0 el de la última.
//
// Hay dos copias:
//   - lineSat, la imagen: la ranura j en lineSat[39 - j], colocada SIN
//     sacudida. Es lo que rehace src/beam_a.s y nada más la toca;
//   - vdpSpriteCache[79 - j], la que se sube: la imagen más la sacudida. El
//     montador del juego la pisa cuando necesita esas entradas, así que de
//     cada frame al siguiente sólo vale hasta lineD.
// Con la sacudida en la copia, que cambie es sumar dos palabras largas por
// ranura a la vista al copiar (Beam_linesCopy), sin tocar la imagen.
//
// Cada línea ocupa ranuras seguidas, una detrás de otra en el orden de las
// holo (lineSt[i].slot, .cnt). El reparto por holo es el de siempre: 40 entre
// las que haya, y si una celda tiene más tramos se toman repartidos.
// ---------------------------------------------------------------------------

// 20 bytes y en este orden: src/beam_a.s la lee con desplazamientos fijos
// (LS_*). El rectángulo va como (inicio, ancho - 1) para que la nave esté
// dentro si (u16)(x - xlo) <= xw: una resta y una comparación por eje.
typedef struct
{
    s16 xlo; u16 xw;    // 0, 2   las x de la nave con las que la celda no cambia
    s16 ylo; u16 yw;    // 4, 6
    s16 hx, hy;         // 8, 10  la holo
    u16 key;            // 12     celda | volteo x << 14 | volteo y << 15
    u8  slot, cnt;      // 14, 15 primera ranura y cuántas
    u16 pad[2];         // 16
} LineSt;

// Lo que src/beam_a.s recibe para recorrer las líneas (Beam_linesScan).
typedef struct
{
    LineSt *st;         // 0   la primera línea
    u16 nScan;          // 4   cuántas
    s16 x0, y0;         // 6, 8   la nave
    u16 mode;           // 10  1: mirar x, 2: mirar y, 3: las dos
    s16 bx, by;         // 12, 14 0x80 (la imagen va sin sacudida)
    u16 attr;           // 16  atributo de los tramos sin volteo
    u16 blk;            // 18  tramos por línea como mucho
    const u16 *pickRow; // 20  lineSegPickRow del bloque
    u16 minSlot;        // 24  la ranura más baja reescrita (0xFFFF: ninguna)
    u16 pad[2];         // 26
    u16 budget;         // 30  ranuras que caben
} LineScan;

#define LS_KEY_NONE     0xFFFF          // la línea hay que rehacerla
#define LS_XLO_NONE     0x7FFF          // y con este rectángulo, se mira seguro
#define LINE_SAT_FIRST  (SAT_MAX_SIZE - BEAM_LINE_SPRITES)     // 40

_Static_assert((PLAYFIELD_W == SCREEN_W) && (PLAYFIELD_H == SCREEN_H), "beam_a.s recorta contra la pantalla");
_Static_assert(BEAM_LINE_SPRITES == LINE_SAT_SLOTS, "beam_a.s");
_Static_assert(sizeof(LineSt) == 20, "LineSt: 20 bytes (beam_a.s, LS_SIZE)");
_Static_assert(__builtin_offsetof(LineScan, budget) == 30, "LineScan: desplazamientos de beam_a.s");
_Static_assert(BEAM_LINE_SPRITES <= SAT_MAX_SIZE / 2, "las líneas, en la mitad alta de la tabla");

// No static: los lee src/beam_a.s.
LineSt    lineSt[MAX_LINES];
VDPSprite lineSat[BEAM_LINE_SPRITES];


static u16 lineSegBase;         // primer tile de lineseg.png en VRAM
static u16 lineAttr;            // TILE_ATTR_FULL de los tramos, sin volteo
// El estado de un frame al siguiente, en un struct porque Beam_linesFlush va
// en ensamblador (src/beam_a.s, desplazamientos G_*).
typedef struct
{
    u16 n;              // 0   líneas (holo activas)
    u16 d;              // 2   ranuras [0, d) bien en vdpSpriteCache
    u16 v;              // 4   ranuras [0, v) bien en la VRAM
    u16 k;              // 6   ranuras a la vista el último frame
    u16 end;            // 8   ranuras ocupadas entre todas
    s16 x, y;           // 10, 12 la nave (Beam_update)
    s16 scx, scy;       // 14, 16 la nave del último recorrido
    u16 done;           // 18  líneas [0, done) hechas con la nave en scx, scy
    s16 gx, gy;         // 20, 22 la sacudida de la copia
    u16 force;          // 24  hay líneas que rehacer (holo nuevas)
    u16 blk;            // 26  tramos por línea como mucho
    u16 budget;         // 28  las ranuras que cabían el último frame
} LineGlob;

_Static_assert(__builtin_offsetof(LineGlob, budget) == 28, "LineGlob: desplazamientos de beam_a.s");

LineGlob  lineG;                // no static: src/beam_a.s
LineScan  lineCtx;              // lo de src/beam_a.s que no cambia de frame a frame

#define lineN       lineG.n
#define lineD       lineG.d
#define lineV       lineG.v
#define lineK       lineG.k
#define lineEnd     lineG.end
#define lineX       lineG.x
#define lineY       lineG.y
#define lineForce   lineG.force
#define lineDone    lineG.done
#define lineBlk     lineG.blk

static const u16 *linePickRow;
static bool lineHolosOK;        // las posiciones de las holo, leídas
static u16 lineStamp;           // con este Proj_holoStamp

static void tailRefresh(void);
static void linesReset(u16 n);

// El tope de la esquina de un tramo dentro del campo según su tamaño (el
// byte de tamaño de la tabla del VDP): 320 - ancho y 240 - alto. Lo usa
// src/beam_a.s con las líneas que pasan cerca del borde.
#define CLIP(sz) (PLAYFIELD_W - ((((sz) >> 2) + 1) * 8)), (PLAYFIELD_H - ((((sz) & 3) + 1) * 8))
const u16 lineClip[32] =
{
    CLIP(0), CLIP(1), CLIP(2), CLIP(3), CLIP(4), CLIP(5), CLIP(6), CLIP(7),
    CLIP(8), CLIP(9), CLIP(10), CLIP(11), CLIP(12), CLIP(13), CLIP(14), CLIP(15),
};
#undef CLIP

void Beam_init(void)
{
    u16 i;

    VSpr_load(&beamSprite, &beamTile[0][0], BEAM_COLORS);
    VSpr_load(&dotSprite, dotTile, 1);

    for (i = 0; i < BEAM_SEGMENTS; i++)
        segSprite[i] = VSpr_add(&beamSprite, PAL1, beamTile[0][0]);
}

// Los puntos de las líneas a las réplicas y de las descargas del rayo se
// reparten LOS ÚLTIMOS de todo el pool, y eso es lo que los arregla.
//
// El orden de reparto es el orden de la tabla del VDP: lo que va antes se
// dibuja ENCIMA, y cuando una línea de pantalla pasa de 20 sprites (o 320 px
// de sprite) el VDP deja de dibujar los que van DETRÁS. Los puntos iban
// después de los proyectiles pero antes de los efectos y los paquetes, así
// que se pintaban encima de la humareda y de los paquetes, y una línea
// horizontal de puntos (6 por réplica, hasta 24) se comía el cupo de su
// scanline y hacía desaparecer paquetes y efectos de esa franja. Una línea
// del suelo tiene que ir por debajo de todo, y si algo sobra, que sobre ella.
void Beam_initDots(void)
{
    u16 i;

    // Son la cola del pool (inc/spr.h): el montador no las recorre mientras
    // no haya ninguna a la vista, que es casi siempre. Las líneas a las holo
    // ya no están en el pool (ver Beam_linesFlush): sólo sus tiles, 24,
    // sacados del fondo (VTILE_SHARED_TILES).
    Spr_tailBegin();
    lineSegBase = VTile_alloc(lineSegTiles.numTile);
    VDP_loadTileSet(&lineSegTiles, lineSegBase, DMA);
    lineAttr = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, lineSegBase);
    for (i = 0; i < MAX_RAY_DOTS; i++)
        rayDot[i] = VSpr_add(&dotSprite, PAL1, dotTile[0]);
    Spr_tailEnd();

    // Nacen aparcados.
    Spr_tailVisible(0);
}

void Beam_clear(void)
{
    u16 i;

    for (i = 0; i < BEAM_SEGMENTS; i++) sprHide(segSprite[i]);
    for (i = 0; i < MAX_RAY_DOTS; i++) sprHide(rayDot[i]);
    rayShown = 0;
    // Las líneas, a rehacer enteras, y las holo, a volver a pedir.
    lineHolosOK = FALSE;
    linesReset(0);
    lineK = 0;
    tailRefresh();
}

static void hideBeam(void)
{
    u16 i;

    for (i = 0; i < BEAM_SEGMENTS; i++) sprHide(segSprite[i]);
}

// object1: la cadena de 14 segmentos por delante de la nave.
static void updateBeam(u16 weaponIndex, u16 ammo, fix16 px, fix16 py, fix16 angle)
{
    const u16 rot = Entity_angleFrame8(angle);
    const u16 color = random() & (BEAM_COLORS - 1);
    const fix16 stepX = fmul(fcos(angle), FIX16(BEAM_SPACING));
    const fix16 stepY = fmul(fsin(angle), FIX16(BEAM_SPACING));
    fix16 x = px;
    fix16 y = py;
    u16 i;

    // script0(1) sólo crea object1 con munición, y case 4 sólo crea object0
    // con el arma PARA puesta.
    if ((weaponIndex != WPN_PARA) || (ammo == 0))
    {
        hideBeam();
        return;
    }

    for (i = 0; i < BEAM_SEGMENTS; i++)
    {
        const s16 ix = F2I(x);
        const s16 iy = F2I(y);

        // object1/Collision_oMalo: resta 1 y se destruye entero. Lo recrea
        // script0 al frame siguiente, así que el haz parpadea sobre el
        // enemigo.
        //
        // NO MATA, y esto rompía la partida: el port usaba Enemy_hit, que
        // además de restar comprueba la muerte y destruye. Como la araña
        // tiene hp 0, el haz la borraba de un roce, y encima sin sumar kill
        // ni pasar por checkMorte. Con el PARA puesto, el haz iba limpiando
        // la arena en silencio; como las oleadas sólo salen cuando muere una
        // CABEZA por checkMorte, dejaban de salir enemigos y la partida se
        // quedaba vacía para siempre. Medido: 500 frames con el PARA, cero
        // kills nuevos y cero arañas.
        if (Enemy_damageNoKill(ix, iy, 1, 1, 1) ||
            Snake_damageNoKill(ix, iy, 1, 1, 1))
        {
            hideBeam();
            return;
        }

        // El haz sale por delante de la nave y se sale del campo cuando está
        // pegada al borde: lo que se sale se aparca (ver inc/spr.h).
        if (MDSPR_OFFSCREEN(ix, iy))
        {
            sprHide(segSprite[i]);
        }
        else
        {
            sprPos(segSprite[i], ix + BEAM_SPR_OFF, iy + BEAM_SPR_OFF);
            sprAnimFrame(segSprite[i], rot, color);
            sprTile(segSprite[i], beamTile[rot][color]);
        }

        x += stepX;
        y += stepY;
    }
}

// Cuántas ranuras de la cola tiene que recorrer el montador: los rayos, si
// los hay (las líneas ya no van en el pool).
static void tailRefresh(void)
{
    Spr_tailVisible(rayShown ? 0xFFFF : 0);
    Spr_tailEntries(rayShown);
}

// Los enlaces fijos de las ranuras (ver arriba): la j enlaza con la entrada
// 78 - j. La última a la vista la pone a 0 Beam_linesFlush.
static FORCE_INLINE void lineLink(u16 j)
{
    lineSat[(BEAM_LINE_SPRITES - 1) - j].link = (u8)((SAT_MAX_SIZE - 2) - j);
}

static void linesReset(u16 n)
{
    u16 i;

    lineN = n;
    lineBlk = n ? lineSegBlkLen[lineSegBlkOf[n]] : 0;
    linePickRow = &lineSegPickRow[(n ? lineSegBlkOf[n] : 0) * (LINESEG_MAXC + 1)];
    for (i = 0; i < MAX_LINES; i++)
    {
        LineSt *l = &lineSt[i];

        l->xlo = LS_XLO_NONE; l->xw = 0;
        l->ylo = LS_XLO_NONE; l->yw = 0;
        l->key = LS_KEY_NONE;
        l->slot = 0;
        l->cnt = 0;
    }
    for (i = 0; i < BEAM_LINE_SPRITES; i++) lineLink(i);
    lineEnd = 0;
    lineD = 0;
    lineV = 0;
    lineDone = 0;
    lineForce = TRUE;
    lineCtx.attr = lineAttr;
    lineCtx.blk = lineBlk;
    lineCtx.pickRow = linePickRow;
    lineCtx.st = lineSt;
    lineCtx.bx = 0x80;
    lineCtx.by = 0x80;
}

// La línea l pasa a tener cnt ranuras: las de las líneas de detrás se corren
// (con sus enlaces, que son de la ranura y no del tramo). Lo llama
// src/beam_a.s cuando cambia el número de tramos de una línea, que con 10
// holo casi nunca pasa (son 4 y las celdas tienen más).
void Beam_lineResize(LineSt *l, u16 cnt)
{
    const s16 d = (s16)cnt - (s16)l->cnt;
    const u16 from = l->slot + l->cnt;
    const u16 m = lineEnd - from;
    LineSt *e = &lineSt[lineN];
    LineSt *k;
    u16 j;

    if (d == 0) return;

    // La ranura j está en lineSat[39 - j]: las de detrás de la línea (de from
    // a lineEnd) son un trozo seguido que baja en memoria.
    if (m)
    {
        VDPSprite *src = &lineSat[BEAM_LINE_SPRITES - from - m];
        VDPSprite *dst = src - d;

        if (d > 0) for (j = 0; j < m; j++) dst[j] = src[j];
        else       for (j = m; j-- > 0; )  dst[j] = src[j];
        for (j = from + d; j < from + d + m; j++) lineLink(j);
    }
    for (k = l + 1; k < e; k++) k->slot = (u8)(k->slot + d);
    lineEnd = (u16)(lineEnd + d);
    l->cnt = (u8)cnt;
    if (lineD > l->slot) lineD = l->slot;
}

// La línea azul de la nave a cada holo activa (gizmosProxy de MonsterLite,
// oPlayer/Draw_0 del GML). Del grosor de la estela y de puntos muy juntos
// (decisión del usuario del 2026-11-21): tramos de varios puntos de 2x2 a
// ~3 px, uno por sprite (tools/ml_lineseg.py).
//
// Aquí sólo se apunta dónde están la nave y las holo; las líneas se rehacen
// en Beam_linesFlush, que corre dentro de Spr_flush y ya sabe cuántas
// entradas de la tabla le quedan libres.
// Las holo han cambiado pero el reparto por línea no (40 / holo da lo
// mismo, p. ej. de 10 a 9): las líneas de las que siguen se quedan como
// están. Las holo no se mueven y Proj_triPositions las da en el orden del
// pool, que no cambia, así que basta con quitar las que ya no están (sus
// ranuras se cierran, Beam_lineResize) y meter las nuevas en su sitio, sin
// ranuras, para que se hagan en el siguiente Beam_linesFlush. Antes se
// rehacían todas: ~10 000 ciclos de golpe cada vez que moría una holo.
// Todas de nuevo, con las holo en (hx, hy).
static void linesResetAt(const s16 *hx, const s16 *hy, u16 n)
{
    u16 i;

    linesReset(n);
    for (i = 0; i < n; i++) { lineSt[i].hx = hx[i]; lineSt[i].hy = hy[i]; }
}

static void linesRemap(const s16 *hx, const s16 *hy, u16 n)
{
    s16 j;
    u16 i, k;
    bool used[MAX_LINES], alive[MAX_LINES];

    // Cada línea vieja se empareja con una holo nueva en su sitio que no
    // tenga ya pareja: dos holo en el mismo píxel son dos líneas, y si muere
    // una, sobra una (antes las dos encontraban la misma y quedaba una
    // línea fantasma).
    for (i = 0; i < n; i++) used[i] = FALSE;
    for (j = 0; j < (s16)lineN; j++)
    {
        alive[j] = FALSE;
        for (i = 0; i < n; i++)
            if (!used[i] && (hx[i] == lineSt[j].hx) && (hy[i] == lineSt[j].hy))
            { used[i] = TRUE; alive[j] = TRUE; break; }
    }

    for (j = (s16)lineN - 1; j >= 0; j--)
    {
        LineSt *l = &lineSt[j];

        if (alive[j]) continue;

        Beam_lineResize(l, 0);
        for (k = (u16)j; k + 1 < lineN; k++) lineSt[k] = lineSt[k + 1];
        lineN--;
        if (lineDone > (u16)j) lineDone--;
    }

    // Tras quitar las que no están, quedan como mucho n: si no, algo no
    // cuadra (no debería pasar, el orden del pool no cambia) y se rehacen
    // todas antes de meter nada.
    if (lineN > n)
    {
        linesResetAt(hx, hy, n);
        return;
    }
    for (i = 0; i < n; i++)
    {
        LineSt *l = &lineSt[i];

        if ((i < lineN) && (hx[i] == l->hx) && (hy[i] == l->hy)) continue;

        for (k = lineN; k > i; k--) lineSt[k] = lineSt[k - 1];
        lineN++;
        l->slot = i ? (u8)(lineSt[i - 1].slot + lineSt[i - 1].cnt) : 0;
        l->cnt = 0;
        l->hx = hx[i];
        l->hy = hy[i];
        l->key = LS_KEY_NONE;
        l->xlo = LS_XLO_NONE;
        l->xw = 0;
        l->ylo = LS_XLO_NONE;
        l->yw = 0;
        if (lineDone > i) lineDone = i;
    }
    // Las que siguen están en el mismo orden (el del pool), así que esto
    // cuadra siempre; si algún día no, se rehacen todas.
    if (lineN != n) { linesResetAt(hx, hy, n); return; }
    lineForce = TRUE;
}

static void updateLines(fix16 px, fix16 py)
{
    u16 i;

    lineX = F2I(px);
    lineY = F2I(py);

    // Las posiciones de las holo, sólo si alguna ha nacido, se ha activado o
    // ha muerto (no se mueven): recorrer el pool de proyectiles cada frame
    // costaba lo suyo (medido: 4 vueltas de 600 con 10 holo).
    if (!lineHolosOK || (Proj_holoStamp() != lineStamp))
    {
        fix16 tx[MAX_LINES], ty[MAX_LINES];
        s16 hx[MAX_LINES], hy[MAX_LINES];
        const u16 n = Proj_triPositions(tx, ty, MAX_LINES);

        for (i = 0; i < n; i++) { hx[i] = F2I(tx[i]); hy[i] = F2I(ty[i]); }
        lineStamp = Proj_holoStamp();

        if (lineHolosOK && lineN && n && (lineSegBlkOf[n] == lineSegBlkOf[lineN]))
        {
            linesRemap(hx, hy, n);
            return;
        }

        lineHolosOK = TRUE;
        if (n != lineN) linesReset(n);
        for (i = 0; i < n; i++)
        {
            LineSt *l = &lineSt[i];

            if ((hx[i] != l->hx) || (hy[i] != l->hy) || (l->key == LS_KEY_NONE))
            {
                l->hx = hx[i];
                l->hy = hy[i];
                l->key = LS_KEY_NONE;
                l->xlo = LS_XLO_NONE;
                l->xw = 0;
                l->ylo = LS_XLO_NONE;
                l->yw = 0;
                lineForce = TRUE;
            }
        }
    }
}

// Las líneas, con lo que el juego deja libre de la tabla del VDP: ha usado
// las entradas 0 a nGame - 1, así que caben 80 - nGame ranuras (40 como
// mucho). Devuelve cuántas van a la vista, que Spr_flush engancha detrás de
// las suyas, y en *satLen cuántas entradas de vdpSpriteCache hay que subir.
//
// Lo que cuesta (ver docs/PORT_AUDIT_GML.md, quinta entrega):
//   - nada se mira ni se rehace de lo que no cabe: el recorrido para en la
//     primera línea que no quepa, y lo de detrás se mira cuando vuelva a
//     caber;
//   - con la nave quieta, sin sacudida nueva y sin holo nuevas, sólo esto;
//   - con la nave en marcha, una resta y una comparación por línea y eje
//     movido (src/beam_a.s) y rehacer las que cambian de celda: dos sumas de
//     palabras largas por tramo;
//   - con la sacudida, copiar las ranuras a la vista sumándosela (~55
//     ciclos por ranura);
//   - la subida va en el mismo DMA que la tabla del juego, y sólo llega
//     hasta las ranuras que han cambiado.
// (en src/beam_a.s, Beam_linesFlush: lo que hace, paso a paso, está allí)

#ifndef PORT_NO_ASM
#define PORT_NO_ASM 0
#endif

#if PORT_NO_ASM
// ---------------------------------------------------------------------------
// Las versiones en C de src/beam_a.s (PORT_NO_ASM=1): portables, más lentas,
// la misma cuenta paso a paso. Si se toca el ensamblador, se toca esto.
// ---------------------------------------------------------------------------
void Beam_lineResize(LineSt *l, u16 cnt);

static u16 lineCellRect(s16 d, s16 h, u16 qmax, s16 *lo, u16 *w, bool *m)
{
    u16 a, q, base;

    *m = (d < 0);
    a = (u16)(*m ? -d : d);
    base = a & 0xFFF8;
    q = a >> 3;
    if (!*m)
    {
        *lo = h + (s16)base;
        if (q < qmax) *w = 7;
        else { *w = 0x3FFF - base; q = qmax; }
    }
    else if (q < qmax)
    {
        *lo = h - (s16)base - 7;
        *w = q ? 7 : 6;
    }
    else
    {
        *lo = h - 0x3FFF;
        *w = 0x3FFF - base;
        q = qmax;
    }
    return q;
}

static void lineMiss(LineScan *c, LineSt *l)
{
    bool mx, my;
    const u16 qx = lineCellRect(c->x0 - l->hx, l->hx, LINESEG_QX - 1, &l->xlo, &l->xw, &mx);
    const u16 qy = lineCellRect(c->y0 - l->hy, l->hy, LINESEG_QY - 1, &l->ylo, &l->yw, &my);
    const u16 flips = (mx ? TILE_ATTR_HFLIP_MASK : 0) | (my ? TILE_ATTR_VFLIP_MASK : 0);
    const u16 cell = lineSegRow[qy] + qx;
    const u16 key = cell | (flips << 3);
    const u16 first = lineSegIdx[cell];
    const u16 count = lineSegIdx[cell + 1] - first;
    const u8 *rec = (const u8 *)&lineSegRec[(u32)first * (LINESEG_REC / 2)];
    const u16 *pick = &lineSegPick[c->pickRow[count]];
    const s16 *box = &lineSegBox[cell * 4];
    const u16 oy = my ? 4 : 0, ox = mx ? 12 : 8;
    u16 take = (count < c->blk) ? count : c->blk;
    u16 j, used = 0;
    u32 k1, k2;
    u8 *dst;
    s16 x0b, x1b, y0b, y1b;

    if (key == l->key) return;
    l->key = key;
    if (take != l->cnt) Beam_lineResize(l, take);
    if (l->slot < c->minSlot) c->minSlot = l->slot;

    k1 = ((u32)(u16)(l->hy + c->by) << 16) | (u16)((SAT_MAX_SIZE - 2) - l->slot);
    k2 = ((u32)(c->attr | flips) << 16) + (u32)(s32)(l->hx + c->bx);
    dst = (u8 *)&lineSat[(BEAM_LINE_SPRITES - 1) - l->slot];

    x0b = mx ? (l->hx - box[1]) : (l->hx + box[0]);
    x1b = mx ? (l->hx - box[0]) : (l->hx + box[1]);
    y0b = my ? (l->hy - box[3]) : (l->hy + box[2]);
    y1b = my ? (l->hy - box[2]) : (l->hy + box[3]);
    for (j = 0; j < take; j++)
    {
        const u8 *sg = rec + pick[j];
        const u32 a = *(const u32 *)(sg + oy) + k1;
        const u32 b = *(const u32 *)(sg + ox) + k2;

        if ((x0b < 0) || (x1b > PLAYFIELD_W - 1) || (y0b < 0) || (y1b > PLAYFIELD_H - 1))
        {
            const u16 sz = (u16)(a >> 8) & 0xF;
            if ((u16)((u16)b - c->bx) > lineClip[sz * 2]) continue;
            if ((u16)((u16)(a >> 16) - c->by) > lineClip[sz * 2 + 1]) continue;
        }
        ((u32 *)dst)[0] = a;
        ((u32 *)dst)[1] = b;
        dst -= sizeof(VDPSprite);
        k1--;
        used++;
    }
    if (used != take) Beam_lineResize(l, used);
}

u16 Beam_linesScan(LineScan *c)
{
    LineSt *l = c->st;
    u16 i;

    for (i = 0; i < c->nScan; i++, l++)
    {
        bool miss = FALSE;

        if (c->budget <= l->slot) break;
        if ((c->mode & 1) && ((u16)(c->x0 - l->xlo) > l->xw)) miss = TRUE;
        if ((c->mode & 2) && ((u16)(c->y0 - l->ylo) > l->yw)) miss = TRUE;
        if (miss) lineMiss(c, l);
    }
    return i;
}

void Beam_linesCopy(const VDPSprite *src, VDPSprite *dst, u16 count, s32 dy16, s32 dx)
{
    const u32 *s = (const u32 *)src;
    u32 *d = (u32 *)dst;

    while (count--)
    {
        *d++ = *s++ + (u32)dy16;
        *d++ = *s++ + (u32)dx;
    }
}

u16 Beam_linesFlush(u16 nGame, u16 *satLen)
{
    u16 budget = SAT_MAX_SIZE - nGame, mode, k;
    LineScan *c = &lineCtx;

    *satLen = nGame;
    if (budget > BEAM_LINE_SPRITES) budget = BEAM_LINE_SPRITES;
    if (lineN == 0) { lineK = 0; return 0; }
    if ((budget == lineG.budget) && (lineX == lineG.scx) && (lineY == lineG.scy) && !lineForce &&
        (g_shakeX == lineG.gx) && (g_shakeY == lineG.gy) && (lineV >= lineK)) return lineK;
    lineG.budget = budget;
    if (lineD > budget) lineD = budget;

    mode = ((lineX != lineG.scx) ? 1 : 0) | ((lineY != lineG.scy) ? 2 : 0);
    if (lineForce) { lineForce = FALSE; lineDone = 0; }
    lineG.scx = lineX;
    lineG.scy = lineY;
    c->x0 = lineX;
    c->y0 = lineY;
    c->budget = budget;
    c->minSlot = 0xFFFF;
    if (mode && lineDone)
    {
        u16 done;

        c->st = lineSt;
        c->nScan = lineDone;
        c->mode = mode;
        done = Beam_linesScan(c);
        if (done < lineDone) lineDone = done;
    }
    if ((lineDone < lineN) && (lineSt[lineDone].slot < budget))
    {
        c->st = &lineSt[lineDone];
        c->nScan = lineN - lineDone;
        c->mode = 3;
        lineDone += Beam_linesScan(c);
    }
    if (lineD > c->minSlot) lineD = c->minSlot;

    k = (lineEnd < budget) ? lineEnd : budget;
    if (k != lineK)
    {
        if (lineK)
        {
            lineLink(lineK - 1);
            if (lineD > lineK - 1) lineD = lineK - 1;
        }
        lineK = k;
        if (k && (lineD > k - 1)) lineD = k - 1;
    }
    if (k == 0)
    {
        if (lineV > budget) lineV = budget;
        return 0;
    }
    lineSat[BEAM_LINE_SPRITES - k].link = 0;
    if ((g_shakeX != lineG.gx) || (g_shakeY != lineG.gy))
    {
        lineG.gx = g_shakeX;
        lineG.gy = g_shakeY;
        lineD = 0;
    }
    if (lineD < k)
    {
        Beam_linesCopy(&lineSat[BEAM_LINE_SPRITES - k], &vdpSpriteCache[SAT_MAX_SIZE - k], k - lineD,
                       -((s32)lineG.gy << 16), -(s32)lineG.gx);
        if (lineV > lineD) lineV = lineD;
        lineD = k;
    }
    if (lineV < k)
    {
        *satLen = SAT_MAX_SIZE - lineV;
        lineV = k;
    }
    else if (lineV > budget) lineV = budget;
    return k;
}
#endif

// La cola de DMA estaba llena y la tabla no se ha subido: nada de las
// líneas está en la VRAM con seguridad.
void Beam_linesDmaLost(void)
{
    lineV = 0;
}

const VDPSprite *Beam_lineEntries(u16 *count)
{
    *count = lineK;
    return &vdpSpriteCache[SAT_MAX_SIZE - lineK];
}

// RayoScr.Update de MonsterLite: la línea de la nave a cada objetivo, con
// ruido y un color al azar por rayo y frame (aquí azul, plata o amarillo, los
// de dot.png). Los vértices 2, 4 y 6 de los 9.
static void updateRays(fix16 px, fix16 py)
{
    u16 slot = 0, n, r, d;
    const MLRay *rays = MLPow_rays(&n);
    const s16 ox = F2I(px);
    const s16 oy = F2I(py);

    for (r = 0; r < n; r++)
    {
        const u16 color = random() % DOT_COLORS;
        const s16 dx = rays[r].tx - ox;
        const s16 dy = rays[r].ty - oy;

        for (d = 1; d <= BEAM_RAY_DOTS; d++)
        {
            // Vector3.Lerp(nave, objetivo, i / 8) con i = 2d, más el ruido.
            const s16 x = ox + ((dx * (s16)d) >> 2) + (s16)(random() % (2 * ML_RAY_NOISE + 1)) - ML_RAY_NOISE;
            const s16 y = oy + ((dy * (s16)d) >> 2) + (s16)(random() % (2 * ML_RAY_NOISE + 1)) - ML_RAY_NOISE;

            if (MDSPR_OFFSCREEN(x + DOT_SPR_OFF, y + DOT_SPR_OFF)) sprHide(rayDot[slot]);
            else
            {
                sprPos(rayDot[slot], x + DOT_SPR_OFF, y + DOT_SPR_OFF);
                sprAnim(rayDot[slot], color);
                sprTile(rayDot[slot], dotTile[color]);
            }
            slot++;
        }
    }

    {
        const u16 shown = slot;

        while (slot < rayShown) sprHide(rayDot[slot++]);
        rayShown = shown;
        tailRefresh();
    }
}

void Beam_updateRays(fix16 px, fix16 py)
{
    updateRays(px, py);
}

void Beam_update(u16 weaponIndex, u16 ammo, fix16 px, fix16 py, fix16 angle)
{
    updateBeam(weaponIndex, ammo, px, py, angle);
    updateLines(px, py);
    updateRays(px, py);
}
