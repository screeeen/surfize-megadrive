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
// Desde el bloque 8 de MonsterLite el círculo es la esfera de un enemigo
// (8,4 px la araña, 9,6 px la cabeza y el eslabón) y la caja, lo que le
// golpea: las balas de MonsterLite (BoxCollider de 16 px) y las armas del
// GML con sus máscaras.
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

// Esfera contra esfera (o punto contra esfera): (dx, dy) es la diferencia
// de centros en píxeles enteros, r2 el cuadrado de la SUMA de los radios ya
// redondeado (ML_R2_LT) y reach su parte entera por arriba (ML_REACH). El
// descarte por eje va primero: casi nunca hay que multiplicar, y cuando hay
// que hacerlo los dos factores caben en 16 bits (muls.w).
static FORCE_INLINE bool Coll_circleCircle(s16 dx, s16 dy, s16 r2, s16 reach)
{
    if ((u16)(dx + reach - 1) >= (u16)((2 * reach) - 1)) return FALSE;
    if ((u16)(dy + reach - 1) >= (u16)((2 * reach) - 1)) return FALSE;

    return ((dx * dx) + (dy * dy)) <= r2;
}

// Rayo de MonsterLite (Physics.Raycast de S/RaycastScr.cs) contra un
// círculo: el rayo sale del origen con dirección (c, s), en fix16 (64 =
// 1,0), y el círculo tiene su centro en (dx, dy) píxeles RESPECTO DEL ORIGEN
// y radio r64 en 1/64 de píxel. Devuelve la distancia a lo largo del rayo en
// 1/64 de píxel si el rayo lo corta por delante, o -1 si no.
//
// Como Unity, no cuenta un collider que ya envuelve al origen (un rayo que
// sale de dentro de una esfera no la ve). Ordenar por la proyección del
// centro y no por el punto de entrada es una aproximación: con radios de 4 a
// 18 px sólo cambia el orden de dos objetos casi solapados en la línea.
static FORCE_INLINE s32 Coll_rayCircle(s16 dx, s16 dy, fix16 c, fix16 s, s16 r64)
{
    s32 along, perp;

    if ((u32)(((s32)dx * dx) + ((s32)dy * dy)) < (u32)(((s32)r64 * r64) >> 12)) return -1;

    along = ((s32)dx * c) + ((s32)dy * s);
    if (along <= 0) return -1;

    perp = ((s32)dy * c) - ((s32)dx * s);
    if (perp < 0) perp = -perp;
    if (perp >= r64) return -1;

    return along;
}

#endif
