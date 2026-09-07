#include <genesis.h>
#include "resources.h"
#include "arena.h"
#include "player.h"

static Player player;
static Sprite *playerSprite;

int main(bool hardReset)
{
    SPR_init();

    PAL_setPalette(PAL1, playerShip.palette->data, DMA);
    playerSprite = SPR_addSprite(&playerShip, 0, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));

    JOY_init();

    Player_init(&player);
    SPR_setPosition(playerSprite, player.x, player.y);

    while (TRUE)
    {
        const u16 state = JOY_readJoypad(JOY_1);

        Player_update(&player, state);

        SPR_setPosition(playerSprite, player.x, player.y);
        SPR_update();

        SYS_doVBlankProcess();
    }

    return 0;
}
