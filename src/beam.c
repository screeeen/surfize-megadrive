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
static MDSpr *lineDot[MAX_LINES * BEAM_LINE_DOTS];
static MDSpr *rayDot[MAX_RAY_DOTS];
static u16    beamTile[8][BEAM_COLORS];
// Cuántos puntos de líneas y de rayos se enseñaron el frame anterior: sólo
// hay que esconder esos, no el pool entero cada frame.
static u16    lineShown, rayShown;
static u16    dotTile[DOT_COLORS];

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

    for (i = 0; i < (MAX_LINES * BEAM_LINE_DOTS); i++)
        lineDot[i] = VSpr_add(&dotSprite, PAL1, dotTile[0]);

    for (i = 0; i < MAX_RAY_DOTS; i++)
        rayDot[i] = VSpr_add(&dotSprite, PAL1, dotTile[0]);
}

void Beam_clear(void)
{
    u16 i;

    for (i = 0; i < BEAM_SEGMENTS; i++) sprHide(segSprite[i]);
    for (i = 0; i < (MAX_LINES * BEAM_LINE_DOTS); i++) sprHide(lineDot[i]);
    for (i = 0; i < MAX_RAY_DOTS; i++) sprHide(rayDot[i]);
    lineShown = 0;
    rayShown = 0;
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

// oPlayer/Draw_0: una línea azul de la nave a cada réplica.
static void updateLines(fix16 px, fix16 py)
{
    fix16 tx[MAX_LINES], ty[MAX_LINES];
    const u16 n = Proj_triPositions(tx, ty, MAX_LINES);
    u16 i, d, slot = 0;

    for (i = 0; i < n; i++)
    {
        // Los puntos se reparten por igual entre la nave y la réplica.
        const fix16 stepX = (tx[i] - px) / (BEAM_LINE_DOTS + 1);
        const fix16 stepY = (ty[i] - py) / (BEAM_LINE_DOTS + 1);
        fix16 x = px;
        fix16 y = py;

        for (d = 0; d < BEAM_LINE_DOTS; d++)
        {
            x += stepX;
            y += stepY;
            // Con la réplica pegada al borde y la sacudida, algún punto
            // puede salirse: se aparca (ver el contrato de inc/spr.h).
            if (MDSPR_OFFSCREEN(F2I(x), F2I(y))) sprHide(lineDot[slot]);
            else sprPos(lineDot[slot], F2I(x) + DOT_SPR_OFF, F2I(y) + DOT_SPR_OFF);
            slot++;
        }
    }

    {
        const u16 shown = slot;

        while (slot < lineShown) sprHide(lineDot[slot++]);
        lineShown = shown;
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
