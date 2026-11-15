#include "entity.h"
#include "fastmath.h"
#include "hole.h"
#include "collision.h"

fix16 Entity_mlSpeed(u16 level, u16 maxMu)
{
    const u16 f = ((level % 10) * 1000) + ML_SPEED_F_BASE_MU;
    const u16 g = maxMu + (f / 10);
    const u16 lo = (f < g) ? f : g;
    const u16 hi = (f < g) ? g : f;
    const u16 v = lo + (random() % (u16)(hi - lo + 1));

    // milésimas de u/s -> px/frame en fix16: v / 1000 * 0.4 * 64 = v * 16 / 625,
    // redondeado. Sólo corre al crear un enemigo.
    return (fix16)((((u32)v * 16) + 312) / 625);
}

void Entity_clear(Entity *e)
{
    e->active = FALSE;
    e->visible = FALSE;
    e->speed = 0;
    e->velo = 0;
    e->alarm = 0;
    e->stateTime = 0;
    e->state = ENT_STEADY;
    e->hp = 0;
}

void Entity_spawnHidden(Entity *e, fix16 x, fix16 y, fix16 velo, fix16 turnRate, s8 hp)
{
    e->x = x;
    e->y = y;
    e->angle = 0;
    // oSpider/oCabeza1 Create_0: speed = 0; visible = 0; alarm[0] = 90.
    // La velocidad se guarda aparte (velo) y sólo se aplica en Alarm_0.
    e->speed = 0;
    e->velo = velo;
    e->turnRate = turnRate;
    e->alarm = ENEMY_APPEAR_DELAY;
    e->stateTime = 0;
    e->state = ENT_STEADY;
    e->visible = FALSE;
    e->active = TRUE;
    e->hp = hp;
}

bool Entity_tickAppear(Entity *e)
{
    if (e->visible) return TRUE;

    if (e->alarm > 0)
    {
        e->alarm--;
        return FALSE;
    }

    // oSpider/Alarm_0: "speed = velo; visible = 1"
    e->speed = e->velo;
    e->visible = TRUE;
    return TRUE;
}

fix16 Entity_angleTo(const Entity *e, fix16 tx, fix16 ty)
{
    // Fast_angle en vez de F16_getAngle: misma convención, 0,23 grados de
    // error como mucho y ~15 veces más rápido (ver src/fastmath.c).
    return Fast_angle(e->x, e->y, tx, ty);
}

void Entity_turnTowards(Entity *e, fix16 targetAngle, fix16 turnRate)
{
    fix16 diff = fnorm(targetAngle - e->angle);

    // Camino corto: pasa el rango [0, 360) a (-180, 180].
    if (diff > FIX16(180)) diff -= FIX16(360);

    if (diff > turnRate)        e->angle = fnorm(e->angle + turnRate);
    else if (diff < -turnRate)  e->angle = fnorm(e->angle - turnRate);
    else                        e->angle = fnorm(targetAngle);
}

void Entity_advance(Entity *e)
{
    if (e->speed == 0) return;

    // El ángulo se mantiene normalizado en Entity_turnTowards, así que basta
    // con pasarlo a grados enteros y leer la tabla: el seno y el coseno de
    // SGDK normalizan otra vez y encadenan comparaciones por cuadrante.
    {
        const u16 deg = (u16)F2I(e->angle);

        e->x += fmul(fcosDeg(deg), e->speed);
        e->y += fmul(fsinDeg(deg), e->speed);
    }
}

void Entity_wrapPlayfield(Entity *e, s16 margin)
{
    const fix16 minX = FIX16(0) + FIX16(margin);
    const fix16 maxX = FIX16(PLAYFIELD_W) - FIX16(margin);
    const fix16 minY = FIX16(0) + FIX16(margin);
    const fix16 maxY = FIX16(PLAYFIELD_H) - FIX16(margin);

    if (e->x < minX) e->x = maxX;
    if (e->x > maxX) e->x = minX;
    if (e->y < minY) e->y = maxY;
    if (e->y > maxY) e->y = minY;
}

u8 Entity_angleFrame8(fix16 angle)
{
    return (u8)(((F2I(fnorm(angle)) + 22) / 45) & 7);
}

u8 Entity_frame8(const Entity *e)
{
    return Entity_angleFrame8(e->angle);
}

u8 Entity_angleFrame32(fix16 angle)
{
    // En unidades de 1/64 de grado: un paso son 11,25 * 64 = 720 y se
    // redondea al más cercano sumando medio paso.
    return (u8)(((u16)(fnorm(angle) + 360) / 720) & (ENTITY_ROT_SMOOTH - 1));
}

u8 Entity_frame32(const Entity *e)
{
    return Entity_angleFrame32(e->angle);
}

s32 Entity_distSq(fix16 ax, fix16 ay, fix16 bx, fix16 by)
{
    // s16 a propósito: así el 68000 multiplica con muls.w (16x16 a 32) en vez
    // de llamar a la rutina de 32x32 del compilador.
    const s16 dx = F2I(ax) - F2I(bx);
    const s16 dy = F2I(ay) - F2I(by);

    return (s32)(dx * dx) + (s32)(dy * dy);
}

// ===========================================================================
// stSteadyW / stAppearA1W / stWalkingA1W
//
// El state_machine del original transiciona con state_nextnow, que ejecuta
// el estado siguiente en el MISMO step, y compara el tiempo con igualdad
// exacta ("if (state_time = 300)"). Esto reproduce las dos cosas.
// ===========================================================================
bool Entity_updateAI(Entity *e, bool hasPlayer, fix16 px, fix16 py, s16 boxOX, s16 boxOY, s16 boxW, s16 boxH)
{
    u16 guard = 3;      // como mucho tres transiciones en el mismo frame
    bool snapped = FALSE;

    while (guard--)
    {
        if (e->state == ENT_STEADY)
        {
            // stSteadyW: quieto hasta state_time == 6
            if (e->stateTime == ST_STEADY_TIME)
            {
                e->state = ENT_ATTACK;
                e->stateTime = 0;
                continue;               // state_nextnow
            }

            e->stateTime++;
            return snapped;
        }

        if (e->state == ENT_ATTACK)
        {
            // stAppearA1W: persigue al jugador girando velo grados por frame
            if (hasPlayer)
                Entity_turnTowards(e, Entity_angleTo(e, px, py), e->turnRate);

            if (e->stateTime == ST_ATTACK_TIME)
            {
                e->state = ENT_WANDER;
                e->stateTime = 0;
                continue;
            }

            e->stateTime++;
            return snapped;
        }

        // stWalkingA1W: va hacia un agujero girando 2 grados por frame.
        //
        // El original re-sortea el agujero destino CADA frame, y además lo
        // hace con un bucle que deja fuera los agujeros 0 y 4
        // (instance_find(oAgujerSuper, f - random(f-1)) con f = 4 da 1, 2 o 3
        // con igual probabilidad). Portado tal cual: el rumbo tiembla y el
        // enemigo deriva hacia el racimo central de agujeros.
        {
            const u16 h = Hole_wanderTarget();

            Entity_turnTowards(e, Entity_angleTo(e, Hole_x(h), Hole_y(h)), WANDER_TURN);

            // "if place_meeting(x,y,oAgujerSuper) {x = oAgujerSuper.x; ...}":
            // oAgujerSuper sin cualificar es la PRIMERA instancia, así que al
            // tocar cualquier agujero el enemigo salta al agujero 0.
            if (Hole_touching(F2I(e->x) + boxOX, F2I(e->y) + boxOY, boxW, boxH))
            {
                e->x = Hole_x(0);
                e->y = Hole_y(0);
                snapped = TRUE;
            }

            if (e->stateTime == ST_WANDER_TIME)
            {
                e->state = ENT_ATTACK;
                e->stateTime = 0;
                continue;
            }

            e->stateTime++;
            return snapped;
        }
    }

    return snapped;
}
