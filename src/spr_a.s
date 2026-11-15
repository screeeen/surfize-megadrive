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

/*  Con PORT_SPR_C=1 manda la implementacion de referencia en C que hay en */
/*  src/spr.c, para poder comparar las dos tablas. Las flags del build llegan */
/*  aqui porque SGDK ensambla con -x assembler-with-cpp. */
#if !defined(PORT_SPR_C) || (PORT_SPR_C == 0)

    .text
    .align  2
    .globl  Spr_build

/*  u16 Spr_build(void) */
/*    Monta vdpSpriteCache desde sprPool y devuelve cuantas entradas escribio. */
/*    No pone el final de lista: eso lo hace el C, que ademas decide que hacer */
/*    cuando no hay ninguna. */

Spr_build:
    movem.l %d2-%d7/%a2-%a3,-(%sp)

    move.w  sprPoolCount,%d1
    beq     .Lempty
    subq.w  #1,%d1    /* contador para dbf */
    lea     sprPool,%a0
    lea     vdpSpriteCache,%a1
    moveq   #0,%d0    /* n: entradas escritas */

.Lloop:
    move.w  (%a0),%d2    /* y */
    cmp.w   #-128,%d2    /* aparcado? */
    beq.s   .Lnext
    tst.w   8(%a0)    /* parts */
    bne.s   .Lmulti

/*  --- caso normal: un sprite de hardware, la entrada ya esta formada --- */
    movem.l (%a0),%d2-%d3    /* d2 = y|sizeLink, d3 = attr|x */
    addq.w  #1,%d0
    or.w    %d0,%d2    /* enlace a la siguiente */
    movem.l %d2-%d3,(%a1)
    lea     8(%a1),%a1
    cmp.w   #80,%d0    /* tope del VDP */
    bcc.s   .Ldone

.Lnext:
    lea     18(%a0),%a0
    dbf     %d1,.Lloop
    bra.s   .Ldone

/*  --- frame partido en varios sprites de hardware (solo la explosion) --- */
.Lmulti:
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
    cmp.w   #80,%d0
    bcc.s   .Ldone
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

#endif
