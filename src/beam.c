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

#define BEAM_SPR_OFF    (-(BEAM_CELL / 2))
#define DOT_SPR_OFF     (-(DOT_CELL / 2))
#define MAX_LINES       ML_HOLO_MAX
#define MAX_RAY_DOTS    (MLPOW_MAX_RAYS * BEAM_RAY_DOTS)

static MDSpr *segSprite[BEAM_SEGMENTS];
static MDSpr *lineDot[BEAM_LINE_SPRITES];
static MDSpr *rayDot[MAX_RAY_DOTS];
static u16    beamTile[8][BEAM_COLORS];
// Cuántos puntos de líneas y de rayos se enseñaron el frame anterior: sólo
// hay que esconder esos, no el pool entero cada frame.
static u16    rayShown;
static u16    dotTile[DOT_COLORS];
// 32768 / k para k = 0..LINE_MAX_LEN (el 0 y el 1 no se usan): las
// divisiones de updateLines, como multiplicación. DIVS y DIVU cuestan unos
// 150 ciclos cada una.
#define LINE_MAX_LEN    400     // la diagonal de 320x240
static u16 recip[LINE_MAX_LEN + 1];

// Los tramos de las líneas a las holo (tools/ml_lineseg.py): 7 direcciones
// entre 0 y 90 grados, cada una con su sprite de varios puntos de 2x2 a ~3 px.
// De 90 a 180, la misma con volteo vertical.
typedef struct
{
    u8  tile;       // primer tile dentro de lineseg.png
    u8  size;       // SPRITE_SIZE del sprite
    u8  ax, ay;     // el centro del primer punto dentro del sprite
    u8  h;          // alto del sprite en px (para el volteo)
    u16 adv16;      // avance de un tramo al siguiente, en 1/16 px
    u16 segK;       // 32768 * 16 / adv16: tramos = len * segK >> 15
} LineSeg;

static const LineSeg lineSeg[7] =
{
    {  0, SPRITE_SIZE(4, 1), 2, 4,  8, 480, 1092 },   // (3,0)  0,0 grados, 10 puntos
    {  4, SPRITE_SIZE(2, 1), 2, 2,  8, 253, 2072 },   // (3,1) 18,4 grados, 5
    {  6, SPRITE_SIZE(2, 2), 2, 4, 16, 288, 1820 },   // (3,2) 33,7 grados, 5
    { 10, SPRITE_SIZE(2, 2), 2, 2, 16, 317, 1654 },   // (2,2) 45,0 grados, 7
    { 14, SPRITE_SIZE(2, 2), 4, 2, 16, 288, 1820 },   // (2,3) 56,3 grados, 5
    { 18, SPRITE_SIZE(1, 2), 2, 2, 16, 253, 2072 },   // (1,3) 71,6 grados, 5
    { 20, SPRITE_SIZE(1, 4), 4, 2, 32, 480, 1092 },   // (0,3) 90,0 grados, 10
};

// coseno y seno de cada dirección, en 1/256: la longitud de la línea es su
// proyección sobre la dirección elegida (como mucho a 9,2 grados de la de
// verdad: se queda corta un 1,3 % a lo sumo), dos multiplicaciones en vez de
// una raíz cuadrada.
static const u16 lineSegCos[7] = { 256, 243, 213, 181, 142, 81, 0 };
static const u16 lineSegSin[7] = { 0, 81, 142, 181, 213, 243, 256 };

// tan de los ángulos a medio camino entre dos direcciones, en 1/256:
// 9,2 / 26,1 / 39,4 / 50,6 / 63,9 / 80,8 grados.
static const u16 lineSegTan[6] = { 42, 125, 210, 312, 524, 1577 };

static u16 lineSegBase;     // primer tile de lineseg.png en VRAM

// Estado de las líneas, que se conserva de un frame al siguiente (ver
// updateLines): cada línea tiene su bloque fijo de sprites (lineBlk, a partes
// iguales entre las que haya) y su cuenta de sprites por franja de 8 px, para
// poder rehacer sólo algunas en cada frame sin perder el tope global por
// franja.
#define LINE_BANDS      ((PLAYFIELD_H / 8) + 1)
#define LINES_PER_FRAME 2       // líneas que se rehacen por frame si se mueve la nave
static u16 lineN = 0xFFFF;      // holo de la última vez (0xFFFF: rehacer todo)
static u16 lineBlk;             // sprites por línea
static u8  lineCnt[MAX_LINES];  // sprites usados en el bloque de cada línea
static u8  lineBand[MAX_LINES][LINE_BANDS];
static u8  bandUse[LINE_BANDS]; // suma de lineBand: sprites de línea por franja
static s16 lineKey[MAX_LINES][4];   // nave x, y y holo x, y con los que se hizo
static u16 lineRR;              // la siguiente línea por turno
static u16 lineHigh;            // sprites de línea copiados a la cola (seguidos)
static u16 lineVis;             // sprites de línea a la vista
// Los tramos se calculan aquí, fuera del pool, en bloques fijos por línea
// (posición sin la sacudida), y los que se ven se copian seguidos a la cola
// del pool (lineDot): así el montador no recorre huecos aparcados.
static MDSpr lineVirt[BEAM_LINE_SPRITES];
static u8  lineB0[MAX_LINES], lineB1[MAX_LINES];  // franjas que ocupa cada línea
static s16 lineLastX, lineLastY;    // la nave la última vez que se miró
static bool lineDirty;          // quedan líneas por rehacer
static u8  lineTick;
static s16 lineSX, lineSY;      // sacudida con la que están colocados

// La dirección de cada (ax, ay) con los dos por debajo de 32: una tabla en
// vez de las seis comparaciones con multiplicación (Beam_init).
static u8 dirLUT[32 * 32];



void Beam_init(void)
{
    u16 i;

    VSpr_load(&beamSprite, &beamTile[0][0], BEAM_COLORS);
    VSpr_load(&dotSprite, dotTile, 1);

    recip[0] = recip[1] = 0;
    for (i = 2; i <= LINE_MAX_LEN; i++) recip[i] = (u16)(32768U / i);

    {
        u16 ax, ay, d;

        for (ay = 0; ay < 32; ay++)
            for (ax = 0; ax < 32; ax++)
            {
                for (d = 0; d < 6; d++)
                    if ((ay << 8) < (ax * lineSegTan[d])) break;
                dirLUT[(ay << 5) | ax] = (u8)d;
            }
    }

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
    // no haya ninguna a la vista, que es casi siempre.
    Spr_tailBegin();
    // Los tramos de las líneas a las holo: 24 tiles, sacados del fondo
    // (VTILE_SHARED_TILES). Cada sprite cambia de tamaño y de tile según la
    // dirección (ver updateLines); se reparten con un 8x8 cualquiera.
    lineSegBase = VTile_alloc(lineSegTiles.numTile);
    VDP_loadTileSet(&lineSegTiles, lineSegBase, DMA);
    for (i = 0; i < BEAM_LINE_SPRITES; i++)
        lineDot[i] = VSpr_add(&trailSprite, PAL1, lineSegBase);

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
    for (i = 0; i < BEAM_LINE_SPRITES; i++) sprHide(lineDot[i]);
    for (i = 0; i < MAX_RAY_DOTS; i++) sprHide(rayDot[i]);
    rayShown = 0;
    lineN = 0xFFFF;         // las líneas, a rehacer enteras
    lineHigh = 0;
    lineVis = 0;
    Spr_tailVisible(0);
    Spr_tailEntries(0);
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

// La línea azul de la nave a cada holo activa (gizmosProxy de MonsterLite,
// oPlayer/Draw_0 del GML). Desde el 2026-11-21 (decisión del usuario), del
// grosor de la estela y de puntos muy juntos: tramos de puntos de 2x2 a ~3 px
// (2,8-3,6 según la dirección, tools/ml_lineseg.py), cada uno un sprite,
// puestos uno detrás de otro sobre la línea de verdad. Como cada tramo va en
// una de 7 direcciones, se vuelve a apoyar en la línea al empezar el
// siguiente. Los tramos se centran en la línea: lo que sobra (menos de un
// tramo) queda repartido en los dos extremos, debajo de la nave y de la holo.
//
// Topes (inc/beam.h): BEAM_LINE_SPRITES tramos entre todas las líneas, a
// partes iguales (si a una no le llegan, sus tramos se separan y quedan
// huecos), y BEAM_LINE_BAND sprites de línea por franja de 8 px (lo que se
// pase se salta y deja un hueco).
// Cuántas ranuras de la cola tiene que recorrer el montador: todas si hay
// rayos (van detrás de las líneas), si no hasta el último sprite de línea.
static void tailRefresh(void)
{
    Spr_tailVisible(rayShown ? 0xFFFF : lineHigh);
    Spr_tailEntries(lineHigh + rayShown);
}

// Rehace la línea i (de la nave en (x0, y0) a la holo en (x1, y1)) en su
// bloque de sprites.
static void buildLine(u16 i, s16 x0, s16 y0, s16 x1, s16 y1)
{
    MDSpr *blk = &lineVirt[i * lineBlk];
    u8 *lb = lineBand[i];
    s16 dx = x1 - x0, dy = y1 - y0;
    const LineSeg *g;
    u16 ax, ay, len, d, segs, flip, used = 0, b, k;
    u16 step16, offs16, rec;
    s32 cx, cy, sx15, sy15;

    lineKey[i][0] = x0; lineKey[i][1] = y0;
    lineKey[i][2] = x1; lineKey[i][3] = y1;

    // Fuera lo que tenía: sus franjas, del total (sólo el tramo que ocupaba).
    for (b = lineB0[i]; b <= lineB1[i]; b++) { bandUse[b] -= lb[b]; lb[b] = 0; }
    lineB0[i] = LINE_BANDS - 1;
    lineB1[i] = 0;

    // Las líneas no tienen sentido: se recorren siempre hacia la derecha,
    // y si suben, con el tramo volteado.
    if (dx < 0) { x0 = x1; y0 = y1; dx = -dx; dy = -dy; }
    ax = dx;
    ay = (dy < 0) ? -dy : dy;
    flip = (dy < 0);

    // La dirección más cercana: la tabla, con (ax, ay) reducidos a menos de
    // 32 (la proporción es lo único que cuenta).
    {
        u16 rx = ax, ry = ay;

        while ((rx | ry) >= 32) { rx >>= 1; ry >>= 1; }
        d = dirLUT[(ry << 5) | rx];
    }
    g = &lineSeg[d];

    len = (u16)((((u32)(u16)ax * (u32)(u16)lineSegCos[d]) + ((u32)(u16)ay * (u32)(u16)lineSegSin[d])) >> 8);
    if (len > LINE_MAX_LEN) len = LINE_MAX_LEN;
    segs = 0;
    if (len >= 2)
    {
        // Cuántos tramos caben enteros: len * 16 / adv16, por el inverso de
        // la tabla, y un ajuste de uno por el redondeo.
        const u16 l16 = (u16)(len << 4);

        segs = (u16)(((u32)(u16)len * (u32)(u16)g->segK) >> 15);
        if ((u16)((u32)(u16)(segs + 1) * (u32)(u16)g->adv16) <= l16) segs++;
        else if (segs && ((u16)((u32)(u16)segs * (u32)(u16)g->adv16) > l16)) segs--;
    }

    if (segs)
    {
        if (segs <= lineBlk)
        {
            step16 = g->adv16;
            offs16 = (u16)(((u16)(len << 4) - (u16)((u32)(u16)segs * (u32)(u16)g->adv16)) >> 1);
        }
        else
        {
            // No llegan: los que haya, repartidos a lo largo, con huecos.
            const u16 l16 = (u16)(len << 4);

            segs = lineBlk;
            step16 = (u16)(((u32)l16 * (u32)recip[segs]) >> 15);
            offs16 = (step16 - g->adv16) >> 1;
        }

        rec = recip[len];

        // Dónde empieza cada tramo: a offs16 + k * step16 (1/16 px) del
        // origen, como fracción de la línea en 1/32768. Las dos fracciones
        // se calculan una vez por línea y los tramos se recorren sumando, en
        // 1/32768 de píxel.
        {
            const u16 t0 = (u16)(((u32)(u16)offs16 * (u32)(u16)rec) >> 4);
            const u16 ts = (u16)(((u32)(u16)step16 * (u32)(u16)rec) >> 4);

            cx = ((s32)x0 << 15) + ((s32)(s16)dx * (s32)(s16)t0);
            cy = ((s32)y0 << 15) + ((s32)(s16)dy * (s32)(s16)t0);
            sx15 = (s32)(s16)dx * (s32)(s16)ts;
            sy15 = (s32)(s16)dy * (s32)(s16)ts;
        }

        for (k = 0; k < segs; k++, cx += sx15, cy += sy15)
        {
            const s16 ox = (s16)(cx >> 15) - g->ax;
            const s16 oy = (s16)(cy >> 15) - (flip ? (s16)(g->h - g->ay) : (s16)g->ay);
            u16 b0, b1;
            bool full = FALSE;

            if ((oy < 0) || (ox < 0) || ((oy + g->h) > PLAYFIELD_H) || (ox >= PLAYFIELD_W)) continue;

            b0 = oy >> 3;
            b1 = (oy + g->h - 1) >> 3;
            for (b = b0; b <= b1; b++) if (bandUse[b] >= BEAM_LINE_BAND) full = TRUE;
            if (full) continue;
            for (b = b0; b <= b1; b++) { bandUse[b]++; lb[b]++; }
            if (b0 < lineB0[i]) lineB0[i] = (u8)b0;
            if (b1 > lineB1[i]) lineB1[i] = (u8)b1;

            {
                MDSpr *sp = &blk[used++];

                // Sin la sacudida: se le resta al copiarlo a la cola.
                sp->x = ox + 0x80;
                sp->y = oy + 0x80;
                sp->sizeLink = ((u16)g->size) << 8;
                sp->attr = TILE_ATTR_FULL(PAL1, FALSE, flip, FALSE, lineSegBase + g->tile);
            }
        }
    }

    lineVis = lineVis - lineCnt[i] + used;
    lineCnt[i] = (u8)used;
}

// La línea azul de la nave a cada holo activa (gizmosProxy de MonsterLite,
// oPlayer/Draw_0 del GML). Desde el 2026-11-21 (decisión del usuario), del
// grosor de la estela y de puntos muy juntos: tramos de puntos de 2x2 a ~3 px
// (tools/ml_lineseg.py), uno por sprite, puestos uno detrás de otro sobre la
// línea de verdad (buildLine).
//
// Para que no cueste (decisión del usuario del 2026-11-22), lo hecho se
// guarda de un frame al siguiente:
//   - si cambia el número de holo, se borran todas y vuelven a salir por
//     turnos;
//   - si sólo cambia la sacudida, se mueven los sprites ya puestos lo mismo
//     que ella (sprPos le resta la sacudida);
//   - si se mueve la nave, se rehacen LINES_PER_FRAME líneas por frame, por
//     turnos: con 10 holo, cada línea va como mucho 4 frames tarde (unos
//     6 px a la velocidad de la nave), y su extremo queda debajo de la nave;
//   - si nada se ha movido, no se hace nada.
static void updateLines(fix16 px, fix16 py)
{
    fix16 tx[MAX_LINES], ty[MAX_LINES];
    const u16 n = Proj_triPositions(tx, ty, MAX_LINES);
    const s16 x0 = F2I(px), y0 = F2I(py);
    u16 i, k, done = 0;

    if ((n == 0) && (lineN == 0)) return;

    if (n != lineN)
    {
        // Todo de nuevo, con bloques a partes iguales: se borra todo y las
        // líneas salen por turnos, como cuando se mueve la nave (rehacer
        // las 10 de golpe se comía el frame).
        for (i = 0; i < BEAM_LINE_SPRITES; i++) sprHide(lineDot[i]);
        memset(lineCnt, 0, sizeof(lineCnt));
        memset(lineBand, 0, sizeof(lineBand));
        memset(bandUse, 0, sizeof(bandUse));
        for (i = 0; i < MAX_LINES; i++) { lineKey[i][0] = 0x7FFF; lineB0[i] = 0; lineB1[i] = 0; }
        lineVis = 0;
        lineN = n;
        lineBlk = n ? (BEAM_LINE_SPRITES / n) : 0;
        lineSX = g_shakeX;
        lineSY = g_shakeY;
        lineRR = 0;
        lineDirty = TRUE;
        lineHigh = 0;
        tailRefresh();
    }

    {
        // La sacudida: mover lo que ya está puesto (lo aparcado no se toca).
        if ((g_shakeX != lineSX) || (g_shakeY != lineSY))
        {
            const s16 ddx = g_shakeX - lineSX;
            const s16 ddy = g_shakeY - lineSY;
            MDSpr **sp = lineDot;
            u16 left = lineHigh;            // los copiados a la cola

            while (left--)
            {
                MDSpr *q = *sp++;

                q->x -= ddx;
                q->y -= ddy;
            }
            lineSX = g_shakeX;
            lineSY = g_shakeY;
        }

        // Las que hayan cambiado, por turnos. Las holo activas no se mueven
        // (sólo giran): con la nave quieta basta con mirarlas de vez en
        // cuando, por si una muere y otra se activa en el mismo frame.
        if ((x0 != lineLastX) || (y0 != lineLastY) || lineDirty || ((++lineTick & 15) == 0))
        {
            bool pending = FALSE;

            for (k = 0; k < n; k++)
            {
                i = lineRR;
                if (++lineRR >= n) lineRR = 0;

                if ((lineKey[i][0] != x0) || (lineKey[i][1] != y0) ||
                    (lineKey[i][2] != F2I(tx[i])) || (lineKey[i][3] != F2I(ty[i])))
                {
                    if (done < LINES_PER_FRAME) { buildLine(i, x0, y0, F2I(tx[i]), F2I(ty[i])); done++; }
                    else { pending = TRUE; break; }
                }
            }
            // Si queda alguna por rehacer, se vuelve a mirar el frame que viene.
            lineDirty = pending;
            lineLastX = x0;
            lineLastY = y0;
        }
    }

    if (done)
    {
        // Los que se ven, seguidos al principio de la cola, con la sacudida.
        const u16 before = lineHigh;
        u16 v = 0;

        for (i = 0; i < n; i++)
        {
            const MDSpr *blk = &lineVirt[i * lineBlk];

            for (k = 0; k < lineCnt[i]; k++, v++)
            {
                MDSpr *d = lineDot[v];

                d->sizeLink = blk[k].sizeLink;
                d->attr = blk[k].attr;
                d->x = blk[k].x - g_shakeX;
                d->y = blk[k].y - g_shakeY;
            }
        }
        for (k = v; k < before; k++) sprHide(lineDot[k]);
        lineHigh = v;
        tailRefresh();
    }
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
