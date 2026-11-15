#ifndef _VRAMSPR_H_
#define _VRAMSPR_H_

#include <genesis.h>
#include "spr.h"

// ===========================================================================
// Tiles de sprite precargados en VRAM
//
// Por defecto SGDK reserva VRAM por sprite y le sube los tiles del frame
// actual cada vez que cambia de frame. En este juego casi todo rota: la nave,
// las 8 arañas (8 rotaciones x 8 frames de animación), las cabezas y las
// colas de los 4 gusanos y los 14 proyectiles. Cada cambio de rotación son 4
// o 9 tiles por DMA, y con la pantalla llena eso da unos 16 KB por frame
// cuando el presupuesto de un vblank PAL son ~8 KB: SGDK descarta las
// transferencias que sobran y, como la tabla de sprites se encola la última,
// la que se perdía era precisamente esa. La pantalla se quedaba congelada.
//
// Aquí se sube UNA sola vez a VRAM el juego completo de frames de cada
// definición (577 tiles entre todas) y cada sprite apunta al índice que toca.
// Cambiar de rotación pasa a ser escribir el índice en el atributo: cero DMA
// por frame, y a la cola sólo le queda la tabla de sprites.
//
// A cambio, la VRAM de tiles de sprite pasa de repartirse bajo demanda a
// estar fija; con 577 tiles de 1612 que tiene el tile space entra de sobra.
// ===========================================================================

// ---------------------------------------------------------------------------
// Reparto de los tiles de VRAM
//
// Antes cada módulo tenía su índice base escrito a mano y VSPR_TILE_BASE era
// "TILE_USER_INDEX + 176", o sea la suma de todos ellos copiada a ojo. Eso se
// rompió en cuanto los objetos del suelo dejaron de ir al plano y los iconos
// del menú crecieron de 3x3 a 4x4: el logo acabó encima de los tiles del
// jugador y la nave desapareció de la pantalla. El fallo es mudo, porque el
// sprite sigue en la tabla, sólo que apuntando a tiles de otro.
//
// Ahora los índices se reparten EN EJECUCIÓN y por orden de carga, así que no
// hay ningún número que mantener sincronizado. Quien carga tiles pide los
// suyos con VTile_alloc() y se queda con lo que le den.
// ---------------------------------------------------------------------------

// Reserva 'count' tiles y devuelve el índice del primero.
u16 VTile_alloc(u16 count);

// ---------------------------------------------------------------------------
// VRAM fuera del espacio de tiles de SGDK
//
// SGDK da por espacio de tiles lo que hay por debajo del primer mapa
// (0xC000), pero por encima quedan dos trozos que el port no usa:
//   - el mapa del plano window (0xD000-0xDFFF, 128 tiles): la ventana está
//     apagada (registros 0x11 y 0x12 a cero) y nadie escribe en ella;
//   - la cola tras la tabla de sprites (0xF680-0xFFFF, 76 tiles): 80
//     sprites x 8 bytes desde 0xF400.
// Un tile vale en cualquier dirección de la VRAM (el índice es de 11 bits),
// así que ahí van los tiles del menú, y el fondo deja de compartir sitio con
// ellos: el menú ya no tiene que borrar el campo.
//
// Devuelve 0 si no cabe en ninguno de los dos trozos.
// ---------------------------------------------------------------------------
u16 VTile_allocSpare(u16 count);

// ---------------------------------------------------------------------------
// La región compartida
//
// En la VRAM caben 1425 tiles de usuario y el juego gastaba 1378: quedaban 47.
// El fondo del campo (inc/bg.h) necesita 168 para que la imagen se reconozca,
// así que no entra por las buenas. (Con la nave y las cabezas por VStream el
// total queda en 1376 CON el fondo; la región compartida se mantiene porque
// sigue sin sobrar sitio para tener menú y fondo a la vez.)
//
// La región la usa ahora sólo el fondo. Antes la compartía con los tiles del
// menú y el menú borraba el campo; desde el 2026-11-12 el menú va a la VRAM
// sobrante (VTile_allocSpare) y el fondo se queda puesto todo el tiempo,
// como el original.
//
// VTile_shared() reparte esa región una sola vez y devuelve siempre el mismo
// índice base.
// ---------------------------------------------------------------------------
// 216 desde el 2026-11-12: los 168 de antes más lo que quedó libre al sacar
// de aquí los tiles del menú. Múltiplo de 8: la hoja de bg2md tiene 8 tiles
// de ancho y rescomp cuenta la última fila entera (un diccionario de 212
// ocupa 216). Los tilesets pequeños de los sprites van a la VRAM sobrante
// (ver upload() en src/vramspr.c), así que aquí sobra sitio; Game_init para
// el juego si algo se pasa (ver VTile_check).
// 528 desde el 2026-11-15: arañas, fogonazo de la muerte y los dos últimos
// eslabones de los gusanos pasaron a VStream y dejaron 305 libres (queda 1).
#define VTILE_SHARED_TILES 528

u16 VTile_shared(void);

// Cuántos tiles de usuario se han repartido: para el presupuesto.
u16 VTile_used(void);

// Para el juego con un mensaje si lo repartido se sale del espacio de tiles:
// VTile_alloc no comprueba nada, y pasarse escribe encima del mapa del plano
// B (0xC000) y deja basura por la pantalla. Se llama al acabar de repartir.
void VTile_check(void);

// Sube todos los frames de 'def' a VRAM (saltándose los tilesets repetidos,
// que rescomp comparte entre animaciones) y deja en table[anim*stride+frame]
// el índice VRAM de cada uno.
void VSpr_load(const SpriteDefinition *def, u16 *table, u16 stride);

// Sube un solo frame de 'def' y devuelve su índice VRAM.
u16 VSpr_loadFrame(const SpriteDefinition *def, u16 anim, u16 frame);



// Crea un sprite del pool propio (ver inc/spr.h) apuntando a los tiles que ya
// están en VRAM. Nace aparcado fuera de pantalla.
MDSpr *VSpr_add(const SpriteDefinition *def, u16 pal, u16 tileIndex);

// ---------------------------------------------------------------------------
// Frames en ROM subidos por DMA al cambiar (VStream)
//
// La excepción a la precarga: la nave y las cabezas de gusano. El original
// las gira de forma continua y a 8 direcciones el salto de 45 grados es lo
// que más canta. A 32 direcciones (11,25 grados) sus hojas son 3 x 32 frames
// y precargarlas no cabe (la VRAM está a 1416 de 1425 tiles), pero son como
// mucho 4 sprites en pantalla: cada uno tiene SU hueco de VRAM del tamaño
// del frame más gordo y, cuando cambia de frame, se encola un DMA con los
// tiles de ese frame desde la ROM. Son <=288 bytes por cambio y ~1,2 KB por
// frame en el peor caso (nave y tres cabezas girando a la vez), de los 8 KB
// que SGDK reparte por vblank en PAL; la tabla de sprites son otros 640.
//
// Los tiles del sprite tienen que ir SIN comprimir en resources.res (NONE):
// el DMA copia tal cual lo que hay en la ROM.
//
// El DMA y la tabla de sprites se vuelcan en el mismo vblank, así que el
// cambio de dibujo y el de tamaño/reparto del frame se ven a la vez.
// ---------------------------------------------------------------------------
typedef struct
{
    u16 base;                       // primer tile del hueco en VRAM
    u16 cap;                        // tiles del hueco: el frame más gordo
} VStream;

// Reserva el hueco para 'def' (todos sus frames) y deja el sprite apuntando
// a él. Devuelve el sprite, aparcado.
MDSpr *VStream_add(VStream *st, const SpriteDefinition *def, u16 pal);

// Cambia de frame: encola la subida de tiles si el frame es otro. 'f' es un
// frame de la definición con la que se reservó el hueco.
void VStream_frame(VStream *st, MDSpr *s, const AnimationFrame *f);

// Hace que la próxima VStream_frame vuelva a subir aunque el frame sea el
// mismo (p. ej. tras reutilizar el hueco o al reaparecer).
static FORCE_INLINE void VStream_invalidate(MDSpr *s) { s->frame = NULL; }

#endif
