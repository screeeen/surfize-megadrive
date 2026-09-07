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
