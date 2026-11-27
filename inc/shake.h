#ifndef _SHAKE_H_
#define _SHAKE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Sacudida de pantalla
//
// Dos modelos, uno de cada fuente:
//
// 1. MonsterLite (bloque 12), S/cameraScript.cs. Dos corrutinas que en cada
//    frame de Update ponen la cámara en un punto al azar y, al acabar, la
//    devuelven a (0, 0):
//
//      shake(t)       (15-45): Random.Range(-4, 4) u en X y (-3, 3) u en Y,
//                     o sea +-80 / +-60 px, durante t frames. Además cambia
//                     el tamaño de la cámara a un entero de 0 a 5 (zoom):
//                     el VDP no escala, y Q20 decide sólo el desplazamiento,
//                     al tamaño original.
//      shakeSmall(t)  (47-76): Random.Range(-0,2, 0,2) u = +-4 px en los dos
//                     ejes, durante t frames.
//
//    El bucle es "while (t > 0) { ...; t -= 1; yield }": t = 0,5 y 0,3 dan
//    una vuelta, 4 dan cuatro. Son frames de Update (60 Hz): ÷1.2.
//
// 2. El GML (scripts/stShake), para lo que sólo existe en el GML: la metralla
//    de oEstallido y el disparo del PIVOT y del PARA. oPlayer/Step_0:
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
//    El salto es siempre a +-magnitud exacta y la magnitud no empieza a bajar
//    hasta que se agota shake_time.
//
// Si coinciden, manda la grande de MonsterLite, luego la pequeña y luego la
// del GML, salvo que la del GML sea más fuerte que la pequeña (la de la
// bomba): entonces manda ella. Varias corrutinas de MonsterLite a la vez se quedan con el tiempo
// mayor (en Unity corren en paralelo y el punto lo pone la última de cada
// frame; aquí la de más alcance).
//
// En Mega Drive no hay cámara: mover la vista es desplazar los planos con los
// registros de scroll, y restar el mismo desplazamiento a cada sprite. El
// desplazamiento se aplica dentro de sprPos (ver inc/spr.h), así que no hay
// que tocar ninguna llamada: son dos restas por sprite y frame.
//
// Desviaciones (ver docs/PORT_AUDIT_GML.md):
//   - el desplazamiento se calcula al principio del frame y no al final, para
//     que el plano y los sprites usen el mismo valor dentro del mismo frame.
//   - la sacudida sólo corre mientras corre la partida (no con el tiempo
//     parado ni en el hitstop): mover el plano con los sprites quietos los
//     descuadraría. Las de MonsterLite duran 1-8 frames.
//   - el HUD y el texto van en el plano A y se sacuden con él; en MonsterLite
//     son OnGUI y no se mueven.
//   - desplazar los planos los envuelve por el otro lado en vez de descubrir
//     el exterior. El fondo (plano B) rellena las columnas que sobran con el
//     otro lado del campo (src/bg.c, drawRow): en horizontal envuelve exacto
//     hasta ±96 px; en vertical sólo sobran 2 filas y por encima del campo
//     asoman 16 px desplazados. El plano A (agujeros, HUD) no tiene nada en
//     las columnas de sobra: por ahí se ve el fondo.
// ===========================================================================

// Posición de la cámara en píxeles de pantalla (camera_set_view_pos).
extern s16 g_shakeX;
extern s16 g_shakeY;

// --- MonsterLite -----------------------------------------------------------

// shake(t): +-80 / +-60 px durante 'frames' frames.
void Shake_big(u16 frames);
// shakeSmall(t): +-4 px durante 'frames' frames.
void Shake_small(u16 frames);

// Los +-4 u / +-3 u de shake y los +-0,2 u de shakeSmall, a 20 px por unidad.
#define ML_SHAKE_BIG_X      80
#define ML_SHAKE_BIG_Y      60
#define ML_SHAKE_SMALL      4

// Las llamadas, con su t de frames de Update pasado a frames (÷1.2, con el
// mismo redondeo que GML_FRAMES) y su línea.
#define ML_SHAKE_START_F    GML_FRAMES(4)   // shake(4):  menú -> partida, S/gameControl.cs:219
#define ML_SHAKE_DEATH_F    GML_FRAMES(4)   // shake(4):  al acabar la cámara lenta, S/gameControl.cs:714
#define ML_SHAKE_BOMB_F     GML_FRAMES(1)   // shake(1):  bomba, S/playerMovement.cs:206
#define ML_SHAKE_RAYS_F     GML_FRAMES(10)  // shakeSmall(10): rayos, S/playerMovement.cs:217
#define ML_SHAKE_WBLOCK_F   GML_FRAMES(1)   // shakeSmall(1):   S/disparo.cs:61
#define ML_SHAKE_LASER_F    GML_FRAMES(1)   // shakeSmall(0.5): S/disparo.cs:83 (una vuelta)
#define ML_SHAKE_MOIRE_F    GML_FRAMES(1)   // shakeSmall(1):   S/disparo.cs:104
#define ML_SHAKE_TWAY_F     GML_FRAMES(1)   // shakeSmall(0.3): S/disparo.cs:122 (una vuelta)
#define ML_SHAKE_CIRC_F     GML_FRAMES(3)   // shakeSmall(3) por frame de bola que persigue,
                                            // S/circularScr.cs:59

// --- GML (stShake) ---------------------------------------------------------

// La magnitud es una distancia del room (x0.2) y el desvanecimiento se resta
// una vez por frame (x0.2 de escala y x1.2 de refresco, o sea x0.24).
#define SHAKE_MAG(v)    GML_FPOS(v)
#define SHAKE_FADE(v)   GML_SPEED(v)

// stShake(_time, _magnitude, _fade). A diferencia del GML, si hay una en
// marcha más fuerte que la nueva, la nueva no hace nada (src/shake.c).
void Shake_start(u16 time, fix16 magnitude, fix16 fade);

// oPlayer/Step_0: cada disparo con arma. Desde el bloque 12, sólo el PIVOT y
// el PARA, que no están en MonsterLite.
#define Shake_shot()        Shake_start(GML_FRAMES(4),  SHAKE_MAG(5),   SHAKE_FADE(0.2))

// oEstallido/Create_0: la metralla
#define Shake_estallido()   Shake_start(GML_FRAMES(5),  SHAKE_MAG(15),  SHAKE_FADE(1.0))

// oExplosionCruz/Create_0: la más violenta del GML, 30 px durante 8 frames y
// bajando 0,14 px por frame (unos 4 s en total). Desde el 2026-11-22 vuelve
// a ser la de la bomba de MonsterLite, sumada a su shake(1) (decisión del
// usuario: "blancazo en pantalla y super screenshake"; la sacudida es la del
// bloque 8 y el blancazo, el de fffe0af). Desde el 2026-11-26 ninguna más
// floja la corta (ver Shake_start).
// Desde el 2026-11-27 dura la mitad (decisión del usuario): el doble de
// desvanecimiento que oExplosionCruz (1,2 en vez de 0,6), con el mismo pico y
// los mismos 8 frames arriba. Medido por el verificador, de la bomba a la
// cámara quieta: 137 frames, frente a 284 antes.
// Desde el 2026-11-27 (segunda vez, decisión del usuario) vuelve a durar la
// mitad: desvanecimiento 2,4 (cuatro veces el de oExplosionCruz), mismo pico y
// mismos frames arriba. Medido: 70 frames de la bomba a la cámara quieta.
#define Shake_bomb()        Shake_start(GML_FRAMES(10), SHAKE_MAG(150), SHAKE_FADE(2.4))

// --- Las dos ---------------------------------------------------------------

// Una vez por frame, mientras corra la partida.
void Shake_update(void);

// ¿Hay alguna sacudida en marcha?
bool Shake_active(void);

// Recentra la vista y corta todas las sacudidas en curso.
void Shake_stop(void);

#endif
