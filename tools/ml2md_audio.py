#!/usr/bin/env python3
"""Convierte los sonidos de MonsterLite a WAV para rescomp (bloque 12).

Los clips salen de la biblioteca de SoundManager tal como la serializa la
escena (Assets/Scenes/MonsterLite.unity, componente SoundManager: el prefab
tiene la biblioteca vieja, sin las voces de mujer ni el "ding"). El
driver XGM2 reproduce PCM de 8 bits sin signo a 13300 Hz, así que todo se baja
a 13300 Hz mono, como tools/gm2md_audio.py hace con los Ogg del GML.

Aquí no hace falta VLC: los originales son WAV y AIFF, que afconvert (de
macOS) lee y remuestrea. Dos pasos con afconvert:
  1. el original a PCM de 16 bits mono, a su frecuencia;
  2. ese PCM, con la frecuencia de la cabecera multiplicada por el tono que
     le pone SoundManager (playDing pone pitch 3 y playCaralludoClip pitch 2),
     a 8 bits sin signo a 13300 Hz. Declarar el doble de frecuencia y
     remuestrear es reproducirlo al doble de velocidad: el tono que oiría
     Unity, con el remuestreo de afconvert (con filtro) y no a pelo.
Después se recorta el silencio del final (muestras a +-1 del centro), que
sólo ocupa ROM: el principio no se toca, para no adelantar el sonido.

El tono aleatorio entre 0,9 y 1,1 que SoundManager pone a casi todo no se
puede reproducir (XGM2 no cambia el tono de una muestra): se convierten a
tono 1. Ver docs/PORT_AUDIT_GML.md, bloque 12.

Desde la quinta entrega del bloque 12, además, las mezclas de la bomba y
los rayos (MIXES, más abajo).

Uso:  python3 tools/ml2md_audio.py
"""

import os
import struct
import subprocess
import sys
import tempfile
import wave

ML = os.path.expanduser("~/MonsterLite/MonsterLite_ultimo_commit/Assets/Sound")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "sound")
RATE = 13300

# (recurso, fichero de Assets/Sound, tono, campo de la biblioteca)
SOUNDS = [
    ("mlShot",       "shot.wav",              1, "nBulletSnd"),
    ("mlWBlock",     "shotBasic.wav",         1, "wBulletSnd"),
    ("mlLaser",      "laser.wav",             1, "laserBulletSnd y moireBulletSnd"),
    ("mlTway",       "3way.wav",              1, "triBulletSnd"),
    ("mlCircular",   "retroUFO.wav",          1, "circularBulletSnd"),
    ("mlXpl",        "explosionDePolvo2.wav", 1, "bombaXPLSnd"),
    ("mlPowUp",      "powUp3Wah_d.wav",       1, "pillaPaqueteGameSnd"),
    ("mlRepetea",    "repeteaSpeech.wav",     1, "repeteaSpeechSnd"),
    ("mlArena",      "woman voice_short.wav", 1, "arenaWomanSnd"),
    ("mlAwesome",    "awe3.wav",              1, "awesomeWomanSnd"),
    ("mlCaralludo",  "Fafarre.aiff",          2, "caralludoSpeechSnd (pitch 2)"),
    ("mlDing",       "ding.wav",              3, "dingSnd (pitch 3)"),
    ("mlBtnShort",   "buttonShort.wav",       1, "buttonShortSnd"),
    ("mlBtnLong",    "buttonLong.wav",        1, "buttonLongSnd"),
]

# Las MEZCLAS (decisión del usuario del 2026-11-26): lo que MonsterLite toca
# a la vez con PlayOneShot en un mismo evento, sumado aquí en una sola
# muestra para que ocupe un solo canal de XGM2 (con dos canales para efectos,
# la bomba con tres voces separadas dejaba sin sitio al resto). Todas
# arrancan en el mismo frame en MonsterLite, así que se suman desde la
# muestra 0. Se suman en 16 bits y se recorta lo que se pase, como haría el
# mezclador de Unity sin limitador.
#   bomba  (S/playerMovement.cs:199-210): playBoomClip (Boom.wav,
#          boomSpeechSnd), bombaAmarillaXPLSnd y explosionWomanSnd
#   rayos  (S/playerMovement.cs:212-220): bombaAmarillaXPLSnd y raysWomanSnd
MIXES = [
    ("mlBombMix", ["Boom.wav", "bombaamarilla.wav", "xplosions.wav"], "bomba"),
    ("mlRaysMix", ["bombaamarilla.wav", "rays.wav"], "rayos"),
]


def afconvert(src, dst, fmt):
    subprocess.run(["afconvert", "-f", "WAVE", "-d", fmt, "-c", "1", src, dst],
                   check=True, capture_output=True)


def with_rate(src, dst, factor):
    """Copia un WAV de 16 bits cambiando sólo la frecuencia de la cabecera."""
    w = wave.open(src)
    params = w.getparams()
    frames = w.readframes(w.getnframes())
    w.close()
    o = wave.open(dst, "wb")
    o.setnchannels(params.nchannels)
    o.setsampwidth(params.sampwidth)
    o.setframerate(params.framerate * factor)
    o.writeframes(frames)
    o.close()


def write_wav(path, pcm, rate):
    """WAV PCM de 8 bits sin signo, mono, con cabecera mínima."""
    n = len(pcm)
    hdr = b"RIFF" + struct.pack("<I", 36 + n) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate, 1, 8)
    hdr += b"data" + struct.pack("<I", n)
    with open(path, "wb") as f:
        f.write(hdr + pcm)


def main():
    if not os.path.isdir(ML):
        sys.exit(f"no existe {ML}")
    os.makedirs(OUT, exist_ok=True)

    total = 0
    with tempfile.TemporaryDirectory() as tmp:
        for res, name, pitch, field in SOUNDS:
            src = os.path.join(ML, name)
            a = os.path.join(tmp, "a.wav")
            b = os.path.join(tmp, "b.wav")
            c = os.path.join(tmp, "c.wav")

            afconvert(src, a, "LEI16")
            with_rate(a, b, pitch)
            afconvert(b, c, f"UI8@{RATE}")

            w = wave.open(c)
            pcm = bytearray(w.readframes(w.getnframes()))
            w.close()

            end = len(pcm)
            while end > 1 and abs(pcm[end - 1] - 128) <= 1:
                end -= 1
            pcm = bytes(pcm[:end])

            write_wav(os.path.join(OUT, res + ".wav"), pcm, RATE)
            total += len(pcm)
            print(f"  {res + '.wav':18s} {len(pcm) / 1024:6.1f} KB  "
                  f"{len(pcm) / RATE:5.2f}s  {name} ({field})")

        for res, names, what in MIXES:
            acc = []
            for name in names:
                a = os.path.join(tmp, "a.wav")
                afconvert(os.path.join(ML, name), a, f"LEI16@{RATE}")
                w = wave.open(a)
                raw = w.readframes(w.getnframes())
                w.close()
                v = struct.unpack("<%dh" % (len(raw) // 2), raw)
                if len(v) > len(acc):
                    acc += [0] * (len(v) - len(acc))
                for i, x in enumerate(v):
                    acc[i] += x
            clipped = sum(1 for x in acc if (x > 32767) or (x < -32768))
            pcm = bytearray()
            for x in acc:
                x = max(-32768, min(32767, x))
                pcm.append(max(0, min(255, (x + 32768 + 128) >> 8)))
            end = len(pcm)
            while end > 1 and abs(pcm[end - 1] - 128) <= 1:
                end -= 1
            pcm = bytes(pcm[:end])
            write_wav(os.path.join(OUT, res + ".wav"), pcm, RATE)
            total += len(pcm)
            print(f"  {res + '.wav':18s} {len(pcm) / 1024:6.1f} KB  "
                  f"{len(pcm) / RATE:5.2f}s  {what}: {' + '.join(names)}"
                  f" ({100.0 * clipped / max(1, len(acc)):.1f} % recortado)")

    print(f"\n  total {total / 1024:.0f} KB de PCM")


if __name__ == "__main__":
    main()
