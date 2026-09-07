# Arena — Mega Drive

Port a Sega Mega Drive / Genesis del juego [arena](../arena) (Unity, móvil), construido con [SGDK](https://github.com/Stephane-D/SGDK).

Bootstrap generado siguiendo el mismo patrón que [ovni-megadrive](../ovni-megadrive), portado en pasos incrementales:

- **Nave del jugador**: se mueve con el D-Pad y wrappea en los bordes de pantalla, igual que `playerMovement.move()` + `globales.min/maxX/Y` en el original.
- **Enemigos "spider"**: aparecen por el borde de la arena (sustituyendo los `posHoles` del original, no portados) y persiguen al jugador en línea recta, igual que el estado `ATTACK` de `enemyMov.move()` — sin el estado `WANDER` ni el giro suavizado (`Quaternion.Slerp`), que no se han portado. Tocar a un enemigo termina la partida, igual que el caso `"arana"` de `playerMovement.OnTriggerEnter` → `gameControl.finishGame()`; como todavía no hay pantalla de game over, la partida simplemente se reinicia (nave al centro, enemigos limpiados).
- **Disparo automático**: la nave dispara sola al enemigo más cercano —spider o snake— (sin botón de disparo), igual que `playerMovement.rotateShipDirection()` + `disparo.dispara()` en el original. Es el caso por defecto de `disparo.dispara()` (bala fina tipo `bulletThin`); las armas `WBLOCK`/`LASER`/`TWAY`/`CIRCULAR`/`MOIRE` de `WeaponsController` no se han portado. Las spiders mueren de un golpe; suma a un marcador `SCORE` simple en pantalla (`globales.kills`, sin el multiplicador por nivel).
- **Enemigo "snake"**: cabeza + 3 segmentos que la siguen con retardo, igual que `headScr.cs` (usando su propia implementación alternativa comentada —historial de posiciones con retardo fijo— en vez de la lista de "breadcrumbs", mismo resultado más simple en esta CPU). La cabeza persigue al jugador igual que una spider pero aguanta 3 golpes (sustituye el HP de `enemyLife`); los segmentos no tienen HP ni pueden dispararse —igual que el original, donde `Bullet.OnTriggerEnter` sólo comprueba las tags `"arana"`/`"snake"`, nunca `"snakeBody"`— pero sí terminan la partida al tocarlos (tag `"snakeBody"` en `playerMovement.OnTriggerEnter`). Los sprites de snake reutilizan la paleta de spider (PAL2): el hardware sólo tiene 4 líneas de paleta de sprites y ya están repartidas entre texto/nave/enemigos/balas, así que spider y snake se distinguen por silueta, no por color.
- Las armas (`WeaponsController`), power-ups (bombas, rayos, paquetes), monedas y HUD/menús completos se portarán en pasos siguientes.

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
