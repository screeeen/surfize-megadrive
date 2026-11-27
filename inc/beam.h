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
// 2. LA LÍNEA A LAS NAVES HOLO
//
// gizmosProxy/LineScr de MonsterLite: una línea azul (de azul a blanco) del
// jugador a cada nave holo activa. Es sólo dibujo, sin colisión. En el GML
// era la línea de oPlayer/Draw_0 a cada réplica.
//
// El VDP no dibuja líneas ni rota ni mezcla alfa: las dos cadenas se hacen
// con sprites. Desviaciones (ver docs/PORT_AUDIT_GML.md): el haz no late de
// escala ni se desvanece al quedarse sin munición (aparece y desaparece), y
// la línea azul son puntos espaciados, no un trazo continuo.
// ===========================================================================

// 3. LOS RAYOS DE MONSTERLITE (bloque 7)
//
// RayoScr: un LineRenderer de 9 vértices de la nave al objetivo, los siete
// intermedios con ruido de +-8 px y color al azar cada frame. Aquí un punto
// en los vértices 2, 4 y 6.
//
// Con las 10 holo de MonsterLite y un rayo por enemigo (hasta 11), los puntos
// no caben si se ponen tantos como antes (6 por línea): 60 + 66 de 80 sprites
// de hardware. Los rayos se dejan en 3 por rayo (33). Ver el presupuesto en
// docs/PORT_AUDIT_GML.md, bloque 7.
#define BEAM_RAY_DOTS   3       // puntos por rayo

// Las líneas a las holo (decisión del usuario del 2026-11-21): el grosor de
// la estela y puntos muy juntos. Tramos de varios puntos de 2x2 a ~3 px, ya
// dibujados en 7 direcciones (tools/ml_lineseg.py), un sprite por tramo: una
// línea de 150 px son de 5 (horizontal) a 10 sprites. BEAM_LINE_SPRITES
// tramos entre todas las líneas, a partes iguales, y nunca más de los que
// deje libres el juego en la tabla del VDP (ver Beam_linesFlush).
//
// Hasta la cuarta entrega había además un tope de 6 sprites de línea por
// franja de 8 líneas de pantalla, para que el montador no rotara la tabla
// por su culpa. Desde la quinta (2026-11-26) las líneas van siempre detrás
// de todo el juego en la lista del VDP y no cuentan para la rotación, así
// que si una línea de pantalla se llena, lo que se pierde es de ellas: el
// tope ya no protegía nada (opción que dio el usuario).
#define BEAM_LINE_SPRITES LINE_SAT_SLOTS    // tramos a la vez, entre todas las líneas (inc/hwlimits.h)

void Beam_init(void);

// Sólo los rayos de MonsterLite, que siguen con el tiempo parado.
void Beam_updateRays(fix16 px, fix16 py);

// Los puntos de líneas y rayos: se reparten los últimos (ver src/beam.c).
void Beam_initDots(void);

// Una vez por frame, dentro de la partida.
void Beam_update(u16 weaponIndex, u16 ammo, fix16 px, fix16 py, fix16 angle);

void Beam_clear(void);

// Las líneas a las holo, montadas en su trozo de la tabla del VDP (las
// entradas 40 a 79 de vdpSpriteCache). Lo llama Spr_flush después de montar
// las nGame entradas del juego; devuelve cuántas de las líneas van a la
// vista (enganchadas detrás de las del juego, desde la 79 hacia abajo) y en
// *satLen cuántas entradas de vdpSpriteCache hay que subir por DMA.
u16 Beam_linesFlush(u16 nGame, u16 *satLen);

// La tabla de este frame no ha llegado a la cola de DMA (llena).
void Beam_linesDmaLost(void);

// Las entradas de las líneas que fueron a la vista en el último Spr_flush
// (para el depurador de gráficos): count entradas seguidas.
const VDPSprite *Beam_lineEntries(u16 *count);

#endif
