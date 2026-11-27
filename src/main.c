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
    // Botón RESET de la consola (reinicio en caliente): SGDK entra por
    // _reset_entry, que no borra la RAM ni vuelve a copiar .data. Las
    // variables globales y estáticas siguen con lo que tenían (nextTile de
    // src/vramspr.c seguiría contando y VTile_check pararía el juego; saldo,
    // armas, munición, sonido y música no volverían a sus valores) y el juego
    // da por hecho en todas partes que arranca con la RAM recién puesta.
    // SYS_hardReset reinicia por _start_entry, que borra la RAM y carga .data
    // como al encender, y vuelve a llamar a main con hardReset a TRUE. Sin
    // SRAM, RESET es lo mismo que apagar y encender (decisión del usuario del
    // 2026-11-21).
    if (!hardReset) SYS_hardReset();

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
#if (PORT_STRESS || PORT_BENCH || PORT_PROF)
    PAL_setColor(0, ROOM_BG_COLOR);
#endif
    // Sin el arnés, el color 0 se queda en negro hasta el fundido del menú:
    // con la pantalla apagada (Logo_show) el VDP enseña ese color, y así no
    // se ve el fondo mientras se prepara el menú. El fundido lo lleva a
    // ROOM_BG_COLOR (ver más abajo).

    // Todos los sprites comparten la paleta sacada del arte original.
    PAL_setPalette(PAL1, spritePalette, DMA);

    Hole_initGraphics();
    Bg_init();
    Game_init();

#if !(PORT_STRESS || PORT_BENCH || PORT_PROF)
    // Del negro en que acaba el logo al menú, fundiendo en vez de cortar
    // (decisión del usuario del 2026-11-27). La pantalla sigue apagada desde
    // Logo_show: se vacía la cola de DMA para que la CRAM tenga ya todas las
    // paletas del menú, se guardan, se pone todo en negro, se enciende y el
    // fundido corre solo en los VBlank mientras el bucle pinta el menú. El arnés de medida no lo hace, para
    // que sus frames sigan siendo los mismos.
    {
        u16 all[64];

        SYS_doVBlankProcess();
        PAL_getColors(0, all, 64);
        all[0] = ROOM_BG_COLOR;
        PAL_setColors(0, palette_black, 64, CPU);   // apunta a 64 negros
        VDP_setEnable(TRUE);
        PAL_fadeInAll(all, MENU_FADE_IN_FRAMES, TRUE);
    }
#endif

    while (TRUE)
    {
        Game_update();
        // Lo que se ha escrito encima de los agujeros (ver src/hole.c).
        Hole_textUpdate();

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
