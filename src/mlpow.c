#include "mlpow.h"
#include "enemy.h"
#include "snake.h"
#include "effect.h"
#include "audio.h"

static MLRay rays[MLPOW_MAX_RAYS];
static u16   rayCount;
static u16   rayTimer;
static fix16 rayOX, rayOY;

u16 MLPow_clearEnemies(void)
{
    const u16 n = Enemy_explodeAll() + Snake_explodeAll();

    // SoundManager.playBombaClip (bombaXPLSnd), una vez por enemigo
    // (S/enemyController.cs:175), y sólo si no suena nada (!isPlaying): aquí
    // una sola, que superpuestas en el mismo frame suenan igual.
    if (n) Audio_sfx(SFX_ML_XPL);

    return n;
}

void MLPow_startRays(fix16 px, fix16 py)
{
    u16 i;
    s16 x, y;

    // Cada rayo nuevo es un objeto aparte con su propio timer; como todos
    // nacen a la vez y el primero que se acaba limpia la pantalla, basta un
    // temporizador. Si ya había rayos, los nuevos se añaden y se queda el
    // temporizador de los viejos: cuando llega, clearEnemies() se lleva
    // también a los objetivos de los nuevos, que al acabarse no encontrarían
    // a nadie. Sólo se pierde su explosión.
    if (rayCount == 0) rayTimer = ML_RAYS_TIMER_F;
    rayOX = px;
    rayOY = py;

    for (i = 0; (i < MAX_SPIDERS) && (rayCount < MLPOW_MAX_RAYS); i++)
    {
        if (!Enemy_slotPos(i, &x, &y)) continue;
        rays[rayCount].kind = 1;
        rays[rayCount].slot = (u8)i;
        rays[rayCount].tx = x;
        rays[rayCount].ty = y;
        rayCount++;
    }

    for (i = 0; (i < MAX_SNAKES) && (rayCount < MLPOW_MAX_RAYS); i++)
    {
        if (!Snake_headPos(i, &x, &y)) continue;
        rays[rayCount].kind = 2;
        rays[rayCount].slot = (u8)i;
        rays[rayCount].tx = x;
        rays[rayCount].ty = y;
        rayCount++;
    }
}

u16 MLPow_tickRays(void)
{
    u16 i;

    if (rayCount == 0) return 0;

    // "timer--; if (timer < 0) { clearEnemies(); explosión; Destroy }"
    if (--rayTimer == 0)
    {
        // randomExplosion en la posición del rayo, que es donde estaba la
        // nave al tocar el power-up. Sin sonido propio: el de clearEnemies.
        Effect_spawnQuiet(rayOX, rayOY);
        rayCount = 0;
        return MLPow_clearEnemies();
    }

    // "if (target)": mientras el objetivo vive, la línea va hasta él; si ha
    // muerto, se queda donde estaba.
    for (i = 0; i < rayCount; i++)
    {
        MLRay *r = &rays[i];
        s16 x, y;
        bool alive;

        if (r->kind == 1)      alive = Enemy_slotPos(r->slot, &x, &y);
        else if (r->kind == 2) alive = Snake_headPos(r->slot, &x, &y);
        else                   continue;

        if (alive) { r->tx = x; r->ty = y; }
        else       r->kind = 3;
    }

    return 0;
}

const MLRay *MLPow_rays(u16 *count)
{
    *count = rayCount;
    return rayCount ? rays : NULL;
}

void MLPow_clear(void)
{
    rayCount = 0;
}
