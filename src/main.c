#include <genesis.h>
#include "resources.h"
#include "arena.h"
#include "player.h"
#include "enemy.h"

// Loosely mirrors gameControl's 0.4s wave tick, but much slower: every spawn
// here is an immediate threat (no waves/levels/kill-scaling yet), so a fast
// tick would overwhelm the player almost immediately.
#define SPAWN_INTERVAL 90

static Player player;
static Sprite *playerSprite;
static s16 spawnTimer;

static void newGame(void)
{
    Enemy_clearAll();
    Player_init(&player);
    SPR_setPosition(playerSprite, player.x, player.y);
    spawnTimer = SPAWN_INTERVAL;
}

int main(bool hardReset)
{
    SPR_init();

    PAL_setPalette(PAL1, playerShip.palette->data, DMA);
    playerSprite = SPR_addSprite(&playerShip, 0, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));

    Enemy_initAll();

    JOY_init();

    newGame();

    while (TRUE)
    {
        const u16 state = JOY_readJoypad(JOY_1);

        Player_update(&player, state);
        Enemy_updateAll(player.x, player.y);

        if (--spawnTimer <= 0)
        {
            Enemy_spawnAtEdge();
            spawnTimer = SPAWN_INTERVAL;
        }

        // Same "touching arana/snake ends the run" rule as
        // playerMovement.OnTriggerEnter -> gameControl.finishGame(). There's
        // no game-over screen yet (see README), so this just restarts, same
        // as ovni-megadrive's maze loop restarting on its win condition.
        if (Enemy_collides(player.x, player.y, PLAYER_W, PLAYER_H))
            newGame();

        SPR_setPosition(playerSprite, player.x, player.y);
        SPR_update();

        SYS_doVBlankProcess();
    }

    return 0;
}
