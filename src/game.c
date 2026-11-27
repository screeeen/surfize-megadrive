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
#include "weapon.h"
#include "mlpow.h"
#include "achieve.h"
#include "trail.h"

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
u16   g_weaponsAvail;
u32   g_coins;
u32   g_coinsEarned;
u16   g_ammo[WPN_COUNT];
u32   g_nextWeapon;
u16   g_weaponsBought;
u32   g_bestAtStart;
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

// El tiempo parado sin B (Q8): frames de partida que lleva el "ARENA!" (la
// gracia de HUD.counter, ver ML_FREEZE_GRACE_F).
static u16 graceFrames;

// El hitstop (Q17): los Thread.Sleep de MonsterLite que quedan por pagar, en
// milisegundos. 100 al tocar la bomba o los rayos y 30 por cada baja con
// bala; cada frame congelado paga 20 (ver ML_KILL_FREEZE_MS en inc/arena.h).
static u16 hitstopMs;

// La muerte de MonsterLite (bloque 12): frames desde que murió la nave. Los
// ML_DEATH_SLOW_F primeros son la cámara lenta; después, la sacudida grande y
// el game over.
static u16 dyingFrames;

// La nave en rojo durante la cámara lenta: el frame que se ve, con cada
// color de PAL1 cambiado por el más parecido a (r, 0, 0), que es lo que hace
// SpriteRenderer.color = Color.red (multiplica). 3x3 tiles.
#define SHIP_TILES      9
static u32 redShip[SHIP_TILES * 8];
static u8  redMap[16];

// El aviso del tiempo parado ("HOLD A/B/C") y el "ARENA!" los pinta el HUD
// (src/hud.c, bloque 9).

// Qué armas se pueden elegir en el menú.
//
// Decisión del usuario al cerrar el bloque 2: las armas de MonsterLite
// (WBLOCK, LASER, TWAY, CIRCULAR, MOIRE) se compran en la tienda GEAR
// (bloque 5) y WBLOCK es gratis (W/WeaponsController.cs: weaponPurchased[0]
// = true). Aquí entran las compradas (g_weaponsBought).
//
// PIVOT y PARA siguen el desbloqueo por récord del GML, stCheckWeapon(), que
// hay que leer con cuidado porque tiene un desfase de uno que NO es un
// despiste del port:
//
//     for (g = 0; g < global.maxKills; g += 1) {
//         if (g >= laserEnabled) { ...añade laser...; nextWeapon = threewayEnabled }
//         ...
//     }
//
// El mayor valor que toma g es maxKills - 1, así que "g >= umbral" pide
// maxKills > umbral. En el GML el PIVOT era la cuarta arma (umbral
// CIRCULAR_ENABLED, 80) y el PARA la quinta (TRI_ENABLED, 100); LASER (40) y
// 3WAY (60) ya no van por récord.
//
// nextWeapon arranca en oWeaponBox/Create_0. En neu_swarm2_40, la versión
// que se porta, vale 40: el primer umbral. Ahora el primero que queda es el
// del PIVOT.
#define NEXT_WEAPON_NONE CIRCULAR_ENABLED

void Game_checkWeapons(void)
{
    u16 avail = g_weaponsBought & WPN_ML_MASK;
    u32 next = NEXT_WEAPON_NONE;

    if (g_maxKills > CIRCULAR_ENABLED) { avail |= 1 << WPN_PIVOT; next = TRI_ENABLED; }
    // neu_swarm2_40 pone aquí nextWeapon = 're' (una cadena: el menú enseña
    // "NEXT re"). Se porta con la marca NEXT_WEAPON_DONE, que el menú pinta así.
    if (g_maxKills > TRI_ENABLED)      { avail |= 1 << WPN_PARA; next = NEXT_WEAPON_DONE; }

    g_weaponsAvail = avail;
    g_nextWeapon = next;
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
// holo) van aparte, en mlPowWave (bloque 7). El texto "!!!" de la 27 lo
// pinta el HUD (bloque 9). generaPaquete() del GML ya no se usa (bloque
// 4): por cada baja con disparo sale moneda o paquete (Pow_drop).
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
    { 0, 0, 0 },    //  3  bomba (mlPowWave)
    { 0, 0, 0 },    //  4  rayos (mlPowWave)
    { 0, 2, 0 },    //  5
    { 0, 0, 0 },    //  6  nave holo (mlPowWave)
    { 0, 2, 0 },    //  7
    { 0, 1, 0 },    //  8
    { 0, 2, 0 },    //  9
    { 1, 1, 0 },    // 10
    { 1, 2, 0 },    // 11
    { 1, 2, 0 },    // 12
    { 2, 0, 0 },    // 13
    { 2, 1, 0 },    // 14
    { 0, 0, 0 },    // 15  no hay caso
    { 0, 0, 0 },    // 16  nave holo (mlPowWave)
    { 1, 1, 0 },    // 17
    { 2, 1, 0 },    // 18
    { 0, 0, 0 },    // 19  nave holo (mlPowWave)
    { 0, 0, 0 },    // 20  nave holo (mlPowWave)
    { 0, 0, 0 },    // 21  nave holo (mlPowWave)
    { 2, 1, 0 },    // 22
    { 2, 1, 0 },    // 23
    { 1, 1, 0 },    // 24
    { 2, 1, 0 },    // 25
    { 1, 1, 0 },    // 26
    { 2, 1, 2 },    // 27  "!!!" (HUD_waveAlert)
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
#if PORT_STRESS
    // El arnés de medida (tools/measure.sh) tiene que medir el CASO PEOR, que
    // es el pool lleno: 3 gusanos, como del nivel 10 en adelante. Con el tope
    // de MonsterLite del nivel en curso (2 hasta el nivel 9), las oleadas
    // dejaban de reponer el tercero en cuanto moría y la medida salía
    // optimista (540/600 frente a 484/600 con 3, bloque 1).
    return MAX_SNAKES;
#else
    const u16 cap = ((g_level / 10) * ML_SNAKES_PER_10LV) + ML_SNAKES_BASE;

    return (cap > MAX_SNAKES) ? MAX_SNAKES : cap;
#endif
}

static void mlCreateSnake(void)
{
    const u16 n1 = Snake_countType(SNAKE_TYPE_1);
    const u16 n2 = Snake_countType(SNAKE_TYPE_2);

    if ((n1 + n2) >= mlSnakeCap()) return;

    if (n1 < MAX_CABEZA_1)      Snake_spawn(SNAKE_TYPE_1);
    else if (n2 < MAX_CABEZA_2) Snake_spawn(SNAKE_TYPE_2);
}

// globales.getRandomPos(): Random.Range(0.1, 0.9) del ancho y del alto. La
// comparte la IA de los enemigos (WANDER).
static void mlRandomPos(fix16 *x, fix16 *y)
{
    Entity_mlRandomPos(x, y);
}

// Los enemigos sin jugador (los 40 f del arranque, el game over y la
// tienda): en ATTACK, enemyMov.move() va a por una nave holo si la hay
// (FindGameObjectWithTag("NaveProxy")) y, si no, sigue hacia su último
// destino. Se busca una vez por frame para todos.
static void enemiesNoPlayer(void)
{
    fix16 hx = 0, hy = 0;
    const bool holo = Proj_anyHolo(&hx, &hy);

    Enemy_updateAll(holo, hx, hy);
    Snake_updateAll(holo, hx, hy);

    // Y si la tocan, la destruyen (collideAndDie): sin esto, fuera de la
    // partida la holo era inmortal y pasaba así a la siguiente.
    if (holo) Proj_holoContact();
}

// Las oleadas de power-ups (S/gameControl.cs:477-571).
static void mlPowWave(u16 r)
{
    fix16 x, y;

    switch (r)
    {
        case 3:     // "if (Random.Range(0, 6) == 1) Instantiate(bomba, ...)"
            if ((random() % ML_BOMB_CHANCE) == 1)
            {
                mlRandomPos(&x, &y);
                Pow_spawnML(POW_BOOM, x, y);
            }
            break;

        case 4:     // "if (Random.Range(0, 7) == 1) Instantiate(rayoBomba, ...)"
            if ((random() % ML_RAYS_CHANCE) == 1)
            {
                mlRandomPos(&x, &y);
                Pow_spawnML(POW_RAYS, x, y);
            }
            break;

        case 6: case 16: case 19: case 20: case 21:
            // "if (FindGameObjectsWithTag("NaveProxy").Length < maxNaveProxy)"
            mlRandomPos(&x, &y);
            Proj_spawnHolo(x, y, FALSE);
            break;

        default:
            break;
    }
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

    // case 27: globales.OLEADA = "!!!" (S/gameControl.cs:597).
    if (r == ML_WAVE_ALERT) HUD_waveAlert();

    for (i = 0; i < w->spiders; i++) mlCreateSpider();
    for (i = 0; i < snakes; i++) mlCreateSnake();
    mlPowWave(r);
}

// Bajas que no vienen de un proyectil (la bomba y los rayos): clearEnemies()
// suma "globales.kills += 1 * globales.level" por cada una, como cualquier
// baja (Q5), y no suelta nada.
static void addKills(u16 n)
{
    if (n == 0) return;

    g_score += (u32)n * g_level;
    if (g_lastScore < g_score) g_lastScore = g_score;
    g_kills += n;
    // Bloque 10: los logros de puntuación (100, 1000, 10000).
    if (g_score >= g_achNextScore) Achieve_score(g_score);
    Game_checkWeapons();
}

// Las bajas de disparos y metralla de un frame: puntuación, logros, números
// voladores y desbloqueo de armas.
static void scoreKills(u16 killed, u16 snakeKills)
{
    // MonsterLite: "globales.kills += 1 * globales.level" por baja. El
    // original da 2 * level a la araña muerta por bala fina
    // (S/bulletThinSrc.cs:49) y level con todo lo demás; por decisión del
    // usuario (Q5) se corrige y TODA baja suma level, también la metralla
    // del GML. Divergencia deliberada, ver docs/PORT_AUDIT_GML.md.
    g_score += (u32)killed * g_level;
    if (g_lastScore < g_score) g_lastScore = g_score;
    g_kills += killed;
    // Bloque 10: los logros de puntuación.
    if (g_score >= g_achNextScore) Achieve_score(g_score);

    // checkMorte() del GML: cada cabeza muerta encadena morteGusano().
    // generaPaquete() ya no (bloque 4: lo que se suelta lo decide cada
    // disparo, ver Pow_drop) y generaMalos() tampoco (las oleadas van por
    // tiempo).
    while (snakeKills--)
    {
        // morteGusano(): puntoToHUD(1). El número volador es de 3 cifras
        // (FLYER_DIGITS): enseña las bajas, como hasta ahora, no la
        // puntuación, que pasa de 999 enseguida.
        Flyer_spawn(Snake_lastKillX(), Snake_lastKillY(), 1, g_kills);
    }

    Game_checkWeapons();
}

bool Game_levelUpgrade(void)
{
    // "if (kills / factorLevel > level)": con enteros, kills / f > level es
    // lo mismo que kills >= f * (level + 1), que evita dividir en cada frame.
    // La división sólo se hace cuando de verdad sube.
    if (g_score < (g_factorLevel * (u32)(g_level + 1))) return FALSE;

    g_level = (u16)(g_score / g_factorLevel);
    g_factorLevel <<= 1;
    // Bloque 10: los logros de los niveles 5, 10 y 15.
    Achieve_level(g_level);
    // globales.showNewLevel = true y ChangeLevelWait: el mensaje de nivel.
    // (setCameraLevelColor no se porta: Q16, ver docs/PORT_AUDIT_GML.md.)
    HUD_levelUp();
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
    //
    // Bloque 3 de MonsterLite: el objetivo es el de getClosest
    // (S/enemyController.cs:194), que recorre arañas y CABEZAS, no eslabones.
    // En el GML era instance_nearest sobre oMalo, eslabones incluidos; pero
    // ahora las balas atraviesan los eslabones, y apuntando al eslabón más
    // cercano casi nunca daban en la cabeza (medido: 0 bajas en 600 f con la
    // bala fina).
    s32 best = 0x7FFFFFFF;
    bool found = Enemy_closest(x, y, outX, outY, &best);

    if (Snake_closestHead(x, y, outX, outY, &best)) found = TRUE;

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
    // La estela de la nave (bloque 12), por debajo de casi todo.
    Trail_init();
    // Los puntos de las líneas, los últimos: por debajo de todo y los
    // primeros en caer si una línea de pantalla se llena (ver src/beam.c).
    // Además son la cola del pool que el montador se salta cuando no hay
    // ninguno a la vista (inc/spr.h).
    Beam_initDots();
    // Todo repartido: si no cabe en la VRAM, mejor parar aquí que pintar basura.
    VTile_check();
    // Ni en el pool de sprites: lo que no cabe no se ve (ver inc/spr.h).
    Spr_check();
    // HUD_init() no va aquí: pinta BULLETS y KILLS, y el original no dibuja
    // el HUD mientras existe oMenu, o sea tampoco en el splash. Lo llama
    // Game_start.

    g_maxKills = 0;
    g_lastScore = 0;
    // WBLOCK, gratis y siempre comprada; el arma de una instalación nueva.
    g_weaponsBought = 1 << WPN_WBLOCK;
    g_weaponIndex = WPN_WBLOCK;
    // Bloque 10: sin logros (en RAM, como el resto del progreso).
    Achieve_init();
    // El progreso (récord, saldo, armas compradas, arma equipada, sonido y
    // música) vive sólo en RAM y se pierde al apagar: decisión del usuario
    // del 2026-11-20, sin SRAM. Al encender, los valores de una partida nueva.
    Game_checkWeapons();

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

    // El rojo de cada color de la paleta de sprites, para la muerte.
    {
        u16 i, j;

        redMap[0] = 0;
        for (i = 1; i < 16; i++)
        {
            const s16 r = (spritePalette[i] >> 1) & 7;
            u16 best = 1;
            s16 bestD = 0x7FFF;

            for (j = 1; j < 16; j++)
            {
                const s16 dr = ((spritePalette[j] >> 1) & 7) - r;
                const s16 dg = (spritePalette[j] >> 5) & 7;
                const s16 db = (spritePalette[j] >> 9) & 7;
                const s16 d = (dr * dr) + (dg * dg) + (db * db);

                if (d < bestD) { bestD = d; best = j; }
            }
            redMap[i] = (u8)best;
        }
    }

    g_state = GAME_SPLASH;

    // menu.Start(): SoundManager.playArenaWoman() (S/menu.cs:27). El menú de
    // MonsterLite sólo se crea una vez, al cargar la escena (initMenu, en el
    // Start de gameControl): la voz suena al salir el splash tras el logo, y
    // no al volver del game over (que en MonsterLite no vuelve al menú).
    Audio_sfx(SFX_ML_ARENA);
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
    MLPow_clear();
    hitstopMs = 0;
    Trail_clear();
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
    graceFrames = 0;

    // toGame(): "globales.kills = 0; globales.level = 1; factorLevel = 1"
    g_score = 0;
    g_level = 1;
    g_factorLevel = 1;
    g_kills = 0;
    // Munición del arma elegida. En MonsterLite hay un contador por arma
    // (WeaponsController.bullets[5]) que no se reinicia al empezar partida:
    // lo que sobra pasa a la siguiente. Q10: se conserva hasta apagar la
    // consola (g_ammo está en RAM, a cero al encender), como todo el progreso.
    // (El GML la ponía a 0 en cada partida.)
    g_playerBullets = g_ammo[g_weaponIndex];
    // gameControl.toGame: "CoinsManager.earnedCoins = 0". El saldo no se
    // toca: dura hasta apagar la consola (sin SRAM).
    g_coinsEarned = 0;
    // El récord de antes de la partida: el game over dice NEW RECORD! si se
    // supera (gameOverSrc, 257-264).
    g_bestAtStart = g_lastScore;

    // El arma NO se reinicia aquí: startGame() del original no toca
    // global.weapon_index, que sólo se pone a 0 en scrInit al arrancar la
    // máquina. Es lo que hace que el arma elegida en el menú valga para la
    // partida siguiente; ponerla a cero aquí dejaba el selector sin efecto y
    // se jugaba siempre con el bloque blanco.

    // El primer gusano ya no sale aquí, como en el startGame() del GML: en
    // MonsterLite sale a los 90 f (ver playUpdate).

    g_state = GAME_PLAY;
    // "ARENA!" puesto y ningún otro mensaje; palabras y cifras en el mismo
    // frame: con sólo HUD_init las cifras llegaban un frame después que las
    // palabras.
    HUD_newGame();
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

// La nave en rojo: el frame que tiene subido, recoloreado y subido encima.
static void tintShipRed(void)
{
    const TileSet *ts = playerSprite->frame->tileset;
    const u32 *src = ts->tiles;
    const u16 n = (ts->numTile < SHIP_TILES) ? ts->numTile : SHIP_TILES;
    u16 i;

    for (i = 0; i < (n * 8); i++)
    {
        const u32 v = src[i];
        u32 o = 0;
        u16 k;

        for (k = 0; k < 32; k += 4) o |= ((u32)redMap[(v >> k) & 0xF]) << k;
        redShip[i] = o;
    }

    DMA_queueDma(DMA_VRAM, redShip, playerStream.base * 32, n * 16, 2);
    // El frame de verdad se vuelve a subir en la partida siguiente: ver
    // VStream_invalidate en dyingUpdate. Aquí no: el frame de la nave tiene
    // varias piezas y el montador de sprites lo lee mientras se vea.
}

void Game_killPlayer(void)
{
    // La munición que sobra se queda en su arma (Q10, ver Game_start).
    g_ammo[g_weaponIndex] = g_playerBullets;

    // Bloque 10: terminar una partida. Su aviso sale ya en la cámara lenta
    // (HUD_menuTick, en dyingStep) y sigue en el game over.
    Achieve_unlock(ACH_FIRST_GAME);

    // mataPlayer() del GML: el fogonazo de oMuerte. Sin su morteSnd: en
    // MonsterLite la muerte no suena (finishGame no toca ningún clip), y el
    // usuario decidió quitarlo (2026-11-21).
    Effect_death(player.e.x, player.e.y);

    // La sacudida en curso se corta: la muerte tiene la suya, al final.
    Shake_stop();
    hitstopMs = 0;

    // Los rayos en curso se pierden con la nave (ver docs/PORT_AUDIT_GML.md,
    // bloque 7).
    MLPow_clear();

    // gameControl.finishGame() -> wait() (S/gameControl.cs:690-725): la nave
    // y su explosión en rojo y Time.timeScale = 0,2 durante dos esperas de
    // 0,1 s de juego (1 s real). La nave sigue ahí y los enemigos la siguen
    // persiguiendo, a un paso de cada cinco. Ver dyingUpdate.
    tintShipRed();
    dyingFrames = 0;
    g_state = GAME_DYING;
}

// Un paso del mundo durante la cámara lenta: lo que en MonsterLite va con el
// tiempo escalado (FixedUpdate, física, animaciones). La nave ya no se mueve
// ni dispara (el estado es GAMEOVER) ni recoge nada.
static void dyingStep(void)
{
    u16 snakeKills = 0;
    u16 killed;

    Enemy_updateAll(TRUE, player.e.x, player.e.y);
    Snake_updateAll(TRUE, player.e.x, player.e.y);

    // Las balas en vuelo siguen y siguen matando: en MonsterLite esas bajas
    // suman (setScores va después de la espera). Sin hitstop.
    killed = Proj_update(&snakeKills);
    killed += Debris_updateAll(&snakeKills);
    (void)Proj_takeBulletKills();
    if (killed) scoreKills(killed, snakeKills);

    Pow_updateAll();
    Beam_update(g_weaponIndex, g_playerBullets, player.e.x, player.e.y, player.e.angle);
    Effect_updateWorld();
    Flyer_updateAll();
    Trail_age();
    // runOut() del HUD va con Time.deltaTime: a la quinta parte.
    HUD_menuTick();
}

static void dyingUpdate(void)
{
    dyingFrames++;

    // El fogonazo del GML va a su ritmo (no es de MonsterLite).
    Effect_updateDeath();

    if (dyingFrames < ML_DEATH_SLOW_F)
    {
        if ((dyingFrames % ML_DEATH_SLOW_DIV) == 0) dyingStep();
        return;
    }

    if (dyingFrames == ML_DEATH_SLOW_F)
    {
        dyingStep();

        // slowDead = false; shake(4); clearPlayer(): la nave desaparece y
        // arranca la sacudida grande.
        sprHide(playerSprite);
        // Aparcada ya, el montador no mira su frame: la siguiente VStream_frame
        // (playerEnter) vuelve a subir el de verdad aunque sea el mismo, y la
        // nave deja de estar roja.
        VStream_invalidate(playerSprite);
        Trail_clear();
        Shake_big(ML_SHAKE_DEATH_F);

        // Lo que se llevaba mataPlayer() del GML: los enemigos siguen vivos
        // y moviéndose durante el menú (sin jugador al que perseguir) hasta
        // que startGame() los borra al empezar la partida siguiente; el fondo
        // y los agujeros siguen puestos. Tampoco se borran los paquetes ni
        // las naves réplica: pasan a la partida siguiente.
        Proj_clearKeepTris();
        Debris_clearAll();
        Flyer_clearAll();
        Beam_clear();
        Pow_replaceAll();
    }

    // La sacudida, con el mundo a velocidad normal (timeScale vuelve a 1 en
    // GAMEOVER). En MonsterLite el game over sale ya en este frame pero no
    // cuenta mientras la cámara se sacude (!globales.shaking): aquí sale al
    // acabar la sacudida, que es cuando empieza a contar (y así la banda del
    // menú, que va en el plano A, no se sacude).
    Shake_update();
    enemiesNoPlayer();
    Effect_updateWorld();
    Pow_updateAll();

    if (!Shake_active())
    {
        Game_checkWeapons();
        Menu_enter();
        // toGameOverRoom -> setData (S/gameControl.cs:906-914) guardaba
        // récord, arma, partidas, sonido y música en PlayerPrefs. Aquí no se
        // guarda nada (sin SRAM): todo eso ya está en RAM y dura hasta apagar.
        g_state = GAME_MENU;
    }
}

// startGame() de MonsterLite, a los 0,8 s: "Instantiate (player, highPos, ...)".
// Player_init la pone en el centro de la pantalla, como el GML (decisión del
// usuario al cerrar el bloque 2): (160, 120).
static void playerEnter(void)
{
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
    enemiesNoPlayer();
    Pow_updateAll();
    Effect_updateAll();
    Flyer_updateAll();
    HUD_update();
}

// ---------------------------------------------------------------------------
// El tiempo sólo corre con A, B o C pulsado (Q8, bloque 2)
//
// MonsterLite pone Time.timeScale = 0 cuando no hay ningún dedo en pantalla,
// pasado el "ARENA!" (HUD.counter >= 60). Con timeScale a 0 Unity deja de
// llamar a FixedUpdate y para la física y el tiempo escalado, pero Update
// sigue cada frame. Lo que hay hoy en el port, repartido así:
//
//   - se para (FixedUpdate, física, corrutinas con WaitForSeconds y
//     animaciones): los enemigos (move() de enemyMov y las migas), la
//     nave (no se mueve, no gira, no dispara), las balas, la metralla, el
//     pivote y las réplicas, la secuencia de arranque y las oleadas, las
//     explosiones, los números que vuelan al HUD (en MonsterLite van con
//     Time.deltaTime, que vale 0) y el latido de lo que hay en el suelo;
//   - sigue: el HUD.
//
// Lo que en MonsterLite sigue corriendo en Update se deja fuera del corte en
// stoppedUpdate: la caducidad de paquetes y monedas (bloque 4), los rayos y
// el giro de la nave holo inactiva (bloque 7) y el temporizador de enemyMov,
// que sigue re-sorteando estado y destino (bloque 8). LASER y MOIRE matando
// y las bolas CIRCULAR persiguiendo todavía no. Ver docs/PORT_AUDIT_GML.md,
// bloques 2 y 8.
//
// La sacudida de MonsterLite (bloque 12, inc/shake.h) también es de Update,
// pero en el port se para con el tiempo: seguir moviendo el plano con los
// sprites quietos los descuadraría, porque el desplazamiento se aplica al
// colocar cada sprite (inc/spr.h). Las de MonsterLite duran 1-8 frames y
// siguen al volver a correr el tiempo.
//
// Tampoco se porta landingAnim (S/gameControl.cs:270): la nave a ×4 con el
// tiempo parado no cabe en VRAM (ver docs/PORT_AUDIT_GML.md, bloque 12).
//
// No se congela:
//   - con PORT_STRESS o PORT_BENCH: el arnés mide el caso peor con todo en
//     marcha y tools/measure.sh no pulsa A, B ni C, así que mide lo mismo que antes;
//   - con el panel del depurador de gráficos abierto (decisión del usuario):
//     B es del depurador mientras el panel está abierto.
// ---------------------------------------------------------------------------
static bool timeStopped(u16 pad)
{
    // HUD.counter: mientras sale "ARENA!" no se congela. Cuenta en todas las
    // builds, para que el cartel se quite también con el arnés.
    if (graceFrames < ML_FREEZE_GRACE_F)
    {
        if (++graceFrames == ML_FREEZE_GRACE_F) HUD_setArena(FALSE);
        return FALSE;
    }

#if (PORT_STRESS || PORT_BENCH)
    (void)pad;
    return FALSE;
#else
    if (pad & ML_TIME_BUTTON) return FALSE;
    if (GfxDebug_panelOpen()) return FALSE;

    return TRUE;
#endif
}

// Un frame con el tiempo parado: sólo el aviso y el HUD.
static void stoppedUpdate(void)
{
    // El aviso, con la fuente del sistema en vez del "KEEP YOUR FINGER\n ON
    // SCREEN!" y la mano (textura dedo) de MonsterLite, que no tienen
    // equivalente con el pad (ver src/hud.c).
    HUD_setHold(TRUE);

    // Lo que en MonsterLite va en Update sigue: la caducidad de paquetes y
    // monedas y la subida del paquete recogido, el giro de la nave holo
    // inactiva, los rayos (que pueden acabar y limpiar la pantalla con el
    // tiempo parado) y el temporizador de enemyMov (bloque 8: re-sortea el
    // estado y el destino aunque el bicho no se mueva).
    Enemy_tickFrozen();
    Snake_tickFrozen();
    Pow_tickFrozen();
    Proj_tickFrozen();
    addKills(MLPow_tickRays());
    Beam_updateRays(player.e.x, player.e.y);
    // levelUpgrade() va en el Update de gameControl, que sigue con el tiempo
    // parado: si los rayos suben de nivel, el mensaje sale ya (y su cuenta
    // espera a que vuelva a correr el tiempo).
    Game_levelUpgrade();
    HUD_update();
}

static void playUpdate(u16 pad)
{
    s16 px, py;
    s16 targetX = 0, targetY = 0;

    // Thread.Sleep: no corre nada, ni el Update. Cada frame paga 20 ms.
    if (hitstopMs >= ML_FRAME_MS)
    {
        hitstopMs -= ML_FRAME_MS;
        return;
    }

    if (timeStopped(pad))
    {
        HUD_tick(FALSE);
        stoppedUpdate();
        return;
    }

    HUD_tick(TRUE);
    HUD_setHold(FALSE);

    if (!startSequence())
    {
        waitingUpdate();
        return;
    }

    // La cámara (inc/shake.h): en MonsterLite, corrutinas de Update; en el
    // GML, al final del Step del jugador. Aquí va al principio del frame para
    // que el plano y los sprites usen el mismo desplazamiento.
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

    // La estela (TrailRenderer de la nave), con el tiempo de juego.
    Trail_update(px, py);


    // oPlayer/Step_0: el apuntado sólo se actualiza si hay algún enemigo
    // ("if instance_exists(oMalo)"). En MonsterLite tampoco se dispara sin
    // enemigo (ver abajo).
    {
        const bool hasTarget = closestThreat(px, py, &targetX, &targetY);

        if (hasTarget) Player_aim(&player, FIX16(targetX), FIX16(targetY));

        // El pivote, las réplicas y las bolas necesitan jugador, input y
        // objetivo.
        Proj_setPlayer(player.e.x, player.e.y, player.e.angle, pad, hasTarget,
                       FIX16(targetX), FIX16(targetY));

        // S/disparo.cs:32-160 (bloque 3 de MonsterLite), en FixedUpdate:
        //
        //     if (closest) {
        //         if (shootTime > 1) shootTime -= 1;
        //         else if (isBullets()) { switch (currentWeapon) { ... } }
        //         else { BulletThin; shootTime = shotTimeNormal; }
        //     }
        //
        // Sólo cuenta y dispara si hay enemigo. Con munición dispara el arma
        // equipada, gasta una y pone su cadencia; sin ella, la bala fina. El
        // PIVOT y el PARA del GML entran en el mismo bucle con su cadencia
        // del GML, y gastan aunque el arma no cree nada (el pivote ya
        // puesto, o la bala del PARA viva), como allí.
        if (hasTarget)
        {
            if (player.controlTime > 1)
            {
                player.controlTime--;
            }
            else if (g_playerBullets > 0)
            {
                static const u8 cadence[WPN_COUNT] =
                {
                    ML_SHOT_WBLOCK_F, ML_SHOT_LASER_F, ML_SHOT_TWAY_F,
                    SHOOT_INT_WEAPON, SHOOT_INT_WEAPON,
                    ML_SHOT_CIRCULAR_F, ML_SHOT_MOIRE_F
                };

                Proj_fireWeapon(g_weaponIndex, player.e.x, player.e.y, player.e.angle);
                g_playerBullets--;
                player.controlTime = cadence[g_weaponIndex];

                // La sacudida pequeña de cada arma (S/disparo.cs); CIRCULAR
                // no tiene (la ponen sus bolas al perseguir). PIVOT y PARA,
                // que no están en MonsterLite, la del GML, stShake(4, 5, 0.2).
                switch (g_weaponIndex)
                {
                    case WPN_WBLOCK:   Shake_small(ML_SHAKE_WBLOCK_F); break;
                    case WPN_LASER:    Shake_small(ML_SHAKE_LASER_F);  break;
                    case WPN_MOIRE:    Shake_small(ML_SHAKE_MOIRE_F);  break;
                    case WPN_TWAY:     Shake_small(ML_SHAKE_TWAY_F);   break;
                    case WPN_CIRCULAR: break;
                    default:           Shake_shot();                   break;
                }
            }
            else
            {
                // La bala fina no sacude en MonsterLite (S/disparo.cs:144-150).
                // La del GML, stShake(1, 1, 0.1), eran 0,2 px: no se veía.
                Proj_fireThin(player.e.x, player.e.y, player.e.angle);
                player.controlTime = ML_SHOT_NORMAL_F;
            }
        }
    }

    // Recogida de paquetes y power-ups (las cuatro colisiones de oPlayer)
    {
        PowerPickup got;

        if (Pow_collect(px, py, g_weaponIndex, &got))
        {
            switch (got.type)
            {
                case POW_COIN:
                    // playerMovement: CoinsManager.addCoins(coinTypeVar), al
                    // saldo y a las monedas de esta partida, y
                    // playPillarPaqueteClip (S/playerMovement.cs:190).
                    g_coins += got.value;
                    g_coinsEarned += got.value;
                    // Bloque 10: las monedas recogidas en total.
                    Achieve_coins(got.value);
                    Audio_sfx(SFX_ML_POWUP);
                    break;

                case POW_PACK:
                    // playPillarPaqueteClip (S/playerMovement.cs:184)
                    g_playerBullets += got.ammo;
                    Audio_sfx(SFX_ML_POWUP);
                    // puntoToHUD(0), que en el original sale DOS veces: lo
                    // llaman el manejador del paquete y el del jugador.
                    Flyer_spawn(got.x, got.y, 0, g_playerBullets);
                    break;

                case POW_BOOM:
                    // playerMovement.OnTriggerEnter, "bomba"
                    // (S/playerMovement.cs:199-210): Sleep(100),
                    // playBoomClip, la salpicadura, clearEnemies() (cada
                    // enemigo explota con bombaXPL y suma level), shake(1),
                    // bombaAmarilla y la voz "EXPLOSIONS". La pausa va aquí
                    // después del estallido en vez de antes: se congela con
                    // las explosiones ya puestas.
#if !PORT_NO_HITSTOP
                    hitstopMs += ML_POW_FREEZE_MS;
#endif
                    Effect_spawnQuiet(got.x, got.y);
                    // Bloque 10: usar una bomba, y 10 bajas con la misma.
                    Achieve_unlock(ACH_USE_BOMB);
                    {
                        const u16 n = MLPow_clearEnemies();

                        if (n >= ACH_BOMB_KILLS) Achieve_unlock(ACH_BOMB_10);
                        addKills(n);
                    }
                    // shake(1) de MonsterLite (±80/±60 px un frame) y, por
                    // debajo, la del oExplosionCruz del GML con su blancazo
                    // (la paleta a medio camino del blanco 50 frames): la
                    // sacudida del bloque 8 y el blancazo de fffe0af, que el
                    // usuario quiere de vuelta (2026-11-22). Las dos sacudidas arrancan al acabar el
                    // hitstop (en él no corre nada); el blancazo se ve ya.
                    Shake_big(ML_SHAKE_BOMB_F);
                    Shake_bomb();
                    Effect_bombFlash();
                    // playBoomClip, bombaAmarilla y "EXPLOSIONS", que
                    // MonsterLite toca en el mismo frame: una sola muestra
                    // ya mezclada (tools/ml2md_audio.py), un solo canal.
                    Audio_sfx(SFX_ML_BOMB_MIX);
                    break;

                case POW_RAYS:
                    // "rayoBomba" (S/playerMovement.cs:212-220): Sleep(100),
                    // bombaAmarilla, un rayo a cada araña y cabeza
                    // (rayobomba()), shakeSmall(10) y la voz "RAYS".
#if !PORT_NO_HITSTOP
                    hitstopMs += ML_POW_FREEZE_MS;
#endif
                    // bombaAmarilla y "RAYS", mezcladas en una muestra.
                    Audio_sfx(SFX_ML_RAYS_MIX);
                    MLPow_startRays(player.e.x, player.e.y);
                    // Bloque 10: usar unos rayos.
                    Achieve_unlock(ACH_USE_RAYS);
                    Shake_small(ML_SHAKE_RAYS_F);
                    break;

                default:
                    break;
            }

        }
    }

    // "NaveProxy": la nave toca una holo y la activa (setWorking), con la voz
    // repetea si no lo estaba.
    // Bloque 10: activar una nave holo.
    if (Proj_holoTouch(px, py))
    {
        // naveProxyController.setWorking: playRepeteaClip (:43).
        Audio_sfx(SFX_ML_REPETEA);
        Achieve_unlock(ACH_HOLO);
    }

    // Los rayos de MonsterLite (RayoScr.Update).
    addKills(MLPow_tickRays());

    // checkMorte()/Collision_oBulletS: cada muerte suma a global.kills y sube
    // global.lastScore si lo supera.
    PROF_STOP(PROF_PLAYER);

    u16 snakeKills = 0;
    PROF_START(PROF_PROJ);
    u16 killed = Proj_update(&snakeKills);
    // La metralla de oEstallido mata igual que una bala (Collision_oDestro).
    killed += Debris_updateAll(&snakeKills);
    PROF_STOP(PROF_PROJ);

    if (killed) scoreKills(killed, snakeKills);

#if !PORT_NO_HITSTOP
    // Thread.Sleep(30) por cada baja con bala (Q17): se paga a partir del
    // frame siguiente.
    hitstopMs += Proj_takeBulletKills() * ML_KILL_FREEZE_MS;
#else
    (void)Proj_takeBulletKills();
#endif

    // levelUpgrade() corre en cada Update de gameControl.
    Game_levelUpgrade();

    // oMalo/Collision_oPlayer -> mataPlayer(): vale cualquier enemigo,
    // incluidos los segmentos del gusano (en MonsterLite, los tags arana,
    // snake y snakeBody de playerMovement.OnTriggerEnter). La nave es la
    // esfera de 3,6 px de MonsterLite contra las de los enemigos (8,4 px la
    // araña, 9,6 px la cabeza y los eslabones, bloque 8).
    if (Enemy_collides(px, py) || Snake_collides(px, py))
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

    // La cámara lenta de la muerte y la sacudida de después (bloque 12).
    if (g_state == GAME_DYING)
    {
        dyingUpdate();
        return;
    }

    // mataPlayer() crea el oMuerte y DESPUÉS abre el menú: el fogonazo sigue
    // vivo encima de él, igual que las explosiones que hubiera en marcha
    // (nada las destruye). Por eso los efectos se siguen actualizando fuera
    // de la partida.
    Effect_updateAll();
    Flyer_updateAll();

    // Los bichos siguen a lo suyo sin jugador: en ATTACK van a por una nave
    // holo si la hay (ver enemiesNoPlayer). En el splash no hay ninguno.
    enemiesNoPlayer();

    // Los paquetes y las monedas caducan también con el menú puesto (en
    // MonsterLite el game over no para su Update).
    Pow_updateAll();

    // Splash e interludio comparten oMenu: el mismo objeto dibuja uno u otro
    // según global.isMenu.
    if (Menu_update(pad, g_state == GAME_SPLASH))
    {
        const bool fromSplash = (g_state == GAME_SPLASH);

        Menu_leave();
        Game_start();
        // Tocar en el MENU de MonsterLite: shake(4) y partida
        // (S/gameControl.cs:219). El PLAY del game over y de la tienda no
        // sacude.
        if (fromSplash) Shake_big(ML_SHAKE_START_F);
    }
    else
    {
        // runOut() del HUD: el grupo de arriba sale deslizándose. Después del
        // menú, para que si este frame abre la tienda (que borra el plano
        // por la CPU) no llegue detrás un trozo del HUD por la cola.
        HUD_menuTick();
    }
}
