// Generado por tools/gm2md.py -- no editar a mano.
// Paletas sacadas del arte del proyecto GameMaker.
#ifndef _GM_PALETTES_H_
#define _GM_PALETTES_H_

#include <genesis.h>

// Capa Compatibility_Colour del room _1: (51, 51, 51)
#define ROOM_BG_COLOR 0x0222

static const u16 spritePalette[16] = {
    0x0E0E, 0x0000, 0x0EEE, 0x00EE, 0x0444, 0x0E00, 0x000E, 0x0888, 0x0222, 0x0642, 0x0008, 0x0CCC, 0x0EE0, 0x0880, 0x0224, 0x0022
};

static const u16 holePalette[16] = {
    0x0E0E, 0x0000, 0x0222, 0x0444, 0x0008, 0x0224, 0x0EE0, 0x0880, 0x0EEE, 0x0CCC, 0x0E00, 0x00EE, 0x0888, 0x0666, 0x0000, 0x0000
};

// Desplazamiento del dibujo respecto al origen de la instancia,
// una entrada por animacion de projectiles.png (ver gm2md.py).
#define PROJ_CELL 24
#define PROJ_SPR_FRAMES 50
static const s8 projSprOffX[PROJ_SPR_FRAMES] = {
    0, 0, 0, 0, 0, 0, 0, 0, -11, -8, -1, 7, 11, 8, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 0, -2, -3, -2, 0, 2, 1, 0, 0, -1, -1, 0, 0, 1, 0, 0, -2, -1, 0, 1, 2, 1, 0, -1
};

static const s8 projSprOffY[PROJ_SPR_FRAMES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, -7, -11, -8, -1, 7, 11, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 2, 0, -2, -3, -2, 0, 1, 1, 0, 0, -1, -1, 0, 0, 0, 0, -1, -2, -1, 0, 1, 2, 1
};

// Hojas recortadas a 16x16 (ver trim en gm2md.py): esquina de cada
// frame respecto al centro del objeto, en el orden rotacion *
// columnas + columna. sprPos(x + offX[i], y + offY[i]).
static const s8 spiderOffX[64] = {
    -6, -5, -5, -5, -6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -6, -6, -7, -7, -7, -7, -7, -7, -7, -7, -8, -9, -9, -9, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -8, -8, -8, -7, -7, -6, -6, -6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -5
};
static const s8 spiderOffY[64] = {
    -6, -6, -7, -7, -7, -7, -7, -7, -5, -5, -5, -5, -5, -5, -5, -5, -6, -5, -5, -5, -6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -6, -6, -7, -7, -7, -7, -7, -7, -7, -7, -8, -9, -9, -9, -8, -8, -8, -8, -8, -8, -8, -9, -8, -8, -8, -8, -7, -7, -7, -8, -8, -8, -7, -7
};
static const s8 snakeHead1OffX[32] = {
    -7, -7, -7, -7, -7, -7, -8, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -6, -6, -6, -6, -6, -6, -7, -7, -7, -7, -7, -7, -7
};
static const s8 snakeHead1OffY[32] = {
    -6, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -8, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -6, -6, -6, -6, -6
};
static const s8 snakeCu2OffX[8] = {
    -7, -7, -7, -7, -7, -6, -6, -7
};
static const s8 snakeCu2OffY[8] = {
    -6, -7, -7, -7, -7, -7, -7, -6
};
static const s8 snakeTailOffX[8] = {
    -7, -7, -8, -8, -7, -6, -6, -6
};
static const s8 snakeTailOffY[8] = {
    -6, -6, -7, -7, -8, -8, -7, -6
};

// ground.png: una animacion por objeto del suelo, un frame
// por tamano del latido (ver gm2md.py).
#define GROUND_CELL 24
#define GROUND_FRAMES 3
#define GROUND_ANIM_PACK 0
#define GROUND_ANIM_BOOM 1
#define GROUND_ANIM_REPLICA 2
#define GROUND_ANIM_RAYS 3

#define PROJ_ANIM_THIN 0
#define PROJ_ANIM_WBLOCK 8
#define PROJ_ANIM_THREEW 16
#define PROJ_ANIM_LASER 24
#define PROJ_ANIM_PARA 32
#define PROJ_ANIM_BOMB 40
#define PROJ_ANIM_OPTION 41
#define PROJ_ANIM_TRI 42

// explosion.png: una animacion por fila (ver gm2md.py).
#define EXPL_ANIM_FLASH 0
#define EXPL_FLASH_FRAMES 5
#define EXPL_ANIM_DUST 1
#define EXPL_DUST_SIZES 3
#define EXPL_DUST_FRAMES 5
#define EXPL_CELL 64

// debris.png: animacion 0 gris oscuro, 1 negro (oDestro/Draw_0).
#define DEBRIS_CELL 16
#define DEBRIS_SIZES 4
#define DEBRIS_COLORS 2

// muerte.png: los tres primeros tamanos de oMuerte.
#define MUERTE_CELL 64
#define MUERTE_FRAMES 3

// beam.png: una animacion por rotacion, un frame por color.
#define BEAM_CELL 16
#define BEAM_COLORS 2

// digits.png: una animacion por digito.
#define DIGIT_CELL 8

// hudfont.png: los glifos que usa el HUD, 2x2 tiles cada uno.
#define HUD_FONT_CELL 16
#define HUD_FONT_GLYPHS 25
static const char hudFontChars[] = "BULETSKIA0123456789OPNXre";

// dot.png: una animacion por color (azul, plata, amarillo).
#define DOT_CELL 8
#define DOT_COLORS 3

#endif
