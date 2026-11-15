#ifndef _HOLE_H_
#define _HOLE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Agujeros de spawn (oAgujerSuper)
//
// Instancias colocadas a mano en el room _1, no generadas: el GML las busca
// con instance_find() y los enemigos nacen en una de ellas (y vuelven a ellas
// en el estado de vagar, stWalkingA1W). Coordenadas del room escaladas x0.2.
//
// EL SEXTO AGUJERO ES UNA RECONSTRUCCIÓN, no sale del GML. El room que hay en
// surfize_changes trae exactamente CINCO instancias de oAgujerSuper
// (verificado con grep, con el parseo de capas y con instanceCreationOrder),
// pero puestos como anillo alrededor del centro del room dejan un hueco que
// canta: cuatro separaciones de 55-61 grados y una de 129.
//
//      24.8 ->  83.7 :  58.8 grados
//      83.7 -> 212.5 : 128.8 grados   <-- aqui
//     212.5 -> 273.5 :  61.0 grados
//     273.5 -> 328.6 :  55.1 grados
//     328.6 ->  24.8 :  56.3 grados
//
// El que falta cae a 148 grados y radio 288, o sea (556, 752), que es
// prácticamente el espejo de (558, 446) respecto del centro: (558, 754). Eso
// es lo que se pone. Si la posición de verdad es otra, se cambia esta línea.
//
// Va el PRIMERO de la tabla a propósito: el único comportamiento determinista
// que depende del orden es dónde nacen las arañas, que el original saca del
// ÚLTIMO agujero, y poniéndolo delante ese sigue siendo el mismo de siempre.
//
// El dibujo (sprAgujero, 197x197 a escala 1.3 -> 51 px) va al plano A como
// bloque de 8x8 tiles, no como sprite: cinco sprites de ese tamaño serían 20
// sprites de hardware de los 80 del VDP. Coste: la posición se ajusta a la
// rejilla de tiles, hasta 4 px respecto al original.
// ===========================================================================
#define HOLE_COUNT 6


// Carga los tilesets. Una sola vez al arrancar.
void Hole_initGraphics(void);

// Pinta los agujeros en el plano A (al arrancar; el menú los respeta y al
// salir los repone fila a fila con Hole_rowTiles).
void Hole_draw(void);

// Deja en buf[0..cols) los tiles del plano A de la fila 'row' tal como los
// pinta Hole_draw (0 donde no hay agujero). Sirve para "borrar" una línea de
// texto sin llevarse los agujeros que pasan por ella.
void Hole_rowTiles(s16 row, u16 *buf, u16 cols);

// oAgujerSuper/Draw_0: el agujero por el que está saliendo un gusano se pinta
// con el reborde rojo (la variable gus). Sólo repinta si cambia.
void Hole_setActive(u16 i, bool active);

// Con TRUE, Hole_setActive sigue llevando el estado pero no repinta: durante
// el menú el texto y los iconos están en el mismo plano que los agujeros, y
// repintar un agujero se los comería. Hole_draw pinta el estado al volver.
void Hole_holdDraw(bool hold);

// Centro del agujero i, en píxeles de pantalla (fix16).
fix16 Hole_x(u16 i);
fix16 Hole_y(u16 i);

// Agujero donde nace un gusano. oCabeza1/Create_0 recorre los agujeros con
// instance_find(oAgujerSuper, p - random(p)) y se queda con el de la última
// vuelta: (N-1) - random(N-1).
u16 Hole_snakeSpawn(void);

// Agujero donde nace una araña. oSpider/Create_0 usa un bucle sin aleatorio
// (instance_find(oAgujerSuper, i) con i terminando en N-1), así que TODAS las
// arañas del original nacen siempre en el ÚLTIMO agujero.
u16 Hole_spiderSpawn(void);

// Destino del estado de vagar: instance_find(oAgujerSuper, f - random(f-1))
// con f = N-1, o sea de 1 a N-2 (ver Entity_updateAI).
u16 Hole_wanderTarget(void);

// TRUE si la caja dada solapa la máscara de algún agujero (place_meeting).
bool Hole_touching(s16 x, s16 y, s16 w, s16 h);



#endif
