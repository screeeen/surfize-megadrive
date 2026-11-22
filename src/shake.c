#include "shake.h"

s16 g_shakeX;
s16 g_shakeY;

// MonsterLite: frames que le quedan a cada corrutina.
static u16   bigLeft;
static u16   smallLeft;

// GML: stShake.
static s16   shTime;
static fix16 shMag;
static fix16 shFade;
static u8    shActive;

// camera_set_view_pos(view_camera[0], x, y): la cámara se mueve a (x, y), o
// sea que el mundo se desplaza (-x, -y). En el VDP eso es mover el plano a la
// izquierda (hscroll negativo) y hacia arriba (vscroll positivo).
//
// Las versiones VSync de SGDK guardan el valor y lo escriben en el VDP
// dentro de SYS_doVBlankProcess, en el mismo vblank en que sube la tabla de
// sprites. Las versiones directas escribian el registro A MITAD DE FRAME,
// mientras el VDP estaba dibujando: la parte de pantalla ya dibujada se
// quedaba con el desplazamiento viejo y la de abajo con el nuevo (una linea
// de corte que bailaba con la sacudida), y ademas el plano se movia un frame
// antes que los sprites, que no cambian hasta el vblank siguiente. Durante
// las sacudidas largas (el rayo, la muerte) los agujeros parecian deslizarse
// bajo la nave.
static void apply(void)
{
    VDP_setHorizontalScrollVSync(BG_A, -g_shakeX);
    VDP_setVerticalScrollVSync(BG_A, g_shakeY);
    // El fondo vive en el plano B y tiene que sacudirse con el A, o los
    // agujeros parecerian deslizarse sobre el campo (ver inc/bg.h).
    VDP_setHorizontalScrollVSync(BG_B, -g_shakeX);
    VDP_setVerticalScrollVSync(BG_B, g_shakeY);
}

// Random.Range(-m, m) con floats: aquí un entero uniforme de -m a m.
static s16 rnd(s16 m)
{
    return (s16)(random() % (u16)((2 * m) + 1)) - m;
}

void Shake_big(u16 frames)
{
    if (bigLeft < frames) bigLeft = frames;
}

void Shake_small(u16 frames)
{
    if (smallLeft < frames) smallLeft = frames;
}

void Shake_start(u16 time, fix16 magnitude, fix16 fade)
{
    // stShake no mira si ya hay una sacudida en curso: la sustituye, aunque la
    // nueva sea más floja que la que había.
    shTime   = (s16)time;
    shMag    = magnitude;
    shFade   = fade;
    shActive = TRUE;
}

bool Shake_active(void)
{
    return (bigLeft | smallLeft | shActive | g_shakeX | g_shakeY) != 0;
}

void Shake_stop(void)
{
    bigLeft = 0;
    smallLeft = 0;
    shActive = FALSE;

    if (g_shakeX | g_shakeY)
    {
        g_shakeX = 0;
        g_shakeY = 0;
        apply();
    }
}

// stShake del GML, un frame. FALSE si no hay ninguna en marcha.
static bool gmlStep(void)
{
    if (!shActive) return FALSE;

    shTime--;

    {
        // choose(-magnitude, magnitude) en los dos ejes. Redondeado al píxel
        // más cercano y no truncado: truncar le quita a cada sacudida la cola
        // en la que la magnitud ya ha bajado de 1 px, que es casi toda.
        const u16 r = random();
        const s16 m = (s16)((shMag + FIX16(0.5)) >> FIX16_FRAC_BITS);

        g_shakeX = (r & 1) ? m : -m;
        g_shakeY = (r & 2) ? m : -m;
    }

    // La magnitud no baja hasta que se agota el tiempo.
    if (shTime <= 0)
    {
        shMag -= shFade;

        if (shMag <= 0)
        {
            shActive = FALSE;
            return FALSE;
        }
    }

    return TRUE;
}

void Shake_update(void)
{
    if (bigLeft)
    {
        bigLeft--;
        g_shakeX = rnd(ML_SHAKE_BIG_X);
        g_shakeY = rnd(ML_SHAKE_BIG_Y);
        // La del GML sigue contando por debajo, como si fueran dos cámaras.
        if (shActive) { const s16 x = g_shakeX, y = g_shakeY; gmlStep(); g_shakeX = x; g_shakeY = y; }
        apply();
        return;
    }

    if (smallLeft)
    {
        smallLeft--;
        g_shakeX = rnd(ML_SHAKE_SMALL);
        g_shakeY = rnd(ML_SHAKE_SMALL);
        if (shActive) { const s16 x = g_shakeX, y = g_shakeY; gmlStep(); g_shakeX = x; g_shakeY = y; }
        apply();
        return;
    }

    if (gmlStep())
    {
        apply();
        return;
    }

    // Al acabar, la cámara vuelve a (0, 0): una escritura y ya.
    if (g_shakeX | g_shakeY)
    {
        g_shakeX = 0;
        g_shakeY = 0;
        apply();
    }
}
