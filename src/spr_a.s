/*  =========================================================================== */
/*  Montador de la tabla de sprites del VDP */
/*  */
/*  Es el bucle mas repetido del juego: corre para TODAS las ranuras del pool en */
/*  TODOS los frames. Medido con el perfilador (PORT_PROF=2) en el caso peor: */
/*  14,7 % del frame, el segundo gasto despues del gusano. */
/*  */
/*  La version en C que sustituye hacia unos 192 ciclos por sprite visible, y */
/*  casi todo eran accesos sueltos a memoria: cinco lecturas del struct (y, */
/*  parts, size, attr, x) y cuatro escrituras de palabra a la tabla. Aqui, con */
/*  los ocho primeros bytes del MDSpr ordenados EXACTAMENTE como una entrada del */
/*  VDP (ver inc/spr.h), la entrada se copia con dos movem.l y quedan ~140. */
/*  */
/*  El que no se ve se descarta con una sola comparacion: aparcar un sprite es */
/*  ponerle la Y en -128, que es un valor que una Y de pantalla valida (128 a */
/*  368) no puede tomar. Ver el contrato en inc/spr.h. */
/*  */
/*  MDSpr, 18 bytes: */
/*     0  s16 y */
/*     2  u16 sizeLink     tamano en el byte alto, enlace en el bajo */
/*     4  u16 attr */
/*     6  s16 x */
/*     8  u16 parts        0 = un solo sprite de hardware (el caso normal) */
/*    10  ptr frame */
/*    14  ptr def */
/*  */
/*  FrameVDPSprite, 6 bytes: offsetY, offsetYFlip, size, offsetX, offsetXFlip, */
/*  numTile. El array empieza en el byte 10 del AnimationFrame. */
/*  =========================================================================== */

/*  Con PORT_NO_ASM=1 mandan las versiones en C de src/spr.c (Spr_build, */
/*  Spr_bandAdd, Spr_bandOver), que hacen lo mismo: el ensamblador es una */
/*  optimizacion, no la unica implementacion. Las flags del build llegan */
/*  aqui porque SGDK ensambla con -x assembler-with-cpp. */
#include "hwlimits.h"

#if !defined(PORT_NO_ASM) || (PORT_NO_ASM == 0)

    .text
    .align  2
    .globl  Spr_build

/*  u16 Spr_build(void) */
/*    Monta vdpSpriteCache desde sprPool y devuelve cuantas entradas escribio. */
/*    No pone el final de lista: eso lo hace el C, que ademas decide que hacer */
/*    cuando no hay ninguna. */

Spr_build:
    movem.l %d2-%d7/%a2-%a3,-(%sp)

    move.w  sprWalkCount,%d1    /* sprPoolCount, o hasta la cola (spr.c) */
    beq     .Lempty
    subq.w  #1,%d1    /* contador para dbf */
    lea     sprPool,%a0
    lea     vdpSpriteCache,%a1
    moveq   #0,%d0    /* n: entradas escritas */
    moveq   #SPR_PARKED_Y,%d7    /* MDSPR_PARKED */

/*  Las aparcadas son la mayoria (~160 de ~240 en el caso peor) y antes */
/*  costaban 44 ciclos cada una: aqui, comparando contra un registro y sin */
/*  cargar la y, 34; las visibles, sin movem y sin pasar dos veces por el */
/*  salto (2026-11-26, plan de 50 fps, paso 1). */
.Lloop:
    cmp.w   (%a0),%d7    /* aparcado? */
    bne.s   .Lvis
    lea     18(%a0),%a0
    dbf     %d1,.Lloop
    bra     .Ldone

.Lvis:
    tst.w   8(%a0)    /* parts */
    bne.s   .Lmulti

/*  --- caso normal: un sprite de hardware, la entrada ya esta formada --- */
    move.l  (%a0)+,%d2    /* d2 = y|sizeLink */
    addq.w  #1,%d0
    or.w    %d0,%d2    /* enlace a la siguiente */
    move.l  %d2,(%a1)+
    move.l  (%a0)+,(%a1)+    /* attr|x */
    lea     10(%a0),%a0    /* a la siguiente ranura (18 en total) */
    cmp.w   #VDP_SAT_ENTRIES,%d0    /* tope del VDP */
    bcc     .Ldone
    dbf     %d1,.Lloop
    bra     .Ldone

.Lnext:
    lea     18(%a0),%a0
    dbf     %d1,.Lloop
    bra     .Ldone

/*  --- frame partido en varios sprites de hardware (solo la explosion) --- */
.Lmulti:
    move.w  (%a0),%d2    /* y */
    move.w  8(%a0),%d4    /* parts */
    move.l  10(%a0),%a2
    lea     10(%a2),%a2    /* frameVDPSprites */
    move.w  4(%a0),%d5    /* attr, que va creciendo con numTile */
    move.w  6(%a0),%d3    /* x */
/*  d2 ya trae la y */

.Lmloop:
    moveq   #0,%d6
    move.b  (%a2),%d6    /* offsetY */
    add.w   %d2,%d6
    move.w  %d6,(%a1)    /* y + offsetY */

    moveq   #0,%d6
    move.b  2(%a2),%d6    /* size */
    lsl.w   #8,%d6
    addq.w  #1,%d0
    or.w    %d0,%d6
    move.w  %d6,2(%a1)    /* size | enlace */

    move.w  %d5,4(%a1)    /* attr */

    moveq   #0,%d6
    move.b  3(%a2),%d6    /* offsetX */
    add.w   %d3,%d6
    move.w  %d6,6(%a1)    /* x + offsetX */

    moveq   #0,%d6
    move.b  5(%a2),%d6    /* numTile */
    add.w   %d6,%d5    /* el tile avanza con cada subsprite */

    lea     6(%a2),%a2
    lea     8(%a1),%a1
    cmp.w   #VDP_SAT_ENTRIES,%d0
    bcc     .Ldone
    subq.w  #1,%d4
    bne.s   .Lmloop
    bra.s   .Lnext

.Ldone:
    movem.l (%sp)+,%d2-%d7/%a2-%a3
    rts

.Lempty:
    moveq   #0,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a3
    rts

/*  =========================================================================== */
/*  Las franjas llenas (la rotacion de src/spr.c) */
/*  */
/*  Hasta el 2026-11-26 la comprobacion iba en C y entera uno de cada cuatro */
/*  frames: ~35 000 ciclos de golpe con la tabla llena (medido: Spr_flush */
/*  pasaba de ~21 000 a 53 000-67 000 en esos frames), casi un cuarto de */
/*  frame. Ahora cada frame suma un cuarto de las entradas (Spr_bandAdd) y */
/*  al cuarto se mira (Spr_bandOver): lo mismo repartido, y en ensamblador. */
/*  */
/*  sprBand: una palabra larga por franja de 8 lineas, sprites << 16 | */
/*  celdas de ancho, indexada por (y del VDP) >> 3: las lineas 0 a 255 de la */
/*  pantalla son las franjas 16 a 47 (y = 128 a 383). 72 franjas: cualquier */
/*  y del VDP de 0 a 511, mas las 4 que puede tocar hacia abajo. */
/*  =========================================================================== */
    .globl  Spr_bandAdd
    .globl  Spr_bandOver
    .globl  sprBandHit

    .bss
sprBandHit: .skip 2
    .text

/*  bool Spr_bandAdd(const VDPSprite *v, u16 count): TRUE si alguna franja */
/*  ha pasado de 20 sprites al sumar (el contador arranca en 0x8000 - 21 y */
/*  se le pone el signo: un bmi por franja, ~8 ciclos). */
Spr_bandAdd:
    movem.l %d2-%d4/%a2-%a3,-(%sp)
    move.l  24(%sp),%a0
    move.w  30(%sp),%d2
    subq.w  #1,%d2
    bmi.s   9f
    lea     sprBandTab,%a2
    lea     sprBand,%a3
1:  move.w  (%a0),%d0
    and.w   #0x3FF,%d0
    cmp.w   #VDP_SPRITE_Y_RANGE,%d0
    bcc.s   8f                  /* muy abajo: no toca la pantalla */
    moveq   #0,%d1
    move.b  2(%a0),%d1          /* tamano */
    lsl.w   #3,%d1
    move.w  %d0,%d3
    and.w   #7,%d3
    or.w    %d3,%d1
    lsl.w   #3,%d1              /* (tamano * 8 + y & 7) * 8 */
    move.l  0(%a2,%d1.w),%d3    /* 1 << 16 | ancho en celdas */
    move.w  4(%a2,%d1.w),%d4    /* franjas que toca - 1 */
    lsr.w   #3,%d0
    add.w   %d0,%d0
    add.w   %d0,%d0
    lea     0(%a3,%d0.w),%a1
2:  add.l   %d3,(%a1)+
    bmi.s   7f                  /* esa franja pasa de 20 sprites */
3:  dbf     %d4,2b
8:  addq.l  #8,%a0
    dbf     %d2,1b
9:  moveq   #0,%d0
    tst.b   sprBandHit
    beq.s   10f
    moveq   #1,%d0
    clr.b   sprBandHit
10: movem.l (%sp)+,%d2-%d4/%a2-%a3
    rts
7:  st      sprBandHit
    bra.s   3b

/*  bool Spr_bandOver(void): alguna franja de la pantalla con mas de 20 */
/*  sprites o 40 celdas; y deja sprBand a cero para la vuelta siguiente. */
Spr_bandOver:
    move.l  %d2,-(%sp)
    lea     sprBand+64,%a0      /* franja 16 */
    moveq   #31,%d1
    moveq   #0,%d0
1:  move.l  (%a0)+,%d2
    bmi.s   2f                  /* mas de 20 sprites */
    cmp.w   #VDP_LINE_CELLS,%d2
    bhi.s   2f
    dbf     %d1,1b
    bra.s   3f
2:  moveq   #1,%d0
3:  lea     sprBand+(SPR_BANDS * 4),%a0     /* a cero de atras adelante */
    move.l  #((0x8000 - (VDP_LINE_SPRITES + 1)) << 16),%d1
    moveq   #8,%d2
4:  move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    move.l  %d1,-(%a0)
    dbf     %d2,4b
    move.l  (%sp)+,%d2
    rts

#endif
