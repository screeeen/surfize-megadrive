#include "snake.h"
#include "resources.h"
#include "arena.h"

#define SNAKE_SPEED 1

// How many head positions separate one segment from the next in the
// trail history -- stands in for headScr's segmentSpacing.
#define TRAIL_GAP    5
#define HISTORY_LEN  (SNAKE_SEGMENTS * TRAIL_GAP + 1)

typedef struct
{
    bool alive;
    s16  x;
    s16  y;
    u8   hp;
    s16  historyX[HISTORY_LEN];
    s16  historyY[HISTORY_LEN];
    u16  historyPos;
    Sprite *headSprite;
    Sprite *segSprite[SNAKE_SEGMENTS];
} SnakeSlot;

static SnakeSlot snakes[MAX_SNAKES];

static bool overlap(s16 ax, s16 ay, s16 aw, s16 ah, s16 bx, s16 by, s16 bw, s16 bh)
{
    return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static void hideAll(SnakeSlot *s)
{
    u16 i;

    SPR_setPosition(s->headSprite, -SNAKE_W, -SNAKE_H);
    for (i = 0; i < SNAKE_SEGMENTS; i++)
        SPR_setPosition(s->segSprite[i], -SNAKE_W, -SNAKE_H);
}

void Snake_initAll(void)
{
    u16 i, j;

    // No PAL_setPalette here on purpose: snakeHead/snakeSeg are drawn in
    // spider's exact two colors, so they render correctly on the PAL2 that
    // Enemy_initAll() already loaded (see snake.h).
    for (i = 0; i < MAX_SNAKES; i++)
    {
        snakes[i].alive = FALSE;
        snakes[i].headSprite = SPR_addSprite(&snakeHead, -SNAKE_W, -SNAKE_H, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        for (j = 0; j < SNAKE_SEGMENTS; j++)
            snakes[i].segSprite[j] = SPR_addSprite(&snakeSeg, -SNAKE_W, -SNAKE_H, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    }
}

void Snake_spawnAtEdge(void)
{
    u16 i, j;
    s16 x, y;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (snakes[i].alive) continue;

        switch (random() & 3)
        {
            case 0: x = 0;                    y = random() % (ARENA_H - SNAKE_H); break;
            case 1: x = ARENA_W - SNAKE_W;     y = random() % (ARENA_H - SNAKE_H); break;
            case 2: x = random() % (ARENA_W - SNAKE_W); y = 0;                    break;
            default: x = random() % (ARENA_W - SNAKE_W); y = ARENA_H - SNAKE_H;   break;
        }

        snakes[i].alive = TRUE;
        snakes[i].x = x;
        snakes[i].y = y;
        snakes[i].hp = SNAKE_HEAD_HP;
        snakes[i].historyPos = 0;
        for (j = 0; j < HISTORY_LEN; j++)
        {
            snakes[i].historyX[j] = x;
            snakes[i].historyY[j] = y;
        }
        return;
    }
}

static void stepToward(s16 *pos, s16 target)
{
    if (*pos < target) *pos += SNAKE_SPEED;
    else if (*pos > target) *pos -= SNAKE_SPEED;
}

static void segmentPos(SnakeSlot *s, u16 segIndex, s16 *outX, s16 *outY)
{
    const u16 delay = (segIndex + 1) * TRAIL_GAP;
    const u16 idx = (s->historyPos + HISTORY_LEN - delay) % HISTORY_LEN;

    *outX = s->historyX[idx];
    *outY = s->historyY[idx];
}

void Snake_updateAll(s16 targetX, s16 targetY)
{
    u16 i, j;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (!snakes[i].alive) continue;

        stepToward(&snakes[i].x, targetX);
        stepToward(&snakes[i].y, targetY);

        snakes[i].historyPos = (snakes[i].historyPos + 1) % HISTORY_LEN;
        snakes[i].historyX[snakes[i].historyPos] = snakes[i].x;
        snakes[i].historyY[snakes[i].historyPos] = snakes[i].y;

        SPR_setPosition(snakes[i].headSprite, snakes[i].x, snakes[i].y);

        for (j = 0; j < SNAKE_SEGMENTS; j++)
        {
            s16 sx, sy;

            segmentPos(&snakes[i], j, &sx, &sy);
            SPR_setPosition(snakes[i].segSprite[j], sx, sy);
        }
    }
}

bool Snake_closest(s16 x, s16 y, s16 *outX, s16 *outY)
{
    u16 i;
    bool found = FALSE;
    s32 bestDist = 0;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (!snakes[i].alive) continue;

        const s32 dx = snakes[i].x - x;
        const s32 dy = snakes[i].y - y;
        const s32 dist = (dx * dx) + (dy * dy);

        if (!found || (dist < bestDist))
        {
            found = TRUE;
            bestDist = dist;
            *outX = snakes[i].x;
            *outY = snakes[i].y;
        }
    }

    return found;
}

bool Snake_hit(s16 x, s16 y, s16 w, s16 h, bool *killed)
{
    u16 i;

    *killed = FALSE;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (!snakes[i].alive) continue;
        if (!overlap(x, y, w, h, snakes[i].x, snakes[i].y, SNAKE_W, SNAKE_H)) continue;

        snakes[i].hp--;
        if (snakes[i].hp == 0)
        {
            snakes[i].alive = FALSE;
            hideAll(&snakes[i]);
            *killed = TRUE;
        }

        return TRUE;
    }

    return FALSE;
}

bool Snake_collides(s16 px, s16 py, s16 pw, s16 ph)
{
    u16 i, j;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        if (!snakes[i].alive) continue;

        if (overlap(px, py, pw, ph, snakes[i].x, snakes[i].y, SNAKE_W, SNAKE_H))
            return TRUE;

        for (j = 0; j < SNAKE_SEGMENTS; j++)
        {
            s16 sx, sy;

            segmentPos(&snakes[i], j, &sx, &sy);
            if (overlap(px, py, pw, ph, sx, sy, SNAKE_W, SNAKE_H))
                return TRUE;
        }
    }

    return FALSE;
}

void Snake_clearAll(void)
{
    u16 i;

    for (i = 0; i < MAX_SNAKES; i++)
    {
        snakes[i].alive = FALSE;
        hideAll(&snakes[i]);
    }
}
