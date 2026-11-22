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

// oPlayer/Create_0 + Step_0. Desde el bloque 3 de MonsterLite el disparo es
// el de MonsterLite (ML_SHOT_* más abajo); del GML sólo queda la cadencia de
// las dos armas que MonsterLite no tiene, PIVOT y PARA.
#define SHOOT_INT_WEAPON    GML_FRAMES(6)       // controlTime 6  -> 5 f

// oBulletPara vspeed -64. (Las balas fina, WBLOCK y 3WAY y el láser del GML
// ya no se usan: van con los valores de MonsterLite, ML_BULLET_SPEED y
// ML_LASER_*.)
#define BULLET_SPEED_PARA   GML_SPEED(64)       // -> 15.36 px/f
#define WEAPON_SPAWN_DIST   GML_LEN(100)        // bomba/rayo     -> 20 px

// La máquina de estados del GML (stSteadyW / stAppearA1W / stWalkingA1W:
// quieto 6, ataca 300 y vaga 600 frames, aparición con alarm[0] = 90, giro
// en grados fijos: velo de cada bicho al atacar, 2 grados al vagar) ya no se
// usa desde el bloque 8 de MonsterLite: la sustituye la IA de enemyMov
// (ML_TURN_K_*, ML_AI_TIMER_*, arriba en este fichero) y los enemigos nacen
// visibles y en marcha. Ver docs/PORT_AUDIT_GML.md, "Bloque 8".
//
// Lo mismo con las velocidades de avance del GML (bloque 1): salen de
// setSpeed por nivel (Entity_mlSpeed, ML_*_MAX_MU). Quedan de referencia:
//   oSpider/Create_0:   velo = 3 (0,72 px/f, 3,6 grados/f), hp = 0
//   oCabeza1/Create_0:  velo = random_range(5, 7.8) (1,20-1,875 px/f y
//                       6-9,36 grados/f), hp = 4, stepdelay = 7 (6 f)
//   oCabeza2b/Create_0: velo = 6.2 (1,49 px/f, 7,44 grados/f), hp = 2,
//                       stepdelay = 6 (5 f)
// El stepdelay tampoco: los eslabones siguen las migas de MonsterLite, cada
// 8 px (ML_SNAKE_CRUMB_PX).
//
// La araña muere de cualquier impacto: en MonsterLite su hp (10) no se mira,
// y en el GML era hp = 0 con las balas restando 4. Se queda el 0.
#define SPIDER_HP           0

// Los dos gusanos del GML con su longitud (Q2): cabeza + 12 y + 10
// eslabones. hp de MonsterLite para los dos (bloque 3, Q2): ML_SNAKE_HP.
#define SNAKE1_HP           ML_SNAKE_HP         // GML: 4
#define SNAKE1_PARTS        13                  // 9 oCorpo + oCu1 + oCu2 + oCabeza2 + cabeza
#define SNAKE2_HP           ML_SNAKE_HP         // GML: 2
#define SNAKE2_PARTS        11                  // 7 oCorpo + oCu1 + oCu2 + oCabeza2 + cabeza

#define SNAKE_MAX_PARTS     SNAKE1_PARTS

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

// ---------------------------------------------------------------------------
// Mensajes de partida del HUD (bloque 9, S/HUD.cs y S/gameControl.cs)
// ---------------------------------------------------------------------------
// La oleada 27 pone globales.OLEADA = "!!!" (S/gameControl.cs:597) y el HUD
// la borra cuando oleadaT, que empieza en 30 y baja 1 por Update mientras
// hay texto, pasa de 0 (S/HUD.cs:172-178): se ve en el frame en que se pone
// y en los 30 siguientes, 31 frames de 60 Hz -> 25,8 -> 26 f (en
// docs/MONSTERLITE_GAP.md ponía 30 y 25). Corre en Update, así que sigue con
// el tiempo parado. Si la oleada vuelve a salir con el texto puesto, oleadaT
// no vuelve a empezar.
#define ML_WAVE_ALERT       27
#define ML_ALERT_F          26
// ChangeLevelWait (S/gameControl.cs:768-779): un bucle de 3 s con
// Time.deltaTime y un WaitForSeconds(0.1): 3,1 s de tiempo de juego = 155 f.
// Los dos van con el tiempo escalado: con el tiempo parado (Q8) no cuentan.
#define ML_LEVEL_MSG_F      155
// "COINS": al recoger una moneda, CoinsManager.isAddingCoins dibuja la línea
// más abajo durante 10 llamadas a OnGUI (S/HUD.cs:381-396), dos por frame de
// 60 Hz -> 5 frames de Update -> 4 f. Ver src/hud.c.
#define ML_COINS_BUMP_F     4

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
// IA de enemyMov (bloque 8 de docs/MONSTERLITE_GAP.md, S/enemyMov.cs)
// ---------------------------------------------------------------------------

// Giro: "rotation = Slerp(rotation, haciaElDestino, turningSpeed *
// Time.deltaTime)" en FixedUpdate, o sea una FRACCIÓN t = turningSpeed *
// 0,02 del ángulo que falta en cada frame (1:1 a 50 Hz). turningSpeed =
// Random.Range(1, turningSpeedMax / 1.4) al nacer; turningSpeedMax [prefab]
// es 8 en la araña y 10 en el gusano. La fracción va en Q12 (1/4096):
//   mínimo 1 x 0,02 x 4096 = 81,92 -> 82
//   máximo m / 1.4 x 0,02 x 4096 = m x 4096 / 70: araña 468,1 -> 468,
//   gusano 585,1 -> 585.
// El sorteo es entero sobre la fracción en Q12: error de redondeo de hasta
// medio paso, 0,6 % en la más pequeña.
#define ML_TURN_K_SHIFT     12
#define ML_TURN_K_MIN       (((2 * 4096) + 50) / 100)       // 82
#define ML_TURN_K_MAX(m)    ((((m) * 4096) + 35) / 70)
#define ML_SPIDER_TURN_K_MAX ML_TURN_K_MAX(8)               // 468
#define ML_SNAKE_TURN_K_MAX  ML_TURN_K_MAX(10)              // 585

// Temporizador de re-sorteo: "timer -= 1" en Update (60 Hz), empieza en 200
// [prefab] y al pasar de 0 re-sortea y vuelve a 30. En WANDER, move()
// (FixedUpdate, 50 Hz) le resta además "level % 100" por tick. Se lleva en
// quintos para que las dos cosas sean enteras: Update resta 1,2 por frame de
// 50 Hz = 6 quintos; WANDER, 5 x (level % 100). 1000 / 6 -> 167 f la primera
// vez (MonsterLite 167,5) y 150 / 6 -> 26 f después (25,8).
#define ML_AI_TIMER_START   (200 * 5)
#define ML_AI_TIMER_RESET   (30 * 5)
#define ML_AI_TIMER_UPDATE  6
#define ML_AI_TIMER_WANDER  5

// Colisiones de los enemigos (bloque 8): esferas de MonsterLite, en décimas
// de píxel. Araña SphereCollider 0.28 x escala 1.5 = 0,42 u = 8,4 px; cabeza
// y eslabón (culobicho, culobicho2) 0.32 x 1.5 = 0,48 u = 9,6 px.
#define ML_SPIDER_RADIUS_X10  84
#define ML_SNAKE_RADIUS_X10   96
// "Distancia < r" con distancias enteras al cuadrado: d^2 <= (r^2 - algo),
// redondeado hacia abajo (como ML_PLAYER_R2). Y el alcance, ceil(r), para
// descartar sin multiplicar.
#define ML_R2_LT(rx10)        ((((rx10) * (rx10)) - 1) / 100)
#define ML_REACH(rx10)        (((rx10) + 9) / 10)
#define ML_SPIDER_R2          ML_R2_LT(ML_SPIDER_RADIUS_X10)                            // 70
#define ML_SPIDER_REACH       ML_REACH(ML_SPIDER_RADIUS_X10)                            // 9
#define ML_SNAKE_R2           ML_R2_LT(ML_SNAKE_RADIUS_X10)                             // 92
#define ML_SNAKE_REACH        ML_REACH(ML_SNAKE_RADIUS_X10)                             // 10
// Esfera contra esfera: se suman los radios. Nave 3,6 px.
#define ML_PLAYER_SPIDER_R2   ML_R2_LT(36 + ML_SPIDER_RADIUS_X10)                       // 143 (12 px)
#define ML_PLAYER_SPIDER_REACH ML_REACH(36 + ML_SPIDER_RADIUS_X10)                      // 12
#define ML_PLAYER_SNAKE_R2    ML_R2_LT(36 + ML_SNAKE_RADIUS_X10)                        // 174 (13,2 px)
#define ML_PLAYER_SNAKE_REACH ML_REACH(36 + ML_SNAKE_RADIUS_X10)                        // 14
// Nave holo, 5,4 px (collideAndDie: sólo "arana" y "snake", la cabeza).
#define ML_HOLO_SPIDER_R2     ML_R2_LT(54 + ML_SPIDER_RADIUS_X10)                       // 190 (13,8 px)
#define ML_HOLO_SPIDER_REACH  ML_REACH(54 + ML_SPIDER_RADIUS_X10)                       // 14
#define ML_HOLO_SNAKE_R2      ML_R2_LT(54 + ML_SNAKE_RADIUS_X10)                        // 224 (15 px)
#define ML_HOLO_SNAKE_REACH   ML_REACH(54 + ML_SNAKE_RADIUS_X10)                        // 15
// Bola CIRCULAR, esfera de 4 px (ML_CIRC_HALF).
#define ML_BALL_SPIDER_R2     ML_R2_LT(40 + ML_SPIDER_RADIUS_X10)                       // 153 (12,4 px)
#define ML_BALL_SPIDER_REACH  ML_REACH(40 + ML_SPIDER_RADIUS_X10)                       // 13
#define ML_BALL_SNAKE_R2      ML_R2_LT(40 + ML_SNAKE_RADIUS_X10)                        // 184 (13,6 px)
#define ML_BALL_SNAKE_REACH   ML_REACH(40 + ML_SNAKE_RADIUS_X10)                        // 14

// Migas del gusano (S/headScr.cs): cada vez que la cabeza se aleja
// segmentSpacing = 0,4 u [prefab] de la última miga se pone otra, y cada
// eslabón salta a la suya. 8 px; la distancia se compara al cuadrado en
// 1/64 de píxel.
#define ML_SNAKE_CRUMB_PX     8
#define ML_SNAKE_CRUMB_F2     ((s32)(ML_SNAKE_CRUMB_PX * 64) * (ML_SNAKE_CRUMB_PX * 64))

// ---------------------------------------------------------------------------
// Jugador de MonsterLite (bloque 2 de docs/MONSTERLITE_GAP.md)
// ---------------------------------------------------------------------------

// Movimiento de la nave: el del GML (decisión del usuario, 2026-11-16, que
// sustituye a Q7 y al wrap sin margen del bloque 2). oPlayer/Step_0: cada
// tecla suma acc = 6 a su eje (oPlayer/Create_0), sin inercia y sin
// normalizar: en diagonal va un 41 % más rápido, como en el original.
// 6 x 0,2 x 1,2 = 1,44 px/f -> 92/64 = 1,4375 en fix16 (-0,17 %).
#define PLAYER_ACC            GML_SPEED(6)

// stPlayerMovement(): wrap en los cuatro bordes con margen offSet = 15
// (oPlayer/Create_0) -> 3 px.
#define PLAYER_WRAP_OFFSET    GML_LEN(15)

// SphereCollider del prefab player [prefab]: radio 0.18 u, escala 1 ->
// 3,6 px. Las distancias de colisión son enteras (píxeles), así que la
// condición "d < 3,6" es "d^2 < 12,96", o sea d^2 <= 12 en enteros: exacta,
// sin redondeo (no hay ningún d^2 entero entre 12 y 12,96).
#define ML_PLAYER_RADIUS_X10  36                // 3,6 px
#define ML_PLAYER_R2          (((ML_PLAYER_RADIUS_X10 * ML_PLAYER_RADIUS_X10) - 1) / 100)  // 12
#define ML_PLAYER_REACH       ((ML_PLAYER_RADIUS_X10 + 9) / 10)                         // 4: ceil(3,6)

// Aparición de la nave y agujeros: por decisión del usuario al cerrar el
// bloque 2 (docs/MONSTERLITE_DECISIONES.md) vuelven a los del port antes
// del bloque 2, no a los de MonsterLite. Los agujeros son el anillo de seis
// de src/hole.c, alrededor del centro del room, y la nave nace en el centro
// de la pantalla, como startGame() del GML:
//   instance_create(global.game_width*0.5, global.game_height*0.5, oPlayer)
// (MonsterLite la pone en highPos, 5,14 u por encima del hexágono:
// S/gameControl.cs:156.)
#define PLAYER_SPAWN_X        (PLAYFIELD_W / 2)  // 160
#define PLAYER_SPAWN_Y        (PLAYFIELD_H / 2)  // 120

// El tiempo sólo corre con A, B o C pulsado (Q8). En MonsterLite, en partida:
//   if (Input.touchCount == 0 && HUD.counter >= 60) Time.timeScale = 0;
//   if (Input.touchCount > 0) Time.timeScale = 1;
// (S/gameControl.cs:265-301). Sin dedo en pantalla, el tiempo se para; el
// port cambia el dedo por mantener B (ver src/game.c, timeStopped). A y C
// hacen lo mismo que B (decisión del usuario, 2026-11-16).
#define ML_TIME_BUTTON        (BUTTON_A | BUTTON_B | BUTTON_C)
// HUD.counter vuelve a 0 fuera de partida y sube una vez por llamada a
// OnGUI mientras sale "ARENA!", hasta 60 (S/HUD.cs:163-166, 470-472):
// mientras tanto no se congela. Unity llama a OnGUI al menos dos veces por
// frame (eventos Layout y Repaint), así que son 30 frames de 60 Hz -> 25 f.
// Con eventos de toque hay más llamadas por frame y dura menos. DEDUCIDO del
// funcionamiento de Unity, no medido: con una sola llamada por frame serían
// 50 f (ver docs/MONSTERLITE_GAP.md §2.4).
#define ML_FREEZE_GRACE_F     25

// ---------------------------------------------------------------------------
// Armas de MonsterLite (bloque 3 de docs/MONSTERLITE_GAP.md, §2.5)
//
// S/disparo.cs:32-160 desde FixedUpdate (50 Hz, 1:1 con la Mega Drive):
// mientras shootTime > 1 baja 1 por tick; cuando llega a 1 dispara y lo pone
// a la cadencia, así que el periodo es la cadencia en frames. Sólo cuenta y
// dispara si hay un enemigo (getClosest). Valores del prefab player.
// ---------------------------------------------------------------------------
#define ML_SHOT_FIRST_F       4     // shootTime inicial
#define ML_SHOT_NORMAL_F      8     // shotTimeNormal: la bala fina, sin munición
#define ML_SHOT_WBLOCK_F      4     // shotTimewBock
#define ML_SHOT_LASER_F       5     // shotTimeLaser
#define ML_SHOT_TWAY_F        3     // shotTimeTWay
#define ML_SHOT_MOIRE_F       2     // shotTimeMoire
// CIRCULAR usa shotTimeTWay (3) en el original, no shotTimeCircular (4), que
// no se lee nunca. Q12: se corrige y usa la suya.
#define ML_SHOT_CIRCULAR_F    4

// AddForce(right · shotForce 150) a una masa de 0.1: 30 u/s -> 12 px/f,
// exacto en fix16. Bullet y BulletThin llevan una caja de 0.8 u -> 16x16 px
// centrada, y se destruyen al salir de pantalla (bulletScrKillSprite).
#define ML_BULLET_SPEED       FIX16(12)
#define ML_BULLET_HALF        8
#define ML_BULLET_BOX         (2 * ML_BULLET_HALF)

// TWAY: tres Bullet a -30, 0 y +30 grados del rumbo.
#define ML_TWAY_SPREAD        FIX16(30)

// WBLOCK: la bala nace desplazada "distanceToGun" =
// GetComponentInChildren<SpriteRenderer>().sprite.bounds.size.x / 2, llamado
// desde disparo, que está en la NAVE: es el sprite de la nave (hijo "nave" de
// player.prefab, nave.png, 58x38 px a 100 px por unidad; sprite.bounds no
// lleva la escala 1.3 del hijo) -> 0,29 u -> 5,8 px. Leído con UnityPy. En
// el original el desplazamiento va en X del mundo, sin rotar; Q12: se
// corrige y va hacia donde apunta la nave.
#define ML_WBLOCK_GUN_PX      6

// Daño a la cabeza del gusano (enemyLife): decreaseHP -1 con la bala fina,
// decreaseDoubleHP -3 con la gorda y las bolas. Y el orden importa: primero
// mira si hp < 0 y SÓLO ENTONCES mata; si no, resta. Con hp 4 la fina mata
// al 6.º impacto y la gorda al 3.º. La gorda que no mata no se destruye:
// atraviesa (S/bulletScript.cs:67-85). La araña muere de cualquier impacto.
#define ML_SNAKE_HP           4     // culobicho [prefab]
#define ML_DMG_THIN           1
#define ML_DMG_FAT            3

// LASER y MOIRE: impacto instantáneo (S/RaycastScr.cs). Un Physics.Raycast
// por frame desde la nave en la dirección de apuntado, sin distancia
// máxima, contra el primer collider de la línea (triggers incluidos). El
// RaycastObj vive 30 frames de Update (LASER) u 8 (MOIRE): 25 y 7 f. El haz
// que se ve (Laser.prefab, LineRenderer) vive timer 2 frames de Update
// (LASER) o 8 (MOIRE): 2 y 7 f.
#define ML_LASER_RAY_F        25
#define ML_MOIRE_RAY_F        7
#define ML_LASER_BEAM_F       2
#define ML_MOIRE_BEAM_F       7

// Radios del rayo contra lo que tapa la línea, en décimas de píxel. Son los
// collider de MonsterLite (§2.5-2.7): araña 0.42 u, cabeza y eslabón 0.48 u,
// agujero 0.88 u, paquete caja de 1 u (se toma la mitad del lado), balas en
// vuelo caja de 0.8 u (ídem).
// (Araña, cabeza y eslabón: ML_SPIDER_RADIUS_X10 y ML_SNAKE_RADIUS_X10, que
// desde el bloque 8 son también los de todas sus colisiones.)
#define ML_RAY_R_HOLE_X10     176
#define ML_RAY_R_PACK_X10     100
#define ML_RAY_R_BULLET_X10   80
// En 1/64 de píxel, que es como sale la distancia perpendicular del rayo.
#define ML_X10_TO_F16(r)      ((s16)((((r) * 64) + 5) / 10))

// CIRCULAR (S/circCont.cs, S/circularScr.cs): bolas a len = 1 u (20 px) de
// la nave; las que no se sueltan orbitan 2 grados por frame de Update
// (2,4/f); las sueltas van al enemigo más cercano a 1 u por frame de Update
// (24 px/f). Esfera de 0.2 u -> 4 px. En el original el bucle es
// "i <= 360" con paso 120 y salen 4 bolas, dos superpuestas; Q12: 3.
#define ML_CIRC_BALLS         3
#define ML_CIRC_RADIUS        20
#define ML_CIRC_SPIN          FIX16R(2.4)
#define ML_CIRC_SPEED         24
#define ML_CIRC_HALF          4

// ---------------------------------------------------------------------------
// Lo que suelta un enemigo (bloque 4 de MonsterLite, §2.7 y §2.9)
// ---------------------------------------------------------------------------
// leavePow (bulletScript, bulletThinSrc, RaycastScr, circularScr):
// Random.Range(0, 2), 50 % moneda y 50 % paquete por baja con disparo. La
// bola CIRCULAR contra una araña suelta siempre paquete.
//
// Paquete y moneda: lifeTime 200 frames de Update [prefab] -> 167 f. Al
// recoger el paquete (triggerAnim) sube aumento = 0,1 u por frame de Update
// (2,4 px/f) y su vida baja 30 más por frame de Update: aquí, con la vida en
// frames de 50 Hz, 31 por frame (los dos lados escalan igual). Caja de 0,5 u
// a escala 2 = 1 u -> 20x20 px centrada.
#define ML_ITEM_LIFE_F        167
#define ML_ITEM_LIFT_DEC      31
#define ML_PACK_RISE          FIX16R(2.4)
#define ML_ITEM_HALF          10
#define ML_ITEM_BOX           (2 * ML_ITEM_HALF)

// Moneda recogida (playerMovement.moveCoin): MoveTowards hacia
// ScreenToWorldPoint(W / 2, -H), una pantalla por debajo del centro de abajo,
// a 8 u/s · deltaTime: 2,67 px por frame de Update -> 3,2 px/f. Como va con
// deltaTime, con el tiempo parado no se mueve; la caducidad, que cuenta
// frames de Update, sí sigue.
#define ML_COIN_FLY_SPEED     FIX16R(3.2)
#define ML_COIN_TARGET_X      (PLAYFIELD_W / 2)
#define ML_COIN_TARGET_Y      (2 * PLAYFIELD_H)

// coinAnim: 6 frames a 12 por segundo -> un frame cada 50 / 12 = 4,17 f.
#define ML_COIN_FRAMES        6
#define ML_COIN_FPS           12

// Coin.getCoinType: hasta el nivel 10, 50 o 60; desde el 11, 50, 60, 150,
// 180 o 100, equiprobables.
#define ML_COIN_LEVEL_HIGH    10

// Monedas a la vez. MonsterLite no las limita; con una baja cada pocos
// frames, 50 % de moneda y 167 f de vida, 16 dan para el caso peor medido
// (~28 bajas en 600 f con la arena llena: ~4 monedas vivas).
#define MAX_COINS             16

// ---------------------------------------------------------------------------
// Bloque 7: los power-ups de MonsterLite (§2.7 de docs/MONSTERLITE_GAP.md)
// ---------------------------------------------------------------------------
// Oleadas (S/gameControl.cs:477-571): la 3 pone una bomba si
// Random.Range(0, 6) == 1, la 4 unos rayos si Random.Range(0, 7) == 1, y la
// 6, 16, 19, 20 y 21 una nave holo si hay menos de maxNaveProxy (10, valor de
// la escena). Las tres en globales.getRandomPos(): x e y al azar entre el 10
// y el 90 % de la pantalla.
#define ML_BOMB_CHANCE        6
#define ML_RAYS_CHANCE        7
#define ML_HOLO_MAX           10
#define ML_RANDPOS_X0         (PLAYFIELD_W / 10)                // 32
#define ML_RANDPOS_W          ((PLAYFIELD_W * 8) / 10)          // 256
#define ML_RANDPOS_Y0         (PLAYFIELD_H / 10)                // 24
#define ML_RANDPOS_H          ((PLAYFIELD_H * 8) / 10)          // 192

// La bomba y los rayos no caducan y MonsterLite no los limita. Como ocupan
// el pool de lo que hay en el suelo (que es también el de los paquetes),
// aquí se topan: si ya hay ML_GROUND_POW_MAX entre los dos, la oleada no
// crea otro. Invención del port (ver docs/PORT_AUDIT_GML.md, bloque 7).
#define ML_GROUND_POW_MAX     6

// Cajas: Bomba.prefab, BoxCollider de 0,7 u (14 px); RayosBomba.prefab, de
// 1 u (20 px). La nave las toca con su esfera de 3,6 px.
#define ML_BOMB_HALF          7
#define ML_RAYS_HALF          10

// playerMovement.OnTriggerEnter, bomba y rayos: System.Threading.Thread.Sleep
// (100) congela el juego entero 100 ms (Q17): 5 frames.
#define ML_POW_FREEZE_F       5

// ---------------------------------------------------------------------------
// Bloque 12: hitstop, cámara lenta, sacudidas y estela
// ---------------------------------------------------------------------------

// El hitstop se cuenta en milisegundos, como el Thread.Sleep del original, y
// se paga en frames de 20 ms (50 Hz): cada frame congelado resta 20 y el
// resto se queda para la siguiente. Así 100 ms son 5 frames exactos y los
// 30 ms de cada baja con bala (globales.milisecsEnemyDestroyed = 30,
// S/globales.cs:81; llamadas en S/bulletScript.cs:62, 80 y
// S/bulletThinSrc.cs:45, 65) salen 1 y 2 frames alternos: 1,5 de media,
// los 30 ms justos (Q17, ver docs/PORT_AUDIT_GML.md, bloque 12).
#define ML_FRAME_MS           20
#define ML_KILL_FREEZE_MS     30
#define ML_POW_FREEZE_MS      (ML_POW_FREEZE_F * ML_FRAME_MS)

// Con PORT_NO_HITSTOP=1 no hay hitstop (ni por bala ni por bomba y rayos):
// sólo para medir con tools/measure.sh el coste de CPU sin el cambio de
// ritmo de partida que trae el hitstop.
#ifndef PORT_NO_HITSTOP
#define PORT_NO_HITSTOP       0
#endif

// Muerte (S/gameControl.cs:690-725): Time.timeScale = 0,2 mientras la
// corrutina wait() hace dos WaitForSeconds(0,1) de tiempo escalado: 0,2 s de
// juego = 1 s real = 50 frames, en los que el mundo avanza un paso de cada 5
// (FixedUpdate a 0,02 s de juego = 0,1 s reales). Después, sacudida grande y
// game over.
#define ML_DEATH_SLOW_F       50
#define ML_DEATH_SLOW_DIV     5

// La estela de la nave (TrailRenderer del hijo "nave" de P/player.prefab):
// m_Time 0,26 s = 13 f, m_MinVertexDistance 0,1 u = 2 px, ancho de 0,08 u
// (1,6 px) a 0,001.
#define ML_TRAIL_F            13
#define ML_TRAIL_MIN_DIST     2

// triggerSplashBomb.timerRays = 30 [prefab player]. RayoScr.Update hace
// "timer--; if (timer < 0)": dispara en el Update 31, a 60 Hz -> 25,8 f.
#define ML_RAYS_TIMER_F       26
// RayoScr: cada vértice intermedio de la línea con ruido Random.Range(-0.4,
// 0.4) u = +-8 px.
#define ML_RAY_NOISE          8

// La nave holo (naveProxy.prefab). Inactiva gira transform.Rotate(1 grado)
// por Update: 1,2 grados/f. Activa dispara BulletThin con shotForce 80 (16
// u/s = 6,4 px/f) cada shotTimeNormal 40 Update (33 f), el primero con
// shootTime 20 (17 f). Su esfera es de radio 0,18 a escala 1,5: 5,4 px.
#define ML_HOLO_SPIN          FIX16R(1.2)
#define ML_HOLO_FIRST_F       17
#define ML_HOLO_SHOT_F        33
#define ML_HOLO_BULLET_SPEED  FIX16R(6.4)
#define ML_HOLO_RADIUS_X10    54
#define ML_HOLO_R2            ((ML_HOLO_RADIUS_X10 * ML_HOLO_RADIUS_X10) / 100)   // 29
#define ML_HOLO_REACH         ((ML_HOLO_RADIUS_X10 + 9) / 10)                     // 6
// La nave la activa al tocarla: esfera contra esfera, 3,6 + 5,4 = 9 px.
#define ML_HOLO_TOUCH_R       ((36 + ML_HOLO_RADIUS_X10) / 10)                                // 9
#define ML_HOLO_TOUCH_R2      (((36 + ML_HOLO_RADIUS_X10) * (36 + ML_HOLO_RADIUS_X10)) / 100) // 81

// Munición por paquete (paqueteScr [prefab]). PIVOT y PARA no existen en
// MonsterLite: siguen con la del GML (AMMO_PIVOT, AMMO_PARA).
#define ML_AMMO_WBLOCK        10
#define ML_AMMO_LASER         4
#define ML_AMMO_TWAY          10
#define ML_AMMO_CIRCULAR      4
#define ML_AMMO_MOIRE         3

// Umbrales de desbloqueo por kills de récord (oWeaponBox/Create_0 +
// stCheckWeapon). NO son los del .yyp de surfize_changes (1/2/3/4, valores de
// prueba): son los de neu_swarm2_40.gmx, del surfize.rar del autor, elegidos
// por él (2026-11-12). Ver docs/PORT_AUDIT_GML.md.
#define LASER_ENABLED       40
#define THREEWAY_ENABLED    60
#define CIRCULAR_ENABLED    80
#define TRI_ENABLED         100

// Munición por paquete de las dos armas del GML que MonsterLite no tiene
// (oPlayer/Collision_oPaquete). Las demás, ML_AMMO_*.
#define AMMO_PIVOT          5
#define AMMO_PARA           8

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

// Las máscaras del GML de la araña (sprite185, caja de 12x12), la cabeza
// (12x11), el eslabón (10x12) y el agujero (3x2, la que usaba el
// teletransporte del estado de vagar) ya no se usan desde el bloque 8 de
// MonsterLite: los enemigos chocan con sus esferas (ML_SPIDER_RADIUS_X10,
// ML_SNAKE_RADIUS_X10). Ver docs/PORT_AUDIT_GML.md, "Bloque 8".

// oPivot/Create_0: 3 satélites (for h=1; h<360; h+=120) a dia = 100 px del
// room, que giran 2 grados por frame de input (t += 2) y se mueven con el
// jugador a acce = 6. Con munición persiguen al objetivo a move_towards_point
// con velocidad random(74).
#define PIVOT_COUNT         3
#define PIVOT_RADIUS        GML_LEN(100)        // -> 20 px
#define PIVOT_SPIN          GML_TURN(2)         // -> 2.4 grados/f
#define PIVOT_HOME_SPEED    GML_SPEED(74)       // tope del random(74)

// oTri (la réplica del GML) lo sustituye la nave holo de MonsterLite en el
// bloque 7: ver ML_HOLO_*.

// Vidas y mechas (alarm de cada objeto)
#define PARA_LIFE           GML_FRAMES(6)       // oBulletPara/Create: alarm = 6
#define BOMB_FUSE           GML_FRAMES(60)      // oBomba/Create: alarm = 60

// Máscaras de los proyectiles (bbox del sprite respecto al origen, x0.2)
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
// bolas CIRCULAR, satélites del PIVOT, bombas, rayos y réplicas): sólo hay un
// arma activa a la vez, así que comparten objetos.
//
// Bloque 3 de MonsterLite (§2.5): las balas viven sólo en pantalla, a
// 12 px/f. El TWAY saca 3 cada 3 f y una bala cruza como mucho ~34 f (la
// diagonal desde una esquina): hasta ~35 vivas con la nave en una esquina,
// ~18 desde el centro. CIRCULAR saca 3 bolas cada 4 f que llegan a su
// objetivo en pocos frames: 4-8 lo normal, ~20 en el peor caso. Era 14 (el
// GML). Ver la medición en docs/PORT_AUDIT_GML.md: si no caben, se pierde la
// bala más nueva del TWAY, que no dispara si el pool está lleno.
// Bloque 7: +10 de las naves holo, que viven en este pool, y +10 para sus
// balas (una cada 33 f a 6,4 px/f: como mucho dos vivas por nave).
#define MAX_PROJECTILES     56
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
