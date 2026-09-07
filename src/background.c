#include "background.h"
#include "resources.h"

#define BG_TILE_VARIANTS 6
#define BG_PLANE_W 40 // 320 / 8
#define BG_PLANE_H 28 // 224 / 8

void Background_load(void)
{
    u16 x, y;

    VDP_loadTileSet(&bgTiles, TILE_USER_INDEX, DMA);

    for (y = 0; y < BG_PLANE_H; y++)
    {
        for (x = 0; x < BG_PLANE_W; x++)
        {
            const u16 variant = random() % BG_TILE_VARIANTS;

            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_USER_INDEX + variant), x, y);
        }
    }
}
