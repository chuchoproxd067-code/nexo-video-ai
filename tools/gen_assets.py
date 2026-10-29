#!/usr/bin/env python3
"""Genera los sonidos y músicas de ejemplo de NEXO VIDEO AI desde cero (síntesis con numpy).

Todo es original y se dedica al dominio público (CC0 1.0): no hay muestras de terceros.
Uso: python3 tools/gen_assets.py <carpeta_assets>
Requiere numpy y ffmpeg (para comprimir a OGG; si no hay codificador se deja WAV).
"""
import os
import subprocess
import sys
import wave

import numpy as np

SR = 44100
rng = np.random.default_rng(20261027)   # determinista: mismos archivos en cada ejecución


def t_axis(sec):
    return np.arange(int(SR * sec)) / SR


def env_ad(n, attack, decay_power=3.0):
    e = np.ones(n)
    a = max(1, int(SR * attack))
    e[:a] = np.linspace(0, 1, a)
    e[a:] = (1 - np.linspace(0, 1, n - a)) ** decay_power
    return e


def lowpass(x, cutoff):
    # filtro de un polo, suficiente para dar color
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def bandpass_sweep(noise, f0, f1):
    # barrido de paso bajo variable mezclando dos filtros (rápido y vectorizable por bloques)
    n = len(noise)
    out = np.zeros(n)
    block = 512
    acc = 0.0
    for s in range(0, n, block):
        f = f0 * (f1 / f0) ** (s / n)
        a = np.exp(-2 * np.pi * f / SR)
        seg = noise[s:s + block]
        for i, v in enumerate(seg):
            acc = (1 - a) * v + a * acc
            out[s + i] = acc
    return out - lowpass(out, 80)


def norm(x, peak=0.89):
    m = np.max(np.abs(x))
    return x / m * peak if m > 0 else x


def fade_edges(x, ms=8):
    k = int(SR * ms / 1000)
    x = x.copy()
    x[:k] *= np.linspace(0, 1, k)
    x[-k:] *= np.linspace(1, 0, k)
    return x


def save(path, x, ogg=True):
    x = np.clip(x, -1, 1)
    pcm = (x * 32767).astype("<i2")
    wav = path + ".wav"
    with wave.open(wav, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    if ogg:
        r = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", wav,
                            "-c:a", "libvorbis", "-q:a", "5", path + ".ogg"])
        if r.returncode == 0:
            os.remove(wav)
            return path + ".ogg"
    return wav


# ---------- efectos de sonido ----------
def whoosh(sec, f0, f1):
    n = int(SR * sec)
    y = bandpass_sweep(rng.standard_normal(n), f0, f1)
    e = np.sin(np.pi * np.linspace(0, 1, n)) ** 2
    return fade_edges(norm(y * e))


def impact():
    t = t_axis(1.4)
    f = 55 + 120 * np.exp(-t * 14)
    body = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 3.2)
    burst = lowpass(rng.standard_normal(len(t)), 1800) * np.exp(-t * 18) * 0.7
    return fade_edges(norm(body + burst))


def riser(sec=3.5):
    t = t_axis(sec)
    n = len(t)
    f = 200 * (4000 / 200) ** (t / sec)
    tone = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.4
    noise = bandpass_sweep(rng.standard_normal(n), 300, 6000) * 1.2
    e = (t / sec) ** 2
    return fade_edges(norm((tone + norm(noise, 0.6)) * e), 20)


def downer(sec=2.0):
    return fade_edges(riser(sec)[::-1].copy(), 20)


def click():
    t = t_axis(0.08)
    return fade_edges(norm(np.sin(2 * np.pi * 1800 * t) * np.exp(-t * 90)), 1)


def pop():
    t = t_axis(0.18)
    f = 700 * np.exp(-t * 20) + 150
    return fade_edges(norm(np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 22)), 2)


def bell(freq=880, sec=1.8):
    t = t_axis(sec)
    y = sum(a * np.sin(2 * np.pi * freq * k * t) * np.exp(-t * d)
            for k, a, d in [(1, 1, 2.2), (2.76, 0.5, 3.4), (5.4, 0.25, 5.0), (8.9, 0.1, 8)])
    return fade_edges(norm(y))


def chime_up():
    parts = [bell(f, 1.2) for f in (660, 880, 1320)]
    out = np.zeros(int(SR * 1.8))
    for i, p in enumerate(parts):
        s = int(SR * 0.12 * i)
        out[s:s + len(p)] += p * 0.7
    return fade_edges(norm(out))


def glitch():
    n = int(SR * 0.5)
    y = np.zeros(n)
    pos = 0
    while pos < n:
        ln = int(SR * rng.uniform(0.01, 0.05))
        f = rng.choice([220, 440, 880, 1760, 3520])
        seg = np.sign(np.sin(2 * np.pi * f * t_axis(ln / SR)[:ln])) * rng.uniform(0.3, 1)
        y[pos:pos + ln] = seg[:max(0, min(ln, n - pos))]
        pos += ln + int(SR * rng.uniform(0, 0.03))
    return fade_edges(norm(y * 0.8))


# ---------- música ----------
NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def hz(note, octave):
    return 440.0 * 2 ** ((NOTE[note] + 12 * (octave - 4) - 9) / 12)


def pad(freqs, sec, vol=0.2):
    t = t_axis(sec)
    y = np.zeros(len(t))
    for f in freqs:
        for det in (-0.4, 0.0, 0.4):
            y += np.sin(2 * np.pi * (f + det) * t) + 0.3 * np.sin(2 * np.pi * 2 * (f + det) * t)
    k = int(SR * min(0.5, sec / 3))
    e = np.ones(len(t)); e[:k] = np.linspace(0, 1, k); e[-k:] = np.linspace(1, 0, k)
    return y * e * vol / len(freqs)


def place(buf, x, start_sec):
    s = int(SR * start_sec)
    end = min(len(buf), s + len(x))
    buf[s:end] += x[:end - s]


def kick():
    t = t_axis(0.35)
    f = 50 + 110 * np.exp(-t * 28)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)


def hat():
    t = t_axis(0.06)
    return rng.standard_normal(len(t)) * np.exp(-t * 70) * 0.35


def snare():
    t = t_axis(0.22)
    return (rng.standard_normal(len(t)) * 0.6 + np.sin(2 * np.pi * 190 * t) * 0.5) * np.exp(-t * 20)


def pluck(freq, sec=0.4):
    t = t_axis(sec)
    y = np.sin(2 * np.pi * freq * t) + 0.4 * np.sin(2 * np.pi * 2 * freq * t) + 0.2 * np.sin(2 * np.pi * 3 * freq * t)
    return y * np.exp(-t * 7)


def music_ambient(bars=8, bpm=60):
    beat = 60 / bpm
    bar = 4 * beat
    chords = [("A", 3, ["A", "C", "E"]), ("F", 3, ["F", "A", "C"]), ("C", 3, ["C", "E", "G"]), ("G", 3, ["G", "B", "D"])]
    buf = np.zeros(int(SR * bar * bars))
    for i in range(bars):
        root, octv, names = chords[i % 4]
        fr = [hz(n, octv + (1 if n in ("C", "D") and root in ("A", "F") else 0)) for n in names]
        place(buf, pad(fr, bar * 1.1), i * bar)
        place(buf, pad([hz(root, 2)], bar * 1.05, 0.25), i * bar)
    return fade_edges(norm(lowpass(buf, 5000), 0.7), 30)


def music_pulse(bars=8, bpm=120):
    beat = 60 / bpm
    bar = 4 * beat
    buf = np.zeros(int(SR * bar * bars))
    prog = [("A", 2), ("F", 2), ("C", 3), ("G", 2)]
    arp = {"A": ["A", "C", "E", "C"], "F": ["F", "A", "C", "A"], "C": ["C", "E", "G", "E"], "G": ["G", "B", "D", "B"]}
    for b in range(bars):
        root, octv = prog[b % 4]
        for q in range(4):
            place(buf, kick() * 0.9, b * bar + q * beat)
            place(buf, hat(), b * bar + q * beat + beat / 2)
        place(buf, snare() * 0.6, b * bar + beat)
        place(buf, snare() * 0.6, b * bar + 3 * beat)
        for e in range(8):
            place(buf, pluck(hz(arp[root][e % 4], 4), 0.3) * 0.28, b * bar + e * beat / 2)
        place(buf, pluck(hz(root, octv), bar * 0.9) * 0.55, b * bar)
    return fade_edges(norm(buf, 0.8), 30)


def music_lofi(bars=8, bpm=80):
    beat = 60 / bpm
    bar = 4 * beat
    buf = np.zeros(int(SR * bar * bars))
    chords = [[("D", 4), ("F", 4), ("A", 4), ("C", 5)], [("G", 3), ("B", 3), ("D", 4), ("F", 4)],
              [("C", 4), ("E", 4), ("G", 4), ("B", 4)], [("A", 3), ("C", 4), ("E", 4), ("G", 4)]]
    for b in range(bars):
        for n, o in chords[b % 4]:
            place(buf, pluck(hz(n, o), bar * 0.95) * 0.16, b * bar)
            place(buf, pluck(hz(n, o), 0.8) * 0.1, b * bar + 2.5 * beat)
        place(buf, kick() * 0.8, b * bar)
        place(buf, kick() * 0.7, b * bar + 2.5 * beat)
        place(buf, snare() * 0.5, b * bar + beat)
        place(buf, snare() * 0.5, b * bar + 3 * beat)
        for q in range(8):
            place(buf, hat() * 0.8, b * bar + q * beat / 2)
    return fade_edges(norm(lowpass(buf, 4200), 0.75), 30)


# ---------------------------------------------------------------------------------------------
# Patrones de transición (lumas PGM): negro = se revela primero, blanco = al final.

LW, LH = 640, 360


def luma_patterns():
    y, x = np.mgrid[0:LH, 0:LW]
    x = (x + 0.5) / LW
    y = (y + 0.5) / LH
    ar = LW / LH
    dx, dy = (x - 0.5) * ar, (y - 0.5)
    r = np.sqrt(dx ** 2 + dy ** 2)
    ang = (np.arctan2(dy, dx) + np.pi) / (2 * np.pi)
    cx, cy = np.floor(x * 10).astype(int), np.floor(y * 6).astype(int)
    cell = np.random.default_rng(7).random((6, 10))[cy, cx]
    bar = np.floor(x * 10).astype(int)
    return {
        "wipe-right": x, "wipe-left": 1 - x, "wipe-down": y, "wipe-up": 1 - y,
        "diagonal": (x + y) / 2, "diagonal-inv": (x + 1 - y) / 2,
        "circle-open": r, "circle-close": 1 - r,
        "diamond": np.abs(dx) / ar + np.abs(dy),
        "clock": ang,
        "blinds-h": (y * 8) % 1.0, "blinds-v": (x * 12) % 1.0,
        "bars-v": np.where(bar % 2 == 0, y, 1 - y),
        "checker": cell * 0.85 + 0.15 * ((x * 10) % 1.0),
        "spiral": (ang + r * 2.2) % 1.0,
    }


def save_lumas(out):
    d = os.path.join(out, "lumas")
    os.makedirs(d, exist_ok=True)
    for name, v in luma_patterns().items():
        v = (v - v.min()) / (v.max() - v.min())
        data = np.round(v * 255).astype(np.uint8)
        with open(os.path.join(d, name + ".pgm"), "wb") as f:
            f.write(b"P5\n%d %d\n255\n" % (LW, LH))
            f.write(data.tobytes())
        print(os.path.relpath(os.path.join(d, name + ".pgm")))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "assets"
    if "--lumas" in sys.argv:          # solo los patrones de transición (no regenera los sonidos)
        save_lumas(out)
        return
    sfx = os.path.join(out, "sfx"); music = os.path.join(out, "music")
    os.makedirs(sfx, exist_ok=True); os.makedirs(music, exist_ok=True)
    items = [
        ("sfx", "whoosh-corto", whoosh(0.6, 400, 5000)),
        ("sfx", "whoosh-largo", whoosh(1.4, 250, 7000)),
        ("sfx", "whoosh-descendente", whoosh(1.0, 7000, 300)),
        ("sfx", "impacto", impact()),
        ("sfx", "subida-tension", riser()),
        ("sfx", "bajada", downer()),
        ("sfx", "clic", click()),
        ("sfx", "pop", pop()),
        ("sfx", "campana", bell()),
        ("sfx", "campanilla-ascendente", chime_up()),
        ("sfx", "glitch", glitch()),
        ("music", "ambiental-calma", music_ambient()),
        ("music", "pulso-energico", music_pulse()),
        ("music", "lofi-relajado", music_lofi()),
    ]
    for kind, name, x in items:
        p = save(os.path.join(sfx if kind == "sfx" else music, name), x)
        print(os.path.relpath(p), f"{len(x) / SR:.1f}s")
    save_lumas(out)


if __name__ == "__main__":
    main()
