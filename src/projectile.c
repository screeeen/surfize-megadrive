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

typedef struct
{
    u8     type;
    u8     anim;        // animación de projectiles.png: arma * 8 + rotación
    fix16  x;
    fix16  y;
    fix16  dx;
    fix16  dy;
    u16    life;        // 0 = sin alarma (muere al salir del room)
    MDSpr *sprite;
} Projectile;

static Projectile projs[MAX_PROJECTILES];

// Índice VRAM de cada animación de la hoja de proyectiles (una por arma y
// rotación), precargadas (ver inc/vramspr.h).
static u16 projTile[PROJ_SPR_FRAMES];

// oPlayer/Create_0: tLaser = 10. El bucle del láser lo decrementa y sólo crea
// segmento cuando baja de 0, así que la PRIMERA ráfaga no dispara ninguno y a
// partir de la segunda salen todos. Portado tal cual.
static fix16 tLaser;

// oOption usa sprExplosion (32x32, origen centrado) y oTri el arte de la nave.
#define OPTION_BOX_OX   GML_LEN(0 - 16)
#define OPTION_BOX_OY   GML_LEN(0 - 16)
#define OPTION_BOX_W    GML_LEN(31)
#define OPTION_BOX_H    GML_LEN(31)

static const s16 boxOX[] = { 0, BULLET_BOX_OX, WBLOCK_BOX_OX, THREEW_BOX_OX, LASER_BOX_OX, PARA_BOX_OX, BOMB_BOX_OX, 0, OPTION_BOX_OX, PLAYER_BOX_OX };
static const s16 boxOY[] = { 0, BULLET_BOX_OY, WBLOCK_BOX_OY, THREEW_BOX_OY, LASER_BOX_OY, PARA_BOX_OY, BOMB_BOX_OY, 0, OPTION_BOX_OY, PLAYER_BOX_OY };
static const s16 boxW[]  = { 0, BULLET_BOX_W,  WBLOCK_BOX_W,  THREEW_BOX_W,  LASER_BOX_W,  PARA_BOX_W,  BOMB_BOX_W,  0, OPTION_BOX_W,  PLAYER_BOX_W };
static const s16 boxH[]  = { 0, BULLET_BOX_H,  WBLOCK_BOX_H,  THREEW_BOX_H,  LASER_BOX_H,  PARA_BOX_H,  BOMB_BOX_H,  0, OPTION_BOX_H,  PLAYER_BOX_H };

static const u8 animBase[] = { 0, PROJ_ANIM_THIN, PROJ_ANIM_WBLOCK, PROJ_ANIM_THREEW,
                               PROJ_ANIM_LASER, PROJ_ANIM_PARA, PROJ_ANIM_BOMB, PROJ_ANIM_BOMB,
                               PROJ_ANIM_OPTION, PROJ_ANIM_TRI };

// Estado del pivote (oPivot): es una segunda "nave" que el input del jugador
// mueve a la vez que la suya, con los satélites orbitando alrededor.
static bool  rayBurning;
static fix16 pivotX, pivotY, pivotT;
static bool  pivotAlive;

// Lo que proyectile necesita del jugador cada frame.
static fix16 playerX, playerY, targetX, targetY;
static bool  playerHasTarget;
static u16   playerPad;
static u16   triTimer[MAX_PROJECTILES];

static void hide(u16 i)
{
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

static void spawn(u8 type, fix16 x, fix16 y, fix16 angle, fix16 speed, u16 life)
{
    const s16 i = freeSlot();

    if (i < 0) return;

    projs[i].type = type;
    projs[i].x = x;
    projs[i].y = y;
    projs[i].dx = (speed != 0) ? fmul(fcos(angle), speed) : 0;
    projs[i].dy = (speed != 0) ? fmul(fsin(angle), speed) : 0;
    projs[i].life = life;
    // La bomba y el satélite no rotan en el original; el resto sí.
    projs[i].anim = animBase[type] +
                    (((type == PROJ_BOMB) || (type == PROJ_RAY) || (type == PROJ_OPTION))
                         ? 0 : Entity_angleFrame8(angle));
    if (type == PROJ_TRI) triTimer[i] = TRI_SHOOT_FREE;

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
        case PROJ_LASER:
        case PROJ_BOMB:   Audio_sfx(SFX_LASER);    break;   // laserSnd
        default: break;
    }
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

    tLaser = FIX16(10);
    pivotAlive = FALSE;
    rayBurning = FALSE;
}

void Proj_setPlayer(fix16 px, fix16 py, u16 pad, bool hasTarget, fix16 tx, fix16 ty)
{
    playerX = px;
    playerY = py;
    playerPad = pad;
    playerHasTarget = hasTarget;
    targetX = tx;
    targetY = ty;
}

void Proj_spawnTri(fix16 x, fix16 y)
{
    u16 i, n = 0;

    for (i = 0; i < MAX_PROJECTILES; i++)
        if (projs[i].type == PROJ_TRI) n++;

    if (n >= MAX_TRI) return;

    spawn(PROJ_TRI, x, y, 0, 0, 0);
}

// oPlayer/Collision_oPivot. OJO: en el original este evento NO LLEGA A
// CORRER. oPivot tiene spriteId y spriteMaskId a null, así que no tiene máscara
// de colisión y el jugador no puede tocarlo nunca. Se deja portado porque el
// evento existe, pero no hay nada que lo dispare, igual que allí.
u16 Proj_triPositions(fix16 *xs, fix16 *ys, u16 max)
{
    u16 i, n = 0;

    for (i = 0; (i < MAX_PROJECTILES) && (n < max); i++)
    {
        if (projs[i].type != PROJ_TRI) continue;

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
    spawn(PROJ_THIN, x, y, angle, BULLET_SPEED_THIN, 0);
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
        case 0:     // WBLOCK
            spawn(PROJ_WBLOCK, x, y, angle, BULLET_SPEED_THIN, 0);
            return TRUE;

        case 1:     // LASER: largo segmentos encadenados cada 32 px del room
            for (i = 0; i < LASER_COUNT; i++)
            {
                const fix16 d = FIX16(LASER_SPACING * i);

                tLaser -= FIX16(1);
                if (tLaser >= 0) continue;

                spawn(PROJ_LASER,
                      x + fmul(fcos(angle), d),
                      y + fmul(fsin(angle), d),
                      angle, LASER_SPEED, LASER_LIFE);
                tLaser = FIX16R(0.6);
            }
            return TRUE;

        case 2:     // 3WAY: tres a -30, 0 y +30 grados
            spawn(PROJ_3WAY, x, y, F16_normalizeAngle(angle - THREEWAY_SPREAD), BULLET_SPEED_THIN, 0);
            spawn(PROJ_3WAY, x, y, angle, BULLET_SPEED_THIN, 0);
            spawn(PROJ_3WAY, x, y, F16_normalizeAngle(angle + THREEWAY_SPREAD), BULLET_SPEED_THIN, 0);
            return TRUE;

        case 3:     // PIVOT: "if !instance_exists(oPivot) instance_create(...)"
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

        case 4:     // PARA: sólo uno a la vez
            if (Proj_paraAlive()) return FALSE;
            spawn(PROJ_PARA, x, y, angle, BULLET_SPEED_PARA, PARA_LIFE);
            return TRUE;

        case 5:     // BOMBA, a 100 px del room por delante
            spawn(PROJ_BOMB,
                  x + fmul(fcos(angle), FIX16(WEAPON_SPAWN_DIST)),
                  y + fmul(fsin(angle), FIX16(WEAPON_SPAWN_DIST)),
                  angle, 0, BOMB_FUSE);
            return TRUE;

        case 6:     // RAY
            // oRay/Create_0: stShake(50, 20, 4), la segunda más larga del juego
            Shake_ray();
            spawn(PROJ_RAY,
                  x + fmul(fcos(angle), FIX16(WEAPON_SPAWN_DIST)),
                  y + fmul(fsin(angle), FIX16(WEAPON_SPAWN_DIST)),
                  angle, 0, RAY_FUSE);
            return TRUE;

        default:
            return FALSE;
    }
}

// Índice del satélite dentro del trío, por orden en el pool.
static u16 satelliteIndex(u16 slot)
{
    u16 i, n = 0;

    for (i = 0; i < slot; i++)
        if (projs[i].type == PROJ_OPTION) n++;

    return n;
}

bool Proj_rayBurning(void) { return rayBurning; }

u16 Proj_update(u16 *snakeKills)
{
    u16 i;
    u16 kills = 0;
    u16 optionsAlive = 0;

    rayBurning = FALSE;

    // oPivot/Step_0: el pivote se mueve con el mismo input que la nave y gira
    // la órbita 2 grados por cada dirección pulsada.
    //
    // En el GML el pivote y la nave avanzan los dos acc = 6, o sea que el
    // pivote va siempre pegado a la nave. Desde el bloque 2 de MonsterLite la
    // nave va a 4 px/f normalizados, así que el pivote se desplaza lo mismo
    // que ella (Player_padDelta) para seguir pegado.
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

        if (projs[i].type == PROJ_RAY) rayBurning = TRUE;
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
                else if (projs[i].type == PROJ_RAY)
                {
                    // oRay/Alarm_0: "with oMalo instance_destroy()" barre la
                    // pantalla, y crea un gusano de tipo 2. Destruye sin sumar
                    // kills, como en el original.
                    Enemy_clearAll();
                    Snake_clearAll();
                    Snake_spawn(SNAKE_TYPE_2);
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

        // oTri: no se mueve; apunta al más cercano y dispara con su cadencia.
        if (projs[i].type == PROJ_TRI)
        {
            if (playerHasTarget)
            {
                const fix16 a = Fast_angle(projs[i].x, projs[i].y, targetX, targetY);

                projs[i].anim = PROJ_ANIM_TRI + Entity_angleFrame8(a);
                sprAnim(projs[i].sprite, projs[i].anim);
                sprTile(projs[i].sprite, projTile[projs[i].anim]);

                if (triTimer[i] > 0) triTimer[i]--;

                if (triTimer[i] == 0)
                {
                    Proj_fireThin(projs[i].x, projs[i].y, a);
                    // oTri/Step_0: stShake(5, 5, 0.2), sólo en la rama con
                    // munición (la de cadencia 24).
                    if (g_playerBullets > 0) Shake_tri();
                    triTimer[i] = (g_playerBullets > 0) ? TRI_SHOOT_AMMO : TRI_SHOOT_FREE;
                    if (g_playerBullets > 0) g_playerBullets--;
                }
            }

            place(i);
            continue;
        }

        projs[i].x += projs[i].dx;
        projs[i].y += projs[i].dy;
        px = F2I(projs[i].x);
        py = F2I(projs[i].y);
        bx = px + boxOX[projs[i].type];
        by = py + boxOY[projs[i].type];

        // Fuera del room (Other_0 -> action_kill_object)
        if (((bx + boxW[projs[i].type]) < 0) || (bx > PLAYFIELD_W) ||
            ((by + boxH[projs[i].type]) < 0) || (by > PLAYFIELD_H))
        {
            hide(i);
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
                if (killed) { kills++; Debris_burst(projs[i].x, projs[i].y); }
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
                if (body && ((projs[i].type == PROJ_WBLOCK) || (projs[i].type == PROJ_OPTION)))
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
            // La cámara de la sacudida se acaba de recentrar.
            place(i);
            triTimer[i] = TRI_SHOOT_FREE;
        }
        else
        {
            hide(i);
        }
    }

    // Sin jugador no hay pivote: sus satélites se han borrado arriba.
    pivotAlive = FALSE;
    tLaser = FIX16(10);
}

void Proj_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_PROJECTILES; i++) hide(i);

    tLaser = FIX16(10);
}
