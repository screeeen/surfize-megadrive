#include "player.h"

void Player_init(Player *p)
{
    // startGame() de MonsterLite: Instantiate(player, highPos, ...), que con
    // el hexágono de agujeros centrado (Q6) cae en (160, 17): ver
    // ML_PLAYER_SPAWN_* en arena.h.
    Entity_clear(&p->e);
    p->e.x = FIX16(ML_PLAYER_SPAWN_X);
    p->e.y = FIX16(ML_PLAYER_SPAWN_Y);
    // La nave empieza mirando ARRIBA. Es una desviación del GML a propósito:
    // allí oPlayer/Create_0 no toca direction, así que se queda en el 0 por
    // defecto de GameMaker, que es mirando a la derecha.
    //
    // Los ángulos de este port van como los de SGDK, con la Y hacia abajo:
    // 0 = derecha, 90 = abajo. Arriba es 270.
    p->e.angle = FIX16(270);
    p->e.active = TRUE;
    p->e.visible = TRUE;
    p->facing = 0;
    p->controlTime = SHOOT_INT_FIRST;
}

void Player_padDelta(u16 pad, fix16 *dx, fix16 *dy)
{
    // playerMovement.move: cada flecha suma speed a su eje. Izquierda y
    // derecha a la vez se anulan, igual que allí.
    s16 sx = 0, sy = 0;
    fix16 v;

    if (pad & BUTTON_LEFT)  sx--;
    if (pad & BUTTON_RIGHT) sx++;
    if (pad & BUTTON_UP)    sy--;
    if (pad & BUTTON_DOWN)  sy++;

    // Q7: la misma velocidad en las ocho direcciones.
    v = (sx && sy) ? ML_PLAYER_SPEED_DIAG : ML_PLAYER_SPEED;

    *dx = (sx < 0) ? -v : ((sx > 0) ? v : 0);
    *dy = (sy < 0) ? -v : ((sy > 0) ? v : 0);
}

void Player_update(Player *p, u16 pad)
{
    fix16 dx, dy;

    Player_padDelta(pad, &dx, &dy);
    p->e.x += dx;
    p->e.y += dy;

    // playerMovement.move, BOUNDARIES: wrap en los bordes exactos.
    Entity_wrapPlayfield(&p->e, PLAYER_WRAP_OFFSET);
}

void Player_aim(Player *p, fix16 tx, fix16 ty)
{
    p->e.angle = Entity_angleTo(&p->e, tx, ty);
}

bool Player_tickFire(Player *p)
{
    // oPlayer/Step_0: "controlTime -= 1; if controlTime < 1 { ... }"
    p->controlTime--;

    if (p->controlTime >= 1) return FALSE;

    return TRUE;
}

u8 Player_facing(const Player *p)
{
    return Entity_frame32(&p->e);
}
