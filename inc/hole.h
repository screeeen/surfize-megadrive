#ifndef _HOLE_H_
#define _HOLE_H_

#include <genesis.h>
#include "arena.h"

// ===========================================================================
// Agujeros de spawn (oAgujerSuper del GML, AgujerosPadre de MonsterLite)
//
// Desde el bloque 2 de MonsterLite son los SEIS agujeros del prefab
// AgujerosPadre, en hexágono, con el hexágono centrado en la pantalla
// (decisión del usuario, Q6): centros en (120, 100), (160, 160), (160, 80),
// (200, 140), (120, 140) y (200, 100). Ver src/hole.c.
//
// Antes eran los cinco oAgujerSuper del room _1 del GML más un sexto
// reconstruido (558, 754) para cerrar el anillo; esa historia está en
// docs/PORT_AUDIT_GML.md ("El sexto agujero").
//
// Los enemigos nacen en un agujero al azar, como en MonsterLite (arañas y
// gusanos). Lo demás sigue siendo del GML hasta el bloque 8 (la IA): el que
// vaga va hacia ellos y, al tocar uno, salta al 0.
//
// El dibujo (sprAgujero, 197x197 a escala 1.3 -> 51 px, decisión 3: arte del
// GML) va al plano A como bloque de 8x8 tiles, no como sprite: seis sprites
// de ese tamaño serían 24 sprites de hardware de los 80 del VDP. Coste: la
// posición se ajusta a la rejilla de tiles; en el hexágono, los cuatro
// agujeros de los lados se pintan 4 px más lejos del centro (ver src/hole.c).
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

// Agujero donde nace una araña: uno al azar, como createSpider() de
// MonsterLite. (En el GML nacían todas en el último.)
u16 Hole_spiderSpawn(void);

// Destino del estado de vagar: instance_find(oAgujerSuper, f - random(f-1))
// con f = N-1, o sea de 1 a N-2 (ver Entity_updateAI).
u16 Hole_wanderTarget(void);

// TRUE si la caja dada solapa la máscara de algún agujero (place_meeting).
bool Hole_touching(s16 x, s16 y, s16 w, s16 h);

// Los píxeles de los agujeros en la celda (col, row) del plano, compuestos
// como se ven (cada uno con su variante). TRUE si algún agujero dibuja en
// ella; si no, buf queda a cero. Para componer encima lo que el menú pinta
// sobre los agujeros (el logo, ver src/menu.c).
bool Hole_cellPixels(s16 col, s16 row, u32 *buf);

// Texto de la fuente del sistema encima de los agujeros sin cortarlos: en
// buf (una fila del plano A ya rellena con Hole_rowTiles) pone las letras de
// 's' desde 'col'; las que caen sobre dibujo de agujero van en un tile
// compuesto (agujero debajo, letra encima). Los espacios dejan lo que hubiera.
void Hole_textRow(const char *s, u16 col, s16 row, u16 *buf, u16 cols);

// Lo mismo, escribiendo la fila entera (40 columnas) en el plano A de una
// vez, y su contrario: la fila con sólo los agujeros.
void Hole_drawTextRow(const char *s, u16 col, s16 row);
void Hole_clearTextRow(s16 row);



#endif
