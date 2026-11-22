#!/usr/bin/env python3
"""Convierte los sonidos del proyecto GameMaker a WAV para rescomp.

Los originales son Ogg Vorbis a 11-44 kHz, estéreo o mono. El driver XGM2 de
SGDK reproduce PCM de 8 bits sin signo en torno a 13 kHz, así que todo se
baja a 13300 Hz mono.

No hay ffmpeg ni sox en el sistema, pero sí VLC, cuya interfaz dummy
transcodifica sin abrir ventana. VLC escribe una cabecera WAV inválida para
streams (formato 0 y tamaño desconocido), así que aquí se extrae el bloque de
datos y se reescribe la cabecera a mano.

Uso:  python3 tools/gm2md_audio.py
"""

import os
import struct
import subprocess
import sys

GM = "/Users/miguelangelalvarez/Desktop/___pendireve/game maker move/surfize_changes"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "sound")
VLC = "/Applications/VLC.app/Contents/MacOS/VLC"
RATE = 13300

# Sonidos que usa el port, con el nombre del recurso. Desde el bloque 12
# (decisión del usuario del 2026-11-21) sólo los que no tienen equivalente en
# MonsterLite; los demás salen de tools/ml2md_audio.py. Los de voz suenan al
# cambiar de arma en el menú (triggerSpeech).
SOUNDS = [
    ("shotSnd",            "sfxShot"),        # oBulletThin
    ("laserSnd",           "sfxLaser"),       # oLaser y oBomba
    ("boomSnd",            "sfxBoom"),
    ("aranamorteSnd",      "sfxAranaMorte"),  # oExplosion y morteGusano
    ("dustSnd",            "sfxDust"),
    ("menuSong",           "musMenu"),        # menu(): audio_play_sound(menuSong)
    # triggerSpeech(weapon_index): una voz por arma al cambiarla en el menú
    ("fantasmaSnd",        "spk0"),           # arma 0, WBLOCK
    ("laserSpeechSnd",     "spk1"),           # arma 1, LASER
    ("threeWaySpeechSnd",  "spk2"),           # arma 2, 3WAY
    ("bolasSpeechSnd",     "spk3"),           # arma 3, PIVOT
    ("retroUFOSpeechSnd",  "spk4"),           # arma 4, PARA
]


def decode(src, dst_raw):
    """OGG -> PCM u8 mono a RATE, vía VLC."""
    cmd = [
        VLC, "-I", "dummy", "--no-interact", "--quiet", src,
        "--sout",
        f"#transcode{{acodec=u8,channels=1,samplerate={RATE}}}:"
        f"standard{{access=file,mux=wav,dst={dst_raw}}}",
        "vlc://quit",
    ]
    subprocess.run(cmd, capture_output=True, check=False)


def extract_data(path):
    """Saca el bloque 'data' del WAV que escribe VLC, cuya cabecera es inválida."""
    d = open(path, "rb").read()
    i = d.find(b"data")
    if i < 0:
        raise RuntimeError(f"{path}: sin bloque data")
    size = struct.unpack("<I", d[i + 4:i + 8])[0]
    payload = d[i + 8:]
    # VLC deja el tamaño a 0 o a 0xFFFFFFFF en streams: usa lo que haya.
    if size == 0 or size > len(payload):
        size = len(payload)
    return payload[:size]


def write_wav(path, pcm, rate):
    """WAV PCM de 8 bits sin signo, mono, con cabecera correcta."""
    n = len(pcm)
    hdr = b"RIFF" + struct.pack("<I", 36 + n) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate, 1, 8)
    hdr += b"data" + struct.pack("<I", n)
    open(path, "wb").write(hdr + pcm)


def main():
    if not os.path.isfile(VLC):
        sys.exit("hace falta VLC para decodificar los Ogg")
    os.makedirs(OUT, exist_ok=True)

    total = 0
    for name, res in SOUNDS:
        src = os.path.join(GM, "sounds", name, name + ".ogg")
        if not os.path.isfile(src):
            print(f"  {name}: no existe, saltado")
            continue

        tmp = os.path.join(OUT, "_tmp.wav")
        decode(src, tmp)
        pcm = extract_data(tmp)
        os.remove(tmp)

        dst = os.path.join(OUT, res + ".wav")
        write_wav(dst, pcm, RATE)
        total += len(pcm)
        print(f"  {res + '.wav':20s} {len(pcm) / 1024:7.1f} KB  "
              f"{len(pcm) / RATE:5.2f}s  (de {name}.ogg)")

    print(f"\n  total {total / 1024:.0f} KB de PCM")


if __name__ == "__main__":
    main()
