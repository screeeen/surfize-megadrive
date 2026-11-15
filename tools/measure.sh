#!/bin/sh
# Mide la velocidad del caso peor (PORT_STRESS=1) sin tocar el repo.
#
# Copia el proyecto a un directorio temporal, mete un contador de vueltas del
# bucle principal (g_loopCount), compila con PORT_STRESS=1 y lo arranca en
# BlastEm con semilla fija: pulsa START+A en el frame 200 y lee el contador y
# las bajas 600 frames después. A 50 fps cada frame es una vuelta, así que
#   vueltas / 600  = fracción de frames completos (1.0 = sin perder ninguno)
# Las partidas son deterministas por build: dos builds se comparan por estos
# dos números. Ver también la memoria "stress-measurement-cliff".
#
# uso: tools/measure.sh [directorio del proyecto]   (por defecto, el actual)
set -e
SRC=$(cd "${1:-.}" && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/measure.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

cp -R "$SRC/Makefile" "$SRC/src" "$SRC/inc" "$SRC/res" "$TMP/"
python3 - "$TMP/src/main.c" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
s = s.replace('int main(bool hardReset)', 'volatile u32 g_loopCount;\n\nint main(bool hardReset)', 1)
s = s.replace('        Game_update();', '        g_loopCount++;\n        Game_update();', 1)
open(p, 'w').write(s)
PY
( cd "$TMP" && make EXTRA_FLAGS="-Wno-unterminated-string-initialization -DPORT_STRESS=1" >"$TMP/build.log" 2>&1 ) \
    || { tail -30 "$TMP/build.log"; exit 1; }

sym() { grep " $1\$" "$TMP/out/release/symbol.txt" | cut -c3-8; }
LOOP=$(printf "%x" $((0x$(sym g_loopCount) + 2)))     # palabra baja del u32
KILLS=$(sym g_kills)

OUT=$(python3 "$SRC/tools/mdrun.py" "$TMP/out/rom.bin" "frames 200" "p/d [0x$LOOP]" \
      'binddown "gamepads.1.start"' 'binddown "gamepads.1.a"' "frames 3" \
      'bindup "gamepads.1.start"' 'bindup "gamepads.1.a"' "frames 597" \
      "p/d [0x$LOOP]" "p/d [0x$KILLS]" 2>&1 | grep -E '^\[0x' | awk '{print $2}' | tr '\n' ' ')
set -- $OUT
echo "vueltas: $(( $2 - $1 )) de 600   bajas: $3"
