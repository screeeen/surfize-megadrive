#include "projectile.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "resources.h"
#include "gm_art.h"
#include "entity.h"
#include "collision.h"
#include "effect.h"
#include "enemy.h"
#include "snake.h"
#include "game.h"
#include "audio.h"
#include "shake.h"
#include "debris.h"
#include "player.h"
#include "weapon.h"
#include "powerup.h"
#include "hole.h"

typedef struct
{
    u8     type;
    u8     anim;        // animación de projectiles.png: arma * 8 + rotación
    fix16  x;
    fix16  y;
    fix16  dx;
    fix16  dy;
    u16    life;        // 0 = sin alarma (muere al salir del room)
    u8     ignore;      // cabezas ya atravesadas (bala gorda, Snake_hitHead)
    u8     released;    // bola CIRCULAR suelta: persigue, ya no orbita
    fix16  orbit;       // bola CIRCULAR: ángulo de su órbita, en grados
    MDSpr *sprite;
} Projectile;

static Projectile projs[MAX_PROJECTILES];

// Índice VRAM de cada animación de la hoja de proyectiles (una por arma y
// rotación), precargadas (ver inc/vramspr.h).
static u16 projTile[PROJ_SPR_FRAMES];

// ---------------------------------------------------------------------------
// LASER y MOIRE (bloque 3 de MonsterLite)
//
// Cada disparo crea dos cosas (S/disparo.cs):
//
// - Un RaycastObj hijo de la nave (S/RaycastScr.cs) que en cada frame lanza
//   Physics.Raycast desde la nave en la dirección de apuntado, sin distancia
//   máxima, y mata al PRIMER collider de la línea si es araña o cabeza (sin
//   mirar el hp). Si lo primero es otra cosa (un eslabón, un agujero, un
//   paquete, una bala en vuelo), ese frame no hace nada. Vive 25 f (LASER)
//   o 7 f (MOIRE), o hasta que mata.
//
//   Como todos los RaycastObj vivos cuelgan de la nave con su rotación, en
//   un mismo frame todos lanzan EL MISMO rayo. Aquí se lleva un único
//   contador con la vida que le queda al más nuevo: mientras no es cero se
//   lanza el rayo, y al matar se pone a cero. En el original, en el frame en
//   que matan, todos los vivos golpean al mismo enemigo (Destroy no surte
//   efecto hasta el final del frame) y cada uno suma la baja y suelta su
//   premio; aquí la baja cuenta una vez. Divergencia documentada.
//
// - El haz que se ve (Laser.prefab, un LineRenderer de 20 u el del LASER y
//   de 24 u el del MOIRE, que es más que la pantalla) durante 2 f o 7 f. El
//   del LASER se queda donde se disparó; el del MOIRE es hijo de la nave y
//   la sigue. El VDP no dibuja líneas: son puntos cada LASER_DASH_STEP
//   píxeles hasta el borde de la pantalla, con el punto amarillo del GML (el
//   de las descargas del rayo, dot.png; la decisión 3 manda que el arte sea
//   del GML), que es el color del LineRenderer de MonsterLite (blanco a
//   amarillo). El segmento de láser del GML se probó y a esta escala (1x6 px)
//   no se veía.
// ---------------------------------------------------------------------------
#define LASER_DASHES     16
#define LASER_DASH_STEP  20

static u16    rayLife;          // frames que le quedan al rayo de impacto
static u16    beamLife;         // frames que le quedan al haz visible
static bool   beamFollows;      // MOIRE: el haz va con la nave
static fix16  beamX, beamY, beamAng;
static MDSpr *dash[LASER_DASHES];
static u16    dashTile[DOT_COLORS];
#define DASH_COLOR 2        // dot.png: azul, plata, amarillo

// oOption usa sprExplosion (32x32, origen centrado) y oTri el arte de la nave.
#define OPTION_BOX_OX   GML_LEN(0 - 16)
#define OPTION_BOX_OY   GML_LEN(0 - 16)
#define OPTION_BOX_W    GML_LEN(31)
#define OPTION_BOX_H    GML_LEN(31)

// Cajas de colisión, por tipo. Las balas de MonsterLite (fina, gorda del
// WBLOCK y del TWAY) llevan su caja de 16x16 centrada; la bola CIRCULAR, la
// esfera de 4 px como caja de 8x8. El arte sigue siendo el del GML (decisión
// 3): la bala gorda del WBLOCK con el dibujo de oWBlock y la del TWAY con el
// de o3way.
#define ML_BOX_O  (-ML_BULLET_HALF)
#define CIRC_O    (-ML_CIRC_HALF)
#define CIRC_W    (2 * ML_CIRC_HALF)

static const s16 boxOX[] = { 0, ML_BOX_O, ML_BOX_O, ML_BOX_O, CIRC_O, PARA_BOX_OX, BOMB_BOX_OX, 0, OPTION_BOX_OX, PLAYER_BOX_OX };
static const s16 boxOY[] = { 0, ML_BOX_O, ML_BOX_O, ML_BOX_O, CIRC_O, PARA_BOX_OY, BOMB_BOX_OY, 0, OPTION_BOX_OY, PLAYER_BOX_OY };
static const s16 boxW[]  = { 0, ML_BULLET_BOX, ML_BULLET_BOX, ML_BULLET_BOX, CIRC_W, PARA_BOX_W, BOMB_BOX_W, 0, OPTION_BOX_W, PLAYER_BOX_W };
static const s16 boxH[]  = { 0, ML_BULLET_BOX, ML_BULLET_BOX, ML_BULLET_BOX, CIRC_W, PARA_BOX_H, BOMB_BOX_H, 0, OPTION_BOX_H, PLAYER_BOX_H };

static const u8 animBase[] = { 0, PROJ_ANIM_THIN, PROJ_ANIM_WBLOCK, PROJ_ANIM_THREEW,
                               PROJ_ANIM_OPTION, PROJ_ANIM_PARA, PROJ_ANIM_BOMB, PROJ_ANIM_BOMB,
                               PROJ_ANIM_OPTION, PROJ_ANIM_TRI };

// Estado del pivote (oPivot): es una segunda "nave" que el input del jugador
// mueve a la vez que la suya, con los satélites orbitando alrededor.
static fix16 pivotX, pivotY, pivotT;
static bool  pivotAlive;

// Lo que proyectile necesita del jugador cada frame.
static fix16 playerX, playerY, playerAngle, targetX, targetY;
static bool  playerHasTarget;
static u16   playerPad;
static u16   triTimer[MAX_PROJECTILES];

// Naves holo en el pool: casi siempre ninguna, y así lo que las busca no
// recorre el pool entero (el caso peor va al borde del frame).
static u16 holoN;
// Reparte entre frames el trabajo de las holo (ver updateHolo).
static u16 holoTick;

static void hide(u16 i)
{
    if (projs[i].type == PROJ_TRI) holoN--;
    projs[i].type = PROJ_NONE;
    sprHide(projs[i].sprite);
}

static void place(u16 i)
{
    // El dibujo va desplazado respecto al origen de la instancia, y ese
    // desplazamiento gira con el sprite: la tabla la genera tools/gm2md.py.
    const u8 a = projs[i].anim;

    sprPos(projs[i].sprite,
                    F2I(projs[i].x) + projSprOffX[a] - (PROJ_CELL / 2),
                    F2I(projs[i].y) + projSprOffY[a] - (PROJ_CELL / 2));
}

static s16 freeSlot(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++)
        if (projs[i].type == PROJ_NONE) return i;

    return -1;
}

static s16 spawn(u8 type, fix16 x, fix16 y, fix16 angle, fix16 speed, u16 life)
{
    const s16 i = freeSlot();

    if (i < 0) return -1;

    projs[i].type = type;
    projs[i].x = x;
    projs[i].y = y;
    projs[i].dx = (speed != 0) ? fmul(fcos(angle), speed) : 0;
    projs[i].dy = (speed != 0) ? fmul(fsin(angle), speed) : 0;
    projs[i].life = life;
    projs[i].ignore = 0;
    projs[i].released = 0;
    projs[i].orbit = 0;
    // La bomba, el satélite y la bola no rotan en el original; el resto sí.
    projs[i].anim = animBase[type] +
                    (((type == PROJ_BOMB) || (type == PROJ_RAY) ||
                      (type == PROJ_OPTION) || (type == PROJ_CIRC))
                         ? 0 : Entity_angleFrame8(angle));
    if (type == PROJ_TRI) { triTimer[i] = ML_HOLO_FIRST_F; holoN++; }

    sprAnim(projs[i].sprite, projs[i].anim);
    sprTile(projs[i].sprite, projTile[projs[i].anim]);
    place(i);

    // El Create_0 de cada proyectil lanza su sonido.
    switch (type)
    {
        case PROJ_THIN:
        case PROJ_PARA:   Audio_sfx(SFX_SHOT);     break;   // shotSnd
        case PROJ_WBLOCK: Audio_sfx(SFX_WBLOCK);   break;   // wBlockSnd
        case PROJ_3WAY:   Audio_sfx(SFX_THREEWAY); break;   // threeWaySnd
        case PROJ_BOMB:   Audio_sfx(SFX_LASER);    break;   // laserSnd
        default: break;
    }

    return i;
}

static void hideDashes(void)
{
    u16 d;

    for (d = 0; d < LASER_DASHES; d++) sprHide(dash[d]);
}

void Proj_initAll(void)
{
    u16 i;

    VSpr_load(&projectiles, projTile, 1);

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        projs[i].type = PROJ_NONE;
        projs[i].sprite = VSpr_add(&projectiles, PAL1, projTile[0]);
    }

    VSpr_load(&dotSprite, dashTile, 1);
    for (i = 0; i < LASER_DASHES; i++)
    {
        dash[i] = VSpr_add(&dotSprite, PAL1, dashTile[DASH_COLOR]);
        sprAnim(dash[i], DASH_COLOR);
    }

    rayLife = 0;
    beamLife = 0;
    pivotAlive = FALSE;
    holoN = 0;
}

void Proj_setPlayer(fix16 px, fix16 py, fix16 angle, u16 pad, bool hasTarget, fix16 tx, fix16 ty)
{
    playerX = px;
    playerY = py;
    playerAngle = angle;
    playerPad = pad;
    playerHasTarget = hasTarget;
    targetX = tx;
    targetY = ty;
}

// ---------------------------------------------------------------------------
// La nave holo de MonsterLite (naveProxy, bloque 7)
//
// Vive en el pool de proyectiles con el arte de la réplica del GML (oTri),
// que es la nave del jugador: released es "working" y orbit su rumbo.
//
// - Inactiva (S/movementProxy.cs, rotateInactive): gira 1 grado por Update.
// - Al tocarla la nave (S/naveProxyController.cs, setWorking) se activa para
//   siempre, con la voz repetea.
// - Activa: apunta al enemigo más cercano A ELLA (getClosest, arañas y
//   cabezas) y le dispara BulletThin a 6,4 px/f cada 33 f, el primero a los
//   17 (S/disparoProxy.cs). No gasta munición.
// - Activa o no, muere al tocar una araña o una cabeza, con una explosión
//   (S/collideAndDie.cs: sólo se habilita en setWorking, pero Unity manda
//   OnTriggerEnter también a los componentes desactivados; bloque 8).
// ---------------------------------------------------------------------------
static void holoAnim(u16 i, fix16 a)
{
    const u8 anim = PROJ_ANIM_TRI + Entity_angleFrame8(a);

    if (projs[i].anim == anim) return;
    projs[i].anim = anim;
    sprAnim(projs[i].sprite, anim);
    sprTile(projs[i].sprite, projTile[anim]);
}

u16 Proj_holoCount(void)
{
    return holoN;
}

bool Proj_spawnHolo(fix16 x, fix16 y, bool active)
{
    s16 i;

    // "if (FindGameObjectsWithTag("NaveProxy").Length < maxNaveProxy)"
    if (Proj_holoCount() >= ML_HOLO_MAX) return FALSE;

    i = spawn(PROJ_TRI, x, y, 0, 0, 0);
    if (i < 0) return FALSE;

    projs[i].released = active ? 1 : 0;
    return TRUE;
}

bool Proj_anyHolo(fix16 *x, fix16 *y)
{
    u16 i;

    if (holoN == 0) return FALSE;

    // FindGameObjectWithTag("NaveProxy") devuelve una cualquiera, activa o
    // no (las dos llevan el tag). Aquí, la de la ranura más baja.
    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        if (projs[i].type != PROJ_TRI) continue;

        *x = projs[i].x;
        *y = projs[i].y;
        return TRUE;
    }

    return FALSE;
}

void Proj_holoContact(void)
{
    u16 i;

    if (holoN == 0) return;

    // collideAndDie, en todas las holo, activas o no (ver updateHolo): la
    // misma prueba, pero cada frame y sin nada más.
    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        s16 hx, hy;

        if (projs[i].type != PROJ_TRI) continue;

        hx = F2I(projs[i].x);
        hy = F2I(projs[i].y);

        if (Enemy_touchCircle(hx, hy, ML_HOLO_SPIDER_R2, ML_HOLO_SPIDER_REACH) ||
            Snake_headTouchCircle(hx, hy, ML_HOLO_SNAKE_R2, ML_HOLO_SNAKE_REACH))
        {
            Effect_spawn(projs[i].x, projs[i].y);
            hide(i);
        }
    }
}

bool Proj_holoTouch(s16 px, s16 py)
{
    bool any = FALSE;
    u16 i;

    if (holoN == 0) return FALSE;

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        s16 dx, dy;

        if ((projs[i].type != PROJ_TRI) || projs[i].released) continue;

        // Primero el cuadrado que envuelve el círculo: así casi nunca hay
        // que multiplicar, y cuando hay que hacerlo cabe en 16 bits.
        dx = F2I(projs[i].x) - px;
        dy = F2I(projs[i].y) - py;
        if ((dx >= ML_HOLO_TOUCH_R) || (dx <= -ML_HOLO_TOUCH_R) ||
            (dy >= ML_HOLO_TOUCH_R) || (dy <= -ML_HOLO_TOUCH_R)) continue;
        if ((u16)((dx * dx) + (dy * dy)) >= ML_HOLO_TOUCH_R2) continue;

        projs[i].released = 1;
        any = TRUE;
    }

    return any;
}

void Proj_tickFrozen(void)
{
    u16 i;

    if (holoN == 0) return;
    holoTick++;

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        if ((projs[i].type != PROJ_TRI) || projs[i].released) continue;

        projs[i].orbit += ML_HOLO_SPIN;
        if (((holoTick + i) & 7) == 0)
        {
            projs[i].orbit = F16_normalizeAngle(projs[i].orbit);
            holoAnim(i, projs[i].orbit);
        }
    }
}

// Un frame de la holo. FALSE si ha muerto.
//
// Coste: con las 10 activas, buscar el más cercano y mirar el contacto en
// cada frame tiraba el caso peor de 483 a 305 vueltas de 600. Se reparte por
// ranuras: el contacto se mira uno de cada 2 frames (un enemigo no avanza ni
// 6 px en dos frames, menos que el radio de la holo) y el apuntado uno de
// cada 4, y siempre en el frame en que dispara. Ver docs/PORT_AUDIT_GML.md,
// bloque 7.
static bool updateHolo(u16 i)
{
    Projectile *p = &projs[i];
    const s16 hx = F2I(p->x);
    const s16 hy = F2I(p->y);
    const u16 phase = holoTick + i;
    bool fire;

    // collideAndDie: al entrar una araña o una cabeza, explosión y fuera. El
    // componente sólo se habilita en setWorking, pero Unity manda
    // OnTriggerEnter también a los MonoBehaviour desactivados, y la holo
    // inactiva tiene el mismo SphereCollider (es con el que la nave la
    // activa): en MonsterLite la inactiva también muere al contacto
    // (decisión del usuario, 2026-11-20, bloque 8). Uno de cada 2 frames:
    // un enemigo no avanza ni 8 px en dos, menos que la suma de radios.
    if (((phase & 1) == 0) &&
        (Enemy_touchCircle(hx, hy, ML_HOLO_SPIDER_R2, ML_HOLO_SPIDER_REACH) ||
         Snake_headTouchCircle(hx, hy, ML_HOLO_SNAKE_R2, ML_HOLO_SNAKE_REACH)))
    {
        // OnExplosion: explosionBlue. El port no tiene explosión azul: la
        // normal.
        Effect_spawn(p->x, p->y);
        return FALSE;
    }

    if (!p->released)
    {
        // Gira 1,2 grados por frame y el dibujo tiene 8 rotaciones: cambia
        // cada ~37 f, así que basta mirarlo uno de cada 8.
        p->orbit += ML_HOLO_SPIN;
        if ((phase & 7) == 0)
        {
            p->orbit = F16_normalizeAngle(p->orbit);
            holoAnim(i, p->orbit);
        }
        return TRUE;
    }

    // dispara(): "if (shootTime > 1) shootTime -= 1; else dispara", sólo con
    // enemigo. Si ya no hay ninguno se sabrá al apuntar.
    fire = (triTimer[i] <= 1);

    if (fire || ((phase & 3) == 0))
    {
        s32 best = 0x7FFFFFFF;
        s16 tx, ty;
        bool found = Enemy_closest(hx, hy, &tx, &ty, &best);

        if (Snake_closestHead(hx, hy, &tx, &ty, &best)) found = TRUE;
        p->ignore = found;
        if (!found) return TRUE;

        // rotateShipDirection: mira al más cercano.
        p->orbit = Fast_angle(p->x, p->y, FIX16(tx), FIX16(ty));
        holoAnim(i, p->orbit);
    }

    // Sin enemigo la última vez que miró, no cuenta ni dispara.
    if (!p->ignore) return TRUE;

    if (!fire)
    {
        triTimer[i]--;
    }
    else
    {
        spawn(PROJ_THIN, p->x, p->y, p->orbit, ML_HOLO_BULLET_SPEED, 0);
        triTimer[i] = ML_HOLO_SHOT_F;
    }

    return TRUE;
}

// oPlayer/Collision_oPivot. OJO: en el original este evento NO LLEGA A
// CORRER. oPivot tiene spriteId y spriteMaskId a null, así que no tiene máscara
// de colisión y el jugador no puede tocarlo nunca. Se deja portado porque el
// evento existe, pero no hay nada que lo dispare, igual que allí.
u16 Proj_triPositions(fix16 *xs, fix16 *ys, u16 max)
{
    u16 i, n = 0;

    if (holoN == 0) return 0;

    for (i = 0; (i < MAX_PROJECTILES) && (n < max); i++)
    {
        if ((projs[i].type != PROJ_TRI) || !projs[i].released) continue;

        xs[n] = projs[i].x;
        ys[n] = projs[i].y;
        n++;
    }

    return n;
}

void Proj_clearSatellites(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++)
        if ((projs[i].type == PROJ_OPTION) || (projs[i].type == PROJ_TRI)) hide(i);

    pivotAlive = FALSE;
}

void Proj_fireThin(fix16 x, fix16 y, fix16 angle)
{
    // BulletThin de MonsterLite: 12 px/f (bloque 3).
    spawn(PROJ_THIN, x, y, angle, ML_BULLET_SPEED, 0);
}

// LASER y MOIRE: el rayo de impacto y el haz (ver arriba).
static void fireBeam(fix16 x, fix16 y, fix16 angle, u16 rayF, u16 beamF, bool follows)
{
    if (rayLife < rayF) rayLife = rayF;
    beamLife = beamF;
    beamFollows = follows;
    beamX = x;
    beamY = y;
    beamAng = angle;
    // laserBulletClip / moireBulletClip de MonsterLite: hasta el bloque 12,
    // el laserSnd del GML.
    Audio_sfx(SFX_LASER);
}

bool Proj_paraAlive(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++)
        if (projs[i].type == PROJ_PARA) return TRUE;

    return FALSE;
}

bool Proj_fireWeapon(u8 weaponIndex, fix16 x, fix16 y, fix16 angle)
{
    u16 i;

    switch (weaponIndex)
    {
        case WPN_WBLOCK:
            // Una Bullet desplazada media anchura del sprite. En el original
            // en X del mundo, sin rotar; Q12: hacia donde apunta la nave.
            spawn(PROJ_WBLOCK,
                  x + fmul(fcos(angle), FIX16(ML_WBLOCK_GUN_PX)),
                  y + fmul(fsin(angle), FIX16(ML_WBLOCK_GUN_PX)),
                  angle, ML_BULLET_SPEED, 0);
            return TRUE;

        case WPN_LASER:
            fireBeam(x, y, angle, ML_LASER_RAY_F, ML_LASER_BEAM_F, FALSE);
            return TRUE;

        case WPN_MOIRE:
            fireBeam(x, y, angle, ML_MOIRE_RAY_F, ML_MOIRE_BEAM_F, TRUE);
            return TRUE;

        case WPN_TWAY:
            // Tres Bullet a -30, 0 y +30 grados, desde la nave.
            spawn(PROJ_3WAY, x, y, F16_normalizeAngle(angle - ML_TWAY_SPREAD), ML_BULLET_SPEED, 0);
            spawn(PROJ_3WAY, x, y, angle, ML_BULLET_SPEED, 0);
            spawn(PROJ_3WAY, x, y, F16_normalizeAngle(angle + ML_TWAY_SPREAD), ML_BULLET_SPEED, 0);
            return TRUE;

        case WPN_CIRCULAR:
            // circCont: las bolas en 0, 120 y 240 grados a 20 px de la nave
            // (Q12: sin la cuarta, superpuesta). Se mueven en Proj_update.
            for (i = 0; i < ML_CIRC_BALLS; i++)
            {
                const fix16 a = FIX16(120 * i);
                const s16 k = spawn(PROJ_CIRC,
                                    x + fmul(fcos(a), FIX16(ML_CIRC_RADIUS)),
                                    y + fmul(fsin(a), FIX16(ML_CIRC_RADIUS)),
                                    0, 0, 0);

                if (k >= 0) projs[k].orbit = a;
            }
            // circularBulletClip: hasta el bloque 12, el shotSnd del GML.
            Audio_sfx(SFX_SHOT);
            return TRUE;

        case WPN_PIVOT:     // "if !instance_exists(oPivot) instance_create(...)"
            if (pivotAlive) return FALSE;

            pivotX = x;
            pivotY = y;
            pivotT = 0;
            pivotAlive = TRUE;

            // for (h = 1; h < 360; h += 120): tres satélites a 120 grados
            for (i = 0; i < PIVOT_COUNT; i++)
            {
                const fix16 h = FIX16(1) + FIX16(120 * i);

                spawn(PROJ_OPTION,
                      x + fmul(fcos(h), FIX16(PIVOT_RADIUS)),
                      y + fmul(fsin(h), FIX16(PIVOT_RADIUS)),
                      0, 0, 0);
            }
            return TRUE;

        case WPN_PARA:      // sólo uno a la vez
            if (Proj_paraAlive()) return FALSE;
            spawn(PROJ_PARA, x, y, angle, BULLET_SPEED_PARA, PARA_LIFE);
            return TRUE;

        default:
            return FALSE;
    }
}

// El haz visible de LASER y MOIRE: trazos a lo largo del rumbo hasta el
// borde de la pantalla.
static void drawBeam(void)
{
    u16 d = 0;

    if (beamLife > 0)
    {
        const fix16 stepX = fmul(fcos(beamAng), FIX16(LASER_DASH_STEP));
        const fix16 stepY = fmul(fsin(beamAng), FIX16(LASER_DASH_STEP));
        fix16 x = beamX + (stepX >> 1);
        fix16 y = beamY + (stepY >> 1);

        beamLife--;

        for (; d < LASER_DASHES; d++)
        {
            const s16 ix = F2I(x);
            const s16 iy = F2I(y);

            if ((ix < 0) || (ix >= PLAYFIELD_W) || (iy < 0) || (iy >= PLAYFIELD_H)) break;

            sprPos(dash[d], ix - (DOT_CELL / 2), iy - (DOT_CELL / 2));
            x += stepX;
            y += stepY;
        }
    }

    for (; d < LASER_DASHES; d++) sprHide(dash[d]);
}

// Un frame del rayo de impacto: el primer collider de la línea. Devuelve 1 si
// ha matado (y en *snake si era un gusano).
static u16 castRay(u16 *snakeKills)
{
    const s16 ox = F2I(playerX);
    const s16 oy = F2I(playerY);
    const fix16 c = fcos(playerAngle);
    const fix16 sn = fsin(playerAngle);
    s32 best, d;
    u16 spider = 0, slot = 0, i;
    bool head = FALSE;
    u8 kind = 0;            // 0 nada o un obstáculo, 1 araña, 2 gusano

    best = Enemy_rayFirst(ox, oy, c, sn, &spider);
    if (best >= 0) kind = 1;

    d = Snake_rayFirst(ox, oy, c, sn, &slot, &head);
    if ((d >= 0) && ((best < 0) || (d < best))) { best = d; kind = head ? 2 : 0; }

    // Lo que tapa sin morir: agujeros, lo que hay en el suelo y las balas
    // en vuelo (todos triggers en MonsterLite).
    d = Hole_rayFirst(ox, oy, c, sn);
    if ((d >= 0) && ((best < 0) || (d < best))) { best = d; kind = 0; }

    d = Pow_rayFirst(ox, oy, c, sn);
    if ((d >= 0) && ((best < 0) || (d < best))) { best = d; kind = 0; }

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        if (projs[i].type == PROJ_THIN)
            d = Coll_rayCircle(F2I(projs[i].x) - ox, F2I(projs[i].y) - oy, c, sn,
                               ML_X10_TO_F16(ML_RAY_R_BULLET_X10));
        // La nave holo también es un trigger (su esfera de 5,4 px).
        else if (projs[i].type == PROJ_TRI)
            d = Coll_rayCircle(F2I(projs[i].x) - ox, F2I(projs[i].y) - oy, c, sn,
                               ML_X10_TO_F16(ML_HOLO_RADIUS_X10));
        else
            continue;

        if ((d >= 0) && ((best < 0) || (d < best))) { best = d; kind = 0; }
    }

    if (kind == 1)
    {
        fix16 x, y;

        Enemy_killLive(spider, &x, &y);
        Effect_spawn(x, y);
        Debris_burst(x, y);
        // RaycastScr.leavePow(), en la posición del enemigo.
        Pow_drop(x, y, FALSE);
        rayLife = 0;
        return 1;
    }

    if (kind == 2)
    {
        Snake_killSlot(slot);
        Effect_spawn(Snake_lastKillX(), Snake_lastKillY());
        Debris_burst(Snake_lastKillX(), Snake_lastKillY());
        Pow_drop(Snake_lastKillX(), Snake_lastKillY(), FALSE);
        (*snakeKills)++;
        rayLife = 0;
        return 1;
    }

    return 0;
}

// Bala o bola de MonsterLite contra los enemigos (bulletThinSrc,
// bulletScript y circularScr, OnTriggerEnter). Devuelve 1 si ha matado.
// *gone a TRUE si la bala se destruye.
static u16 hitML(u16 i, s16 bx, s16 by, u16 *snakeKills, bool *gone)
{
    Projectile *p = &projs[i];
    const s16 w = boxW[p->type];
    const bool thin = (p->type == PROJ_THIN);
    const bool circ = (p->type == PROJ_CIRC);
    // Bloque 8: las balas son su caja de 16 px (BoxCollider) contra la
    // esfera del enemigo; la bola, su esfera de 4 px contra la del enemigo.
    const s16 cx = F2I(p->x);
    const s16 cy = F2I(p->y);
    bool killed, hit;

    *gone = FALSE;

    // La araña muere de cualquier impacto (su hp no se mira) y la bala se
    // destruye.
    if (circ) hit = Enemy_hitCircle(cx, cy, ML_BALL_SPIDER_R2, ML_BALL_SPIDER_REACH, &killed);
    else      hit = Enemy_hit(bx, by, w, w, DMG_TO_SPIDER, &killed);

    if (hit)
    {
        // circularScr sólo hace la explosión si queda menos de 2 enemigos.
        if (!circ || ((Enemy_countAlive() + Snake_countType(SNAKE_TYPE_1) +
                       Snake_countType(SNAKE_TYPE_2)) < 2))
            Effect_spawn(p->x, p->y);
        *gone = TRUE;
        if (killed)
        {
            Debris_burst(p->x, p->y);
            // leavePow() en la posición de la bala; la bola contra una araña
            // suelta siempre paquete (circularScr).
            Pow_drop(p->x, p->y, circ);
            return 1;
        }
        return 0;
    }

    // La cabeza: la fina resta 1 y se destruye; la gorda resta 3 y sólo se
    // destruye si mata (si no, atraviesa); la bola resta 3 y se destruye.
    // Los eslabones no les afectan.
    if (circ) hit = Snake_hitHeadCircle(cx, cy, ML_BALL_SNAKE_R2, ML_BALL_SNAKE_REACH,
                                        ML_DMG_FAT, &p->ignore, &killed);
    else      hit = Snake_hitHead(bx, by, w, w, thin ? ML_DMG_THIN : ML_DMG_FAT, &p->ignore, &killed);

    if (hit)
    {
        if (!circ || ((Enemy_countAlive() + Snake_countType(SNAKE_TYPE_1) +
                       Snake_countType(SNAKE_TYPE_2)) < 2))
            Effect_spawn(p->x, p->y);

        if (thin || circ || killed) *gone = TRUE;

        if (killed)
        {
            (*snakeKills)++;
            // A petición del autor la metralla sale en TODA muerte causada
            // por un disparo (ver la nota del GML más abajo).
            Debris_burst(p->x, p->y);
            Pow_drop(p->x, p->y, FALSE);
            return 1;
        }
    }

    return 0;
}

// La bola CIRCULAR, un frame (S/circularScr.cs, Update). TRUE si sigue viva.
static bool updateBall(u16 i)
{
    Projectile *p = &projs[i];

    // "if (bullets[currentWeapon] > 0) release = true", y no vuelve a false.
    if (g_playerBullets > 0) p->released = 1;

    if (p->released)
    {
        // Va al enemigo más cercano A ELLA (getClosest(transform.position))
        // a 24 px/f, y si no hay ninguno se destruye.
        const s16 bx = F2I(p->x);
        const s16 by = F2I(p->y);
        s32 best = 0x7FFFFFFF;
        s16 tx, ty;
        bool found;

        // getClosest: arañas y cabezas, no eslabones. Desde el bloque 8 todos
        // los enemigos nacen visibles, como en MonsterLite (antes las bolas
        // sólo podían ir a por los que ya habían salido del agujero).
        found = Enemy_closest(bx, by, &tx, &ty, &best);
        if (Snake_closestHead(bx, by, &tx, &ty, &best)) found = TRUE;
        if (!found) return FALSE;

        // MoveTowards: si está a menos de un paso, llega.
        if (best <= (ML_CIRC_SPEED * ML_CIRC_SPEED))
        {
            p->x = FIX16(tx);
            p->y = FIX16(ty);
        }
        else
        {
            const fix16 a = Fast_angle(p->x, p->y, FIX16(tx), FIX16(ty));

            p->x += fmul(fcos(a), FIX16(ML_CIRC_SPEED));
            p->y += fmul(fsin(a), FIX16(ML_CIRC_SPEED));
        }
    }
    else
    {
        // RotateAround de la nave, 2 grados por frame de Update.
        p->orbit = F16_normalizeAngle(p->orbit + ML_CIRC_SPIN);
        p->x = playerX + fmul(fcos(p->orbit), FIX16(ML_CIRC_RADIUS));
        p->y = playerY + fmul(fsin(p->orbit), FIX16(ML_CIRC_RADIUS));
    }

    return TRUE;
}

// Índice del satélite dentro del trío, por orden en el pool.
static u16 satelliteIndex(u16 slot)
{
    u16 i, n = 0;

    for (i = 0; i < slot; i++)
        if (projs[i].type == PROJ_OPTION) n++;

    return n;
}

u16 Proj_update(u16 *snakeKills)
{
    u16 i;
    u16 kills = 0;
    u16 optionsAlive = 0;

    holoTick++;

    // LASER y MOIRE: el rayo de impacto de este frame y el haz que se ve.
    if (rayLife > 0)
    {
        rayLife--;
        kills += castRay(snakeKills);
    }
    if (beamFollows)
    {
        beamX = playerX;
        beamY = playerY;
        beamAng = playerAngle;
    }
    drawBeam();

    // oPivot/Step_0: el pivote se mueve con el mismo input que la nave y gira
    // la órbita 2 grados por cada dirección pulsada.
    //
    // En el GML el pivote y la nave avanzan los dos acc = 6, o sea que el
    // pivote va siempre pegado a la nave. Se desplaza lo mismo que ella
    // (Player_padDelta), así sigue pegado si la velocidad de la nave cambia.
    if (pivotAlive)
    {
        fix16 dx, dy;

        Player_padDelta(playerPad, &dx, &dy);
        pivotX += dx;
        pivotY += dy;
        if (playerPad & BUTTON_LEFT)  pivotT -= PIVOT_SPIN;
        if (playerPad & BUTTON_RIGHT) pivotT += PIVOT_SPIN;
        if (playerPad & BUTTON_UP)    pivotT -= PIVOT_SPIN;
        if (playerPad & BUTTON_DOWN)  pivotT += PIVOT_SPIN;
        pivotT = F16_normalizeAngle(pivotT);

        // oPivot/Step_0 llama a stPlayerMovement(), o sea que el pivote hace
        // el MISMO wrap que la nave. Sin esto la nave aparecía por el otro
        // lado de la pantalla y el pivote se quedaba atrás, así que las bolas
        // acababan orbitando un punto cualquiera lejos del jugador.
        {
            const fix16 lo = FIX16(PLAYER_WRAP_OFFSET);
            const fix16 hiX = FIX16(PLAYFIELD_W) - lo;
            const fix16 hiY = FIX16(PLAYFIELD_H) - lo;

            if (pivotX < lo)  pivotX = hiX;
            if (pivotX > hiX) pivotX = lo;
            if (pivotY < lo)  pivotY = hiY;
            if (pivotY > hiY) pivotY = lo;
        }
    }

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        s16 px, py, bx, by;
        bool killed, body;

        if (projs[i].type == PROJ_NONE) continue;

        if (projs[i].type == PROJ_OPTION) optionsAlive++;

        // Alarmas: láser y para mueren por tiempo; bomba y rayo detonan.
        if (projs[i].life > 0)
        {
            projs[i].life--;

            if (projs[i].life == 0)
            {
                if (projs[i].type == PROJ_BOMB)
                {
                    // oBomba/Alarm_0 -> oBombaExp, que hace daño por contacto
                    Effect_spawn(projs[i].x, projs[i].y);
                    kills += Enemy_damageArea(F2I(projs[i].x) + BOMB_BOX_OX,
                                              F2I(projs[i].y) + BOMB_BOX_OY,
                                              BOMB_BOX_W, BOMB_BOX_H);
                }

                hide(i);
                continue;
            }
        }

        // oOption: con munición persigue al objetivo del jugador; sin ella
        // vuelve a su sitio en la órbita del pivote.
        if (projs[i].type == PROJ_OPTION)
        {
            if (!pivotAlive)
            {
                hide(i);
                continue;
            }

            // oOption/Draw_0 lo pinta con alfa random(2): la mitad de los
            // frames sale translúcido o directamente invisible, o sea que
            // PARPADEA. Sin alfa, lo más parecido es enseñarlo o esconderlo.
            if (random() & 1) sprHide(projs[i].sprite);

            if (g_playerBullets > 0)
            {
                if (playerHasTarget)
                {
                    // move_towards_point(target, random(74))
                    const fix16 a = Fast_angle(projs[i].x, projs[i].y, targetX, targetY);
                    const fix16 v = (fix16)(random() % (u16)PIVOT_HOME_SPEED);

                    projs[i].dx = fmul(fcos(a), v);
                    projs[i].dy = fmul(fsin(a), v);
                }
            }
            else
            {
                const fix16 h = pivotT + FIX16(1) + FIX16(120 * satelliteIndex(i));

                projs[i].x = pivotX + fmul(fcos(h), FIX16(PIVOT_RADIUS));
                projs[i].y = pivotY + fmul(fsin(h), FIX16(PIVOT_RADIUS));
                projs[i].dx = 0;
                projs[i].dy = 0;
            }
        }

        // La nave holo (ver updateHolo). El campo ignore, que en ella no se
        // usa para balas, guarda si encontró enemigo la última vez.
        if (projs[i].type == PROJ_TRI)
        {
            if (!updateHolo(i)) { hide(i); continue; }
            place(i);
            continue;
        }

        // La bola CIRCULAR orbita o persigue (no se mueve por velocidad) y
        // no muere al salir de pantalla: en MonsterLite no lleva
        // bulletScrKillSprite. Si se sale, sólo deja de pintarse.
        if (projs[i].type == PROJ_CIRC)
        {
            bool gone;

            if (!updateBall(i)) { hide(i); continue; }

            bx = F2I(projs[i].x) + CIRC_O;
            by = F2I(projs[i].y) + CIRC_O;
            kills += hitML(i, bx, by, snakeKills, &gone);
            if (gone) { hide(i); continue; }

            if (MDSPR_OFFSCREEN(F2I(projs[i].x) + projSprOffX[projs[i].anim] - (PROJ_CELL / 2),
                                F2I(projs[i].y) + projSprOffY[projs[i].anim] - (PROJ_CELL / 2)))
                sprHide(projs[i].sprite);
            else
                place(i);
            continue;
        }

        projs[i].x += projs[i].dx;
        projs[i].y += projs[i].dy;
        px = F2I(projs[i].x);
        py = F2I(projs[i].y);
        bx = px + boxOX[projs[i].type];
        by = py + boxOY[projs[i].type];

        // Fuera del room (Other_0 -> action_kill_object). Las balas de
        // MonsterLite, igual: bulletScrKillSprite las destruye al salir de
        // pantalla (OnBecameInvisible); aquí, cuando la caja de 16 px queda
        // entera fuera.
        if (((bx + boxW[projs[i].type]) < 0) || (bx > PLAYFIELD_W) ||
            ((by + boxH[projs[i].type]) < 0) || (by > PLAYFIELD_H))
        {
            hide(i);
            continue;
        }

        // Las balas de MonsterLite (bloque 3): fina, y gorda del WBLOCK y del
        // TWAY. Los eslabones no les afectan.
        if ((projs[i].type == PROJ_THIN) || (projs[i].type == PROJ_WBLOCK) ||
            (projs[i].type == PROJ_3WAY))
        {
            bool gone;

            kills += hitML(i, bx, by, snakeKills, &gone);
            if (gone) { hide(i); continue; }
            place(i);
            continue;
        }

        // La bomba y el rayo no colisionan mientras cuentan su mecha.
        if ((projs[i].type != PROJ_BOMB) && (projs[i].type != PROJ_RAY))
        {
            if (Enemy_hit(bx, by, boxW[projs[i].type], boxH[projs[i].type],
                          DMG_TO_SPIDER, &killed))
            {
                // oSpider/Collision_oBulletPara no crea oExplosion: crea un
                // oAutoDestruccion, que es otra cosa (ver inc/effect.h).
                if (projs[i].type == PROJ_PARA) Effect_autoDest(projs[i].x, projs[i].y);
                else                            Effect_spawn(projs[i].x, projs[i].y);
                hide(i);
                if (killed)
                {
                    kills++;
                    Debris_burst(projs[i].x, projs[i].y);
                    // Bloque 4: también sueltan las armas del GML, que sin
                    // generaPaquete() no tendrían de dónde sacar munición.
                    Pow_drop(projs[i].x, projs[i].y, FALSE);
                }
                continue;
            }

            if (Snake_hit(bx, by, boxW[projs[i].type], boxH[projs[i].type], &killed, &body))
            {
                // Igual que arriba: oCabeza*/Collision_oBulletPara crea un
                // oAutoDestruccion en vez de la explosión normal.
                if (projs[i].type == PROJ_PARA) Effect_autoDest(projs[i].x, projs[i].y);
                else                            Effect_spawn(projs[i].x, projs[i].y);

                // oWBlock/Collision_oMalo y oOption/Collision_oMalo crean un
                // oEstallido, y sólo llegan a correr cuando lo golpeado es un
                // eslabón: contra una araña o una cabeza el manejador del
                // enemigo destruye el proyectil antes (ver inc/snake.h).
                if (body && (projs[i].type == PROJ_OPTION))
                    Debris_burst(projs[i].x, projs[i].y);

                hide(i);
                if (killed)
                {
                    kills++;
                    (*snakeKills)++;
                    // Cada muerte revienta en metralla. En el GML sólo lo
                    // hacían oWBlock y oOption contra un eslabón; a petición
                    // del autor la ráfaga sale en TODA muerte causada por un
                    // disparo. No se dispara desde las muertes que causa la
                    // propia metralla, para que no se realimente.
                    Debris_burst(projs[i].x, projs[i].y);
                    Pow_drop(projs[i].x, projs[i].y, FALSE);
                }
                continue;
            }
        }

        place(i);
    }

    // oPivot/Step_0, primera línea: "if instance_number(oOption) < 1
    // instance_destroy()". Cuando cae el último satélite el pivote SE MUERE,
    // y eso es lo que hace que el arma vuelva: el siguiente disparo encuentra
    // que no hay oPivot y crea uno nuevo con sus tres bolas.
    //
    // El port no lo tenía, así que el pivote vivía para siempre: las tres
    // bolas morían al primer contacto (también contra un eslabón de gusano) y
    // no volvían a salir en toda la partida.
    if (pivotAlive && (optionsAlive == 0)) pivotAlive = FALSE;

    return kills;
}

void Proj_clearKeepTris(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++)
    {
        if (projs[i].type == PROJ_TRI)
        {
            // La cámara de la sacudida se acaba de recentrar. shootTime no
            // se reinicia: en MonsterLite la holo sigue igual.
            place(i);
        }
        else
        {
            hide(i);
        }
    }

    // Sin jugador no hay pivote: sus satélites se han borrado arriba. Ni
    // rayo ni haz, que cuelgan de la nave; las bolas CIRCULAR, igual
    // (circCont las destruye sin jugador), y ya se han borrado arriba.
    pivotAlive = FALSE;
    rayLife = 0;
    beamLife = 0;
    beamFollows = FALSE;
    hideDashes();
}

void Proj_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++) hide(i);

    rayLife = 0;
    beamLife = 0;
    beamFollows = FALSE;
    hideDashes();
}
