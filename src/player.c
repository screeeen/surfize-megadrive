#include "player.h"

void Player_init(Player *p)
{
    // startGame(): instance_create(global.game_width*0.5, global.game_height*0.5, oPlayer)
    Entity_clear(&p->e);
    p->e.x = FIX16(PLAYFIELD_W / 2);
    p->e.y = FIX16(PLAYFIELD_H / 2);
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

void Player_update(Player *p, u16 pad)
{
    // oPlayer/Step_0: if global.kleft {x -= acc} ... un eje por tecla.
    if (pad & BUTTON_LEFT)  p->e.x -= PLAYER_ACC;
    if (pad & BUTTON_RIGHT) p->e.x += PLAYER_ACC;
    if (pad & BUTTON_UP)    p->e.y -= PLAYER_ACC;
    if (pad & BUTTON_DOWN)  p->e.y += PLAYER_ACC;

    // stPlayerMovement(): wrap en los cuatro bordes con margen offSet.
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
