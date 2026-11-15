#include "gfxdebug.h"
#include "game.h"
#include "spr.h"
#include "vramspr.h"
#include "fastmath.h"
#include "bench.h"
#include "resources.h"
#include "entity.h"
#include "collision.h"
#include "hole.h"
#include "bg.h"
#include "hud.h"
#include "player.h"
#include "enemy.h"
#include "snake.h"
#include "projectile.h"
#include "effect.h"
#include "powerup.h"
#include "menu.h"
#include "audio.h"
#include "shake.h"
#include "debris.h"
#include "flyer.h"
#include "beam.h"
#include "spr.h"
#include "gm_art.h"

#ifndef PORT_VRAM_REPORT
// Imprime el reparto de tiles de VRAM, los sprites de hardware en uso y el
// contador de FPS/CPU en pantalla (ver inc/vramspr.h).
//
// OJO: NO sirve para medir el caso peor. Con la arena llena el juego va justo
// al borde del frame, y los dos sprintf y las cuatro escrituras de texto de
// este bloque bastan para empujarlo por encima del 100 %: con el contador
// puesto el arnés de estrés cae a 25 fps y marca 100-140 % cuando sin él va
// exactamente igual que antes. Medido en el pulido de gráficos (ver el
// informe): la velocidad de dos builds se compara por la firma de la partida
// (kills y munición en un frame emulado fijo), no por este contador.
#define PORT_VRAM_REPORT 0
#endif

#ifndef PORT_DEBUG_BG
#define PORT_DEBUG_BG 0
#endif

#if PORT_DEBUG_BG
// ---------------------------------------------------------------------------
// Fondos de contraste, para poder mirar el arte sin que el gris del campo se
// coma las siluetas. Se van pasando con el botón C durante la partida.
//
// OJO al usarlo: el VDP no mezcla colores, pero el campo de juego es un color
// plano, así que todo lo que en el original es translúcido (la humareda, la
// metralla, el haz del PARA, el borde de los agujeros) lleva su color YA
// MEZCLADO con el fondo de verdad. Con otro fondo esos tonos dejan de cuadrar
// y se ven apagados: no es el arte estando mal, es justo la técnica. Para
// juzgar colores, fondo normal (el primero de la lista).
// ---------------------------------------------------------------------------
static const u16 debugBg[] =
{
    ROOM_BG_COLOR,  // el de verdad
    0x0E0E,         // magenta: ningún sprite lo usa, todo destaca
    0x0000,         // negro
    0x0EEE,         // blanco
    0x0888,         // gris medio
    0x00E0,         // verde
    0x000E,         // rojo
    0x0E00,         // azul
};

static u16 debugBgIdx;
static u16 debugBgPad;
#endif

u32   g_score;
u16   g_level;
u32   g_factorLevel;
u16   g_kills;
u32   g_lastScore;
u16   g_maxKills;
u16   g_playerBullets;
u16   g_weaponIndex;
u16   g_weaponsUnlocked;
u32   g_nextWeapon;
u16   g_credits;
u8    g_state;

static Player player;
static VStream playerStream;
static MDSpr *playerSprite;

// startGame() de MonsterLite es una corrutina con esperas: frames desde
// Game_start (se para al llegar al primer gusano) y cuenta atrás del bucle de
// oleadas. playerIn dice si la nave ya existe (en MonsterLite no existe
// durante los primeros 40 f: ni se ve, ni dispara, ni choca).
static u16 startFrames;
static u16 waveTimer;
static bool playerIn;

// stCheckWeapon(). Hay que leerlo con cuidado porque tiene un desfase de uno
// que NO es un despiste del port:
//
//     for (g = 0; g < global.maxKills; g += 1) {
//         if (g >= laserEnabled) { ...añade laser...; nextWeapon = threewayEnabled }
//         ...
//     }
//
// El mayor valor que toma g es maxKills - 1, así que "g >= laserEnabled" con
// laserEnabled = 1 pide maxKills >= 2. O sea que los umbrales de verdad son
// 2, 3, 4 y 5 kills, no 1, 2, 3 y 4. El port los tenía uno por debajo y
// regalaba el láser con la primera muerte.
//
// nextWeapon arranca en oWeaponBox/Create_0. En surfize_changes valía 999999
// y el menú enseñaba "NEXT 999999" hasta el primer desbloqueo; en
// neu_swarm2_40, la versión que se porta, vale 40: el primer umbral.
#define NEXT_WEAPON_NONE LASER_ENABLED

static void checkWeapon(void)
{
    u16 unlocked = 1;           // WBLOCK siempre está
    u32 next = NEXT_WEAPON_NONE;

    if (g_maxKills > LASER_ENABLED)    { unlocked = 2; next = THREEWAY_ENABLED; }
    if (g_maxKills > THREEWAY_ENABLED) { unlocked = 3; next = CIRCULAR_ENABLED; }
    if (g_maxKills > CIRCULAR_ENABLED) { unlocked = 4; next = TRI_ENABLED; }
    // neu_swarm2_40 pone aquí nextWeapon = 're' (una cadena: el menú enseña
    // "NEXT re"). Se porta con la marca NEXT_WEAPON_DONE, que el menú pinta así.
    if (g_maxKills > TRI_ENABLED)      { unlocked = 5; next = NEXT_WEAPON_DONE; }

    g_weaponsUnlocked = unlocked;
    g_nextWeapon = next;
}

// generaPaquete(): se llama con cada cabeza muerta, desde su posición.
// El original escala: hasta 16 paquetes crea paquetes; luego una bomba, luego
// naves réplica, luego rayos. Esos tres power-ups están pendientes, así que
// por ahora sólo salen paquetes hasta el tope.
static void generaPaquete(fix16 x, fix16 y)
{
    Pow_generate(x, y);
}

// ---------------------------------------------------------------------------
// Oleadas de MonsterLite (startGame, S/gameControl.cs:457-685)
//
// Sustituyen a generaMalos() del GML, que creaba enemigos al morir cada
// cabeza. Aquí, cada 20 f:
//
//     int index = globales.kills % 40;
//     int r = Random.Range (0, index);     // entero: [0, index), 0 si index <= 1
//     switch (r) { ... }
//
// globales.kills es la puntuación (g_score). El valor máximo de r es 38: las
// entradas 39 y 40 del switch son inalcanzables y se copian sólo por
// completar la tabla. La 15 no existe en el switch (no crea nada).
//
// Por entrada: arañas, gusanos y gusanos extra si level > 5 (oleadas 27, 31
// y 36). El orden araña/gusano dentro de una oleada no importa: los topes de
// cada tipo son independientes.
//
// Las oleadas de power-ups (3 bomba 1/6, 4 rayos 1/7, 6/16/19/20/21 nave
// holo) y el texto "!!!" de la 27 son de los bloques 7 y 9: de momento no
// crean nada. Los power-ups del GML siguen saliendo por generaPaquete().
// ---------------------------------------------------------------------------
typedef struct
{
    u8 spiders;
    u8 snakes;
    u8 extraSnakes;     // además, si g_level > ML_WAVE_EXTRA_LEVEL
} MLWave;

static const MLWave mlWaves[ML_WAVE_MOD + 1] =
{
    { 0, 2, 0 },    //  0
    { 0, 2, 0 },    //  1
    { 1, 1, 0 },    //  2
    { 0, 0, 0 },    //  3  bomba (bloque 7)
    { 0, 0, 0 },    //  4  rayos (bloque 7)
    { 0, 2, 0 },    //  5
    { 0, 0, 0 },    //  6  nave holo (bloque 7)
    { 0, 2, 0 },    //  7
    { 0, 1, 0 },    //  8
    { 0, 2, 0 },    //  9
    { 1, 1, 0 },    // 10
    { 1, 2, 0 },    // 11
    { 1, 2, 0 },    // 12
    { 2, 0, 0 },    // 13
    { 2, 1, 0 },    // 14
    { 0, 0, 0 },    // 15  no hay caso
    { 0, 0, 0 },    // 16  nave holo (bloque 7)
    { 1, 1, 0 },    // 17
    { 2, 1, 0 },    // 18
    { 0, 0, 0 },    // 19  nave holo (bloque 7)
    { 0, 0, 0 },    // 20  nave holo (bloque 7)
    { 0, 0, 0 },    // 21  nave holo (bloque 7)
    { 2, 1, 0 },    // 22
    { 2, 1, 0 },    // 23
    { 1, 1, 0 },    // 24
    { 2, 1, 0 },    // 25
    { 1, 1, 0 },    // 26
    { 2, 1, 2 },    // 27  "!!!" (bloque 9)
    { 1, 0, 0 },    // 28
    { 2, 1, 0 },    // 29
    { 1, 0, 0 },    // 30
    { 1, 2, 2 },    // 31
    { 2, 1, 0 },    // 32
    { 1, 2, 0 },    // 33
    { 2, 0, 0 },    // 34
    { 1, 1, 0 },    // 35
    { 2, 2, 2 },    // 36
    { 1, 0, 0 },    // 37
    { 2, 0, 0 },    // 38
    { 2, 0, 0 },    // 39  inalcanzable
    { 2, 0, 0 },    // 40  inalcanzable
};

// createSpider() de MonsterLite no tiene tope; el port mantiene el de 8
// (MAX_SPIDERS, decisión del usuario, Q4). Enemy_spawn ya no hace nada si
// el pool está lleno.
static void mlCreateSpider(void)
{
    Enemy_spawn();
}

// createSnake() (S/enemyController.cs:141-157): sólo crea si hay menos
// cabezas que maxNumSnakes = (level / 10) * 3 + 2, recortado al pool del port.
//
// MonsterLite tiene un solo tipo de gusano y el port mantiene los dos del GML
// (Q2). Qué tipo sale no lo dice MonsterLite; se toma el criterio del GML, el
// de generaMalos(): tipo 1 (12 eslabones) hasta llenar su tope y, si está
// lleno, tipo 2 (10). Con el tope de 2 de los niveles 1-9 sólo salen de tipo
// 1; el de tipo 2 aparece desde el nivel 10 (y con la bomba y los rayos del
// GML, que lo crean ellos). Ver docs/PORT_AUDIT_GML.md, "MonsterLite".
static u16 mlSnakeCap(void)
{
    const u16 cap = ((g_level / 10) * ML_SNAKES_PER_10LV) + ML_SNAKES_BASE;

    return (cap > MAX_SNAKES) ? MAX_SNAKES : cap;
}

static void mlCreateSnake(void)
{
    const u16 n1 = Snake_countType(SNAKE_TYPE_1);
    const u16 n2 = Snake_countType(SNAKE_TYPE_2);

    if ((n1 + n2) >= mlSnakeCap()) return;

    if (n1 < MAX_CABEZA_1)      Snake_spawn(SNAKE_TYPE_1);
    else if (n2 < MAX_CABEZA_2) Snake_spawn(SNAKE_TYPE_2);
}

static void mlWave(void)
{
    const u16 index = (u16)(g_score % ML_WAVE_MOD);
    // Random.Range(0, index) con enteros: el extremo superior no entra, y con
    // index <= 0 devuelve el mínimo.
    const u16 r = (index > 0) ? (random() % index) : 0;
    const MLWave *w = &mlWaves[r];
    u16 i;
    u16 snakes = w->snakes;

    if (g_level > ML_WAVE_EXTRA_LEVEL) snakes += w->extraSnakes;

    for (i = 0; i < w->spiders; i++) mlCreateSpider();
    for (i = 0; i < snakes; i++) mlCreateSnake();
}

bool Game_levelUpgrade(void)
{
    // "if (kills / factorLevel > level)": con enteros, kills / f > level es
    // lo mismo que kills >= f * (level + 1), que evita dividir en cada frame.
    // La división sólo se hace cuando de verdad sube.
    if (g_score < (g_factorLevel * (u32)(g_level + 1))) return FALSE;

    g_level = (u16)(g_score / g_factorLevel);
    g_factorLevel <<= 1;
    return TRUE;
}

// instance_nearest(x, y, oMalo): el jugador apunta al enemigo más cercano,
// sean arañas o cabezas de gusano (todos descienden de oMalo).
//
// [FASE 5] oCorpo también desciende de oMalo, así que en el original el
// apuntado cuenta además los segmentos del gusano, que son invulnerables:
// la nave puede quedarse disparando a un segmento. Aquí todavía sólo entran
// arañas y cabezas.
static bool closestThreat(s16 x, s16 y, s16 *outX, s16 *outY)
{
    // Un solo recorrido con la mejor distancia arrastrada: antes cada módulo
    // buscaba su mínimo por su cuenta y luego se comparaban los dos, lo que
    // obligaba a recorrer los 52 eslabones del gusano siempre. Ahora las
    // arañas van primero (son ocho y suelen estar cerca) y con su distancia
    // en la mano el gusano se descarta entero por su caja envolvente.
    s32 best = 0x7FFFFFFF;
    bool found = Enemy_closest(x, y, outX, outY, &best);

    if (Snake_closest(x, y, outX, outY, &best)) found = TRUE;

    return found;
}

void Game_init(void)
{
    // El pool de sprites se reparte una sola vez al arrancar (SPR_addSprite
    // no se llama en caliente), igual que el resto de los módulos. La paleta
    // PAL1 la carga main() y la comparten todos los sprites.
    // 32 direcciones en ROM, subidas por DMA al girar (inc/vramspr.h).
    playerSprite = VStream_add(&playerStream, &playerShip, PAL1);

    HUD_initGraphics();

    // Los tiles del menú (iconos de arma, logo y barra) se suben aquí: sin
    // esta llamada el menú pintaba su mapa de tiles apuntando a VRAM sin
    // cargar y no se veía ni un icono. Estaba escrita y sin llamar desde la
    // FASE 8.
    Menu_init();

    Enemy_initAll();
    Snake_initAll();
    Proj_initAll();
    // El orden de reparto es el orden de la tabla de sprites del VDP, y lo
    // que no cabe en las 80 entradas se cae por el final: primero lo que hay
    // que ver siempre (nave, enemigos, disparos) y al final los efectos.
    Beam_init();
    Effect_initAll();
    Debris_initAll();
    Flyer_initAll();
    Pow_initAll();
    // Los puntos de las líneas, los últimos: por debajo de todo y los
    // primeros en caer si una línea de pantalla se llena (ver src/beam.c).
    Beam_initDots();
    // Todo repartido: si no cabe en la VRAM, mejor parar aquí que pintar basura.
    VTile_check();
    // HUD_init() no va aquí: pinta BULLETS y KILLS, y el original no dibuja
    // el HUD mientras existe oMenu, o sea tampoco en el splash. Lo llama
    // Game_start.

    g_maxKills = 0;
    g_lastScore = 0;
    checkWeapon();

    // El original NO empieza jugando. El room _1 trae ya colocados un oMenu y
    // un oSplashS, y global.started es false: lo primero que se ve es la
    // pantalla de titulo sobre los cinco agujeros, esperando a que metas un
    // credito. La partida sólo arranca cuando oMenu ve el boton de disparo
    // con creditos, que es lo que hace Menu_update() aqui.
    //
    // Los agujeros se pintan ya, porque en el room existen desde el principio
    // (depth 999, al fondo) y se ven detras del titulo.
    // El fondo del campo también: se ve detrás del título y no se quita nunca
    // (el menú se dibuja encima del room).
    Bg_draw();
    Hole_draw();
    Player_init(&player);
    sprHide(playerSprite);

    g_state = GAME_SPLASH;
}

void Game_start(void)
{
    // La vista se recentra antes de repintar: el plano A va desplazado si la
    // partida anterior murió a mitad de una sacudida (ver inc/shake.h).
    Shake_stop();

    // El fondo y los agujeros los deja repuestos Menu_leave (el texto del
    // menú los pisa en el plano A y la banda en el B).

    Enemy_clearAll();
    Snake_clearAll();
    // startGame() sólo hace "with oMalo instance_destroy()": las naves
    // réplica y lo que haya en el suelo siguen de la partida anterior.
    Proj_clearKeepTris();
    Effect_clearAll();
    Debris_clearAll();
    Flyer_clearAll();
    Beam_clear();
    Pow_replaceAll();

    // La nave no existe hasta los 40 f (startGame de MonsterLite espera 0,8 s
    // antes de instanciarla): se queda aparcada y playerEnter() la pone.
    playerIn = FALSE;
    sprHide(playerSprite);
    startFrames = 0;
    waveTimer = 0;

    // toGame(): "globales.kills = 0; globales.level = 1; factorLevel = 1"
    g_score = 0;
    g_level = 1;
    g_factorLevel = 1;
    g_kills = 0;
    // oPlayer/Create_0: "global.playerBulletsCounter = 0". Cada partida
    // empieza sin munición, así que de salida dispara la bala por defecto.
    g_playerBullets = 0;

    // El arma NO se reinicia aquí: startGame() del original no toca
    // global.weapon_index, que sólo se pone a 0 en scrInit al arrancar la
    // máquina. Es lo que hace que el arma elegida en el menú valga para la
    // partida siguiente; ponerla a cero aquí dejaba el selector sin efecto y
    // se jugaba siempre con el bloque blanco.

    // El primer gusano ya no sale aquí, como en el startGame() del GML: en
    // MonsterLite sale a los 90 f (ver playUpdate).

    g_state = GAME_PLAY;
    // Palabras y cifras en el mismo frame: con sólo HUD_init las cifras
    // llegaban un frame después que las palabras.
    HUD_init();
    HUD_update();

#if (PORT_STRESS || PORT_BENCH)
    // Arnés de medición: ver inc/bench.h.
    Bench_fillArena();
#endif
#if PORT_BENCH
    Bench_run();
#endif
}

void Game_killPlayer(void)
{
    // mataPlayer(): "instance_create(x, y, oMuerte)" y luego
    // "with oPlayer instance_destroy()": la nave desaparece mientras dura el
    // menú, pero antes deja el fogonazo. oMuerte/Create_0 suena morteSnd.
    Effect_death(player.e.x, player.e.y);
    sprHide(playerSprite);
    Audio_sfx(SFX_MORTE);

    // La sacudida vive en oPlayer: sin jugador el original deja de
    // actualizarla, y aquí además se recentra la vista para que el menú no
    // salga torcido.
    Shake_stop();

    // mataPlayer() sólo destruye oPlayer. Los enemigos siguen vivos y
    // moviéndose durante el menú (sin jugador al que perseguir) hasta que
    // startGame() los borra al empezar la partida siguiente; el fondo y los
    // agujeros siguen puestos. Tampoco se borran los paquetes ni las naves
    // réplica: pasan a la partida siguiente.
    Proj_clearKeepTris();
    Debris_clearAll();
    Flyer_clearAll();
    Beam_clear();
    Pow_replaceAll();

    checkWeapon();
    Menu_enter();
    g_state = GAME_MENU;
}

// startGame() de MonsterLite, a los 0,8 s: "Instantiate (player, highPos, ...)".
// La posición sigue siendo el centro del GML hasta el bloque 2 (Q6).
static void playerEnter(void)
{
    // startGame() del GML: el jugador aparece en el centro del room
    // (global.game_width*0.5, global.game_height*0.5).
    Player_init(&player);
    sprPos(playerSprite,
                    F2I(player.e.x) + PLAYER_SPR_OFF_X,
                    F2I(player.e.y) + PLAYER_SPR_OFF_Y);
    // El frame se sube ya: el sprite acaba de dejar de estar aparcado.
    VStream_frame(&playerStream, playerSprite,
                  playerShip.animations[Player_facing(&player)]->frames[0]);
    playerIn = TRUE;
}

// La corrutina startGame() de MonsterLite: 40 f sin nave, a los 90 f un
// gusano y, en el mismo frame (el bucle no espera antes de su primera
// vuelta), la primera oleada; después una cada 20 f. Devuelve FALSE mientras
// la nave todavía no existe.
static bool startSequence(void)
{
    if (startFrames < ML_START_SNAKE_F)
    {
        startFrames++;

        if (startFrames == ML_START_PLAYER_F) playerEnter();

        if (startFrames == ML_START_SNAKE_F)
        {
            mlCreateSnake();
            waveTimer = 0;
        }
    }

    if (startFrames >= ML_START_SNAKE_F)
    {
        if (waveTimer == 0)
        {
            mlWave();
            waveTimer = ML_WAVE_PERIOD_F;
        }
        waveTimer--;
    }

    return playerIn;
}

// Los 40 f del principio, sin nave: los bichos (sólo los hay con el arnés de
// estrés o si los deja algo de la partida anterior) siguen sin objetivo, y
// los efectos y el HUD siguen corriendo.
static void waitingUpdate(void)
{
    Shake_update();
    Enemy_updateAll(FALSE, 0, 0);
    Snake_updateAll(FALSE, 0, 0);
    Pow_updateAll();
    Effect_updateAll();
    Flyer_updateAll();
    HUD_update();
}

static void playUpdate(u16 pad)
{
    s16 px, py;
    s16 targetX = 0, targetY = 0;

    if (!startSequence())
    {
        waitingUpdate();
        return;
    }

    // La cámara de stShake. En el original se actualiza al final del Step del
    // jugador; aquí va al principio del frame para que el plano y los sprites
    // usen el mismo desplazamiento (ver inc/shake.h).
    Shake_update();

    // Orden del original: la IA de los enemigos está en su evento Begin Step
    // (Step_1), antes del Step del jugador (Step_0), así que los enemigos
    // apuntan a la posición del jugador del frame anterior.
    PROF_START(PROF_ENEMY);
    Enemy_updateAll(TRUE, player.e.x, player.e.y);
    PROF_STOP(PROF_ENEMY);

    PROF_START(PROF_SNAKE);
    Snake_updateAll(TRUE, player.e.x, player.e.y);
    PROF_STOP(PROF_SNAKE);

    PROF_START(PROF_PLAYER);
    Player_update(&player, pad);

    px = F2I(player.e.x);
    py = F2I(player.e.y);


    // oPlayer/Step_0: el apuntado sólo se actualiza si hay algún enemigo
    // ("if instance_exists(oMalo)"), pero el disparo va aparte y es
    // incondicional: sin enemigos la nave sigue disparando al último rumbo.
    {
        const bool hasTarget = closestThreat(px, py, &targetX, &targetY);

        if (hasTarget) Player_aim(&player, FIX16(targetX), FIX16(targetY));

        // El pivote y las réplicas necesitan jugador, input y objetivo.
        Proj_setPlayer(player.e.x, player.e.y, pad, hasTarget,
                       FIX16(targetX), FIX16(targetY));
    }

    if (Player_tickFire(&player))
    {
        // oPlayer/Step_0: con munición dispara el arma en curso y gasta una.
        //
        // OJO con dónde está cada cosa en el original: el switch de las armas
        // puede no crear nada (el PIVOT no hace nada si el pivote ya está
        // puesto, el PARA si ya hay una bala viva), pero la MUNICIÓN y la
        // CADENCIA se gastan igual, porque están FUERA del switch:
        //
        //     switch (global.weapon_index) { ... case 3: if !instance_exists
        //          (oPivot) {instance_create(x,y,oPivot)} break; ... }
        //     if (global.weapon_index == 2) controlTime = 12 else controlTime = 6;
        //     if global.playerBulletsCounter >= 0 {global.playerBulletsCounter -= 1}
        //
        // El port lo tenía condicionado a que el arma hubiera creado algo, y
        // eso ROMPÍA LA PARTIDA con el PIVOT: el pivote no se puede volver a
        // crear, así que no se gastaba munición nunca, la munición no llegaba
        // nunca a cero y por tanto no entraba la bala por defecto. En cuanto
        // los tres satélites morían contra un enemigo, el jugador se quedaba
        // SIN NINGÚN DISPARO: no podía matar, no morían cabezas, y
        // generaMalos() no volvía a correr. El juego se quedaba vacío.
        if (g_playerBullets > 0)
        {
            Proj_fireWeapon(g_weaponIndex, player.e.x, player.e.y, player.e.angle);
            g_playerBullets--;
            player.controlTime = (g_weaponIndex == 2) ? SHOOT_INT_3WAY : SHOOT_INT_WEAPON;
            // oPlayer/Step_0: stShake(4, 5, 0.2) tras cualquier arma
            Shake_shot();
        }

        // Y a continuación, en el MISMO frame, el original comprueba nivel de
        // munición otra vez: si acaba de quedarse a cero, también sale la bala
        // por defecto. Portado tal cual.
        if (g_playerBullets == 0)
        {
            Proj_fireThin(player.e.x, player.e.y, player.e.angle);
            player.controlTime = SHOOT_INT_DEFAULT;
            // oPlayer/Step_0: stShake(1, 1, 0.1)
            Shake_thinShot();
        }
    }

    // Recogida de paquetes y power-ups (las cuatro colisiones de oPlayer)
    {
        PowerPickup got;

        if (Pow_collect(px + PLAYER_BOX_OX, py + PLAYER_BOX_OY,
                        PLAYER_BOX_W, PLAYER_BOX_H, g_weaponIndex, &got))
        {
            switch (got.type)
            {
                case POW_PACK:
                    // oPaquete/Collision_oPlayer: powUpSnd
                    g_playerBullets += got.ammo;
                    Audio_sfx(SFX_POWUP);
                    // puntoToHUD(0), que en el original sale DOS veces: lo
                    // llaman el manejador del paquete y el del jugador.
                    Flyer_spawn(got.x, got.y, 0, g_playerBullets);
                    break;

                case POW_BOOM:
                    // Collision_oBoomPowUp pinta una explosión en cada oMalo y
                    // les pone hp = 0, que NO mata a nadie: fuera del impacto
                    // de bala ese hp no se comprueba en ninguna parte. Lo que
                    // limpia la pantalla es el Animation End del
                    // oExplosionCruz que crea, dos frames después
                    // (sprExplosion son 2 frames a 1 por frame):
                    //
                    //   with oCorpo/oCu2/oCu1/oCabeza1/oCabeza2/oMalo
                    //        instance_destroy()
                    //   repeat(2) instance_create(-10, -10, oCabeza2b)
                    //
                    // Destruye sin pasar por checkMorte(), así que no suma
                    // kills ni genera paquetes ni oleada, igual que el rayo, y
                    // deja dos gusanos de tipo 2 recién nacidos.
                    Enemy_clearAll();
                    Snake_clearAll();
                    Effect_spawn(player.e.x, player.e.y);
                    // oExplosionCruz/Draw_0: el blanco al 50 % que tapa la
                    // pantalla entera durante 60 frames.
                    Effect_bombFlash();
                    // oExplosionCruz/Create_0: stShake(10, 150, .6)
                    Shake_cruz();
                    Snake_spawn(SNAKE_TYPE_2);
                    Snake_spawn(SNAKE_TYPE_2);
                    // Collision_oBoomPowUp: aranamorteSnd + bombaSnd + risaSnd
                    Audio_sfx(SFX_BOMBA);
                    Audio_sfx(SFX_RISA);
                    break;

                case POW_REPLICA:
                    // Collision_oNaveReplicaPowUp: "instance_create(other.x,
                    // other.y, oTri)"
                    // Collision_oNaveReplicaPowUp: repeteaSnd
                    Proj_spawnTri(got.x, got.y);
                    Audio_sfx(SFX_REPETEA);
                    break;

                case POW_RAYS:
                    // Collision_oRayosPowUp: "if (global.explosions)
                    // instance_create(other.x, other.y, oRay)"
                    // Collision_oRayosPowUp: rayosSnd
                    Proj_fireWeapon(6, got.x, got.y, player.e.angle);
                    Audio_sfx(SFX_RAYOS);
                    break;
            }

        }
    }

    // checkMorte()/Collision_oBulletS: cada muerte suma a global.kills y sube
    // global.lastScore si lo supera.
    PROF_STOP(PROF_PLAYER);

    u16 snakeKills = 0;
    PROF_START(PROF_PROJ);
    u16 killed = Proj_update(&snakeKills);
    // La metralla de oEstallido mata igual que una bala (Collision_oDestro).
    killed += Debris_updateAll(&snakeKills);
    PROF_STOP(PROF_PROJ);

    if (killed)
    {
        // MonsterLite: "globales.kills += 1 * globales.level" por baja. El
        // original da 2 * level a la araña muerta por bala fina
        // (S/bulletThinSrc.cs:49) y level con todo lo demás; por decisión del
        // usuario (Q5) se corrige y TODA baja suma level, también la metralla
        // del GML. Divergencia deliberada, ver docs/PORT_AUDIT_GML.md.
        g_score += (u32)killed * g_level;
        if (g_lastScore < g_score) g_lastScore = g_score;
        g_kills += killed;

        // checkMorte() del GML: cada cabeza muerta encadena morteGusano() +
        // generaPaquete(). generaMalos() ya no: las oleadas van por tiempo.
        while (snakeKills--)
        {
            // morteGusano(): puntoToHUD(1). El número volador es de 3 cifras
            // (FLYER_DIGITS): enseña las bajas, como hasta ahora, no la
            // puntuación, que pasa de 999 enseguida.
            Flyer_spawn(Snake_lastKillX(), Snake_lastKillY(), 1, g_kills);
            generaPaquete(Snake_lastKillX(), Snake_lastKillY());
        }

        checkWeapon();
    }

    // levelUpgrade() corre en cada Update de gameControl.
    Game_levelUpgrade();

    // oMalo/Collision_oPlayer -> mataPlayer(): vale cualquier enemigo,
    // incluidos los segmentos del gusano.
    if (Enemy_collides(px + PLAYER_BOX_OX, py + PLAYER_BOX_OY, PLAYER_BOX_W, PLAYER_BOX_H) ||
        Snake_collides(px + PLAYER_BOX_OX, py + PLAYER_BOX_OY, PLAYER_BOX_W, PLAYER_BOX_H))
    {
#if !PORT_STRESS || defined(PORT_STRESS_MORTAL)
        Game_killPlayer();
        return;
#endif
    }

    sprPos(playerSprite, px + PLAYER_SPR_OFF_X, py + PLAYER_SPR_OFF_Y);
    // rescomp convierte cada FILA de la hoja en una animación distinta, así
    // que la rotación se elige con SPR_setAnim: SPR_setFrame indexa dentro de
    // la animación y con un índice fuera de rango SGDK sigue adelante y
    // dereferencia basura.
    {
        const u16 rot = Player_facing(&player);

        VStream_frame(&playerStream, playerSprite, playerShip.animations[rot]->frames[0]);
    }
    Pow_updateAll();

    // object0/object1 y la línea azul a las réplicas.
    Beam_update(g_weaponIndex, g_playerBullets, player.e.x, player.e.y, player.e.angle);

    PROF_START(PROF_EFFECT);
    Effect_updateAll();
    Flyer_updateAll();
    PROF_STOP(PROF_EFFECT);

    PROF_START(PROF_HUD);
    HUD_update();
    PROF_STOP(PROF_HUD);
    PROF_FRAME();
}

void Game_update(void)
{
#if PORT_VRAM_REPORT
    {
        char vb[24];
        sprintf(vb, "VRAM %u/%u", VTile_used(), (u16)TILE_USER_LENGTH);
        VDP_drawText(vb, 0, 28);
        VDP_showFPS(FALSE, 20, 28);
        VDP_showCPULoad(28, 28);
        sprintf(vb, "SAT %u", Spr_satUsed());
        VDP_drawText(vb, 11, 28);
    }
#if PORT_BENCH
    // Frames que tardan BENCH_ITERS llamadas de cada modulo (ver inc/bench.h).
    {
        char bb[40];
        sprintf(bb, "EU%u SU%u EC%u SC%u PS%u CL%u", benchFrames[BENCH_ENEMY_UPDATE],
                benchFrames[BENCH_SNAKE_UPDATE], benchFrames[BENCH_ENEMY_COLLIDES],
                benchFrames[BENCH_SNAKE_COLLIDES], benchFrames[BENCH_PROJ_SCAN],
                benchFrames[BENCH_CLOSEST]);
        VDP_drawText(bb, 0, 24);
        sprintf(bb, "SP%u HU%u PC%u PO%u PA%u AI%u AD%u", benchFrames[BENCH_SPR_UPDATE],
                benchFrames[BENCH_HUD_UPDATE], benchFrames[BENCH_POW_COLLECT],
                benchFrames[BENCH_SPR_SETPOS], benchFrames[BENCH_SPR_SETANIM],
                benchFrames[BENCH_ENTITY_AI], benchFrames[BENCH_ENTITY_ADVANCE]);
        VDP_drawText(bb, 0, 25);
    }
#endif
#endif

    const u16 pad = JOY_readJoypad(JOY_1);

    // El cambio de fondo pedido desde el menú (tres frames, ver src/bg.c).
    Bg_update();
    // La caída del tick de la cuenta del menú.
    Audio_update();

#if PORT_DEBUG_BG
    // El disparo es automático, así que en partida los botones están libres.
    if (g_state == GAME_PLAY)
    {
        const u16 pressed = pad & ~debugBgPad;

        if (pressed & BUTTON_C)
        {
            if (++debugBgIdx >= (sizeof(debugBg) / sizeof(debugBg[0]))) debugBgIdx = 0;
            PAL_setColor(0, debugBg[debugBgIdx]);
        }
    }
    debugBgPad = pad;
#endif

    if (g_state == GAME_PLAY)
    {
        // Depurador de gráficos (sólo con PORT_DEBUG_GFX, ver inc/gfxdebug.h).
        GfxDebug_input(pad);
        playUpdate(pad);
        return;
    }

    // mataPlayer() crea el oMuerte y DESPUÉS abre el menú: el fogonazo sigue
    // vivo encima de él, igual que las explosiones que hubiera en marcha
    // (nada las destruye). Por eso los efectos se siguen actualizando fuera
    // de la partida.
    Effect_updateAll();
    Flyer_updateAll();

    // Los bichos siguen a lo suyo sin jugador: con hasPlayer a FALSE la IA
    // no pasa a atacar (ver Entity_updateAI). En el splash no hay ninguno.
    Enemy_updateAll(FALSE, 0, 0);
    Snake_updateAll(FALSE, 0, 0);

    // Splash e interludio comparten oMenu: el mismo objeto dibuja uno u otro
    // según global.isMenu.
    if (Menu_update(pad, g_state == GAME_SPLASH))
    {
        Menu_leave();
        Game_start();
    }
}
