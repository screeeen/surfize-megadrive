#include "trail.h"
#include "spr.h"
#include "vramspr.h"
#include "resources.h"
#include "arena.h"

#define TRAIL_SPR_OFF   (-4)        // el punto está en el centro de la celda de 8
#define TRAIL_BIG_AGE   7           // edad hasta la que va el punto de 2x2

typedef struct
{
    s16 x, y;
    u16 born;                       // valor de 'now' al dejarlo
} Vertex;

// Cada vértice del anillo tiene su sprite fijo (dot[k] es el de vtx[k]): se
// coloca al nacer, cambia de punto una vez (a los TRAIL_BIG_AGE frames) y se
// aparca al caducar. Así un frame normal no toca la tabla de sprites más que
// para el vértice nuevo; sólo con la cámara sacudiéndose hay que recolocarlos
// todos (sprPos resta la sacudida).
static MDSpr  *dot[ML_TRAIL_F];
static u16     dotTile[2];       // trail.png: el punto de 2x2 y el de 1x1
static Vertex  vtx[ML_TRAIL_F];     // anillo: vtx[head] es el más nuevo
static u16     head, count;
static u16     now;
static s16     placedSX, placedSY;  // la sacudida con la que se colocaron

void Trail_init(void)
{
    u16 i;

    VSpr_load(&trailSprite, dotTile, 1);

    for (i = 0; i < ML_TRAIL_F; i++)
        dot[i] = VSpr_add(&trailSprite, PAL1, dotTile[0]);

    Trail_clear();
}

void Trail_clear(void)
{
    u16 i;

    for (i = 0; i < ML_TRAIL_F; i++) sprHide(dot[i]);
    head = 0;
    count = 0;
}

static void place(u16 k)
{
    sprPos(dot[k], vtx[k].x + TRAIL_SPR_OFF, vtx[k].y + TRAIL_SPR_OFF);
}

// Envejece un frame: caduca lo de 13 f o más (TrailRenderer.time) y pasa al
// punto fino lo que cumple TRAIL_BIG_AGE.
static void age(void)
{
    u16 i, k;

    now++;

    // El más viejo es vtx[head - (count - 1)]. Como mucho nace uno por
    // frame, así que el anillo de 13 no se desborda.
    while (count > 0)
    {
        k = head + ML_TRAIL_F - (count - 1);
        if (k >= ML_TRAIL_F) k -= ML_TRAIL_F;
        if ((u16)(now - vtx[k].born) < ML_TRAIL_F) break;
        sprHide(dot[k]);
        count--;
    }

    for (i = 0, k = head; i < count; i++)
    {
        if ((u16)(now - vtx[k].born) == TRAIL_BIG_AGE)
        {
            // Fila 1 de trail.png: el punto de 1x1 de la mitad vieja.
            sprAnim(dot[k], 1);
            sprTile(dot[k], dotTile[1]);
        }
        k = (k == 0) ? (ML_TRAIL_F - 1) : (k - 1);
    }

    if ((g_shakeX != placedSX) || (g_shakeY != placedSY))
    {
        placedSX = g_shakeX;
        placedSY = g_shakeY;
        for (i = 0, k = head; i < count; i++)
        {
            place(k);
            k = (k == 0) ? (ML_TRAIL_F - 1) : (k - 1);
        }
    }
}

void Trail_update(s16 x, s16 y)
{
    age();

    // Vértice nuevo si se ha movido más de 2 px desde el último (o si no
    // queda ninguno). Con el wrap el salto es de toda la pantalla: Unity
    // tiraría una línea de un borde al otro; aquí sólo sale el punto nuevo.
    if (count > 0)
    {
        const s16 dx = x - vtx[head].x;
        const s16 dy = y - vtx[head].y;

        if (((s32)dx * dx + (s32)dy * dy) <= (ML_TRAIL_MIN_DIST * ML_TRAIL_MIN_DIST))
            return;

        if (++head >= ML_TRAIL_F) head = 0;
    }

    vtx[head].x = x;
    vtx[head].y = y;
    vtx[head].born = now;
    if (count < ML_TRAIL_F) count++;

    // Fila 0 de trail.png: el punto de 2x2 de la mitad nueva.
    sprAnim(dot[head], 0);
    sprTile(dot[head], dotTile[0]);
    place(head);
}

void Trail_age(void)
{
    age();
}
