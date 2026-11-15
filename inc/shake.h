#ifndef _SHAKE_H_
#define _SHAKE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Sacudida de pantalla (stShake)
//
// El original mueve la cámara. scripts/stShake/stShake.gml sólo guarda los
// parámetros en oPlayer, y oPlayer/Step_0 los consume al final de cada frame:
//
//     if (shake) {
//        shake_time -= 1;
//        var _xval = choose(-shake_magnitude, shake_magnitude);
//        var _yval = choose(-shake_magnitude, shake_magnitude);
//        camera_set_view_pos(view_camera[0], _xval, _yval);
//        if (shake_time <= 0) {
//           shake_magnitude -= shake_fade;
//           if (shake_magnitude <= 0) {
//              camera_set_view_pos(view_camera[0], 0, 0);
//              shake = false;
//           }
//        }
//     }
//
// Dos detalles que hay que portar tal cual: el salto es siempre a +-magnitud
// exacta (nunca a un valor intermedio, nunca a cero), y la magnitud no empieza
// a bajar hasta que se agota shake_time, así que la duración real de una
// sacudida es time + magnitud/fade frames, no time.
//
// La sacudida vive en oPlayer, así que sólo existe mientras el jugador existe:
// durante el menú o la muerte el original no la actualiza. Las siete llamadas
// del original están en las macros de abajo.
//
// En Mega Drive no hay cámara: mover la vista es desplazar el plano A (que en
// partida lleva los agujeros, los paquetes y el HUD) con los registros de
// scroll, y restar el mismo desplazamiento a cada sprite. El desplazamiento se
// aplica dentro de sprPos (ver inc/spr.h), así que no hay que tocar
// ninguna llamada: son dos restas por sprite y frame.
//
// Desviaciones (ver docs/PORT_AUDIT_GML.md):
//   - el desplazamiento se calcula al principio del frame y no al final, para
//     que el plano y los sprites usen el mismo valor dentro del mismo frame.
//   - al morir el jugador el original deja la cámara donde estuviera; aquí se
//     recentra, que si no la pantalla se queda torcida al entrar en el menú.
//   - desplazar el plano A lo envuelve por el otro lado en vez de descubrir el
//     exterior del room. En partida el plano está vacío salvo agujeros,
//     paquetes y HUD, así que por el borde asoma el color de fondo, igual que
//     en el original.
// ===========================================================================

// Posición de la cámara en píxeles de pantalla (camera_set_view_pos).
extern s16 g_shakeX;
extern s16 g_shakeY;

// La magnitud es una distancia del room (x0.2) y el desvanecimiento se resta
// una vez por frame (x0.2 de escala y x1.2 de refresco, o sea x0.24).
#define SHAKE_MAG(v)    GML_FPOS(v)
#define SHAKE_FADE(v)   GML_SPEED(v)

// stShake(_time, _magnitude, _fade)
void Shake_start(u16 time, fix16 magnitude, fix16 fade);

// Una vez por frame, mientras el jugador exista.
void Shake_update(void);

// Recentra la vista y corta la sacudida en curso.
void Shake_stop(void);

// ---------------------------------------------------------------------------
// Las llamadas del original, cada una con su línea de GML
// ---------------------------------------------------------------------------

// oPlayer/Step_0: cada disparo con arma
#define Shake_shot()        Shake_start(GML_FRAMES(4),  SHAKE_MAG(5),   SHAKE_FADE(0.2))

// oPlayer/Step_0: cada disparo de la bala por defecto. La magnitud es 1 del
// room, que a x0.2 son 0,2 px: por debajo del píxel, así que no se ve. Se
// porta igual para no inventar una constante que el original no tiene.
#define Shake_thinShot()    Shake_start(GML_FRAMES(1),  SHAKE_MAG(1),   SHAKE_FADE(0.1))

// oEstallido/Create_0: la metralla
#define Shake_estallido()   Shake_start(GML_FRAMES(5),  SHAKE_MAG(15),  SHAKE_FADE(1.0))

// oTri/Step_0: disparo de la nave réplica con munición
#define Shake_tri()         Shake_start(GML_FRAMES(5),  SHAKE_MAG(5),   SHAKE_FADE(0.2))

// oRay/Create_0: 50 frames a magnitud 20 y fade 4
#define Shake_ray()         Shake_start(GML_FRAMES(50), SHAKE_MAG(20),  SHAKE_FADE(4.0))

// oExplosionCruz/Create_0: la más violenta del juego, 30 px durante 4 s
#define Shake_cruz()        Shake_start(GML_FRAMES(10), SHAKE_MAG(150), SHAKE_FADE(0.6))

// oSplashS/Step_0 llama a stShake(2,2,0.2) justo antes de destruirse, pero
// stShake hace "with (oPlayer)" y durante el splash no hay jugador: en el
// original esa llamada no hace nada. No se porta.

#endif
