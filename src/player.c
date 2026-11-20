#include "player.h"

void Player_init(Player *p)
{
    // En el centro de la pantalla, como startGame() del GML (decisión del
    // usuario al cerrar el bloque 2): ver PLAYER_SPAWN_* en arena.h.
    Entity_clear(&p->e);
    p->e.x = FIX16(PLAYER_SPAWN_X);
    p->e.y = FIX16(PLAYER_SPAWN_Y);
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
    // shootTime = 4 [prefab player] (S/disparo.cs). El GML esperaba 25 f.
    p->controlTime = ML_SHOT_FIRST_F;
}

void Player_padDelta(u16 pad, fix16 *dx, fix16 *dy)
{
    // oPlayer/Step_0: if global.kleft {x -= acc} ... un eje por tecla, sin
    // normalizar la diagonal. Izquierda y derecha a la vez se anulan.
    *dx = 0;
    *dy = 0;
    if (pad & BUTTON_LEFT)  *dx -= PLAYER_ACC;
    if (pad & BUTTON_RIGHT) *dx += PLAYER_ACC;
    if (pad & BUTTON_UP)    *dy -= PLAYER_ACC;
    if (pad & BUTTON_DOWN)  *dy += PLAYER_ACC;
}

void Player_update(Player *p, u16 pad)
{
    fix16 dx, dy;

    Player_padDelta(pad, &dx, &dy);
    p->e.x += dx;
    p->e.y += dy;

    // stPlayerMovement(): wrap en los cuatro bordes con margen offSet.
    Entity_wrapPlayfield(&p->e, PLAYER_WRAP_OFFSET);
}

void Player_aim(Player *p, fix16 tx, fix16 ty)
{
    p->e.angle = Entity_angleTo(&p->e, tx, ty);
}

u8 Player_facing(const Player *p)
{
    return Entity_frame32(&p->e);
}
