#ifndef _HWLIMITS_H_
#define _HWLIMITS_H_

// ===========================================================================
// Los topes del hardware que el port tiene que respetar, en un solo sitio.
//
// Sólo #define: lo incluyen también los .s (SGDK ensambla con
// -x assembler-with-cpp). En otra plataforma, éste es el fichero que cambia.
// ===========================================================================

#define SCREEN_W            320     // PAL H40 V30
#define SCREEN_H            240
#define VDP_SAT_ENTRIES     80      // entradas de la tabla de sprites (H40)
#define VDP_LINE_SPRITES    20      // sprites por línea de pantalla (H40)
#define VDP_LINE_CELLS      40      // celdas de 8 px por línea (320 px, H40)
#define VDP_SPRITE_BYTES    8       // una entrada de la tabla
#define VDP_SPRITE_Y_RANGE  512     // la Y del VDP de un sprite (9 bits útiles)

// Las franjas de 8 líneas que cuenta la rotación (src/spr.c, sprBand): una
// por cada 8 de VDP_SPRITE_Y_RANGE más las 8 que puede tocar un sprite de 32.
#define SPR_BANDS           ((VDP_SPRITE_Y_RANGE / 8) + 8)

// El reparto del port dentro de la tabla: las entradas de la 80 - 40 a la 79
// pueden ser de las líneas a las holo (src/beam.c), detrás del juego.
#define LINE_SAT_SLOTS      40      // = BEAM_LINE_SPRITES
#define LINE_SAT_FIRST_ENTRY (VDP_SAT_ENTRIES - LINE_SAT_SLOTS)   // 40

// Coordenada de un sprite aparcado (inc/spr.h, MDSPR_PARKED).
#define SPR_PARKED_Y        (-128)

#endif
