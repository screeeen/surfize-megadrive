#ifndef _BENCH_H_
#define _BENCH_H_

#include <genesis.h>

// ===========================================================================
// Arnés de medición (FASE 10). Inerte salvo que se compile con los flags
// puestos a 1; no entra en la ROM que se distribuye.
//
//   PORT_STRESS  llena la arena al arrancar (8 arañas + los 4 gusanos) y hace
//                al jugador inmortal, para leer el caso peor en el contador
//                de CPU de pantalla. Con PORT_STRESS_MORTAL además el
//                jugador puede morir y nace una nave réplica: sirve para ver
//                qué sobrevive a la muerte (paquetes y réplicas).
//
//   PORT_BENCH   mide, llamada a llamada, lo que cuesta cada módulo del bucle
//                de juego y deja el resultado en benchFrames[], que se lee
//                desde el depurador:
//
//                  python3 tools/mdrun.py out/rom.bin "frames 400"
//                      "p/d [0x<dir de benchFrames>]" (una por entrada)
//
//                Cada entrada son los frames que tardan BENCH_ITERS llamadas,
//                así que el coste por llamada en ciclos del 68000 es
//                frames * 153409 / BENCH_ITERS (PAL: 7,67 MHz / 50 Hz).
// ===========================================================================

#ifndef PORT_STRESS
#define PORT_STRESS 0
#endif

#ifndef PORT_BENCH
#define PORT_BENCH 0
#endif

#define BENCH_ITERS 400

enum
{
    BENCH_ENEMY_UPDATE = 0,
    BENCH_SNAKE_UPDATE,
    BENCH_ENEMY_COLLIDES,
    BENCH_SNAKE_COLLIDES,
    BENCH_PROJ_SCAN,
    BENCH_CLOSEST,
    BENCH_SPR_UPDATE,
    BENCH_HUD_UPDATE,
    BENCH_POW_COLLECT,
    BENCH_SPR_SETPOS,
    BENCH_SPR_SETANIM,
    BENCH_ENTITY_AI,
    BENCH_ENTITY_ADVANCE,
    BENCH_COUNT
};

#if (PORT_STRESS || PORT_BENCH)
// Llena la arena con los topes del port.
void Bench_fillArena(void);
#endif

#if PORT_BENCH
extern u16 benchFrames[BENCH_COUNT];
void Bench_run(void);
#endif

// ---------------------------------------------------------------------------
// Perfilado por etapas dentro de la partida de verdad (PORT_PROF).
//
// Mide con el contador de línea del VDP: una línea son 490 ciclos del 68000 y
// un frame PAL tiene 313. Acumula sobre muchos frames y se lee profAcc[] y
// profFrames desde el depurador; los saltos del contador (el de PAL no es
// lineal) se descartan por tamaño.
// ---------------------------------------------------------------------------
#ifndef PORT_PROF
#define PORT_PROF 0
#endif

enum
{
    PROF_ENEMY = 0,
    PROF_SNAKE,
    PROF_PLAYER,
    PROF_PROJ,
    PROF_EFFECT,
    PROF_HUD,
    PROF_SPRUPDATE,
    PROF_VBLANK,
    PROF_S_AI,
    PROF_S_CACHE,
    PROF_S_HEAD,
    PROF_S_PARTS,
    PROF_COUNT
};

#if PORT_PROF
extern u32 profAcc[PROF_COUNT];
// El peor frame de cada etapa. La media no basta para saber por qué se cae un
// frame de vez en cuando: lo que hay que buscar es el pico.
extern u16 profMax[PROF_COUNT];
extern u16 profFrames;
// Carga de CPU del frame, de SGDK: el peor frame y cuántos se pasaron del
// 100 %. El contador de línea no sirve para esto porque se envuelve.
extern u16 profCpuMax;
extern u16 profCpuOver;
extern u16 profCpuSum;
// Cuántos vblanks se come cada etapa. El contador de línea del VDP se
// envuelve cada 256 líneas, así que no puede medir un pico de más de un
// frame; esto sí: cuenta las veces que el reloj de vblank avanza DENTRO de
// la etapa, que es exactamente lo que hace caer el framerate.
extern u16 profVb[PROF_COUNT];

// PORT_PROF=1 deja sólo los contadores de frame y de carga, que casi no
// cuestan; PORT_PROF=2 añade el cronómetro por etapas, que sí se nota en la
// propia medida (dos lecturas del VDP y una del reloj por etapa y frame).
#if PORT_PROF >= 2
#define PROF_START(id)  const u16 _p##id = (GET_HVCOUNTER >> 8); const u32 _v##id = vtimer
#define PROF_STOP(id)   do {                                            \
        const u16 _d = (u16)(((GET_HVCOUNTER >> 8) - _p##id) & 0xFF);   \
        if (_d < 150) profAcc[id] += _d;                                \
        if (_d > profMax[id]) profMax[id] = _d;                         \
        profVb[id] += (u16)(vtimer - _v##id);                           \
    } while (0)
#else
#define PROF_START(id)  do {} while (0)
#define PROF_STOP(id)   do {} while (0)
#endif

#define PROF_FRAME()    (profFrames++)
#define PROF_CPU()      do {                                            \
        const u16 _l = SYS_getCPULoad();                                \
        if (_l > profCpuMax)  profCpuMax = _l;                          \
        if (_l > 100)         profCpuOver++;                            \
        profCpuSum += _l;                                               \
    } while (0)
#else
#define PROF_START(id)  do {} while (0)
#define PROF_STOP(id)   do {} while (0)
#define PROF_FRAME()    do {} while (0)
#define PROF_CPU()      do {} while (0)
#endif

#endif
