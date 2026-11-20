#include "logo.h"
#include "gfxdebug.h"
#include <genesis.h>
#include "resources.h"
#include "arena.h"
#include "gm_art.h"
#include "hole.h"
#include "bg.h"
#include "audio.h"
#include "game.h"
#include "fastmath.h"
#include "bench.h"

#ifndef PORT_DEBUG_BG
#define PORT_DEBUG_BG 0
#endif
#include "spr.h"

// ===========================================================================
// Arranque
//
// El original corre en una pantalla fija de 1600x1200 a 60 fps. El destino
// es PAL 320x240 a 50 Hz, que conserva su 4:3 exacto (ver arena.h y
// docs/PORT_AUDIT_GML.md).
//
// VDP_setScreenHeight240() sólo funciona en consola PAL. En NTSC el VDP se
// queda en 224 líneas: la lógica mantiene el campo de 240 px (las distancias
// y las posiciones de los agujeros siguen siendo las del original) pero la
// franja inferior de 16 px no se ve, y el juego corre un 20% más rápido
// porque las constantes están calculadas para 50 Hz.
// ===========================================================================

static void videoInit(void)
{
    if (IS_PAL_SYSTEM) VDP_setScreenHeight240();
    else               VDP_setScreenHeight224();

    VDP_setScreenWidth320();
}

int main(bool hardReset)
{
    videoInit();

    // El motor de sprites de SGDK no se usa: los tiles se precargan en VRAM
    // (inc/vramspr.h) y la tabla de sprites del VDP la monta inc/spr.h, que no
    // tiene el tope de 80 objetos ni los ~400 ciclos por sprite de SPR_update.
    Spr_init();
    // Tabla de senos por grado (ver inc/fastmath.h).
    Fast_init();
    Audio_init();
    JOY_init();

#if !(PORT_STRESS || PORT_BENCH || PORT_PROF)
    // logo.unity de MonsterLite, antes de repartir la VRAM (ver inc/logo.h).
    // El arnés de medida se lo salta para que sus frames sigan siendo los
    // mismos.
    Logo_show();
#endif

    // La semilla por defecto es fija: sin ella cada partida repetiría la
    // misma secuencia de agujeros y velocidades de gusano.
#if (PORT_STRESS || PORT_BENCH || PORT_PROF)
    // Con el arnés de medición la semilla es fija: si no, cada arranque da
    // una partida distinta y dos medidas no se pueden comparar.
    setRandomSeed(0x1234);
#else
    setRandomSeed(GET_VCOUNTER ^ 0x1234);
#endif

    // Capa Compatibility_Colour del room _1: el fondo es un color plano.
    // La textura background38 que lleva encima es gris al 6% de opacidad
    // sobre ese color, diferencia que no llega a la resolución de color del
    // VDP (3 bits por componente), así que no se porta: ver el informe.
    // Con PORT_DEBUG_BG el fondo arranca en el normal y se va cambiando con
    // el botón C durante la partida (ver Game_update). No entra en la ROM que
    // se distribuye.
    PAL_setColor(0, ROOM_BG_COLOR);

    // Todos los sprites comparten la paleta sacada del arte original.
    PAL_setPalette(PAL1, spritePalette, DMA);

    Hole_initGraphics();
    Bg_init();
    Game_init();

    while (TRUE)
    {
        Game_update();

        PROF_START(PROF_SPRUPDATE);
        Spr_flush();
        PROF_STOP(PROF_SPRUPDATE);

        // Depurador de gráficos (sólo con PORT_DEBUG_GFX, ver inc/gfxdebug.h).
        GfxDebug_frame();

        PROF_START(PROF_VBLANK);
        SYS_doVBlankProcess();
        PROF_STOP(PROF_VBLANK);
        PROF_CPU();
    }

    return 0;
}
