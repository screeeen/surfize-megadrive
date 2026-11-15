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

// Círculo de centro (cx, cy) contra la caja [bx, bx + bw) x [by, by + bh),
// todo en píxeles enteros: TRUE si la distancia del centro al píxel más
// cercano de la caja, al cuadrado, es <= r2. 'reach' es la parte entera por
// arriba del radio: si en algún eje la caja queda más lejos, se descarta sin
// multiplicar, que es lo que pasa casi siempre.
//
// Las cajas son semiabiertas, igual que en Coll_overlap: la caja ocupa los
// píxeles bx .. bx + bw - 1. Por eso el borde derecho es bx + bw - 1 y no
// bx + bw: con bx + bw la nave llegaba a 3 px por la izquierda y por arriba
// pero a 4 por la derecha y por abajo (lo midió el verificador).
//
// Es la nave de MonsterLite (una esfera de 3,6 px, ML_PLAYER_R2) contra las
// cajas de los enemigos del GML, que siguen siendo cajas hasta el bloque 8.
static FORCE_INLINE bool Coll_circleBox(s16 cx, s16 cy, s16 r2, s16 reach,
                                        s16 bx, s16 by, s16 bw, s16 bh)
{
    s16 dx = 0, dy = 0;

    if (cx < bx)                    dx = bx - cx;
    else if (cx > (bx + bw - 1))    dx = cx - (bx + bw - 1);
    if (dx >= reach) return FALSE;

    if (cy < by)                    dy = by - cy;
    else if (cy > (by + bh - 1))    dy = cy - (by + bh - 1);
    if (dy >= reach) return FALSE;

    return ((dx * dx) + (dy * dy)) <= r2;
}

#endif
