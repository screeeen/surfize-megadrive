#include "logo.h"
#include "arena.h"
#include "resources.h"

// logoScene.wait: WaitForSeconds(0.8) -> 40 f.
#define LOGO_FRAMES     40

// 108x108 px centrados en (160, 132): 2 u por debajo del centro de la
// cámara, que son 12 px (ver tools/ml_logo.py). La imagen mide 112x112 con
// el dibujo centrado; se pone en la celda más cercana.
#define LOGO_COL        ((160 - 56 + 4) / 8)        // 13
#define LOGO_ROW        ((132 - 56 + 4) / 8)        // 10

void Logo_show(void)
{
    u16 pal[16];
    u16 saved[16];
    u16 f, i;

    // PAL0 es la de la fuente del sistema: se guarda y se repone al acabar.
    PAL_getColors(0, saved, 16);
    PAL_setColors(0, palette_black, 16, CPU);
    VDP_drawImageEx(BG_A, &silogoImage,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    LOGO_COL, LOGO_ROW, FALSE, TRUE);

    for (f = 0; f < LOGO_FRAMES; f++)
    {
        // fadeTime = f / 50 s: el gris i (de 0 a 15) por el alfa, en los
        // 8 niveles del VDP. El color 0 es el negro del fondo.
        for (i = 0; i < 16; i++)
        {
            const u16 v = (u16)(((u32)i * 7 * f) / (15 * 50));    // 0..7
            const u16 c = v << 1;

            pal[i] = (c << 8) | (c << 4) | c;
        }
        PAL_setColors(0, pal, 16, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    // logoScene: Application.LoadLevel de golpe, sin fundido de salida.
    VDP_clearPlane(BG_A, TRUE);
    PAL_setColors(0, saved, 16, CPU);
}
