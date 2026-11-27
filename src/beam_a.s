/*  =========================================================================== */
/*  Las lineas a las naves holo: recorrido y colocacion de los tramos */
/*  */
/*  Lo caliente de las lineas (src/beam.c, Beam_linesFlush). Desde la quinta */
/*  entrega (2026-11-26) las lineas no van en el pool de sprites: tienen las */
/*  entradas 40 a 79 de la tabla del VDP, ya montadas en lineSat, y esto solo */
/*  reescribe las de las lineas que cambian de celda. */
/*  */
/*  u16 Beam_linesScan(LineScan *c) */
/*    Por cada linea, en orden, mientras su primera ranura quepa en las */
/*    entradas libres (c->budget): si la sacudida ha cambiado desde que se */
/*    coloco, mover sus ranuras; si la nave sigue dentro del rectangulo de su */
/*    celda, nada mas (una resta y una comparacion por eje movido). Si no, la */
/*    celda nueva, su rectangulo y, si la celda cambia, los tramos: cada uno */
/*    son dos palabras largas de res/lineseg_tab.h que ya tienen la forma de */
/*    la entrada del VDP y el volteo hecho, mas dos sumas (la holo en */
/*    pantalla con el enlace, y el atributo con la x). Devuelve cuantas */
/*    lineas ha recorrido. */
/*  */
/*  Las disposiciones de LineSt y LineScan estan en src/beam.c (con */
/*  _Static_assert); las de las tablas, en tools/ml_lineseg.py. */
/*  =========================================================================== */

#include "hwlimits.h"

/*  Con PORT_NO_ASM=1 mandan las versiones en C de src/beam.c, que hacen lo */
/*  mismo (out/bench/check_lines.py compara las dos contra el modelo). */
#if !defined(PORT_NO_ASM) || (PORT_NO_ASM == 0)

    .text
    .align  2
    .globl  Beam_linesScan
    .globl  Beam_linesFlush
    .globl  Beam_linesCopy

/*  LineSt, 20 bytes */
    .set    LS_XLO,  0
    .set    LS_XW,   2
    .set    LS_YLO,  4
    .set    LS_YW,   6
    .set    LS_HX,   8
    .set    LS_HY,   10
    .set    LS_KEY,  12
    .set    LS_SLOT, 14
    .set    LS_CNT,  15
    .set    LS_SIZE, 20

/*  LineScan */
    .set    C_ST,    0
    .set    C_N,     4
    .set    C_X0,    6
    .set    C_Y0,    8
    .set    C_MODE,  10
    .set    C_BX,    12
    .set    C_BY,    14
    .set    C_ATTR,  16
    .set    C_BLK,   18
    .set    C_PICK,  20
    .set    C_MIN,   24
    .set    C_BUDGET, 30

/*  LineGlob (src/beam.c) */
    .set    G_N,     0
    .set    G_D,     2
    .set    G_V,     4
    .set    G_K,     6
    .set    G_END,   8
    .set    G_X,     10
    .set    G_Y,     12
    .set    G_SCX,   14
    .set    G_SCY,   16
    .set    G_DONE,  18
    .set    G_GX,    20
    .set    G_GY,    22
    .set    G_FORCE, 24
    .set    G_BUDGET, 28

    .set    QXMAX,   40     /* LINESEG_QX - 1 */
    .set    QYMAX,   30     /* LINESEG_QY - 1 */
    .set    LASTSAT, ((LINE_SAT_SLOTS - 1) * VDP_SPRITE_BYTES)    /* lineSat[39], la ranura 0 */
    .set    LINK0,   (VDP_SAT_ENTRIES - 2)    /* enlace de la ranura 0 (entrada 79 -> 78) */
    .set    VFLIP,   0x1000
    .set    HFLIP,   0x0800

/*  Un recorrido por cada eje que haya que mirar (cx, cy: la nave se ha */
/*  movido en x, en y). Para en la primera linea cuya primera ranura no */
/*  quepa (budget): rehacer una puede correr las de detras, asi que se mira */
/*  linea a linea. */
.macro WALK name, cx, cy
\name:
1:  cmp.b   LS_SLOT(%a5),%d3
    bls     .Lwdone
  .if \cx
    move.w  %d6,%d0
    sub.w   LS_XLO(%a5),%d0
    cmp.w   LS_XW(%a5),%d0
    bhi.s   3f
  .endif
  .if \cy
    move.w  %d7,%d0
    sub.w   LS_YLO(%a5),%d0
    cmp.w   LS_YW(%a5),%d0
    bhi.s   3f
  .endif
    bra.s   4f
3:  bsr     .Lmiss
4:  lea     LS_SIZE(%a5),%a5
    addq.w  #1,%d4
    cmp.w   C_N(%a0),%d4
    bcs.s   1b
    bra     .Lwdone
.endm

/*  u16 Beam_linesScan(LineScan *c): devuelve cuantas lineas ha recorrido */
/*  (las de detras no caben en budget y se quedan como estaban). */
Beam_linesScan:
    move.l  4(%sp),%a0
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l  C_ST(%a0),%a5
    moveq   #0,%d4              /* linea */
    move.w  C_BUDGET(%a0),%d3
    move.w  C_X0(%a0),%d6
    move.w  C_Y0(%a0),%d7
    tst.w   C_N(%a0)
    beq     .Lwdone
    move.w  C_MODE(%a0),%d0
    add.w   %d0,%d0
    add.w   %d0,%d0
    lea     .Lwtab(%pc),%a1
    move.l  0(%a1,%d0.w),%a1
    jmp     (%a1)

    .align  2
.Lwtab:
    .long   .Lwdone, .Lw1, .Lw2, .Lw3

    WALK    .Lw1, 1, 0
    WALK    .Lw2, 0, 1
    WALK    .Lw3, 1, 1

.Lwdone:
    move.w  %d4,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

/*  --------------------------------------------------------------------------- */
/*  La nave ha salido del rectangulo de la linea de a5: celda nueva y, si */
/*  cambia, los tramos. Conserva a0, a5 y d3-d7. */
/*  --------------------------------------------------------------------------- */
.Lmiss:
    movem.l %d3-%d7,-(%sp)
    move.w  %d6,%d0
    sub.w   LS_HX(%a5),%d0      /* dx */
    move.w  %d7,%d1
    sub.w   LS_HY(%a5),%d1      /* dy */
    moveq   #0,%d5              /* bits de volteo del atributo */
    moveq   #8,%d6              /* palabra larga de la x: +8, o +12 volteada */
    moveq   #0,%d7              /* palabra larga de la y: +0, o +4 volteada */

/*  x: celda qx = |dx| >> 3 y su rectangulo (inicio, ancho - 1) */
    move.w  LS_HX(%a5),%d3
    tst.w   %d0
    bmi.s   .Lxm
    move.w  %d0,%d2
    and.w   #0xFFF8,%d2         /* lo */
    lsr.w   #3,%d0              /* qx */
    add.w   %d2,%d3
    move.w  %d3,LS_XLO(%a5)     /* hx + lo */
    moveq   #7,%d3
    cmp.w   #QXMAX,%d0
    bcs.s   .Lxw
    move.w  #0x3FFF,%d3         /* la ultima celda llega hasta el final */
    sub.w   %d2,%d3
    moveq   #QXMAX,%d0
    bra.s   .Lxw
.Lxm:
    neg.w   %d0
    moveq   #12,%d6
    move.w  #HFLIP,%d5
    move.w  %d0,%d2
    and.w   #0xFFF8,%d2
    lsr.w   #3,%d0
    cmp.w   #QXMAX,%d0
    bcc.s   .Lxmc
    sub.w   %d2,%d3
    subq.w  #7,%d3
    move.w  %d3,LS_XLO(%a5)     /* hx - lo - 7 */
    moveq   #7,%d3
    tst.w   %d0
    bne.s   .Lxw
    moveq   #6,%d3              /* dx de -7 a -1: dx = 0 es de la otra */
    bra.s   .Lxw
.Lxmc:
    sub.w   #0x3FFF,%d3
    move.w  %d3,LS_XLO(%a5)
    move.w  #0x3FFF,%d3
    sub.w   %d2,%d3
    moveq   #QXMAX,%d0
.Lxw:
    move.w  %d3,LS_XW(%a5)

/*  y: lo mismo */
    move.w  LS_HY(%a5),%d3
    tst.w   %d1
    bmi.s   .Lym
    move.w  %d1,%d2
    and.w   #0xFFF8,%d2
    lsr.w   #3,%d1
    add.w   %d2,%d3
    move.w  %d3,LS_YLO(%a5)
    moveq   #7,%d3
    cmp.w   #QYMAX,%d1
    bcs.s   .Lyw
    move.w  #0x3FFF,%d3
    sub.w   %d2,%d3
    moveq   #QYMAX,%d1
    bra.s   .Lyw
.Lym:
    neg.w   %d1
    moveq   #4,%d7
    or.w    #VFLIP,%d5
    move.w  %d1,%d2
    and.w   #0xFFF8,%d2
    lsr.w   #3,%d1
    cmp.w   #QYMAX,%d1
    bcc.s   .Lymc
    sub.w   %d2,%d3
    subq.w  #7,%d3
    move.w  %d3,LS_YLO(%a5)
    moveq   #7,%d3
    tst.w   %d1
    bne.s   .Lyw
    moveq   #6,%d3
    bra.s   .Lyw
.Lymc:
    sub.w   #0x3FFF,%d3
    move.w  %d3,LS_YLO(%a5)
    move.w  #0x3FFF,%d3
    sub.w   %d2,%d3
    moveq   #QYMAX,%d1
.Lyw:
    move.w  %d3,LS_YW(%a5)

/*  la celda y la clave: celda | volteo x << 14 | volteo y << 15, que son */
/*  justo los bits de volteo del atributo (0x0800, 0x1000) tres mas arriba */
    add.w   %d1,%d1
    lea     lineSegRow,%a1
    add.w   0(%a1,%d1.w),%d0    /* c */
    move.w  %d5,%d1
    lsl.w   #3,%d1
    or.w    %d0,%d1
    cmp.w   LS_KEY(%a5),%d1
    beq     .Lmdone             /* la misma celda: sólo cambia el rectangulo */
    move.w  %d1,LS_KEY(%a5)

/*  --- rehacer: los tramos de la celda c --- */
    add.w   %d0,%d0             /* c * 2 */
    lea     lineSegIdx,%a1
    moveq   #0,%d2
    move.w  0(%a1,%d0.w),%d2    /* el primero */
    move.w  2(%a1,%d0.w),%d3
    sub.w   %d2,%d3             /* cuantos */
    add.w   %d0,%d0
    add.w   %d0,%d0             /* c * 8 */
    lea     lineSegBox,%a4
    add.w   %d0,%a4             /* la caja de la celda */
    lsl.l   #4,%d2
    lea     lineSegRec,%a3
    add.l   %d2,%a3             /* el primer tramo */
    move.l  C_PICK(%a0),%a1
    move.w  %d3,%d2
    add.w   %d2,%d2
    move.w  0(%a1,%d2.w),%d2    /* el reparto para esa cuenta */
    add.w   %d2,%d2
    lea     lineSegPick,%a2
    add.w   %d2,%a2
    cmp.w   C_BLK(%a0),%d3      /* como mucho, los del bloque */
    bls.s   1f
    move.w  C_BLK(%a0),%d3
1:  moveq   #0,%d0
    move.b  LS_CNT(%a5),%d0
    cmp.w   %d0,%d3
    beq.s   2f
    bsr     .Lresize            /* otro numero de ranuras: correr las de detras */
2:

/*  d4 = (y de la holo en pantalla) << 16 | enlace de su primera ranura */
    move.w  LS_HY(%a5),%d4
    add.w   C_BY(%a0),%d4
    swap    %d4
    moveq   #0,%d0
    move.b  LS_SLOT(%a5),%d0
    moveq   #LINK0,%d1
    sub.w   %d0,%d1
    move.w  %d1,%d4
    cmp.w   C_MIN(%a0),%d0
    bcc.s   3f
    move.w  %d0,C_MIN(%a0)
3:
/*  a1 = lineSat[39 - ranura] */
    lea     lineSat+LASTSAT,%a1
    lsl.w   #3,%d0
    sub.w   %d0,%a1
/*  a6 = (atributo con volteo) << 16 + (x de la holo en pantalla) */
    move.w  C_ATTR(%a0),%d0
    or.w    %d5,%d0
    swap    %d0
    clr.w   %d0
    move.w  LS_HX(%a5),%d1
    add.w   C_BX(%a0),%d1
    ext.l   %d1
    add.l   %d1,%d0
    move.l  %d0,%a6

/*  ¿Cabe la caja entera de la celda (volteada) en el campo? Entonces ningun */
/*  tramo se sale y no hace falta mirarlos uno a uno. */
    move.w  LS_HX(%a5),%d0
    cmp.w   #8,%d6
    bne.s   4f
    move.w  %d0,%d1
    add.w   (%a4),%d1
    bmi     .Lslow
    add.w   2(%a4),%d0
    cmp.w   #(SCREEN_W - 1),%d0
    bgt     .Lslow
    bra.s   5f
4:  move.w  %d0,%d1
    sub.w   2(%a4),%d1
    bmi     .Lslow
    sub.w   (%a4),%d0
    cmp.w   #(SCREEN_W - 1),%d0
    bgt     .Lslow
5:  move.w  LS_HY(%a5),%d0
    tst.w   %d7
    bne.s   6f
    move.w  %d0,%d1
    add.w   4(%a4),%d1
    bmi     .Lslow
    add.w   6(%a4),%d0
    cmp.w   #(SCREEN_H - 1),%d0
    bgt     .Lslow
    bra.s   .Lfast
6:  move.w  %d0,%d1
    sub.w   6(%a4),%d1
    bmi     .Lslow
    sub.w   4(%a4),%d0
    cmp.w   #(SCREEN_H - 1),%d0
    bgt     .Lslow

/*  --- lo normal: dos palabras largas y dos sumas por tramo --- */
.Lfast:
    move.w  %d3,%d2
    subq.w  #1,%d2
    bmi.s   .Lplaced
.Lf:
    move.w  (%a2)+,%d0
    lea     0(%a3,%d0.w),%a4
    move.l  0(%a4,%d7.w),%d1
    add.l   %d4,%d1             /* y | tamano | enlace */
    move.l  0(%a4,%d6.w),%d0
    add.l   %a6,%d0             /* atributo | x */
    move.l  %d1,(%a1)
    move.l  %d0,4(%a1)
    subq.l  #8,%a1
    subq.w  #1,%d4              /* la ranura siguiente enlaza con la de abajo */
    dbf     %d2,.Lf

.Lplaced:
.Lmdone:
    movem.l (%sp)+,%d3-%d7
    rts

/*  --- cerca del borde: cada tramo, dentro del campo como hasta ahora --- */
.Lslow:
    moveq   #0,%d5              /* colocados */
    move.w  %d3,%d2
    subq.w  #1,%d2
    bmi.s   .Lsdone
.Ls:
    move.w  (%a2)+,%d0
    lea     0(%a3,%d0.w),%a4
    move.l  0(%a4,%d7.w),%d1
    add.l   %d4,%d1
    move.l  0(%a4,%d6.w),%d0
    add.l   %a6,%d0
    move.w  %d1,%d3
    lsr.w   #8,%d3              /* el tamano */
    lsl.w   #2,%d3
    lea     lineClip,%a4
    add.w   %d3,%a4
    move.w  %d0,%d3
    sub.w   C_BX(%a0),%d3       /* x en el campo, sin signo */
    cmp.w   (%a4),%d3
    bhi.s   7f
    swap    %d1
    move.w  %d1,%d3
    swap    %d1
    sub.w   C_BY(%a0),%d3       /* y en el campo */
    cmp.w   2(%a4),%d3
    bhi.s   7f
    move.l  %d1,(%a1)
    move.l  %d0,4(%a1)
    subq.l  #8,%a1
    subq.w  #1,%d4
    addq.w  #1,%d5
7:  dbf     %d2,.Ls
.Lsdone:
    moveq   #0,%d0
    move.b  LS_CNT(%a5),%d0
    cmp.w   %d0,%d5
    beq.s   .Lplaced
    move.w  %d5,%d3
    bsr.s   .Lresize            /* se ha quedado con menos */
    bra.s   .Lplaced

/*  Beam_lineResize(a5, d3), en C (src/beam.c). Conserva a0. */
.Lresize:
    move.l  %a0,-(%sp)
    moveq   #0,%d0
    move.w  %d3,%d0
    move.l  %d0,-(%sp)
    move.l  %a5,-(%sp)
    jsr     Beam_lineResize
    addq.l  #8,%sp
    move.l  (%sp)+,%a0
    rts

/*  --------------------------------------------------------------------------- */
/*  void Beam_linesCopy(const VDPSprite *src, VDPSprite *dst, u16 count, */
/*                      s32 dy16, s32 dx) */
/*    Copia count entradas de la imagen de las lineas (sin sacudida) a la */
/*    tabla del VDP sumandoles la sacudida: (dy << 16) a la palabra larga de */
/*    y | tamano | enlace y dx a la de atributo | x. Ninguna de las dos */
/*    palabras pasa de 0 ni de 65535, asi que el acarreo no pisa el campo de */
/*    al lado. De cuatro en cuatro con movem: ~55 ciclos por entrada. */
/*  --------------------------------------------------------------------------- */
Beam_linesCopy:
    movem.l %d2-%d7/%a2-%a4,-(%sp)
    move.l  40(%sp),%a0
    move.l  44(%sp),%a1
    move.w  50(%sp),%d0
    move.l  52(%sp),%a2         /* dy << 16 */
    move.l  56(%sp),%a3         /* dx */
    move.w  %d0,-(%sp)
    lsr.w   #2,%d0
    beq.s   2f
    subq.w  #1,%d0
1:  movem.l (%a0)+,%d1-%d7/%a4
    add.l   %a2,%d1
    add.l   %a3,%d2
    add.l   %a2,%d3
    add.l   %a3,%d4
    add.l   %a2,%d5
    add.l   %a3,%d6
    add.l   %a2,%d7
    adda.l  %a3,%a4
    movem.l %d1-%d7/%a4,(%a1)
    lea     32(%a1),%a1
    dbf     %d0,1b
2:  move.w  (%sp)+,%d0
    and.w   #3,%d0
    beq.s   4f
    subq.w  #1,%d0
3:  move.l  (%a0)+,%d1
    add.l   %a2,%d1
    move.l  %d1,(%a1)+
    move.l  (%a0)+,%d1
    add.l   %a3,%d1
    move.l  %d1,(%a1)+
    dbf     %d0,3b
4:  movem.l (%sp)+,%d2-%d7/%a2-%a4
    rts

/*  =========================================================================== */
/*  u16 Beam_linesFlush(u16 nGame, u16 *satLen) */
/*  */
/*  Las lineas, con lo que el juego deja libre de la tabla del VDP: ha usado */
/*  las entradas 0 a nGame - 1, asi que caben budget = 80 - nGame ranuras (40 */
/*  como mucho). Devuelve cuantas van a la vista, que Spr_flush engancha */
/*  detras de las suyas, y en *satLen cuantas entradas de vdpSpriteCache hay */
/*  que subir. En ensamblador porque corre cada frame: en C eran ~600 ciclos */
/*  aunque no hubiera nada que hacer. */
/*  */
/*    1. El montador acaba de escribir las del juego: en vdpSpriteCache, las */
/*       ranuras de budget en adelante ya no son de las lineas (d <= budget). */
/*    2. Las lineas [0, done) estan hechas con la nave en (scx, scy): si se */
/*       ha movido, se miran por el eje que haya cambiado. Las de detras (no */
/*       cabian, o son nuevas) se miran enteras en cuanto quepan. Con la nave */
/*       quieta y nada nuevo que quepa, no se recorre nada. */
/*    3. k = min(ocupadas, budget); el enlace de la ultima a la vista, a 0 (y */
/*       el de la anterior, si k cambia, a su valor). */
/*    4. La sacudida va en la copia: si ha cambiado, se copian todas. */
/*    5. A vdpSpriteCache, de d a k, sumando la sacudida (Beam_linesCopy). */
/*    6. A la VRAM, en el mismo DMA que la tabla del juego (que empieza en la */
/*       entrada 0): hasta la de la ranura v, si alguna no esta ya. Si no se */
/*       sube nada de las lineas, el DMA del juego pisa las de budget en */
/*       adelante. */
/*  =========================================================================== */
Beam_linesFlush:
    movem.l %d2-%d4/%a2-%a3,-(%sp)
    move.w  26(%sp),%d0         /* nGame */
    move.l  28(%sp),%a1
    move.w  %d0,(%a1)           /* *satLen = nGame */
    lea     lineG,%a2
    moveq   #VDP_SAT_ENTRIES,%d2
    sub.w   %d0,%d2             /* budget */
    cmp.w   #LINE_SAT_SLOTS,%d2
    bls.s   1f
    moveq   #LINE_SAT_SLOTS,%d2
1:  tst.w   G_N(%a2)
    beq     .Fnone
/*  0: lo normal con la nave quieta: nada se ha movido, ni la sacudida, ni */
/*  hay holo nuevas, y caben las mismas que el frame anterior. Entonces todo */
/*  sigue igual (la VRAM ya tiene las k ranuras y el DMA del juego no las */
/*  pisa): ~150 ciclos. */
    cmp.w   G_BUDGET(%a2),%d2
    bne.s   0f
    move.w  G_X(%a2),%d0
    cmp.w   G_SCX(%a2),%d0
    bne.s   0f
    move.w  G_Y(%a2),%d0
    cmp.w   G_SCY(%a2),%d0
    bne.s   0f
    tst.w   G_FORCE(%a2)
    bne.s   0f
    move.w  g_shakeX,%d0
    cmp.w   G_GX(%a2),%d0
    bne.s   0f
    move.w  g_shakeY,%d0
    cmp.w   G_GY(%a2),%d0
    bne.s   0f
    move.w  G_K(%a2),%d0
    cmp.w   G_V(%a2),%d0        /* y la VRAM las tiene (Beam_linesDmaLost) */
    bhi.s   0f
    movem.l (%sp)+,%d2-%d4/%a2-%a3
    rts
0:  move.w  %d2,G_BUDGET(%a2)
/*  1 */
    cmp.w   G_D(%a2),%d2
    bcc.s   2f
    move.w  %d2,G_D(%a2)
/*  2 */
2:  moveq   #0,%d3
    move.w  G_X(%a2),%d0
    cmp.w   G_SCX(%a2),%d0
    beq.s   3f
    moveq   #1,%d3
3:  move.w  G_Y(%a2),%d1
    cmp.w   G_SCY(%a2),%d1
    beq.s   4f
    addq.w  #2,%d3
4:  tst.w   G_FORCE(%a2)
    beq.s   5f
    clr.w   G_FORCE(%a2)
    clr.w   G_DONE(%a2)
5:  move.w  %d0,G_SCX(%a2)
    move.w  %d1,G_SCY(%a2)
    lea     lineCtx,%a3
    move.w  %d0,C_X0(%a3)
    move.w  %d1,C_Y0(%a3)
    move.w  %d2,C_BUDGET(%a3)
    move.w  #-1,C_MIN(%a3)
    tst.w   %d3
    beq.s   6f
    move.w  G_DONE(%a2),%d0
    beq.s   6f
    move.l  #lineSt,C_ST(%a3)
    move.w  %d0,C_N(%a3)
    move.w  %d3,C_MODE(%a3)
    move.l  %a3,-(%sp)
    jsr     Beam_linesScan
    addq.l  #4,%sp
    cmp.w   G_DONE(%a2),%d0
    bcc.s   6f
    move.w  %d0,G_DONE(%a2)
6:  move.w  G_DONE(%a2),%d0
    cmp.w   G_N(%a2),%d0
    bcc.s   7f
    move.w  %d0,%d1             /* lineSt + done * 20 */
    add.w   %d1,%d1
    add.w   %d1,%d1
    add.w   %d0,%d1
    add.w   %d1,%d1
    add.w   %d1,%d1
    lea     lineSt,%a1
    add.w   %d1,%a1
    cmp.b   LS_SLOT(%a1),%d2
    bls.s   7f                  /* la siguiente no cabe */
    move.l  %a1,C_ST(%a3)
    move.w  G_N(%a2),%d1
    sub.w   %d0,%d1
    move.w  %d1,C_N(%a3)
    move.w  #3,C_MODE(%a3)
    move.l  %a3,-(%sp)
    jsr     Beam_linesScan
    addq.l  #4,%sp
    add.w   %d0,G_DONE(%a2)
7:  move.w  C_MIN(%a3),%d0
    cmp.w   G_D(%a2),%d0
    bcc.s   8f
    move.w  %d0,G_D(%a2)
/*  3: d4 = k */
8:  move.w  G_END(%a2),%d4
    cmp.w   %d2,%d4
    bls.s   9f
    move.w  %d2,%d4
9:  lea     lineSat,%a1
    move.w  G_K(%a2),%d0
    cmp.w   %d0,%d4
    beq.s   12f
    tst.w   %d0
    beq.s   10f
    moveq   #LINE_SAT_SLOTS,%d1             /* la ranura K - 1 es lineSat[40 - K] */
    sub.w   %d0,%d1
    lsl.w   #3,%d1
    moveq   #(VDP_SAT_ENTRIES - 1),%d3             /* su enlace: 78 - (K - 1) */
    sub.w   %d0,%d3
    move.b  %d3,3(%a1,%d1.w)
    subq.w  #1,%d0
    cmp.w   G_D(%a2),%d0
    bcc.s   10f
    move.w  %d0,G_D(%a2)
10: move.w  %d4,G_K(%a2)
    move.w  %d4,%d0
    beq.s   12f
    subq.w  #1,%d0
    cmp.w   G_D(%a2),%d0
    bcc.s   12f
    move.w  %d0,G_D(%a2)
12: tst.w   %d4
    beq     .Fzero
    moveq   #LINE_SAT_SLOTS,%d1
    sub.w   %d4,%d1
    lsl.w   #3,%d1
    clr.b   3(%a1,%d1.w)        /* la ultima a la vista acaba la lista */
/*  4 */
    move.w  g_shakeX,%d0
    move.w  g_shakeY,%d1
    cmp.w   G_GX(%a2),%d0
    bne.s   13f
    cmp.w   G_GY(%a2),%d1
    beq.s   14f
13: move.w  %d0,G_GX(%a2)
    move.w  %d1,G_GY(%a2)
    clr.w   G_D(%a2)
/*  5 */
14: move.w  G_D(%a2),%d3
    cmp.w   %d4,%d3
    bcc.s   15f
    move.w  G_GX(%a2),%d0
    ext.l   %d0
    neg.l   %d0
    move.l  %d0,-(%sp)          /* dx */
    move.w  G_GY(%a2),%d0
    swap    %d0
    clr.w   %d0
    neg.l   %d0
    move.l  %d0,-(%sp)          /* dy << 16 */
    moveq   #0,%d0
    move.w  %d4,%d0
    sub.w   %d3,%d0
    move.l  %d0,-(%sp)          /* k - d */
    move.w  %d4,%d0
    lsl.w   #3,%d0
    lea     vdpSpriteCache+(VDP_SAT_ENTRIES * VDP_SPRITE_BYTES),%a1
    sub.w   %d0,%a1
    move.l  %a1,-(%sp)          /* &vdpSpriteCache[80 - k] */
    lea     lineSat+(LINE_SAT_SLOTS * VDP_SPRITE_BYTES),%a1
    sub.w   %d0,%a1
    move.l  %a1,-(%sp)          /* &lineSat[40 - k] */
    jsr     Beam_linesCopy
    lea     20(%sp),%sp
    cmp.w   G_V(%a2),%d3
    bcc.s   16f
    move.w  %d3,G_V(%a2)
16: move.w  %d4,G_D(%a2)
/*  6 */
15: move.w  G_V(%a2),%d0
    cmp.w   %d4,%d0
    bcc.s   .Fclamp
    moveq   #VDP_SAT_ENTRIES,%d1
    sub.w   %d0,%d1
    move.l  28(%sp),%a1
    move.w  %d1,(%a1)           /* *satLen = 80 - v */
    move.w  %d4,G_V(%a2)
    bra.s   .Fret
.Fzero:
.Fclamp:
    cmp.w   G_V(%a2),%d2
    bcc.s   .Fret
    move.w  %d2,G_V(%a2)
.Fret:
    move.w  %d4,%d0
    movem.l (%sp)+,%d2-%d4/%a2-%a3
    rts
.Fnone:
    clr.w   G_K(%a2)
    moveq   #0,%d0
    movem.l (%sp)+,%d2-%d4/%a2-%a3
    rts

#endif
