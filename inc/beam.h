#ifndef _BEAM_H_
#define _BEAM_H_

#include <genesis.h>
#include "arena.h"
#include "hole.h"

// ===========================================================================
// Las dos cadenas de sprites del original
//
// 1. EL HAZ DEL ARMA PARA (object0 + object1)
//
// oPlayer/Step_0 case 4 crea object0, cuyo Step llama a script0(1):
//
//     if global.playerBulletsCounter > 0 and instance_exists(object1) = 0
//      { b = instance_create(x, y, object1); b.master = id }
//     if instance_exists(oPlayer) { x = oPlayer.x; y = oPlayer.y }
//
// y object1/Draw_0 encadena 14 copias de sprite0 a lo largo de
// oPlayer.image_angle + 90 (que es su direction, porque el Draw de la nave
// hace image_angle = direction - 90), separadas por la altura del sprite,
// cambiando de color cada frame entre azul, rojo y gris y con la escala
// latiendo entre 0,5 y 1.
//
// object1/Collision_oMalo: "with other if hp != undefined hp -= 1;
// instance_destroy()". No desciende de oBulletS, así que no hay manejador del
// enemigo que gane: el haz resta 1 y SE BORRA, y script0 lo vuelve a crear al
// frame siguiente mientras quede munición. O sea: 1 de daño por frame de
// contacto, y el haz parpadea al tocar.
//
// 2. LA LÍNEA A LAS RÉPLICAS
//
// oPlayer/Draw_0: draw_line_color(x, y, oTri.x, oTri.y, c_blue, c_blue) por
// cada réplica viva. Es sólo dibujo, sin colisión.
//
// El VDP no dibuja líneas ni rota ni mezcla alfa: las dos cadenas se hacen
// con sprites. Desviaciones (ver docs/PORT_AUDIT_GML.md): el haz no late de
// escala ni se desvanece al quedarse sin munición (aparece y desaparece), y
// la línea azul son puntos espaciados, no un trazo continuo.
// ===========================================================================

#define BEAM_LINE_DOTS  6       // puntos por línea a cada réplica
#define BEAM_RAY_LINES  HOLE_COUNT   // una descarga del rayo a cada agujero
#define BEAM_RAY_DOTS   6

void Beam_init(void);

// Los puntos de líneas y rayos: se reparten los últimos (ver src/beam.c).
void Beam_initDots(void);

// Una vez por frame, dentro de la partida.
void Beam_update(u16 weaponIndex, u16 ammo, fix16 px, fix16 py, fix16 angle);

void Beam_clear(void);

#endif
