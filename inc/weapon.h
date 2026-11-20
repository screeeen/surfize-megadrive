#ifndef _WEAPON_H_
#define _WEAPON_H_

#include <genesis.h>

// ===========================================================================
// Las armas del port (bloque 3 de MonsterLite)
//
// Decisión del usuario al cerrar el bloque 2 (docs/MONSTERLITE_DECISIONES.md):
// las armas que están en los dos juegos (WBLOCK, LASER y TWAY, el 3WAY del
// GML) van en UNA versión, la de MonsterLite, y se consiguen por compra
// (WBLOCK, gratis). CIRCULAR y MOIRE, de MonsterLite, también por compra.
// PIVOT y PARA, que sólo están en el GML, siguen con sus valores y su
// desbloqueo por récord. (BOMBA y RAY no eran armas elegibles en el port: son
// los power-ups.)
//
// El orden es el del selector del menú: los cinco iconos de weapons.png
// (GML) y luego los dos de weapons_ml.png (MonsterLite, Q11).
// ===========================================================================
typedef enum
{
    WPN_WBLOCK = 0,
    WPN_LASER,
    WPN_TWAY,
    WPN_PIVOT,
    WPN_PARA,
    WPN_CIRCULAR,
    WPN_MOIRE,
    WPN_COUNT
} WeaponId;

// Armas de MonsterLite: se consiguen comprándolas en la tienda (bloque 5).
#define WPN_ML_MASK  ((1 << WPN_WBLOCK) | (1 << WPN_LASER) | (1 << WPN_TWAY) | \
                      (1 << WPN_CIRCULAR) | (1 << WPN_MOIRE))

#endif
