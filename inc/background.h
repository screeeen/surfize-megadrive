#ifndef _BACKGROUND_H_
#define _BACKGROUND_H_

#include <genesis.h>

// Loose stand-in for BackgroundScript.cs's tiled backgroundFlow.png: that
// asset is a 256x256, 8-frame animated photographic noise texture, far
// beyond this hardware's tile/VRAM budget, so this is a handful of static
// dither tiles (same technique as ovni-megadrive's maze wall variants) --
// a starfield-like sparse sparkle instead of a faithful conversion.
//
// Deliberately reuses playerShip's exact two colors (navy + cyan) and is
// drawn with PAL1: the hardware has only 4 sprite/BG palette lines and
// they're already fully spoken for (see enemy.h/snake.h for the same trick
// applied to spider/snake reusing PAL2). Call after the PAL_setPalette(PAL1,
// playerShip...) call in main() so the palette is already loaded.
void Background_load(void);

#endif
