#ifndef _FASTMATH_H_
#define _FASTMATH_H_

#include <genesis.h>

// ===========================================================================
// Aritmética de punto fijo del bucle de juego
//
// SGDK declara F16_toInt, F16_mul, F16_sin/cos y F16_normalizeAngle en
// maths.h y las define en maths.c: para el resto del proyecto son llamadas a
// función de verdad, porque el enlazado va sin LTO (la libmd.a precompilada
// trae bytecode de un GCC más viejo que este toolchain y lto1 no lo lee). En
// el bucle de juego eso son cientos de `jsr` con argumentos por pila cada
// frame, para operaciones que son un desplazamiento o una consulta de tabla.
//
// Medido con el banco de pruebas (ver inc/bench.h) en el caso peor del port:
//   - F16_getAngle: ~3000 ciclos por llamada (F16_atan2 hace una división de
//     32 bits por software y un polinomio de grado 7), y se llama una vez por
//     enemigo y frame.
//   - el resto (F16_toInt sobre todo) se llama ~600 veces por frame.
// Un frame PAL son 153409 ciclos del 68000: eso no cabía.
//
// Aquí están las mismas operaciones como inline, y un atan2 por tabla que usa
// la división 32/16 del 68000 en vez de la rutina software.
// ===========================================================================

// F16_toInt / FIX16 sin llamada.
#define F2I(v) ((s16)((v) >> FIX16_FRAC_BITS))
#define I2F(v) ((fix16)((s16)(v) << FIX16_FRAC_BITS))

// F16_mul: muls.w de 16x16 a 32 bits y vuelta.
static FORCE_INLINE fix16 fmul(fix16 a, fix16 b)
{
    return (fix16)(((s32)(a * b)) >> FIX16_FRAC_BITS);
}

// F16_normalizeAngle sin división: los ángulos del juego salen de sumar o
// restar un giro, así que nunca se van más de una vuelta fuera de rango y
// basta con sumar o restar 360. El '%' de SGDK es un divs.w de 158 ciclos
// cada vez, y esto se llama varias veces por enemigo y frame.
static FORCE_INLINE fix16 fnorm(fix16 angle)
{
    while (angle < 0)             angle += FIX16(360);
    while (angle >= FIX16(360))   angle -= FIX16(360);
    return angle;
}

// Seno por grado entero, 0..449, para poder sacar el coseno como
// sinTab[grados + 90] sin normalizar otra vez. Se rellena en Fast_init()
// llamando al propio F16_sin de SGDK, así que los valores son exactamente
// los suyos: esto es velocidad, no una aproximación distinta.
extern fix16 sinTab[450];

void Fast_init(void);

// Para ángulos ya normalizados a [0,360), que es como los guarda el juego.
static FORCE_INLINE fix16 fsinDeg(u16 deg) { return sinTab[deg]; }
static FORCE_INLINE fix16 fcosDeg(u16 deg) { return sinTab[deg + 90]; }

// F16_sin/F16_cos con la misma tabla de 1 grado de SGDK (trigtab_f16).
static FORCE_INLINE fix16 fsin(fix16 angle)
{
    const fix16 a = fnorm(angle);

    if (a <= FIX16(90))  return trigtab_f16[a >> FIX16_FRAC_BITS];
    if (a <= FIX16(180)) return trigtab_f16[(FIX16(180) - a) >> FIX16_FRAC_BITS];
    if (a <= FIX16(270)) return -trigtab_f16[(a - FIX16(180)) >> FIX16_FRAC_BITS];
    return -trigtab_f16[(FIX16(360) - a) >> FIX16_FRAC_BITS];
}

static FORCE_INLINE fix16 fcos(fix16 angle)
{
    return fsin(angle + FIX16(90));
}

// Rumbo de (x1,y1) a (x2,y2) en grados fix16 [0,360), con Y hacia abajo:
// misma convención que F16_getAngle de SGDK (0 = derecha, 90 = abajo).
fix16 Fast_angle(fix16 x1, fix16 y1, fix16 x2, fix16 y2);

#endif
