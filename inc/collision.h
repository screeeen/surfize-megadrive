#ifndef _COLLISION_H_
#define _COLLISION_H_

#include <genesis.h>

// ===========================================================================
// Colisiones
//
// GameMaker resuelve las colisiones por máscara de sprite y dispara eventos
// (Collision_oX, place_meeting). El VDP no detecta colisiones entre sprites,
// así que el port compara cajas (AABB) entre pools, en enteros.
//
// Las cajas salen de los tamaños de sprite del original escalados x0.2 (ver
// docs/PORT_AUDIT_GML.md), así que las distancias de contacto son las del
// original salvo por el redondeo a píxel entero del hardware.
// ===========================================================================

// Inline a propósito: con ocho argumentos, una llamada de verdad cuesta más
// en empujar la pila que la propia comparación, y el barrido de proyectiles
// la ejecuta ~170 veces por frame (medido: 79 % de un frame PAL antes de
// inlinearla y de cachear las cajas).
static FORCE_INLINE bool Coll_overlap(s16 ax, s16 ay, s16 aw, s16 ah,
                                      s16 bx, s16 by, s16 bw, s16 bh)
{
    return (ax < (bx + bw)) && ((ax + aw) > bx) &&
           (ay < (by + bh)) && ((ay + ah) > by);
}

#endif
