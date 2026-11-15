#!/usr/bin/env python3
"""Conduce BlastEm por su depurador para poder ver y medir el juego sin manos.

BlastEm trae un depurador de línea de comandos (`blastem -d`) que no funciona
con la entrada redirigida: hay que darle un terminal de verdad. Esto se lo da
con un pty y le va pasando comandos.

Lo que abre esto:

  - `bindup "ui.screenshot"` guarda un PNG del fotograma en $HOME, que se puede
    mirar directamente: es la única forma de ver la pantalla desde aquí, porque
    macOS no deja grabar la pantalla al terminal.
  - `binddown "gamepads.1.start"` / `bindup ...` simulan el mando, así que se
    puede entrar al menú y jugar sin tocar el teclado. (El `?` del depurador
    dice `bindown`, pero el comando real es `binddown`.)
  - `breakpoint 0x<dir>` + `continue` paran en una función concreta; las
    direcciones salen de `out/release/symbol.txt`.
  - `frames N` avanza N fotogramas exactos.
  - `vs` imprime la tabla de sprites del VDP (las 80 entradas de hardware):
    sirve para comprobar qué se está dibujando, de qué tamaño y dónde.
  - `p/d [0x<dir>]` lee una palabra de la RAM del 68000: con los símbolos se
    pueden leer variables del juego o de SGDK (p. ej. la cola de DMA).

Ejemplos:

    # ver la pantalla a los 200 fotogramas
    python3 tools/mdrun.py out/rom.bin "frames 200" 'bindup "ui.screenshot"' "frames 1"

    # parar en la primera explosión y listar los sprites de hardware
    python3 tools/mdrun.py out/rom.bin "breakpoint 0x76c" continue "frames 1" vs

Argumentos: el ROM y luego los comandos del depurador en orden. Un argumento
`sleep:N` espera N segundos de reloj (útil entre capturas, porque el nombre del
PNG lleva la hora con precisión de segundo y si no se pisan unas a otras).
"""

import os
import pty
import select
import signal
import sys
import time

PROMPT_WAIT = 30.0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    rom, cmds = sys.argv[1], sys.argv[2:]

    pid, fd = pty.fork()
    if pid == 0:
        os.environ["SDL_AUDIODRIVER"] = "dummy"
        os.execvp("blastem", ["blastem", "-d", "-r", "E", rom])

    def pump(timeout=PROMPT_WAIT):
        end = time.time() + timeout
        got = ""
        while time.time() < end:
            ready, _, _ = select.select([fd], [], [], 0.2)
            if not ready:
                continue
            try:
                data = os.read(fd, 1 << 16)
            except OSError:
                break
            if not data:
                break
            got += data.decode("utf8", "replace")
            if got.rstrip().endswith(">"):
                break
        return got

    pump(15.0)                      # arranque + primer prompt
    log = []

    for cmd in cmds:
        if cmd.startswith("sleep:"):
            time.sleep(float(cmd[6:]))
            log.append((cmd, ""))
            continue
        os.write(fd, (cmd + "\n").encode())
        log.append((cmd, pump()))

    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)

    for cmd, out in log:
        print(f"##### {cmd}")
        for line in out.splitlines():
            # BlastEm recorre las fuentes del sistema al arrancar y lo cuenta
            if "because of its extension" in line:
                continue
            print(line)

    return 0


if __name__ == "__main__":
    sys.exit(main())
