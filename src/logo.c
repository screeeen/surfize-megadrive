#include "logo.h"
#include "arena.h"
#include "resources.h"

// logoScene.wait: WaitForSeconds(0.8) -> 40 f de entrada. En MonsterLite el
// alfa sólo llega a 0,8 y se corta de golpe a la partida; desde el 2026-11-27
// (decisión del usuario: "los fundidos... se va al corte") la entrada llega al
// blanco entero, se queda un momento y sale fundiendo a negro. Divergencia en
// docs/PORT_AUDIT_GML.md.
#define LOGO_IN_FRAMES      40
#define LOGO_HOLD_FRAMES    30
#define LOGO_OUT_FRAMES     25

// 108x108 px centrados en (160, 132): 2 u por debajo del centro de la
// cámara, que son 12 px (ver tools/ml_logo.py). La imagen mide 112x112 con
// el dibujo centrado; se pone en la celda más cercana.
#define LOGO_COL        ((160 - 56 + 4) / 8)        // 13
#define LOGO_ROW        ((132 - 56 + 4) / 8)        // 10

// El gris i (0..15) del logo multiplicado por el nivel (0..7). El color 0 es
// el negro del fondo.
static void setLevel(u16 *pal, u16 level)
{
    u16 i;

    for (i = 0; i < 16; i++)
    {
        const u16 c = (u16)(((u32)i * level * 2 + 7) / 15) & 0xE;

        pal[i] = (c << 8) | (c << 4) | c;
    }
}

void Logo_show(void)
{
    u16 pal[16];
    u16 saved[16];
    u16 f;

    // PAL0 es la de la fuente del sistema: se guarda y se repone al acabar.
    PAL_getColors(0, saved, 16);
    PAL_setColors(0, palette_black, 16, CPU);
    VDP_drawImageEx(BG_A, &silogoImage,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    LOGO_COL, LOGO_ROW, FALSE, TRUE);

    // Entrada: de 0 a 7 (los 8 niveles del VDP) en LOGO_IN_FRAMES, llegando
    // al blanco en el último frame.
    for (f = 1; f <= LOGO_IN_FRAMES; f++)
    {
        setLevel(pal, (u16)((7u * f) / LOGO_IN_FRAMES));
        PAL_setColors(0, pal, 16, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    for (f = 0; f < LOGO_HOLD_FRAMES; f++) SYS_doVBlankProcess();

    // Salida: de 7 a 0, acabando en negro antes de borrar el plano.
    for (f = 1; f <= LOGO_OUT_FRAMES; f++)
    {
        setLevel(pal, (u16)(7u - ((7u * f) / LOGO_OUT_FRAMES)));
        PAL_setColors(0, pal, 16, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    // Se apaga la pantalla: con el display apagado el VDP pinta el color 0,
    // que sigue en negro, mientras main prepara el menú. La enciende el
    // fundido de entrada del menú (src/main.c).
    VDP_setEnable(FALSE);
    VDP_clearPlane(BG_A, TRUE);
    PAL_setColors(0, saved, 16, CPU);
}
