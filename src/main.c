#include <genesis.h>
#include "resources.h"
#include "arena.h"
#include "background.h"
#include "player.h"
#include "enemy.h"
#include "snake.h"
#include "bullet.h"

// Loosely mirrors gameControl's 0.4s wave tick, but much slower: every spawn
// here is an immediate threat (no waves/levels/kill-scaling yet), so a fast
// tick would overwhelm the player almost immediately.
#define SPAWN_INTERVAL       60
#define SNAKE_SPAWN_INTERVAL 180

// Stand-in for disparo.shotTimeNormal (set per-instance in the Unity
// Inspector, not in code, so the exact value isn't ported).
#define SHOOT_INTERVAL 14

static Player player;
static Sprite *playerSprite;
static s16 spawnTimer;
static s16 snakeSpawnTimer;
static s16 shootTimer;
static u16 kills;
static u16 lastDrawnKills;

// Same change-guarded redraw as ovni-megadrive/mega-drive-hello-world's
// score line: VDP_drawText is too slow to call unconditionally every frame.
static void drawScore(void)
{
    char buf[16];

    if (kills == lastDrawnKills) return;
    lastDrawnKills = kills;

    sprintf(buf, "SCORE %u", kills);
    VDP_drawText(buf, 1, 1);
}

static s32 distSq(s16 ax, s16 ay, s16 bx, s16 by)
{
    const s32 dx = ax - bx;
    const s32 dy = ay - by;

    return (dx * dx) + (dy * dy);
}

// Combines Enemy_closest() (spiders) and Snake_closest() (snake heads) into
// a single nearest-threat lookup, same target choice as
// enemyController.getClosest() over its one shared enemies list.
static bool closestThreat(s16 x, s16 y, s16 *outX, s16 *outY)
{
    s16 ex, ey, sx, sy;
    const bool haveEnemy = Enemy_closest(x, y, &ex, &ey);
    const bool haveSnake = Snake_closest(x, y, &sx, &sy);

    if (haveEnemy && (!haveSnake || distSq(x, y, ex, ey) <= distSq(x, y, sx, sy)))
    {
        *outX = ex;
        *outY = ey;
        return TRUE;
    }

    if (haveSnake)
    {
        *outX = sx;
        *outY = sy;
        return TRUE;
    }

    return FALSE;
}

static void newGame(void)
{
    Enemy_clearAll();
    Snake_clearAll();
    Bullet_clearAll();
    Player_init(&player);
    SPR_setPosition(playerSprite, player.x, player.y);
    spawnTimer = SPAWN_INTERVAL;
    snakeSpawnTimer = SNAKE_SPAWN_INTERVAL;
    shootTimer = SHOOT_INTERVAL;
    kills = 0;
    lastDrawnKills = 0xFFFF;
}

int main(bool hardReset)
{
    SPR_init();

    PAL_setPalette(PAL1, playerShip.palette->data, DMA);
    playerSprite = SPR_addSprite(&playerShip, 0, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));

    Background_load();

    Enemy_initAll();
    Snake_initAll();
    Bullet_initAll();

    JOY_init();

    newGame();

    while (TRUE)
    {
        const u16 state = JOY_readJoypad(JOY_1);
        s16 targetX, targetY;

        Player_update(&player, state);
        Enemy_updateAll(player.x, player.y);
        Snake_updateAll(player.x, player.y);
        Bullet_updateAll();

        if (--spawnTimer <= 0)
        {
            Enemy_spawnAtEdge();
            spawnTimer = SPAWN_INTERVAL;
        }

        if (--snakeSpawnTimer <= 0)
        {
            Snake_spawnAtEdge();
            snakeSpawnTimer = SNAKE_SPAWN_INTERVAL;
        }

        // Auto-aim + auto-fire at the closest enemy: no fire button, same as
        // playerMovement.rotateShipDirection() + disparo.dispara() in the
        // original (the ship always shoots at whatever's nearest once loaded).
        if (closestThreat(player.x, player.y, &targetX, &targetY))
        {
            Player_faceTarget(&player, targetX, targetY);

            if (--shootTimer <= 0)
            {
                Bullet_fire(player.x + (PLAYER_W / 2), player.y + (PLAYER_H / 2), targetX, targetY);
                shootTimer = SHOOT_INTERVAL;
            }
        }

        // Bullet.OnTriggerEnter's "arana"/"snake" cases: kill on hit, add to
        // score (globales.kills += 1 * globales.level -- no levels yet, so
        // just +1 per kill).
        kills += Bullet_checkHits();

        // Same "touching arana/snake/snakeBody ends the run" rule as
        // playerMovement.OnTriggerEnter -> gameControl.finishGame(). There's
        // no game-over screen yet (see README), so this just restarts, same
        // as ovni-megadrive's maze loop restarting on its win condition.
        if (Enemy_collides(player.x, player.y, PLAYER_W, PLAYER_H) ||
            Snake_collides(player.x, player.y, PLAYER_W, PLAYER_H))
            newGame();

        SPR_setPosition(playerSprite, player.x, player.y);
        SPR_setFrame(playerSprite, player.facing);
        drawScore();
        VDP_showFPS(FALSE, 33, 0);
        SPR_update();

        SYS_doVBlankProcess();
    }

    return 0;
}
