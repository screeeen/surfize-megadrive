#ifndef _SPR_H_
#define _SPR_H_

#include <genesis.h>
#include "shake.h"
#include "hwlimits.h"

// ===========================================================================
// Motor de sprites propio
//
// Sustituye al de SGDK entero. Razones, las dos medidas:
//
// 1. COSTE. SPR_update son ~400 ciclos por sprite: recorre una lista enlazada,
//    mira banderas de animación, temporizadores, reserva de VRAM, subida de
//    tiles, cálculo de visibilidad y actualización diferida. De todo eso este
//    juego no usa NADA: los tiles están precargados (ver inc/vramspr.h), no
//    hay animación automática, no hay reserva dinámica y la posición y el
//    frame se escriben a mano. Con 79 sprites eran ~12 % del frame para montar
//    una tabla de 80 entradas de 8 bytes.
//
// 2. TOPE. SGDK limita a 80 objetos Sprite (MAX_SPRITE en su sprite_eng.c) y
//    el port ya estaba en 79. Eso dejaba sin portar la metralla de oEstallido
//    y el haz del arma PARA. El VDP sólo pinta 80 sprites por frame, pero eso
//    es un tope de lo VISIBLE, no de lo que puede existir: aquí hay 128
//    ranuras y el montador se corta en 80 entradas, descartando las últimas.
//    Como el orden de la tabla es el orden de creación, lo que se pierde
//    primero son los efectos, que es lo que se quiere perder.
//
// Lo que queda es: recorrer el array, descartar lo que no se ve y escribir
// cuatro palabras por sprite de hardware. ~30 ciclos.
//
// La cámara de stShake (inc/shake.h) se aplica aquí, en sprPos, para que no
// haya que tocar ninguna llamada del juego.
// ===========================================================================

// 80 es el tope del VDP; estas son ranuras lógicas, de las que se pintan las
// 80 primeras que caigan dentro de la pantalla.
// Eran 256 y se pedían 293: los últimos 37 (los puntos de los rayos y parte
// de los de las líneas a las holo) caían en el sumidero y no se dibujaban.
// En el bloque 12 llegaron a 316 (la estela se lleva 13 y las líneas a las
// holo pasaron de 30 puntos a 40); desde su quinta entrega las líneas ya no
// van en el pool (tienen su trozo de la tabla del VDP, ver src/beam.c) y
// quedan 276. Spr_check para el juego al arrancar si se pasa.
#define MDSPR_POOL      288

// OJO AL ORDEN DE LOS CAMPOS: los ocho primeros bytes son, byte a byte, una
// entrada de la tabla de sprites del VDP (VDPSprite: y, size_link, attribut,
// x). Eso permite que el montador la copie de un tirón con dos movem.l en vez
// de leer cinco campos sueltos y escribir cuatro palabras. src/spr_a.s cuenta
// con esta disposición: si se toca aquí, hay que tocarla allí.
typedef struct
{
    s16 y;                          // 0   pantalla, con el +0x80 del VDP
    u16 sizeLink;                   // 2   tamaño en el byte alto; el enlace lo
                                    //     pone el montador en el byte bajo
    u16 attr;                       // 4   TILE_ATTR_FULL: paleta, prioridad, tile
    s16 x;                          // 6
    // 0 = el frame cabe en UN sprite de hardware alineado con la celda, que
    // es el caso de todo este juego menos la explosión: el montador copia los
    // ocho bytes y no mira el frame. Si no, cuántos sprites tiene.
    u16 parts;                      // 8
    const AnimationFrame *frame;    // 10
    const SpriteDefinition *def;    // 14
} MDSpr;                            // 18

// Fuera de la pantalla por los dos lados y más allá del sprite más grande
// (64 px), para que el descarte lo tire siempre.
#define MDSPR_PARKED    SPR_PARKED_Y

void Spr_init(void);

// Reserva una ranura. Nunca devuelve NULL: si se agota el pool devuelve una
// ranura sumidero que no se dibuja (ver src/spr.c).
MDSpr *Spr_alloc(const SpriteDefinition *def, u16 attr);

// Monta la tabla de sprites del VDP y la encola. Una vez por frame.
void Spr_flush(void);

// Cuántas entradas de hardware se usaron en el último montaje, y cuántas
// ranuras hay repartidas: para el presupuesto.
// Desde la quinta entrega del bloque 12, la cuenta incluye las entradas de
// las líneas a las holo, que no salen del pool (ver Beam_linesFlush);
// Spr_satGame, sólo las del juego (las de vdpSpriteCache).
u16 Spr_satUsed(void);
u16 Spr_satGame(void);
u16 Spr_allocated(void);

// La COLA del pool: un tramo de ranuras, al final de todo, que casi siempre
// está entero aparcado (los 33 puntos de los rayos; hasta la cuarta entrega
// del bloque 12 también los 40 de las líneas a las holo). Recorrer una ranura aparcada cuesta 44 ciclos (ver la auditoría,
// "El montador de sprites en ensamblador"), así que el montador se salta la
// cola más allá de lo que su dueño diga que puede estar a la vista.
// Spr_tailBegin y Spr_tailEnd rodean su reparto; Spr_tailVisible lo llama el
// dueño cada vez que cambia, con cuántas ranuras de la cola hay que recorrer
// desde su principio (0: ninguna; 0xFFFF: todas).
void Spr_tailBegin(void);
void Spr_tailEnd(void);
void Spr_tailVisible(u16 count);
// Cuántos sprites de la cola hay a la vista (un sprite de hardware cada
// uno): no cuentan para la rotación por franjas llenas (Spr_flush).
void Spr_tailEntries(u16 count);

// Para el juego con un mensaje si se han pedido más ranuras de las que hay
// (las que no caben van al sumidero y no se ven). Se llama al acabar de
// repartir, como VTile_check.
void Spr_check(void);

// ---------------------------------------------------------------------------
// Operaciones del bucle de juego, todas en línea
// ---------------------------------------------------------------------------

// Las coordenadas son del campo de juego; lo que va a la tabla es la pantalla,
// y entre las dos está la cámara de stShake. Fuera de una sacudida g_shakeX/Y
// valen 0 y esto son dos restas de más.
static FORCE_INLINE void sprPos(MDSpr *s, s16 x, s16 y)
{
    s->x = x - g_shakeX + 0x80;
    s->y = y - g_shakeY + 0x80;
}

// Monta la tabla de sprites del VDP desde el pool. En ensamblador, porque es
// el bucle más repetido del juego: ver src/spr_a.s. Devuelve cuántas entradas
// de hardware ha escrito.
u16 Spr_build(void);

// g_shakeX/Y son globales, así que el compilador tiene que releerlas en cada
// vuelta de un bucle que escribe a través de un puntero: no puede demostrar
// que no se pisen. En los bucles largos (los 52 eslabones de gusano) eso son
// dos accesos de más por sprite. Quien coloca muchos sprites saca el sesgo
// fuera del bucle con esto y escribe la posición ya resuelta.
static FORCE_INLINE s16 sprBiasX(void) { return 0x80 - g_shakeX; }
static FORCE_INLINE s16 sprBiasY(void) { return 0x80 - g_shakeY; }

static FORCE_INLINE void sprPosBias(MDSpr *s, s16 x, s16 y)
{
    s->x = x;
    s->y = y;
}

// Aparca el sprite. Antes esto era sprPos(s, -32, -32), que con la celda de
// 64 px de la explosión seguía entrando en la tabla.
static FORCE_INLINE void sprHide(MDSpr *s)
{
    s->x = MDSPR_PARKED;
    s->y = MDSPR_PARKED;
}

// CONTRATO: lo que no se vea, se aparca.
//
// El montador no comprueba si el sprite cae dentro de la pantalla: sólo mira
// si está aparcado. Esa comprobación eran cuatro comparaciones por sprite y
// frame, un 5 % del frame con la arena llena, y daba verdadero casi siempre
// porque en este juego todo lo vivo está dentro del campo.
//
// A cambio, los pocos módulos cuyos sprites PUEDEN salirse (los números que
// vuelan al HUD, el haz del PARA y la humareda, que va a la deriva) aparcan
// ellos mismos lo que se sale. Si no, esa ranura se come una de las 80
// entradas del VDP, que es el recurso escaso.
#define MDSPR_IS_PARKED(s)  ((s)->y == MDSPR_PARKED)

// Margen por fuera del campo con el que un módulo decide aparcar: el sprite
// más grande es de 64 px.
#define MDSPR_OFFSCREEN(ix, iy) \
    (((ix) <= -64) || ((ix) >= PLAYFIELD_W) || ((iy) <= -64) || ((iy) >= PLAYFIELD_H))

// Cambia a qué tiles apunta. Es lo que sustituye a la subida por DMA.
static FORCE_INLINE void sprTile(MDSpr *s, u16 tileIndex)
{
    s->attr = (s->attr & ~TILE_INDEX_MASK) | tileIndex;
}

// El pool y su cuenta los lee también src/spr_a.s, así que no son estáticos.
extern MDSpr sprPool[];
extern u16   sprPoolCount;

// Elige frame. Lo único que hace falta es el reparto en sprites de hardware:
// rescomp parte cada frame en los que necesite (la celda de 64x64 de la
// explosión son cuatro de 32x32, que es el máximo del VDP) y de ahí salen los
// desplazamientos y los tamaños.
// El reparto en sprites de hardware se resuelve AQUI, al cambiar de frame
// (unas pocas veces por sprite y partida), y no en el montador, que corre
// para todos los sprites en todos los frames.
static FORCE_INLINE void sprSetFrame(MDSpr *s, const AnimationFrame *f)
{
    const s16 ns = f->numSprite;

    s->frame = f;

    if (ns < 0)
    {
        // Marca de rescomp: un solo sprite alineado, sin desplazamientos.
        s->parts = 0;
        s->sizeLink = ((u16)f->frameVDPSprites[0].size) << 8;
    }
    else
    {
        s->parts = (u16)ns;
    }
}

static FORCE_INLINE void sprAnimFrame(MDSpr *s, u16 anim, u16 frame)
{
    sprSetFrame(s, s->def->animations[anim]->frames[frame]);
}

static FORCE_INLINE void sprAnim(MDSpr *s, u16 anim)
{
    sprSetFrame(s, s->def->animations[anim]->frames[0]);
}

#endif
