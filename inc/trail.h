#ifndef _TRAIL_H_
#define _TRAIL_H_

#include <genesis.h>

// ===========================================================================
// La estela de la nave (bloque 12)
//
// MonsterLite: TrailRenderer en el hijo "nave" de P/player.prefab. Cada vez
// que la nave se ha movido más de m_MinVertexDistance (0,1 u = 2 px) desde el
// último vértice deja uno nuevo; cada vértice vive m_Time 0,26 s (13 f) y el
// trazo va del más viejo a la nave, de 1,6 px de ancho junto a la nave a nada
// en la cola. Corre con el tiempo de juego: se para con el tiempo parado.
//
// El VDP no dibuja líneas: un sprite de 8x8 por vértice con un punto en el
// centro, de 2x2 en la mitad nueva y de 1x1 en la vieja (tools/ml_trail.py).
// A la velocidad de la nave (1,44 px/f por eje) sale un vértice cada 1-2
// frames, puntos a 2-3 px: casi un trazo. Como mucho ML_TRAIL_F sprites, de
// los últimos de la tabla (ver Trail_init). trail.png son 2 tiles; el mismo
// punto de 2x2, en azul, es el de las líneas a las holo (tools/ml_lineseg.py).
// ===========================================================================

// Sube los dos puntos de trail.png (2 tiles) y reparte los sprites. Va justo antes de los
// puntos de las líneas y los rayos (Beam_initDots), que tienen que ser lo
// último del pool (son su cola, inc/spr.h): por debajo de casi todo y de lo
// primero que cae si una línea de pantalla se llena.
void Trail_init(void);

// Sin estela (nueva partida, muerte).
void Trail_clear(void);

// Un frame de juego con la nave en (x, y), en píxeles.
void Trail_update(s16 x, s16 y);

// Un frame de juego sin nave que deje vértices (la cámara lenta de la
// muerte): sólo envejece y redibuja.
void Trail_age(void);

#endif
