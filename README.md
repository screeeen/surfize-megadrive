# Arena — Mega Drive

Port a Sega Mega Drive / Genesis del juego [arena](../arena) (Unity, móvil), construido con [SGDK](https://github.com/Stephane-D/SGDK).

Bootstrap generado siguiendo el mismo patrón que [ovni-megadrive](../ovni-megadrive), portado en pasos incrementales:

- **Nave del jugador**: se mueve con el D-Pad y wrappea en los bordes de pantalla, igual que `playerMovement.move()` + `globales.min/maxX/Y` en el original.
- **Enemigos "spider"**: aparecen por el borde de la arena (sustituyendo los `posHoles` del original, no portados) y persiguen al jugador en línea recta, igual que el estado `ATTACK` de `enemyMov.move()` — sin el estado `WANDER` ni el giro suavizado (`Quaternion.Slerp`), que no se han portado. Tocar a un enemigo termina la partida, igual que el caso `"arana"` de `playerMovement.OnTriggerEnter` → `gameControl.finishGame()`; como todavía no hay pantalla de game over, la partida simplemente se reinicia (nave al centro, enemigos limpiados).
- **Disparo automático**: la nave dispara sola al enemigo más cercano —spider o snake— (sin botón de disparo), igual que `playerMovement.rotateShipDirection()` + `disparo.dispara()` en el original. Es el caso por defecto de `disparo.dispara()` (bala fina tipo `bulletThin`); las armas `WBLOCK`/`LASER`/`TWAY`/`CIRCULAR`/`MOIRE` de `WeaponsController` no se han portado. La bala viaja en línea recta al ángulo real hacia el objetivo (`F16_getAngle`/`F16_sin`/`F16_cos` de SGDK, tablas de 1°) — no en 8 direcciones fijas como al principio. Las spiders mueren de un golpe; suma a un marcador `SCORE` simple en pantalla (`globales.kills`, sin el multiplicador por nivel).
- **Enemigo "snake"**: cabeza + 3 segmentos que la siguen con retardo, igual que `headScr.cs` (usando su propia implementación alternativa comentada —historial de posiciones con retardo fijo— en vez de la lista de "breadcrumbs", mismo resultado más simple en esta CPU). La cabeza persigue al jugador igual que una spider pero aguanta 3 golpes (sustituye el HP de `enemyLife`); los segmentos no tienen HP ni pueden dispararse —igual que el original, donde `Bullet.OnTriggerEnter` sólo comprueba las tags `"arana"`/`"snake"`, nunca `"snakeBody"`— pero sí terminan la partida al tocarlos (tag `"snakeBody"` en `playerMovement.OnTriggerEnter`). Los sprites de snake reutilizan la paleta de spider (PAL2): el hardware sólo tiene 4 líneas de paleta de sprites y ya están repartidas entre texto/nave/enemigos/balas, así que spider y snake se distinguen por silueta, no por color.
- Las armas (`WeaponsController`), power-ups (bombas, rayos, paquetes), monedas y HUD/menús completos se portarán en pasos siguientes.
- **Contador de FPS**: `VDP_showFPS()` de SGDK, arriba a la derecha. Útil para confirmar que el juego sigue a 60fps al subir el ritmo (ver más abajo).

Velocidades (nave, spider/snake, bala) e intervalos (spawn, disparo) se subieron a petición para un ritmo más rápido; no son valores portados de ningún sitio, son ajuste directo de sensación de juego.

**Estado actual: la lógica de juego (spawns/HP/movimiento de enemigos) está en pausa.** Las simplificaciones de arriba (movimiento en 8 direcciones en vez de giro real con trigonometría, sin estado `WANDER`, spawns por borde en vez de `posHoles`, HP inventado, constantes de balance sin verificar) no son fieles al original — están documentadas, no ocultas, pero no es un port exacto. Hay una versión del juego hecha en GameMaker (texto, más fácil de leer valores exactos que los prefabs binarios de Unity) desde la que se retomará el port de esa lógica. Mientras tanto, lo gráfico (assets, fondo, rotación de sprites) sigue avanzando — no depende de esos valores.

- **Rotación de la nave**: 8 sprites pre-rotados (`player.png`, uno por dirección, generados rotando la silueta real de `nave.png`) en vez de rotación continua (el hardware no rota sprites en tiempo real). Cada frame, la nave apunta al frame más cercano a la dirección real hacia el objetivo, eligiendo por producto escalar contra los 8 vectores de dirección — mismo resultado que redondear el ángulo real de `calculateAngle()` (atan2) al múltiplo de 45° más cercano, sin necesitar ni atan2 ni ángulos de por medio. Los enemigos (spider/snake) todavía no rotan.
- **Fondo**: un patrón de "estrellas" disperso (6 tiles de 8×8 con 0-1 chispas cada uno) en vez de la textura de ruido animada de 8 frames `backgroundFlow.png` (256×256, inviable en la VRAM de Mega Drive). Reutiliza la paleta de la nave (PAL1: fondo en el mismo azul marino, chispas del mismo cian) porque las 4 líneas de paleta del hardware ya están repartidas entre texto/nave/enemigos/balas.

## Port fiel desde el original GameMaker

El port existente salió del proyecto **Unity** (`../arena`), con varias constantes de balance puestas a ojo, y después se rehízo desde la versión **GameMaker** (`~/Desktop/___pendireve/game maker move/surfize_changes`), ya auditada por completo. Desde el 2026-11-15 la fuente de verdad es **MonsterLite** (`~/MonsterLite/MonsterLite_ultimo_commit`, Unity/C#), y el GML queda como segunda referencia para lo que MonsterLite no define:

- **[docs/PORT_AUDIT_GML.md](docs/PORT_AUDIT_GML.md)** — auditoría del GML (mecánicas, IA de 3 estados, oleadas, 7 armas, power-ups), constantes reales convertidas a Mega Drive, tabla de equivalencias GameMaker→SGDK, divergencias del port actual, inventario de recursos y plan de fases.
- **[docs/MONSTERLITE_GAP.md](docs/MONSTERLITE_GAP.md)** — inventario de MonsterLite, lo que le falta al port, bloques de trabajo y preguntas abiertas.
- **`/port-arena`** (`.claude/commands/port-arena.md`) — prompt de porting listo para ejecutar por bloques, con las decisiones de base fijadas: PAL 320×240 @50 Hz y conversión de unidades (MonsterLite: 20 px por unidad, `FixedUpdate` 1:1, `Update` ÷1.2; GML: posiciones ×0.2, velocidades ×0.24, giro ×1.2, frames ÷1.2). Arte reconvertido desde los PNG de GameMaker y longitud de gusano fiel están pendientes de confirmar.

Las simplificaciones descritas más abajo quedan como estado previo: se corrigen en las fases 4–6 de ese plan.

## Assets gráficos

Los sprites de nave, spider y snake están trazados del arte real de Unity (`arena/Assets/Art/nave.png` y `culobicho.png`), no inventados:

- **`player.png`** (8 frames de 32×32, una tira): silueta extraída del canal alfa de `nave.png` (el PNG original es una máscara blanca pensada para tintar en Unity — el alfa, no el RGB, tiene la forma real), rotada en pasos de 45° para las 8 direcciones. Coloreada en cian (`#78DCFF`).
- **`spider.png`** (24×24) y **`snake_head.png`** (32×32, misma silueta a mayor escala): silueta extraída del alfa de `culobicho.png` (pinzas/patas incluidas). Coloreadas en naranja (`#FF6E46`), paleta compartida (ver más arriba).
- **`snake_seg.png`** (16×16): círculo extraído del alfa de `culobicho2.png`, el segmento de cuerpo real del original.
- **`bullet.png`** (8×8): no tiene un equivalente claro en el arte original (`sBulletThin.png`/`bulletsAnimation.png` no tienen una silueta identificable — probablemente el disparo real usa un `LineRenderer`, no un sprite), así que sigue siendo un diseño simple propio.

## Requisitos

- [SGDK](https://github.com/Stephane-D/SGDK), con la variable `GDK` apuntando a la carpeta clonada (p. ej. en `~/.zshrc`: `export GDK="$HOME/dev/sgdk"`)
- `m68k-elf-gcc` / `m68k-elf-binutils` (Homebrew)
- Java (para las herramientas `rescomp`/`sizebnd` de SGDK)
- Make
- [BlastEm](https://www.retrodev.com/blastem/) para probar el ROM

## Build

```bash
make        # genera out/rom.bin
make clean  # limpia out/
```

## Ejecutar en emulador

```bash
blastem out/rom.bin
```

## VS Code

Tareas en `.vscode/tasks.json`: **SGDK: build**, **SGDK: clean**, **SGDK: run in BlastEm**.

## Próximos pasos

- Portar las oleadas/spawns por nivel de `gameControl.startGame()` (hoy: spawns en intervalo fijo, sin escalado por `globales.level`).
- Portar el sistema de armas (`WeaponsController`: WBLOCK/LASER/TWAY/CIRCULAR/MOIRE) y sus balas (`bulletThin`/`bulletFat`/láser).
- Portar monedas (`CoinsManager`) y power-ups (bomba, rayo, paquete).
- HUD completo (nivel, arma actual, monedas) y estados de juego (menú, game over) — hoy `gameControl.State`.

## Ver los sprites sobre otro fondo

El campo de juego es un gris oscuro muy parecido a las siluetas de los
enemigos, así que para mirar el arte conviene cambiarlo. Se compila así:

```sh
make EXTRA_FLAGS="-Wno-unterminated-string-initialization -DPORT_DEBUG_BG=1"
```

Y ya **dentro de la partida se va cambiando con el botón C**. El disparo es
automático, así que en juego los botones están libres. Cada pulsación pasa al
siguiente:

| | |
|---|---|
| 1 | el fondo de verdad |
| 2 | **magenta** — ningún sprite usa ese color, así que todo destaca |
| 3 | negro |
| 4 | blanco |
| 5 | gris medio |
| 6, 7, 8 | verde, rojo, azul |

La lista está en `src/game.c` (`debugBg[]`), por si quieres otros: son
colores del VDP en formato `0x0BGR` con un dígito hexadecimal **par** por
canal (`0x000E` rojo, `0x00E0` verde, `0x0E00` azul, `0x0888` gris medio).

Para volver a la ROM normal, `make` a secas: nada de esto entra en ella.

**Un aviso al usarlo.** El VDP no mezcla colores, pero el campo de juego es un
color plano, así que todo lo que en el original es translúcido —la humareda,
la metralla, el haz del arma PARA, el borde de los agujeros— lleva su color
**ya mezclado con el fondo de verdad**. Con otro fondo esos tonos dejan de
cuadrar y se ven apagados o verdosos. No es un fallo del arte: es justo la
técnica que lo hace funcionar. Si vas a juzgar colores, hazlo con el fondo
normal.

### Las teclas de número son del emulador, no del juego

Si pulsando cifras cambia la velocidad, eso es BlastEm: tiene el teclado
numérico asignado a velocidades de emulación. El juego no lee el teclado, sólo
el mando.

## Los otros interruptores de build

| flag | para qué |
|---|---|
| `PORT_STRESS=1` | llena la arena a los topes y hace inmortal al jugador: el caso peor |
| `PORT_PROF=1` | contadores de frames y de carga de CPU (casi no cuestan) |
| `PORT_PROF=2` | además, cronómetro por etapas del bucle |
| `PORT_SPR_C=1` | usa la versión en C del montador de sprites en vez de `src/spr_a.s`, para poder comparar las dos tablas |
| `PORT_DEBUG_BG=n` | el fondo de contraste de arriba |
| `HUD_SHOW_PERF` | en `inc/hud.h`: el contador de FPS y CPU en pantalla |
