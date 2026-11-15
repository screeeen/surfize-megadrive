#include "bench.h"

#if PORT_PROF
u32 profAcc[PROF_COUNT];
u16 profMax[PROF_COUNT];
u16 profCpuMax;
u16 profCpuOver;
u16 profCpuSum;
u16 profVb[PROF_COUNT];
u16 profFrames;
#endif

#if (PORT_STRESS || PORT_BENCH)

#include "arena.h"
#include "enemy.h"
#include "snake.h"
#include "projectile.h"
#include "powerup.h"
#include "hud.h"
#include "entity.h"
#include "spr.h"
#include "vramspr.h"
#include "resources.h"
#include "effect.h"

// Los topes del port: 8 arañas, 3 gusanos de tipo 1 y 1 de tipo 2 (52
// eslabones), 14 proyectiles y los paquetes que quepan. Es el caso peor que
// el juego puede poner en pantalla.
void Bench_fillArena(void)
{
    u16 i;

    for (i = 0; i < MAX_SPIDERS; i++) Enemy_spawn();
    for (i = 0; i < MAX_CABEZA_1; i++) Snake_spawn(SNAKE_TYPE_1);
    for (i = 0; i < MAX_CABEZA_2; i++) Snake_spawn(SNAKE_TYPE_2);

    // Unos cuantos paquetes por el suelo, como tras varias muertes.
    for (i = 0; i < 8; i++)
        Pow_generate(FIX16(40 + (i * 30)), FIX16(60 + ((i & 3) * 40)));

#ifdef PORT_TEST_BOMB
    // El blancazo de la bomba sin tener que ir a por el paquete.
    Effect_bombFlash();
#endif
#ifdef PORT_STRESS_MORTAL
    // Una réplica, para comprobar que sobrevive a la muerte del jugador.
    Proj_spawnTri(FIX16(250), FIX16(200));
#endif
}

#endif  // PORT_STRESS || PORT_BENCH

#if PORT_BENCH

u16 benchFrames[BENCH_COUNT];

// Resultado "sumidero" para que el optimizador no borre las llamadas puras.
static volatile u16 sink;

static void settle(void)
{
    u16 k;

    // Los enemigos nacen con la alarma de aparición: hay que dejarles pasar
    // los 75 frames del original para medirlos ya visibles y moviéndose.
    for (k = 0; k < 120; k++)
    {
        Enemy_updateAll(TRUE, FIX16(160), FIX16(120));
        Snake_updateAll(TRUE, FIX16(160), FIX16(120));
    }
}

void Bench_run(void)
{
    u16 k;
    u32 t0;

    Bench_fillArena();
    settle();

#define BENCH(slot, ...)                                    \
    do {                                                    \
        t0 = vtimer;                                        \
        for (k = 0; k < BENCH_ITERS; k++) { __VA_ARGS__; }  \
        benchFrames[slot] = (u16)(vtimer - t0);             \
    } while (0)

    BENCH(BENCH_ENEMY_UPDATE,   Enemy_updateAll(TRUE, FIX16(160), FIX16(120)));
    BENCH(BENCH_SNAKE_UPDATE,   Snake_updateAll(TRUE, FIX16(160), FIX16(120)));

    // Las de colisión se miden sin acierto: es el recorrido completo, que es
    // justo el caso peor.
    BENCH(BENCH_ENEMY_COLLIDES, sink += Enemy_collides(-200, -200, 7, 11));
    BENCH(BENCH_SNAKE_COLLIDES, sink += Snake_collides(-200, -200, 7, 11));

    // El barrido que hace Proj_update por cada bala viva: 14 balas contra
    // arañas y cabezas.
    BENCH(BENCH_PROJ_SCAN, {
        u16 b;
        bool killed;
        for (b = 0; b < MAX_PROJECTILES; b++)
        {
            bool body;
            sink += Enemy_hit(-200, -200, 4, 5, 1, &killed);
            sink += Snake_hit(-200, -200, 4, 5, &killed, &body);
        }
    });

    BENCH(BENCH_CLOSEST, {
        s16 ax, ay;
        s32 bd = 0x7FFFFFFF;
        sink += Enemy_closest(160, 120, &ax, &ay, &bd);
        sink += Snake_closest(160, 120, &ax, &ay, &bd);
    });

    BENCH(BENCH_SPR_UPDATE, Spr_flush());
    BENCH(BENCH_HUD_UPDATE, HUD_update());

    BENCH(BENCH_POW_COLLECT, {
        PowerPickup got;
        sink += Pow_collect(-200, -200, 7, 11, 0, &got);
    });

    // Colocar y cambiar de frame, que es lo que hacen los dos bucles grandes.
    {
        MDSpr *spr = VSpr_add(&playerShip, PAL1, 0);

        if (spr)
        {
            BENCH(BENCH_SPR_SETPOS,   sprPos(spr, 100 + (k & 1), 100));
            BENCH(BENCH_SPR_SETANIM,  sprAnim(spr, k & 7));
            sprHide(spr);
        }
    }

    {
        Entity probe;

        Entity_spawnHidden(&probe, FIX16(100), FIX16(100), FIX16(1), FIX16(2), 4);
        probe.visible = TRUE;
        probe.speed = FIX16(1);
        probe.state = ENT_ATTACK;

        BENCH(BENCH_ENTITY_AI, Entity_updateAI(&probe, TRUE, FIX16(160), FIX16(120), 0, 0, 7, 11));
        probe.state = ENT_ATTACK;
        probe.stateTime = 0;
        BENCH(BENCH_ENTITY_ADVANCE, Entity_advance(&probe));
    }

#undef BENCH
}

#endif  // PORT_BENCH
