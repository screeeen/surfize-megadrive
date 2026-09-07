# Arena — Mega Drive

Port a Sega Mega Drive / Genesis del juego [arena](../arena) (Unity, móvil), construido con [SGDK](https://github.com/Stephane-D/SGDK).

Bootstrap generado siguiendo el mismo patrón que [ovni-megadrive](../ovni-megadrive): por ahora solo arranca y muestra la nave del jugador moviéndose por la arena (D-Pad, con wraparound en los bordes de pantalla, igual que `playerMovement.move()` + `globales.min/maxX/Y` en el original). La lógica de oleadas de enemigos (`enemyController`), disparo automático hacia el enemigo más cercano (`disparo.cs`), armas (`WeaponsController`), power-ups (bombas, rayos, paquetes) y HUD/menús se portará en pasos siguientes.

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

- Portar `enemyController` (spawns de "snake"/"spider" en oleadas, movimiento hacia el jugador).
- Portar `disparo.cs` (auto-apuntado + disparo automático al enemigo más cercano) y las balas (`bulletThin`/`bulletFat`/láser).
- Portar el sistema de armas (`WeaponsController`), monedas (`CoinsManager`) y power-ups (bomba, rayo, paquete).
- HUD (kills, nivel) y estados de juego (menú, game over) — hoy `gameControl.State`.
