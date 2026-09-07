# Arena — Mega Drive

Port a Sega Mega Drive / Genesis del juego [arena](../arena) (Unity, móvil), construido con [SGDK](https://github.com/Stephane-D/SGDK).

Bootstrap generado siguiendo el mismo patrón que [ovni-megadrive](../ovni-megadrive), portado en pasos incrementales:

- **Nave del jugador**: se mueve con el D-Pad y wrappea en los bordes de pantalla, igual que `playerMovement.move()` + `globales.min/maxX/Y` en el original.
- **Enemigos "spider"**: aparecen por el borde de la arena (sustituyendo los `posHoles` del original, no portados) y persiguen al jugador en línea recta, igual que el estado `ATTACK` de `enemyMov.move()` — sin el estado `WANDER` ni el giro suavizado (`Quaternion.Slerp`), que no se han portado. Tocar a un enemigo termina la partida, igual que el caso `"arana"` de `playerMovement.OnTriggerEnter` → `gameControl.finishGame()`; como todavía no hay pantalla de game over, la partida simplemente se reinicia (nave al centro, enemigos limpiados).
- **Disparo automático**: la nave dispara sola al enemigo más cercano (sin botón de disparo), igual que `playerMovement.rotateShipDirection()` + `disparo.dispara()` en el original. Es el caso por defecto de `disparo.dispara()` (bala fina tipo `bulletThin`); las armas `WBLOCK`/`LASER`/`TWAY`/`CIRCULAR`/`MOIRE` de `WeaponsController` no se han portado. Cada impacto mata al bicho de un golpe (el sistema de HP de `enemyLife`, usado por "snake", no se ha portado) y suma a un marcador `SCORE` simple en pantalla (`globales.kills`, sin el multiplicador por nivel).
- El enemigo "snake" (segmentado, `headScr.cs`), las armas (`WeaponsController`), power-ups (bombas, rayos, paquetes), monedas y HUD/menús completos se portarán en pasos siguientes.

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

- Portar el enemigo "snake" (segmentado, `headScr.cs`), su HP (`enemyLife`) y las oleadas/spawns por nivel de `gameControl.startGame()`.
- Portar el sistema de armas (`WeaponsController`: WBLOCK/LASER/TWAY/CIRCULAR/MOIRE) y sus balas (`bulletThin`/`bulletFat`/láser).
- Portar monedas (`CoinsManager`) y power-ups (bomba, rayo, paquete).
- HUD completo (nivel, arma actual, monedas) y estados de juego (menú, game over) — hoy `gameControl.State`.
