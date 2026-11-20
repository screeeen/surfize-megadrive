#include "entity.h"
#include "fastmath.h"
#include "game.h"

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

void Entity_mlRandomPos(fix16 *x, fix16 *y)
{
    *x = FIX16(ML_RANDPOS_X0 + (random() % ML_RANDPOS_W));
    *y = FIX16(ML_RANDPOS_Y0 + (random() % ML_RANDPOS_H));
}

void Entity_clear(Entity *e)
{
    e->active = FALSE;
    e->visible = FALSE;
    e->speed = 0;
    e->timer = 0;
    e->turnK = 0;
    e->state = ENT_ATTACK;
    e->hp = 0;
}

void Entity_spawnML(Entity *e, fix16 x, fix16 y, fix16 speed, u16 turnKMax, s8 hp)
{
    e->x = x;
    e->y = y;
    // transform.rotation = Euler(.., .., Random.Range(0f, 360f)): en 1/64 de
    // grado, 0 .. 23039.
    e->angle = (fix16)(random() % (u16)FIX16(360));
    e->speed = speed;
    // turningSpeed = Random.Range(1f, turningSpeedMax / 1.4f), ya como la
    // fracción por frame del Slerp (ML_TURN_K_*).
    e->turnK = ML_TURN_K_MIN + (random() % (u16)(turnKMax - ML_TURN_K_MIN + 1));
    // optionState = (enemyState) Random.Range(0, 2): ATTACK o WANDER.
    e->state = (random() & 1) ? ENT_WANDER : ENT_ATTACK;
    e->timer = ML_AI_TIMER_START;
    // randomPos = globales.getRandomPos(). target es un Vector2 sin
    // inicializar: (0, 0) del mundo, que con la cámara en el origen
    // (S/cameraScript.cs) es el centro de la pantalla. Sólo se nota si nace
    // en ATTACK sin jugador ni nave holo.
    Entity_mlRandomPos(&e->rx, &e->ry);
    e->tx = FIX16(PLAYFIELD_W / 2);
    e->ty = FIX16(PLAYFIELD_H / 2);
    e->visible = TRUE;
    e->active = TRUE;
    e->hp = hp;
}

fix16 Entity_angleTo(const Entity *e, fix16 tx, fix16 ty)
{
    // Fast_angle en vez de F16_getAngle: misma convención, 0,23 grados de
    // error como mucho y ~15 veces más rápido (ver src/fastmath.c).
    return Fast_angle(e->x, e->y, tx, ty);
}

void Entity_advance(Entity *e)
{
    if (e->speed == 0) return;

    // El ángulo se mantiene normalizado (Entity_moveML), así que basta
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
// enemyMov de MonsterLite (bloque 8)
// ===========================================================================

// "level % 100" en quintos, calculado sólo cuando cambia el nivel: el módulo
// es un divu.w de unos 140 ciclos y se pediría por enemigo y frame.
static u16 wanderLevel = 0xFFFF;
static s16 wanderDec;

static FORCE_INLINE s16 wanderStep(void)
{
    if (g_level != wanderLevel)
    {
        wanderLevel = g_level;
        wanderDec = (s16)(ML_AI_TIMER_WANDER * (g_level % 100));
    }
    return wanderDec;
}
void Entity_moveML(Entity *e, bool hasTarget, fix16 tx, fix16 ty)
{
    // switch (optionState): ATTACK va al jugador o, si no hay, a una nave
    // holo (lo resuelve quien llama); sin ninguno, target no cambia. WANDER
    // va a randomPos y además baja el temporizador.
    if (e->state == ENT_ATTACK)
    {
        if (hasTarget)
        {
            e->tx = tx;
            e->ty = ty;
        }
    }
    else
    {
        e->tx = e->rx;
        e->ty = e->ry;
        // "timer -= globales.level % 100", en quintos.
        e->timer -= wanderStep();
    }

    {
        // calculateAngle(target) con la posición de ANTES de avanzar, y
        // Translate(right * step) con el rumbo de antes de girar.
        const fix16 want = Fast_angle(e->x, e->y, e->tx, e->ty);
        fix16 diff;

        Entity_advance(e);

        // Quaternion.Slerp(rotation, Euler(0, 0, want), turningSpeed * 0.02):
        // gira por el camino corto la fracción turnK / 4096 de lo que falta.
        // diff cabe en 16 bits (+-180 grados = +-11520) y turnK también, así
        // que el producto es un muls.w. Se redondea al 1/64 de grado más
        // cercano: el giro deja de avanzar cuando faltan menos de
        // 2048 / turnK sesentaicuatroavos (0,39 grados con el giro más lento).
        diff = fnorm(want - e->angle);
        if (diff > FIX16(180)) diff -= FIX16(360);

        e->angle = fnorm(e->angle +
                         (fix16)((((s32)(s16)diff * (s16)e->turnK) + (1 << (ML_TURN_K_SHIFT - 1)))
                                 >> ML_TURN_K_SHIFT));
    }
}

void Entity_tickML(Entity *e)
{
    // "timer -= 1; if (timer < 0) { resetMovement(); timer = 30; }" a 60 Hz.
    e->timer -= ML_AI_TIMER_UPDATE;
    if (e->timer >= 0) return;

    // resetMovement(): estado y randomPos nuevos. También pone target en el
    // jugador si lo hay, pero eso no se ve: en ATTACK move() lo vuelve a
    // poner en el jugador en el tick siguiente y en WANDER lo pisa randomPos.
    e->state = (random() & 1) ? ENT_WANDER : ENT_ATTACK;
    Entity_mlRandomPos(&e->rx, &e->ry);
    e->timer = ML_AI_TIMER_RESET;
}
