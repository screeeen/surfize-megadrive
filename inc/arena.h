#ifndef _ARENA_H_
#define _ARENA_H_

#include <genesis.h>

// ===========================================================================
// Capas de conversión MonsterLite / GameMaker -> Mega Drive
//
// Fuente de verdad: MonsterLite (Unity, C#). El GML de surfize_changes
// (GameMaker Studio 2) es la segunda referencia: sólo para lo que MonsterLite
// no define. Ver docs/MONSTERLITE_GAP.md y docs/PORT_AUDIT_GML.md.
//
// Objetivo: PAL 320x240 a 50 Hz, una sola pantalla con wrap toroidal.
//
// Valores de MonsterLite (cámara ortográfica de 6 en apaisado: 16x12 u):
//   1 unidad de mundo      = 20 px
//   ticks de FixedUpdate   x 1          (50 Hz, igual que PAL)
//   frames de Update/OnGUI / 1.2        (60 Hz)
//   segundos               x 50
//   unidades/s             x 0.4        -> px/frame
//
// Valores del GML (room _1 de 1600x1200 a 60 fps), sólo donde MonsterLite no
// llega. Son las macros GML_* de abajo:
//   posiciones y tamaños   x 0.2        (320/1600 == 240/1200)
//   velocidades px/frame   x 0.24       (0.2 de escala x 60/50 de refresco)
//   giro en grados/frame   x 1.2        (los grados no escalan con la
//                                        resolución, sólo con el refresco)
//   contadores de frames   / 1.2
//
// Las constantes de juego que hay hoy en este fichero vienen todas del GML;
// las de MonsterLite irán entrando por bloques (docs/MONSTERLITE_GAP.md §5).
//
// Todas las macros se evalúan en tiempo de compilación: el flotante lo
// resuelve el compilador, en ejecución sólo hay enteros y fix16.
// ===========================================================================

#define GML_ROOM_W      1600
#define GML_ROOM_H      1200
#define GML_HZ          60.0
#define TARGET_HZ       50.0
#define GML_SCALE       0.2
#define GML_RATE        (GML_HZ / TARGET_HZ)    // 1.2

// fix16 con redondeo al 1/64 más cercano: el FIX16() de SGDK trunca, y
// truncar velocidades pequeñas desvía más de lo que admite la fidelidad
// (p.ej. 1.872 -> 119/64 = 1.859 truncado, frente a 120/64 = 1.875).
#define FIX16R(v)       ((fix16)(((v) * (1 << FIX16_FRAC_BITS)) + (((v) < 0) ? -0.5 : 0.5)))

// Posiciones, tamaños y contadores de frames se calculan con aritmética
// ENTERA, redondeando al más cercano: así son expresiones constantes enteras
// y pueden dimensionar arrays (el historial del gusano, por ejemplo).
//   x0.2 redondeado  = (v +- 2) / 5
//   /1.2 redondeado  = (v * 5 + 3) / 6
#define GML_POS(v)      ((s16)(((v) + (((v) < 0) ? -2 : 2)) / 5))
#define GML_LEN(v)      GML_POS(v)
#define GML_FRAMES(v)   ((u16)((((v) * 5) + 3) / 6))

// Las velocidades sí usan flotante, que el compilador resuelve a un fix16.
#define GML_FPOS(v)     FIX16R((v) * GML_SCALE)
#define GML_SPEED(v)    FIX16R((v) * GML_SCALE * GML_RATE)
#define GML_TURN(v)     FIX16R((v) * GML_RATE)

// ---------------------------------------------------------------------------
// Campo de juego
//
// Geometría fija a 320x240 en las dos regiones para que las distancias sean
// las del original. En consola NTSC el VDP sólo muestra 224 líneas
// (VDP_setScreenHeight240 "only work on PAL system"), así que la franja
// inferior de 16 px existe en lógica pero no se ve: limitación documentada
// de la decisión PAL, no un cambio de jugabilidad.
// ---------------------------------------------------------------------------
#define PLAYFIELD_W     GML_POS(GML_ROOM_W)     // 320
#define PLAYFIELD_H     GML_POS(GML_ROOM_H)     // 240

// ---------------------------------------------------------------------------
// Constantes de jugabilidad, todas leídas del GML
// ---------------------------------------------------------------------------

// oPlayer/Create_0 + Step_0. La velocidad (acc = 6 -> 1,44 px/f) y el margen
// del wrap (offSet = 15 -> 3 px) del GML ya no se usan: son los de
// MonsterLite, ver "Jugador de MonsterLite" más abajo.
#define SHOOT_INT_DEFAULT   GML_FRAMES(12)      // controlTime 12 -> 10 f
#define SHOOT_INT_WEAPON    GML_FRAMES(6)       // controlTime 6  -> 5 f
#define SHOOT_INT_3WAY      GML_FRAMES(12)      // 3WAY           -> 10 f

// Balas: oBulletThin/oWBlock/o3way vspeed -32, oBulletPara -64, oLaser 40
#define BULLET_SPEED_THIN   GML_SPEED(32)       // -> 7.69 px/f
#define BULLET_SPEED_PARA   GML_SPEED(64)       // -> 15.36 px/f
#define LASER_SPEED         GML_SPEED(40)       // -> 9.59 px/f
#define LASER_SPACING       GML_LEN(32)         // i*32           -> 6 px
#define LASER_LIFE          GML_FRAMES(8)       // alarm = 8      -> 7 f
// largo = 10 en el original: diez segmentos encadenados cada 32 px del room
// forman un haz de 60 px en pantalla.
//
// Estuvo recortado a 3 por un cálculo mío equivocado: creí que competían por
// los 80 objetos Sprite del VDP, pero los segmentos salen del pool de 14
// proyectiles, que ya estaba reservado. Con cadencia 5 y vida 7 llegan a
// haber ~14 segmentos en vuelo, justo el tamaño del pool, así que en fuego
// sostenido se puede perder alguno (igual que el 3WAY).
#define LASER_COUNT         10
#define WEAPON_SPAWN_DIST   GML_LEN(100)        // bomba/rayo     -> 20 px
#define THREEWAY_SPREAD     FIX16R(30.0)        // direction +-30 grados

// Máquina de estados de enemigo (stSteadyW / stAppearA1W / stWalkingA1W)
#define ENEMY_APPEAR_DELAY  GML_FRAMES(90)      // alarm[0] = 90  -> 75 f
#define ST_STEADY_TIME      GML_FRAMES(6)       // -> 5 f
#define ST_ATTACK_TIME      GML_FRAMES(300)     // -> 250 f
#define ST_WANDER_TIME      GML_FRAMES(600)     // -> 500 f
#define WANDER_TURN         GML_TURN(2)         // 2 grados/f     -> 2.4

// OJO (bloque 1 de MonsterLite): las velocidades de avance de araña y gusano
// de abajo (SPIDER_SPEED, SNAKE1_SPEED_*, SNAKE2_SPEED) ya NO se usan: la
// velocidad sale de setSpeed por nivel (Entity_mlSpeed, ML_*_MAX_MU). Se
// quedan como referencia del GML. Los giros y el hp siguen siendo del GML
// hasta el bloque 8.
//
// oSpider/Create_0: velo = 3, hp = 0 (las balas restan 4 -> muere de 1 tiro)
#define SPIDER_SPEED        GML_SPEED(3)        // -> 0.72 px/f
#define SPIDER_TURN         GML_TURN(3)         // -> 3.6 grados/f
#define SPIDER_HP           0

// oCabeza1/Create_0: velo = random_range(5, 7.8), hp = 4, bodylength = 14,
// stepdelay = 7. El giro usa el mismo valor que velo (turn_towards_direction
// recibe velo como grados/frame), así que cada instancia gira a su ritmo.
#define SNAKE1_SPEED_MIN    GML_SPEED(5.0)      // -> 1.20 px/f
#define SNAKE1_SPEED_MAX    GML_SPEED(7.8)      // -> 1.875 px/f
#define SNAKE1_TURN_MIN     GML_TURN(5.0)       // -> 6.0 grados/f
#define SNAKE1_TURN_MAX     GML_TURN(7.8)       // -> 9.36 grados/f
#define SNAKE1_HP           4                   // -> 5 impactos
#define SNAKE1_PARTS        13                  // 9 oCorpo + oCu1 + oCu2 + oCabeza2 + cabeza
#define SNAKE1_STEPDELAY    GML_FRAMES(7)       // -> 6 f

// oCabeza2b/Create_0: velo = 6.2, hp = 2, bodylength = 12, stepdelay = 6
#define SNAKE2_SPEED        GML_SPEED(6.2)      // -> 1.49 px/f
#define SNAKE2_TURN         GML_TURN(6.2)       // -> 7.44 grados/f
#define SNAKE2_HP           2                   // -> 3 impactos
#define SNAKE2_PARTS        11                  // 7 oCorpo + oCu1 + oCu2 + oCabeza2 + cabeza
#define SNAKE2_STEPDELAY    GML_FRAMES(6)       // -> 5 f

#define SNAKE_MAX_PARTS     SNAKE1_PARTS

// El historial necesita 13 partes x 6 frames + 1 = 79 posiciones, pero se
// redondea a la potencia de 2 siguiente para indexarlo con una máscara: el
// 68000 no tiene división, y el módulo se evaluaba tres veces por frame para
// cada uno de los 52 eslabones en pantalla.
#define SNAKE_MAX_HISTORY   128
#define SNAKE_HIST_MASK     (SNAKE_MAX_HISTORY - 1)

// scrInit: topes de población del original
#define GML_MAX_SPIDERS     8
#define GML_MAX_CABEZA_1    4
#define GML_MAX_CABEZA_2    2

// Topes efectivos del port. El pool de objetos Sprite de SGDK es de 80
// (MAX_SPRITE en src/sprite_eng.c) y el VDP tampoco muestra más de 80 por
// frame, así que los topes del GML no caben: 4x13 + 2x11 = 74 partes de
// gusano + 8 arañas + jugador + balas = 103 objetos, y SPR_addSprite
// devolvería NULL. Conforme a la decisión tomada se recorta la CANTIDAD de
// gusanos, nunca su longitud:
//
//   jugador 1 + balas 8 + arañas 8 + 4 cabezas 4 + 46 seguidores = 67 de 80
//
// SEGUNDO RECORTE (2026-11-08), ya no por sprites sino por CPU. Con el motor
// de sprites propio (inc/spr.h) el tope de 80 objetos dejó de existir, pero
// medido en el emulador cada gusano cuesta un 20 % del frame: con 3 + 1 el
// caso peor va a 43,1 fps con picos del 117 %, y con 2 + 1 a 47,8 fps con un
// pico del 68 %. Trece eslabones son trece posiciones que leer del historial,
// trece sprites que colocar y trece cajas más en cada consulta de colisión.
//
// Consecuencia en juego: como máximo 2 gusanos de tipo 1 y 1 de tipo 2 a la
// vez, en vez de 4 y 2. Las oleadas saturan antes su techo de población, y la
// pantalla va más despejada que en el original.
//
// TERCERA MEDICIÓN (2026-11-15, bloque 1 de MonsterLite, Q3): MonsterLite
// pide 5 gusanos desde el nivel 10 y 8 desde el 20. Con tools/measure.sh y el
// tope forzado al pool (vueltas de 600 en el caso peor): 3 gusanos (2 + 1)
// 484; 4 (3 + 1) 365; 5 (3 + 2) 348. Además un cuarto gusano no cabe en VRAM
// (1436 de 1425 tiles: harían falta ~12 tiles del fondo). Se queda en 3.
#define MAX_SPIDERS        GML_MAX_SPIDERS     // 8, igual que el original
#define MAX_CABEZA_1        2                   // GML: 4
#define MAX_CABEZA_2        1                   // GML: 2
#define MAX_SNAKES          (MAX_CABEZA_1 + MAX_CABEZA_2)
// SPIDERS_PER_WAVE (3) y MALOS_COUNTER_MAX (4) eran de generaMalos() del GML,
// que ya no se usa: las oleadas son las de MonsterLite (bloque 1, abajo).

// ---------------------------------------------------------------------------
// Estructura de partida de MonsterLite (bloque 1 de docs/MONSTERLITE_GAP.md)
//
// S/ = ~/MonsterLite/MonsterLite_ultimo_commit/Assets/Scripts/
// ---------------------------------------------------------------------------

// startGame (S/gameControl.cs:416, 445): WaitForSeconds(0.8) antes de crear
// la nave, WaitForSeconds(1) antes del primer gusano, y luego el bucle de
// oleadas con WaitForSeconds(0.4). Segundos x 50.
#define ML_START_PLAYER_F   40                  // 0,8 s
#define ML_START_SNAKE_F    (ML_START_PLAYER_F + 50)  // + 1 s
#define ML_WAVE_PERIOD_F    20                  // 0,4 s

// "int index = globales.kills % 40" (S/gameControl.cs:460)
#define ML_WAVE_MOD         40
// Oleadas 27, 31 y 36: "+2 gusanos si (int)globales.level > 5"
#define ML_WAVE_EXTRA_LEVEL 5

// setMaxNumSnakes (S/enemyController.cs:217):
//   maxNumSnakes = (level / 10) * 3 + 2   -> 2 / 5 / 8 / ...
// Recortado al pool de gusanos del port (MAX_SNAKES, 3), que es el tope que
// aguanta la CPU: medido con tools/measure.sh, ver docs/PORT_AUDIT_GML.md,
// sección "MonsterLite", bloque 1. En los niveles 1-9 el de MonsterLite (2)
// cabe tal cual; del 10 en adelante se queda en 3.
#define ML_SNAKES_BASE      2
#define ML_SNAKES_PER_10LV  3

// enemyMov.setSpeed (S/enemyMov.cs:41-53), en milésimas de unidad/s:
//   f = (level % 10) + 0.8;  speed = Random.Range(f, maxSpeed + f / 10)
// maxSpeed [prefab]: araña 1 (aranaPrefab), gusano 4 (culobicho).
// A px/frame: u/s x 20 / 50 = x 0.4 (FixedUpdate a 50 Hz, 1:1).
#define ML_SPEED_F_BASE_MU  800                 // el + 0.8
#define ML_SPIDER_MAX_MU    1000
#define ML_SNAKE_MAX_MU     4000

// ---------------------------------------------------------------------------
// Jugador de MonsterLite (bloque 2 de docs/MONSTERLITE_GAP.md)
// ---------------------------------------------------------------------------

// playerMovement.move (S/playerMovement.cs:35-65), rama de teclas: speed =
// 0.2 u por tick de FixedUpdate [prefab player] en cada eje pulsado, sin
// inercia. 0.2 u x 20 px = 4 px/f, exacto en fix16. El original suma los dos
// ejes sin normalizar (5,66 px/f en diagonal); por decisión del usuario (Q7)
// el port normaliza: en diagonal cada eje va a 4 / sqrt(2) = 2,8284 px/f,
// que en fix16 son 181/64 = 2,8281 (velocidad total 3,9996 px/f, -0,01 %).
// Constantes, sin raíces en ejecución.
#define ML_PLAYER_SPEED       FIX16(4)
#define ML_PLAYER_SPEED_DIAG  FIX16R(4.0 * 0.70710678118654752)

// playerMovement.move, "BOUNDARIES" (S/playerMovement.cs:129-146): wrap
// toroidal en los bordes exactos de la cámara (globales.minX/maxX/minY/maxY,
// la mitad de la vista a cada lado), sin margen.
#define PLAYER_WRAP_OFFSET    0

// SphereCollider del prefab player [prefab]: radio 0.18 u, escala 1 ->
// 3,6 px. Las distancias de colisión son enteras (píxeles), así que la
// condición "d < 3,6" es "d^2 < 12,96", o sea d^2 <= 12 en enteros: exacta,
// sin redondeo (no hay ningún d^2 entero entre 12 y 12,96).
#define ML_PLAYER_RADIUS_X10  36                // 3,6 px
#define ML_PLAYER_R2          (((ML_PLAYER_RADIUS_X10 * ML_PLAYER_RADIUS_X10) - 1) / 100)  // 12
#define ML_PLAYER_REACH       ((ML_PLAYER_RADIUS_X10 + 9) / 10)                         // 4: ceil(3,6)

// AgujerosPadre [prefab]: seis agujeros hijos en posiciones locales de 1 o
// 2 u, en hexágono (ver src/hole.c). Por decisión del usuario (Q6) el
// hexágono va CENTRADO en pantalla, no 2,57 u por debajo del centro como en
// el dispositivo apaisado (posH, S/gameControl.cs:167-169).
#define ML_HOLES_CX           (PLAYFIELD_W / 2)  // 160
#define ML_HOLES_CY           (PLAYFIELD_H / 2)  // 120
#define ML_UNIT_PX            20                 // 1 u de mundo

// La nave nace en highPos = ScreenToWorldPoint(W / 2, H / 1.4)
// (S/gameControl.cs:156) y el hexágono está en posH = (W / 2, H - H / 1.4):
// en apaisado la nave queda (2 / 1.4 - 1) x 12 u = 5,14 u = 102,9 px por
// ENCIMA del centro del hexágono. Q6: la misma posición respecto al
// hexágono, con el hexágono centrado -> (160, 120 - 102,9) = (160, 17).
#define ML_PLAYER_SPAWN_ABOVE ((s16)((((2.0 / 1.4) - 1.0) * PLAYFIELD_H) + 0.5))   // 103
#define ML_PLAYER_SPAWN_X     ML_HOLES_CX
#define ML_PLAYER_SPAWN_Y     (ML_HOLES_CY - ML_PLAYER_SPAWN_ABOVE)                // 17

// El tiempo sólo corre con B pulsado (Q8). En MonsterLite, en partida:
//   if (Input.touchCount == 0 && HUD.counter >= 60) Time.timeScale = 0;
//   if (Input.touchCount > 0) Time.timeScale = 1;
// (S/gameControl.cs:265-301). Sin dedo en pantalla, el tiempo se para; el
// port cambia el dedo por mantener B (ver src/game.c, timeStopped).
#define ML_TIME_BUTTON        BUTTON_B
// HUD.counter vuelve a 0 fuera de partida y sube una vez por llamada a
// OnGUI mientras sale "ARENA!", hasta 60 (S/HUD.cs:163-166, 470-472):
// mientras tanto no se congela. Unity llama a OnGUI al menos dos veces por
// frame (eventos Layout y Repaint), así que son 30 frames de 60 Hz -> 25 f.
// Con eventos de toque hay más llamadas por frame y dura menos. DEDUCIDO del
// funcionamiento de Unity, no medido: con una sola llamada por frame serían
// 50 f (ver docs/MONSTERLITE_GAP.md §2.4).
#define ML_FREEZE_GRACE_F     25

// Umbrales de desbloqueo por kills de récord (oWeaponBox/Create_0 +
// stCheckWeapon). NO son los del .yyp de surfize_changes (1/2/3/4, valores de
// prueba): son los de neu_swarm2_40.gmx, del surfize.rar del autor, elegidos
// por él (2026-11-12). Ver docs/PORT_AUDIT_GML.md.
#define LASER_ENABLED       40
#define THREEWAY_ENABLED    60
#define CIRCULAR_ENABLED    80
#define TRI_ENABLED         100

// Munición por paquete según arma (oPlayer/Collision_oPaquete)
#define AMMO_WBLOCK         10
#define AMMO_LASER          4
#define AMMO_3WAY           10
#define AMMO_PIVOT          5
#define AMMO_PARA           8
#define AMMO_BOMBA          1
#define AMMO_RAY            1

// generaPaquete
#define PAQUETES_16         16
#define PAQUETES_18         18
#define PAQUETES_24         24

// ---------------------------------------------------------------------------
// Máscaras de colisión
//
// En GameMaker la posición de una instancia es el ORIGEN de su sprite, que
// no tiene por qué ser el centro, y la colisión usa la bbox de la máscara.
// Aquí cada caja se expresa como desplazamiento desde el origen, escalada
// x0.2, para que las distancias de contacto sean las del original.
//
// El original usa máscaras precisas (collisionKind 4 en la nave, 0 en la
// araña) o en diamante (3 en el gusano); el port compara AABB, que es lo que
// cabe en el presupuesto de CPU. Consecuencia: el contacto es algo más
// generoso en las esquinas de la caja. Documentado en docs/PORT_AUDIT_GML.md.
// ---------------------------------------------------------------------------

// sprite173 (nave): 38x58, origen (19,19), bbox L1 R37 T0 B55. Desde el
// bloque 2 de MonsterLite la nave del jugador choca con su círculo de 3,6 px
// (ML_PLAYER_R2); esta caja sólo la usa ya la nave réplica del GML (oTri, en
// src/projectile.c), que tiene el mismo arte.
#define PLAYER_BOX_OX   GML_LEN(1 - 19)         // -4
#define PLAYER_BOX_OY   GML_LEN(0 - 19)         // -4
#define PLAYER_BOX_W    GML_LEN(37 - 1)         // 7
#define PLAYER_BOX_H    GML_LEN(55 - 0)         // 11

// sBulletThin: 20x28, origen (10,14), bbox L0 R19 T0 B27 (rectángulo)
#define BULLET_BOX_OX   GML_LEN(0 - 10)         // -2
#define BULLET_BOX_OY   GML_LEN(0 - 14)         // -3
#define BULLET_BOX_W    GML_LEN(19 - 0)         // 4
#define BULLET_BOX_H    GML_LEN(27 - 0)         // 5

// sprite185 (araña): 64x64, origen (30,40), bbox L0 R62 T2 B63
#define SPIDER_BOX_OX   GML_LEN(0 - 30)         // -6
#define SPIDER_BOX_OY   GML_LEN(2 - 40)         // -8
#define SPIDER_BOX_W    GML_LEN(62 - 0)         // 12
#define SPIDER_BOX_H    GML_LEN(63 - 2)         // 12

// spr_rtype_outslay_head (cabeza): 64x64, origen (32,32), bbox L2 R63 T7 B60
#define SNAKE_HEAD_BOX_OX GML_LEN(2 - 32)       // -6
#define SNAKE_HEAD_BOX_OY GML_LEN(7 - 32)       // -5
#define SNAKE_HEAD_BOX_W  GML_LEN(63 - 2)       // 12
#define SNAKE_HEAD_BOX_H  GML_LEN(60 - 7)       // 11

// spr_rtype_outslay_body (oCorpo): 51x64, origen (26,31), bbox L0 R50 T4 B63
#define SNAKE_BODY_BOX_OX GML_LEN(0 - 26)       // -5
#define SNAKE_BODY_BOX_OY GML_LEN(4 - 31)       // -5
#define SNAKE_BODY_BOX_W  GML_LEN(50 - 0)       // 10
#define SNAKE_BODY_BOX_H  GML_LEN(63 - 4)       // 12

// sprAgujero: 197x197, origen (99,85), bbox L88 R102 T72 B83 (elipse). La
// máscara del agujero es diminuta comparada con el dibujo: 15x12 px del
// room, no el disco entero. De ahí que el enemigo que vaga sólo se
// teletransporte al pasar casi por el centro del agujero.
#define HOLE_BOX_OX     GML_LEN(88 - 99)        // -2
#define HOLE_BOX_OY     GML_LEN(72 - 85)        // -3
#define HOLE_BOX_W      GML_LEN(102 - 88)       // 3
#define HOLE_BOX_H      GML_LEN(83 - 72)        // 2

// oPlayer/Create_0: controlTime = 30 (primer disparo tras 30 frames)
#define SHOOT_INT_FIRST   GML_FRAMES(30)        // -> 25 f

// oPivot/Create_0: 3 satélites (for h=1; h<360; h+=120) a dia = 100 px del
// room, que giran 2 grados por frame de input (t += 2) y se mueven con el
// jugador a acce = 6. Con munición persiguen al objetivo a move_towards_point
// con velocidad random(74).
#define PIVOT_COUNT         3
#define PIVOT_RADIUS        GML_LEN(100)        // -> 20 px
#define PIVOT_SPIN          GML_TURN(2)         // -> 2.4 grados/f
#define PIVOT_HOME_SPEED    GML_SPEED(74)       // tope del random(74)

// oTri/Create_0 y Step_0: réplica estática que dispara sola.
#define TRI_SHOOT_AMMO      GML_FRAMES(24)      // -> 20 f
#define TRI_SHOOT_FREE      GML_FRAMES(62)      // -> 52 f
// El original NO limita las réplicas: el "< 2" de generaPaquete es el tope de
// POWER-UPS en el suelo, no de oTri, y oPlayer/Collision_oNaveReplicaPowUp
// crea uno cada vez sin mirar cuántos hay. En las capturas del juego
// publicado se ven cuatro a la vez. Aquí el tope lo pone el pool de
// proyectiles, que comparten.
#define MAX_TRI             4

// Vidas y mechas (alarm de cada objeto)
#define PARA_LIFE           GML_FRAMES(6)       // oBulletPara/Create: alarm = 6
#define BOMB_FUSE           GML_FRAMES(60)      // oBomba/Create: alarm = 60
#define RAY_FUSE            GML_FRAMES(30)      // oRay/Create: alarm = 30

// Máscaras de los proyectiles (bbox del sprite respecto al origen, x0.2)
#define WBLOCK_BOX_OX       GML_LEN(0 - 45)     // sBullet 96x116, origen (45,4)
#define WBLOCK_BOX_OY       GML_LEN(0 - 4)
#define WBLOCK_BOX_W        GML_LEN(95)
#define WBLOCK_BOX_H        GML_LEN(115)
#define THREEW_BOX_OX       GML_LEN(0 - 12)     // spr3way 24x24, origen (12,12)
#define THREEW_BOX_OY       GML_LEN(0 - 12)
#define THREEW_BOX_W        GML_LEN(23)
#define THREEW_BOX_H        GML_LEN(23)
#define LASER_BOX_OX        GML_LEN(0 - 1)      // sprLaser 6x32, origen (1,30)
#define LASER_BOX_OY        GML_LEN(0 - 30)
#define LASER_BOX_W         GML_LEN(5)
#define LASER_BOX_H         GML_LEN(31)
#define PARA_BOX_OX         GML_LEN(0 - 4)      // sBulletPara 10x28, origen (4,17)
#define PARA_BOX_OY         GML_LEN(0 - 17)
#define PARA_BOX_W          GML_LEN(9)
#define PARA_BOX_H          GML_LEN(27)
#define BOMB_BOX_OX         GML_LEN(0 - 55)     // sprWeapon6 110x110, origen (55,55)
#define BOMB_BOX_OY         GML_LEN(0 - 55)
#define BOMB_BOX_W          GML_LEN(109)
#define BOMB_BOX_H          GML_LEN(109)

// oPaquete: sprPaquete 64x64, origen (32,32), bbox 4..58
#define PACK_BOX_OX         GML_LEN(4 - 32)
#define PACK_BOX_OY         GML_LEN(4 - 32)
#define PACK_BOX_W          GML_LEN(54)
#define PACK_BOX_H          GML_LEN(54)
#define MAX_PACKAGES        24                  // PAQUETES_24, el tope del original

// ---------------------------------------------------------------------------
// Pool de balas
//
// El GML no limita las balas. Cota derivada: la bala por defecto vive
// 320/7.69 = 42 frames y sale cada 10, así que hay 5 en vuelo como máximo; 8
// deja margen. Las armas de FASE 7 piden más (3WAY mete 3 cada 10 frames,
// LASER son 10 segmentos), y entonces habrá que recortar en otro sitio,
// porque el pool de objetos Sprite de SGDK ya está a 67 de 80.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// oEstallido / oDestro: la metralla
//
// oEstallido/Create_0 lanza 12 astillas cada 30 grados a velocidad 10, y cada
// una resta 1 de hp por contacto (oSpider/oCabeza1/oCabeza2b
// Collision_oDestro) y muere al salir de la vista, sin wrap (Other_40 =
// "fuera de la vista 0" -> action_kill_object).
// ---------------------------------------------------------------------------
// El GML tiene la versión "buena" comentada al lado de la que quedó:
//
//     //optimize
//     // for(i=0;i<360;i+=random(60)){
//     for(i=0;i<360;i+=30){
//         b = instance_create(x,y,oDestro)
//         b.speed = 10//random_range(4,64)
//
// o sea: paso angular aleatorio y velocidad aleatoria por astilla, apagados
// por rendimiento. Aquí se recuperan los dos, que es lo que hace que la
// ráfaga no salga siempre igual.
//
// El paso va acotado por abajo a 15 grados: con random(60) puro el paso puede
// salir casi cero y la ráfaga pediría cientos de astillas (por eso lo
// comentaron). Entre 15 y 45 grados salen de 8 a 24 astillas, 12 de media,
// que es justo lo que daba el paso fijo de 30.
#define DEBRIS_STEP_MIN     15
#define DEBRIS_STEP_RAND    30                  // random(60) acotado
#define DEBRIS_PER_BURST    24                  // tope con el paso mínimo
#define DEBRIS_SPEED_MIN    GML_SPEED(4)        // random_range(4, 64)
#define DEBRIS_SPEED_MAX    GML_SPEED(64)
#define DMG_DEBRIS          1                   // Collision_oDestro: hp -= 1
// Hueco para una ráfaga grande y lo que quede de la anterior.
#define MAX_DEBRIS          32

// ---------------------------------------------------------------------------
// oPuntoMorte: el número que sale volando hacia el HUD
//
// Create_0: speed = 0.1, acc = 3, alarm = 30. Step_0: speed += acc y
// move_towards_point hacia (140, 0) con type 0 (balas) o
// (game_width - 140, 0) con type 1 (kills).
// ---------------------------------------------------------------------------
#define FLYER_SPEED0        GML_SPEED(0.1)
#define FLYER_ACCEL         FIX16R(3.0 * GML_SCALE * GML_RATE * GML_RATE)
#define FLYER_LIFE          GML_FRAMES(30)
#define FLYER_TARGET_X0     GML_POS(140)                    // balas
#define FLYER_TARGET_X1     GML_POS(GML_ROOM_W - 140)       // kills
#define FLYER_DIGITS        3
#define MAX_FLYERS          3

// ---------------------------------------------------------------------------
// oMuerte: el fogonazo de la muerte del jugador
//
// Create_0: alarm = 18. Draw_0: t += 1 y dibuja a escala t*8 hasta t > 20.
// Step_0 sacude la VISTA cambiándole el tamaño (un zoom aleatorio de hasta
// +-1600 sobre 1600), que en Mega Drive no existe: se cambia por la sacudida
// más violenta del juego. Ver src/effect.c.
// ---------------------------------------------------------------------------
#define MUERTE_LIFE         GML_FRAMES(18)

// ---------------------------------------------------------------------------
// object0 / object1: el haz del arma PARA
//
// script0(1) crea object1 mientras haya munición, y su Draw_0 encadena 14
// copias de sprite0 a lo largo del rumbo de la nave, separadas por la altura
// del sprite (16 px del room), que hace daño por contacto.
// ---------------------------------------------------------------------------
#define BEAM_SEGMENTS       14                  // maxhi = 14
#define BEAM_SPACING        GML_LEN(16)         // hi = sprite_get_height(sprite0)
#define BEAM_START_OFF      GML_LEN(-25)        // hix = y - 25

// Pool único para todo lo que lanza el jugador (balas de cualquier arma,
// segmentos de láser, bombas y rayos): sólo hay un arma activa a la vez, así
// que comparten objetos. 14 + 4 explosiones + 61 de jugador y enemigos = 79
// de los 80 objetos Sprite de SGDK.
#define MAX_PROJECTILES     14
// morteGusano() crea una explosión por eslabón (13 en el gusano largo), así
// que el pool tiene que dar para una cascada; con 8 se ven las ocho últimas,
// que es lo que llena la pantalla.
#define MAX_EFFECTS         8

// image_speed de los sprites del original: playbackSpeedType 1 con speed 1.0
// es un frame de animación por frame de juego a 60 Hz; a 50 Hz hacen falta
// 1.2 para que el ciclo dure lo mismo en tiempo real.
#define GML_SPEED_ANIM      GML_TURN(1.0)

// Daño por impacto, de los manejadores de cada enemigo
#define DMG_TO_SPIDER       4                   // oSpider/Collision_oBulletS
#define DMG_TO_SNAKE        1                   // oCabeza1/Collision_oBulletS

// ---------------------------------------------------------------------------
// Reparto de las 4 líneas de paleta (16 colores cada una)
// ---------------------------------------------------------------------------
//   PAL0  fuente del sistema / HUD
//   PAL1  jugador + fondo (comparten línea, ver background.c)
//   PAL2  enemigos: araña y gusano
//   PAL3  balas, explosiones y power-ups
// ---------------------------------------------------------------------------

#endif
