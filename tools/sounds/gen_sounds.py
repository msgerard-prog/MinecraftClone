#!/usr/bin/env python3
"""Generate the game's sounds (M22.4, ADR 0008): our own synthesis, nothing sampled.

Writes assets/minecraft/sounds/<folder>/<name>.wav - 16-bit mono 22,050 Hz PCM, in
vanilla's folder layout (dig/, step/, mob/<mob>/, random/, ambient/...), so a resource
pack can replace a file with a WAV of the same name. world/Sounds.cpp lists which
files each sound event plays.

Deterministic (seeded per file). Pure Python. Usage: tools/sounds/gen_sounds.py
"""
import math
import random
import struct
import sys
from pathlib import Path

RATE = 22050
OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/sounds"


# --- Building blocks ---------------------------------------------------------------

def silence(seconds):
    return [0.0] * int(seconds * RATE)


def noise(n, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def lowpass(x, cutoff):
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
    y, out = 0.0, []
    for s in x:
        y += a * (s - y)
        out.append(y)
    return out


def highpass(x, cutoff):
    low = lowpass(x, cutoff)
    return [s - l for s, l in zip(x, low)]


def bandpass(x, lo, hi):
    return lowpass(highpass(x, lo), hi)


def resonator(x, freq, q):
    """Two-pole resonant filter (ringing body: wood knocks, metal)."""
    r = math.exp(-math.pi * freq / (q * RATE))
    c = 2.0 * r * math.cos(2.0 * math.pi * freq / RATE)
    y1 = y2 = 0.0
    out = []
    for s in x:
        y = s * (1 - r) + c * y1 - r * r * y2
        y2, y1 = y1, y
        out.append(y)
    return out


def env(n, attack, decay, sustain=0.0):
    """Linear attack (seconds), then exponential decay with time constant `decay`."""
    a = max(1, int(attack * RATE))
    out = []
    for i in range(n):
        if i < a:
            out.append(i / a)
        else:
            t = (i - a) / RATE
            out.append(sustain + (1.0 - sustain) * math.exp(-t / decay))
    return out


def mul(x, y):
    return [a * b for a, b in zip(x, y)]


def add(*tracks):
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for t in tracks:
        for i, s in enumerate(t):
            out[i] += s
    return out


def at(track, seconds, total):
    """Place `track` starting at `seconds` in a buffer of `total` seconds."""
    out = silence(total)
    o = int(seconds * RATE)
    for i, s in enumerate(track):
        if o + i < len(out):
            out[o + i] += s
    return out


def tone(seconds, f0, f1=None, wave="sine", vibrato=0.0, vib_rate=0.0, rng=None):
    """A tone gliding from f0 to f1 (exponentially), optional vibrato."""
    n = int(seconds * RATE)
    f1 = f0 if f1 is None else f1
    phase, out = 0.0, []
    for i in range(n):
        t = i / max(1, n - 1)
        f = f0 * (f1 / f0) ** t
        if vibrato:
            f *= 1.0 + vibrato * math.sin(2 * math.pi * vib_rate * i / RATE)
        phase += f / RATE
        p = phase % 1.0
        if wave == "sine":
            s = math.sin(2 * math.pi * p)
        elif wave == "saw":
            s = 2.0 * p - 1.0
        elif wave == "square":
            s = 1.0 if p < 0.5 else -1.0
        else:  # triangle
            s = 4.0 * abs(p - 0.5) - 1.0
        out.append(s)
    return out


def normalize(x, peak=0.8):
    m = max(1e-9, max(abs(s) for s in x))
    return [s * peak / m for s in x]


def fade_out(x, seconds=0.01):
    n = min(len(x), int(seconds * RATE))
    for i in range(n):
        x[len(x) - 1 - i] *= i / n
    return x


def write(name, samples, peak=0.8):
    path = OUT / f"{name}.wav"
    path.parent.mkdir(parents=True, exist_ok=True)
    data = normalize(fade_out(list(samples)), peak)
    pcm = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in data)
    header = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(pcm))
    path.write_bytes(header + pcm)


# --- Block materials (vanilla sound types: dig = break/place, step = step/hit) ------

def crunch(rng, seconds, grains, lo, hi, grain_len=0.012, decay=0.08):
    """Granular crunch: many short noise grains, denser at the start."""
    total = seconds
    out = silence(total)
    for _ in range(grains):
        start = (rng.random() ** 2) * (total - grain_len)
        g = mul(noise(int(grain_len * RATE), rng), env(int(grain_len * RATE), 0.001, grain_len / 3))
        g = [s * rng.uniform(0.3, 1.0) for s in g]
        out = add(out, at(g, start, total))
    out = bandpass(out, lo, hi)
    return mul(out, env(len(out), 0.002, decay))


def material(kind, rng, step):
    """One variant of a material's dig (step=False) or step sound."""
    s = 0.6 if step else 1.0
    if kind == "stone":
        return crunch(rng, 0.22 * s, int(40 * s), 600, 3500, decay=0.06 * s)
    if kind == "wood":
        knock = resonator(mul(noise(int(0.2 * s * RATE), rng), env(int(0.2 * s * RATE), 0.001, 0.01)),
                          rng.uniform(180, 320), 8)
        return add([k * 6 for k in knock], crunch(rng, 0.2 * s, 12, 400, 2500, decay=0.05))
    if kind == "gravel":
        return crunch(rng, 0.3 * s, int(90 * s), 300, 2200, grain_len=0.006, decay=0.12 * s)
    if kind == "grass":
        x = bandpass(noise(int(0.25 * s * RATE), rng), 1500, 6000)
        return mul(x, env(len(x), 0.03 * s, 0.07 * s))
    if kind == "sand":
        x = crunch(rng, 0.25 * s, int(120 * s), 800, 4000, grain_len=0.004, decay=0.09 * s)
        return add(x, mul(bandpass(noise(len(x), rng), 2000, 5000), env(len(x), 0.02, 0.05)))
    if kind == "wool":
        x = lowpass(noise(int(0.2 * s * RATE), rng), 700)
        return mul(x, env(len(x), 0.02 * s, 0.05 * s))
    if kind == "snow":
        return crunch(rng, 0.22 * s, int(50 * s), 400, 2800, grain_len=0.01, decay=0.08 * s)
    if kind == "metal":
        n = int(0.5 * s * RATE)
        strike = mul(noise(n, rng), env(n, 0.001, 0.004))
        ring = add(*[resonator(strike, f * rng.uniform(0.97, 1.03), 60) for f in (520, 1230, 1890)])
        return mul(add([r * 8 for r in ring], strike), env(n, 0.001, 0.15 * s))
    if kind == "glass":  # glass steps/digs sound like stone in vanilla too
        return crunch(rng, 0.2 * s, int(35 * s), 1200, 5000, decay=0.05 * s)
    raise ValueError(kind)


MATERIALS = ["stone", "wood", "gravel", "grass", "sand", "wool", "snow", "metal", "glass"]


def glass_break(rng):
    total = 0.7
    out = silence(total)
    for _ in range(14):
        f = rng.uniform(2200, 6500)
        ping = mul(tone(0.4, f), env(int(0.4 * RATE), 0.001, rng.uniform(0.04, 0.12)))
        out = add(out, at([p * rng.uniform(0.3, 1.0) for p in ping], rng.random() * 0.25, total))
    hit = mul(highpass(noise(int(0.3 * RATE), rng), 1500), env(int(0.3 * RATE), 0.001, 0.05))
    return add(out, at(hit, 0.0, total))


# --- Mobs ---------------------------------------------------------------------------

def voice(rng, seconds, f0, f1, wave="saw", formant=(300, 2200), vibrato=0.0, vib_rate=5.0,
          breath=0.15, attack=0.04, decay=None):
    x = tone(seconds, f0, f1, wave, vibrato, vib_rate)
    x = bandpass(x, *formant)
    if breath:
        x = add(x, [s * breath for s in bandpass(noise(len(x), rng), formant[0], formant[1] * 2)])
    return mul(x, env(len(x), attack, decay or seconds / 2.5, 0.0))


def mob_sounds(name, rng):
    """(say variants, hurt variants, death) for a mob; None where vanilla has none."""
    r = rng.uniform
    if name == "zombie":
        say = [voice(rng, r(0.7, 1.0), r(80, 100), r(65, 80), formant=(150, 900), vibrato=0.05, breath=0.4)
               for _ in range(3)]
        hurt = [voice(rng, 0.35, r(130, 150), 100, formant=(200, 1200), breath=0.4) for _ in range(2)]
        return say, hurt, voice(rng, 1.1, 110, 50, formant=(150, 900), breath=0.5)
    if name == "cow":
        say = [voice(rng, r(0.9, 1.2), r(115, 130), r(95, 105), formant=(200, 1100), vibrato=0.02, breath=0.05,
                     attack=0.12) for _ in range(3)]
        hurt = [voice(rng, 0.4, 160, 120, formant=(250, 1300), breath=0.1) for _ in range(2)]
        return say, hurt, None  # (vanilla: the hurt sound)
    if name == "pig":
        def oink():
            g = voice(rng, 0.14, r(300, 360), r(220, 260), wave="square", formant=(400, 2000), breath=0.2,
                      attack=0.01)
            return add(at(g, 0, 0.45), at(g, r(0.18, 0.24), 0.45))
        say = [oink() for _ in range(3)]
        return say, [voice(rng, 0.3, 500, 380, wave="square", formant=(500, 2500))], \
            voice(rng, 0.7, 420, 200, wave="square", formant=(400, 2200), breath=0.3)
    if name == "sheep":
        say = [voice(rng, r(0.6, 0.8), r(280, 320), r(260, 290), formant=(400, 2600), vibrato=0.08, vib_rate=11,
                     breath=0.1) for _ in range(3)]
        return say, say[:1], None
    if name == "chicken":
        def cluck():
            out = silence(0.5)
            for k in range(rng.randint(2, 4)):
                c = voice(rng, 0.07, r(600, 800), r(450, 600), wave="square", formant=(600, 3000), breath=0.1,
                          attack=0.005)
                out = add(out, at(c, k * 0.11, 0.5))
            return out
        say = [cluck() for _ in range(3)]
        return say, [voice(rng, 0.2, 900, 700, wave="square", formant=(700, 3500))], None
    if name == "skeleton":
        def rattle(n, total):
            out = silence(total)
            for _ in range(n):
                k = resonator(mul(noise(400, rng), env(400, 0.0005, 0.003)), r(900, 1800), 12)
                out = add(out, at([s * 4 for s in k], rng.random() * (total - 0.03), total))
            return out
        return [rattle(10, 0.4) for _ in range(3)], [rattle(16, 0.3)], rattle(30, 0.8)
    if name == "creeper":  # no ambient sound; hurt and death are breathy rasps
        rasp = lambda d: mul(bandpass(noise(int(d * RATE), rng), 300, 1800), env(int(d * RATE), 0.02, d / 3))
        return None, [rasp(0.3) for _ in range(2)], rasp(0.6)
    if name == "spider":
        def hiss():
            x = bandpass(noise(int(0.5 * RATE), rng), 2000, 7000)
            x = mul(x, [0.5 + 0.5 * math.sin(2 * math.pi * 30 * i / RATE) for i in range(len(x))])
            return mul(x, env(len(x), 0.05, 0.15))
        return [hiss() for _ in range(2)], [hiss()], voice(rng, 0.6, 400, 150, formant=(300, 3000), breath=0.6)
    if name == "enderman":
        say = [voice(rng, r(0.6, 0.9), r(300, 400), r(150, 250), wave="sine", formant=(100, 3000), vibrato=0.15,
                     vib_rate=7, breath=0.05) for _ in range(3)]
        return say, [voice(rng, 0.4, 600, 300, wave="sine", formant=(100, 3000), vibrato=0.2, vib_rate=9)], \
            voice(rng, 1.4, 500, 80, wave="sine", formant=(80, 3000), vibrato=0.25, vib_rate=6)
    if name == "ghast":
        say = [voice(rng, r(1.0, 1.4), r(550, 650), r(480, 560), wave="sine", formant=(300, 3000), vibrato=0.06,
                     vib_rate=5, breath=0.1, attack=0.2) for _ in range(3)]
        return say, [voice(rng, 0.5, 900, 700, wave="saw", formant=(500, 3500), breath=0.2)], \
            voice(rng, 1.4, 900, 300, wave="saw", formant=(400, 3500), vibrato=0.1, vib_rate=8)
    if name == "blaze":
        def breathe():
            x = lowpass(noise(int(1.0 * RATE), rng), 900)
            x = mul(x, [max(0.0, math.sin(2 * math.pi * 1.5 * i / RATE)) for i in range(len(x))])
            return x
        return [breathe() for _ in range(2)], [voice(rng, 0.3, 300, 200, formant=(200, 2000), breath=0.6)], \
            voice(rng, 0.8, 250, 80, formant=(150, 2000), breath=0.7)
    if name in ("magma_cube", "slime"):
        low = 70 if name == "magma_cube" else 110
        def squish(d):
            thud = mul(tone(d, low * 1.6, low, "sine"), env(int(d * RATE), 0.003, d / 4))
            wet = mul(bandpass(noise(int(d * RATE), rng), 400, 2500), env(int(d * RATE), 0.005, d / 6))
            return add([t * 2 for t in thud], wet)
        return None, [squish(0.25) for _ in range(2)], squish(0.4)
    if name == "piglin":
        say = [voice(rng, r(0.3, 0.5), r(180, 220), r(140, 170), wave="square", formant=(250, 1800), breath=0.35,
                     attack=0.02) for _ in range(3)]
        return say, [voice(rng, 0.3, 260, 200, wave="square", formant=(300, 2000), breath=0.3)], \
            voice(rng, 0.8, 220, 90, wave="square", formant=(200, 1800), breath=0.4)
    if name == "zombified_piglin":
        say = [voice(rng, r(0.6, 0.9), r(110, 130), r(85, 100), wave="square", formant=(180, 1200), vibrato=0.04,
                     breath=0.45) for _ in range(3)]
        return say, [voice(rng, 0.35, 170, 130, wave="square", formant=(200, 1400), breath=0.4)], \
            voice(rng, 1.0, 140, 60, wave="square", formant=(150, 1200), breath=0.5)
    if name == "hoglin":
        say = [voice(rng, r(0.4, 0.6), r(90, 110), r(70, 85), wave="saw", formant=(120, 900), breath=0.5)
               for _ in range(3)]
        return say, [voice(rng, 0.3, 140, 100, formant=(150, 1100), breath=0.5)], \
            voice(rng, 0.9, 120, 50, formant=(100, 900), breath=0.6)
    if name == "strider":
        say = [voice(rng, r(0.3, 0.5), r(500, 700), r(400, 900), wave="triangle", formant=(300, 3000),
                     vibrato=0.1, vib_rate=14, breath=0.05) for _ in range(2)]
        return say, say[:1], voice(rng, 0.6, 700, 250, wave="triangle", formant=(300, 3000), vibrato=0.1)
    if name == "shulker":
        def clack():
            k = resonator(mul(noise(1500, rng), env(1500, 0.0005, 0.006)), r(250, 400), 10)
            return add(at([s * 5 for s in k], 0, 0.3))
        return [clack() for _ in range(2)], [clack()], voice(rng, 0.5, 400, 150, formant=(200, 2000))
    if name == "ender_dragon":
        roar = lambda d, f0, f1: add(voice(rng, d, f0, f1, formant=(80, 1500), breath=0.8, attack=0.2),
                                     [s * 0.5 for s in voice(rng, d, f0 * 1.5, f1 * 1.5, formant=(150, 2500))])
        return [roar(2.0, 70, 55) for _ in range(2)], [roar(0.6, 110, 80)], roar(3.0, 80, 35)
    if name == "villager":  # a short nasal "hmm" (ours), rising on "yes", falling when hurt
        say = [voice(rng, r(0.35, 0.5), r(150, 170), r(170, 200), wave="saw", formant=(500, 2600), vibrato=0.03,
                     breath=0.1, attack=0.03) for _ in range(3)]
        hurt = [voice(rng, 0.3, r(220, 240), 160, wave="saw", formant=(500, 2600), breath=0.2) for _ in range(2)]
        return say, hurt, voice(rng, 0.8, 200, 90, wave="saw", formant=(400, 2400), breath=0.3)
    if name == "zombie_villager":  # a groaning, nasal "hmm" (ours)
        say = [voice(rng, r(0.6, 0.9), r(110, 130), r(85, 100), wave="saw", formant=(300, 2000), vibrato=0.05,
                     breath=0.4) for _ in range(3)]
        hurt = [voice(rng, 0.35, r(160, 180), 120, wave="saw", formant=(350, 2000), breath=0.4) for _ in range(2)]
        return say, hurt, voice(rng, 1.0, 140, 60, wave="saw", formant=(250, 1800), breath=0.5)
    if name == "iron_golem":  # clanks: metal hits ringing (ours); golems have no idle sound
        def clank(f, d):
            return mul(add(*[resonator(mul(noise(int(d * RATE), rng), env(int(d * RATE), 0.0005, 0.004)), f * p, 60)
                             for p in (1.0, 2.4)]), env(int(d * RATE), 0.001, d / 3))
        return None, [clank(r(180, 220), 0.4) for _ in range(2)], add(clank(140, 0.8), at(clank(100, 0.6), 0.25, 0.8))
    if name == "witch":  # a cackle (ours): quick rising-falling laughs
        def cackle(n):
            out = silence(0.12 * n + 0.2)
            for k in range(n):
                out = add(out, at(voice(rng, 0.12, r(500, 600), r(380, 450), wave="saw", formant=(700, 3000), breath=0.2,
                                        attack=0.01), k * 0.12, 0.12 * n + 0.2))
            return out
        return [cackle(3 + i) for i in range(3)], [cackle(1), cackle(2)], cackle(5)
    if name == "wandering_trader":  # the villager "hmm", a little lower and slower (ours)
        say = [voice(rng, r(0.45, 0.6), r(130, 150), r(150, 175), wave="saw", formant=(450, 2400), vibrato=0.03,
                     breath=0.12, attack=0.04) for _ in range(3)]
        hurt = [voice(rng, 0.3, r(200, 220), 150, wave="saw", formant=(450, 2400), breath=0.2) for _ in range(2)]
        return say, hurt, voice(rng, 0.8, 180, 80, wave="saw", formant=(400, 2200), breath=0.3)
    if name == "pillager":  # a nasal grumble (ours)
        say = [voice(rng, r(0.35, 0.55), r(120, 140), r(100, 115), wave="saw", formant=(400, 2200), breath=0.3)
               for _ in range(3)]
        hurt = [voice(rng, 0.3, r(190, 210), 140, wave="saw", formant=(400, 2200), breath=0.3) for _ in range(2)]
        return say, hurt, voice(rng, 0.8, 170, 70, wave="saw", formant=(350, 2000), breath=0.4)
    if name in ("vindicator", "evoker"):  # grumbles, the evoker higher (ours)
        f = 125 if name == "vindicator" else 165
        say = [voice(rng, r(0.35, 0.55), f * r(0.95, 1.05), f * 0.85, wave="saw", formant=(380, 2200), breath=0.3)
               for _ in range(3)]
        hurt = [voice(rng, 0.3, f * 1.5, f * 1.1, wave="saw", formant=(400, 2200), breath=0.3) for _ in range(2)]
        return say, hurt, voice(rng, 0.8, f * 1.3, f * 0.5, wave="saw", formant=(350, 2000), breath=0.4)
    if name == "vex":  # thin shrieks (ours)
        say = [voice(rng, r(0.25, 0.4), r(900, 1100), r(1200, 1500), wave="triangle", formant=(800, 5000),
                     vibrato=0.08, vib_rate=18, breath=0.1) for _ in range(3)]
        return say, say[:2], voice(rng, 0.6, 1300, 500, wave="triangle", formant=(600, 5000), vibrato=0.1)
    if name == "ravager":  # deep roars (ours)
        roar = lambda d, f0, f1: voice(rng, d, f0, f1, wave="saw", formant=(60, 900), breath=0.7, attack=0.05)
        return [roar(1.0, 70, 55) for _ in range(3)], [roar(0.5, 110, 80) for _ in range(2)], roar(1.6, 90, 35)
    if name in ("cod", "salmon", "tropical_fish", "pufferfish"):  # wet flops (ours); fish have no voice
        def flop(f):
            n = int(0.15 * RATE)
            return mul(add(resonator(mul(noise(n, rng), env(n, 0.001, 0.01)), f, 12),
                           [s * 0.4 for s in lowpass(noise(n, rng), 900)]), env(n, 0.002, 0.04))
        base = {"cod": 420, "salmon": 380, "tropical_fish": 520, "pufferfish": 330}[name]
        return None, [flop(base * r(0.9, 1.1)) for _ in range(2)], add(flop(base * 0.8), at(flop(base * 0.7), 0.12, 0.3))
    if name in ("squid", "glow_squid"):  # soft squelches (ours)
        def squish(d, f0, f1):
            x = tone(d, f0, f1, "triangle", vibrato=0.05, vib_rate=9)
            return mul(add(lowpass(x, 900), [s * 0.3 for s in bandpass(noise(int(d * RATE), rng), 200, 1500)]),
                       env(int(d * RATE), 0.03, d / 2))
        lift = 1.2 if name == "glow_squid" else 1.0
        return ([squish(r(0.4, 0.6), 140 * lift, 110 * lift) for _ in range(3)],
                [squish(0.25, 220 * lift, 160 * lift) for _ in range(2)], squish(0.8, 180 * lift, 70 * lift))
    if name == "player":
        hurt = [add(voice(rng, 0.18, r(190, 220), 150, formant=(200, 1500), breath=0.3, attack=0.005),
                    mul(lowpass(noise(int(0.18 * RATE), rng), 300), env(int(0.18 * RATE), 0.001, 0.03)))
                for _ in range(3)]
        return None, hurt, None
    raise ValueError(name)


MOBS = ["zombie", "cow", "pig", "sheep", "chicken", "skeleton", "creeper", "spider", "enderman", "ghast",
        "blaze", "magma_cube", "slime", "piglin", "zombified_piglin", "hoglin", "strider", "shulker",
        "ender_dragon", "player", "villager", "zombie_villager", "iron_golem", "witch", "wandering_trader", "pillager",
        "vindicator", "evoker", "vex", "ravager", "cod", "salmon", "tropical_fish", "pufferfish", "squid",
        "glow_squid"]


# --- Everything else ----------------------------------------------------------------

def misc(rng):
    out = {}
    for i in (1, 2, 3):  # explosions: a thump, a noise blast, a long rumble
        n = int(2.0 * RATE)
        blast = mul(lowpass(noise(n, rng), 1200), env(n, 0.003, 0.35))
        rumble = mul(lowpass(noise(n, rng), 150), env(n, 0.01, 0.8))
        thump = mul(tone(0.5, 90, 35), env(int(0.5 * RATE), 0.002, 0.12))
        out[f"random/explode{i}"] = add(blast, [r * 3 for r in rumble], [t * 2 for t in thump])
    out["random/pop"] = mul(tone(0.08, 500, 1200), env(int(0.08 * RATE), 0.002, 0.02))
    out["random/orb"] = mul(add(tone(0.25, 1760), [s * 0.5 for s in tone(0.25, 2640)]), env(int(0.25 * RATE), 0.002, 0.06))
    lv = silence(1.2)
    for k, f in enumerate((523, 659, 784, 1047)):
        lv = add(lv, at(mul(tone(0.6, f, wave="triangle"), env(int(0.6 * RATE), 0.005, 0.2)), k * 0.12, 1.2))
    out["random/levelup"] = lv
    out["random/bow"] = add(mul(tone(0.25, 300, 140, "triangle"), env(int(0.25 * RATE), 0.002, 0.05)),
                            mul(bandpass(noise(int(0.25 * RATE), rng), 1000, 6000), env(int(0.25 * RATE), 0.001, 0.08)))
    for i in (1, 2):
        out[f"random/bowhit{i}"] = mul(resonator(noise(int(0.2 * RATE), rng), rng.uniform(300, 450), 6),
                                       env(int(0.2 * RATE), 0.001, 0.03))
    def creak(f0, f1, d):
        x = tone(d, f0, f1, "saw", vibrato=0.08, vib_rate=40)
        return mul(bandpass(x, 300, 2500), env(int(d * RATE), 0.02, d / 2))
    knock = lambda f: [s * 5 for s in resonator(mul(noise(4000, rng), env(4000, 0.0005, 0.008)), f, 8)]
    out["random/door_open"] = add(at(creak(220, 330, 0.35), 0, 0.5), at(knock(200), 0.32, 0.5))
    out["random/door_close"] = add(at(creak(300, 200, 0.2), 0, 0.4), at(knock(160), 0.18, 0.4))
    out["random/chestopen"] = add(at(creak(150, 260, 0.45), 0, 0.6), at(knock(260), 0.0, 0.6))
    out["random/chestclosed"] = add(at(creak(240, 160, 0.25), 0, 0.5), at(knock(180), 0.25, 0.5))
    out["random/click"] = [s * 4 for s in resonator(mul(noise(1500, rng), env(1500, 0.0003, 0.002)), 2000, 10)]
    out["random/wood_click"] = [s * 4 for s in resonator(mul(noise(1500, rng), env(1500, 0.0003, 0.003)), 700, 10)]
    n = int(1.4 * RATE)
    fuse = bandpass(noise(n, rng), 2500, 8000)
    out["random/fuse"] = mul(fuse, [0.6 + 0.4 * rng.random() for _ in range(n)])
    out["random/fizz"] = mul(highpass(noise(int(0.6 * RATE), rng), 2500), env(int(0.6 * RATE), 0.005, 0.18))
    for i in (1, 2):
        e = silence(0.3)
        for k in range(3):
            e = add(e, at(crunch(rng, 0.06, 15, 500, 3000, grain_len=0.004, decay=0.02), k * 0.08, 0.3))
        out[f"random/eat{i}"] = e
    gulp = lambda: mul(tone(0.12, 250, 400, "sine"), env(int(0.12 * RATE), 0.01, 0.04))
    out["random/drink"] = add(at(gulp(), 0, 0.5), at(gulp(), 0.18, 0.5), at(gulp(), 0.34, 0.5))
    out["random/burp"] = voice(rng, 0.35, 120, 90, formant=(150, 1200), breath=0.3, attack=0.02)
    sp = int(0.8 * RATE)
    out["random/splash"] = mul(bandpass(noise(sp, rng), 300, 5000), env(sp, 0.003, 0.2))
    for i in (1, 2):
        sw = int(0.5 * RATE)
        out[f"liquid/swim{i}"] = mul(bandpass(noise(sw, rng), 200, 2500), env(sw, 0.08, 0.12))
    out["random/hurt"] = out.get("random/hurt", None) or mul(tone(0.15, 200, 140), env(int(0.15 * RATE), 0.002, 0.04))
    out["random/glass1"] = glass_break(rng)
    out["random/glass2"] = glass_break(rng)
    out["random/anvil_land"] = mul(add(*[resonator(mul(noise(int(0.8 * RATE), rng), env(int(0.8 * RATE), 0.0005, 0.003)), f, 120)
                                          for f in (430, 1080, 1720)]), env(int(0.8 * RATE), 0.001, 0.25))
    out["random/anvil_use"] = out["random/anvil_land"][: int(0.4 * RATE)]
    out["random/enchant"] = add(*[at(mul(tone(0.5, f, wave="sine", vibrato=0.01, vib_rate=6),
                                         env(int(0.5 * RATE), 0.01, 0.2)), k * 0.07, 1.0)
                                  for k, f in enumerate((880, 1175, 1397, 1760, 2093))])
    out["tile/piston/out"] = add(mul(tone(0.25, 90, 140, "saw"), env(int(0.25 * RATE), 0.005, 0.06)),
                                 mul(lowpass(noise(int(0.25 * RATE), rng), 1500), env(int(0.25 * RATE), 0.002, 0.04)))
    out["tile/piston/in"] = add(mul(tone(0.25, 140, 90, "saw"), env(int(0.25 * RATE), 0.005, 0.06)),
                                mul(lowpass(noise(int(0.25 * RATE), rng), 1200), env(int(0.25 * RATE), 0.002, 0.04)))
    # Ambience.
    f = int(1.2 * RATE)
    crackle = silence(1.2)
    for _ in range(40):
        c = mul(noise(200, rng), env(200, 0.0003, 0.002))
        crackle = add(crackle, at([s * rng.uniform(0.2, 1.0) for s in c], rng.random() * 1.15, 1.2))
    out["fire/fire"] = add(highpass(crackle, 1500), [s * 0.15 for s in lowpass(noise(f, rng), 400)])
    out["liquid/lavapop"] = mul(tone(0.1, 180, 600, "sine"), env(int(0.1 * RATE), 0.002, 0.03))
    for i in (1, 2):
        rn = int(2.0 * RATE)
        out[f"ambient/weather/rain{i}"] = mul(bandpass(noise(rn, rng), 800, 7000),
                                              [0.8 + 0.2 * math.sin(2 * math.pi * 0.7 * k / RATE) for k in range(rn)])
        tn = int(4.0 * RATE)
        crack = mul(highpass(noise(tn, rng), 600), env(tn, 0.005, 0.15))
        roll = mul(lowpass(noise(tn, rng), 200), env(tn, 0.3, 1.3))
        out[f"ambient/weather/thunder{i}"] = add([c * 0.6 for c in crack], [r * 4 for r in roll])
    pn = int(3.0 * RATE)
    out["portal/portal"] = mul(add(tone(3.0, 110, 140, "saw", vibrato=0.05, vib_rate=0.5),
                                   [s * 0.6 for s in tone(3.0, 220, 280, "saw", vibrato=0.05, vib_rate=0.7)]),
                               env(pn, 0.8, 1.5, 0.3))
    out["portal/portal"] = lowpass(out["portal/portal"], 900)
    out["mob/endermen/portal"] = mul(tone(0.5, 200, 1200, "sine", vibrato=0.2, vib_rate=25),
                                     env(int(0.5 * RATE), 0.01, 0.15))
    out["random/successful_hit"] = mul(tone(0.1, 1500, 1500, "sine"), env(int(0.1 * RATE), 0.001, 0.03))
    out["damage/crit"] = add(mul(highpass(noise(int(0.2 * RATE), rng), 2000), env(int(0.2 * RATE), 0.001, 0.04)),
                             mul(tone(0.2, 900, 500, "square"), env(int(0.2 * RATE), 0.001, 0.03)))
    out["damage/hit"] = mul(lowpass(noise(int(0.15 * RATE), rng), 900), env(int(0.15 * RATE), 0.001, 0.03))
    out["random/break"] = crunch(rng, 0.3, 30, 800, 4000, decay=0.08)  # tools breaking
    # Note block instruments (M23.6; wiki: Note Block › Instruments): each sample is the
    # instrument's middle F# (note 12), the game shifts the pitch per note. Our own
    # synthesis of each instrument's character.
    def pluck(f, seconds, decay, wave="triangle", bright=0.0):
        n = int(seconds * RATE)
        body = mul(tone(seconds, f, wave=wave), env(n, 0.002, decay))
        if bright:
            body = add(body, [s * bright for s in mul(tone(seconds, f * 2, wave="sine"), env(n, 0.001, decay / 3))])
        return body
    def bell(f, seconds, decay, partials=(1.0, 2.76, 5.4)):
        n = int(seconds * RATE)
        return add(*[[s / (k + 1) for s in mul(tone(seconds, f * p), env(n, 0.001, decay / (k + 1)))]
                     for k, p in enumerate(partials)])
    fs = 369.99  # F#4
    out["note/harp"] = pluck(fs, 1.2, 0.35, bright=0.3)
    out["note/bass"] = pluck(fs / 4, 1.0, 0.3, wave="saw")
    out["note/guitar"] = lowpass(pluck(fs / 2, 1.0, 0.3, wave="saw", bright=0.2), 2500)
    out["note/banjo"] = highpass(pluck(fs, 0.6, 0.12, wave="saw"), 400)
    out["note/bit"] = mul(tone(0.5, fs, wave="square"), env(int(0.5 * RATE), 0.001, 0.25, 0.3))
    out["note/pling"] = add(pluck(fs, 1.2, 0.4, wave="sine"), [s * 0.4 for s in pluck(fs * 2, 1.2, 0.2, wave="sine")])
    out["note/flute"] = mul(tone(0.9, fs * 2, wave="sine", vibrato=0.006, vib_rate=5), env(int(0.9 * RATE), 0.05, 0.3, 0.4))
    out["note/didgeridoo"] = lowpass(mul(tone(1.0, fs / 4, wave="saw", vibrato=0.02, vib_rate=3),
                                         env(int(1.0 * RATE), 0.05, 0.4, 0.5)), 600)
    out["note/bell"] = bell(fs * 4, 1.5, 0.6)
    out["note/chime"] = bell(fs * 4, 2.0, 0.9, partials=(1.0, 2.0, 3.01))
    out["note/xylophone"] = bell(fs * 4, 0.5, 0.08, partials=(1.0, 3.93))
    out["note/iron_xylophone"] = bell(fs, 0.8, 0.2, partials=(1.0, 3.0, 5.2))
    out["note/cow_bell"] = bell(fs * 2, 0.6, 0.12, partials=(1.0, 1.5, 2.6))
    bd = int(0.4 * RATE)
    out["note/bd"] = add(mul(tone(0.4, 120, 45), env(bd, 0.001, 0.09)),
                         [s * 0.3 for s in mul(lowpass(noise(bd, rng), 300), env(bd, 0.001, 0.03))])
    out["note/snare"] = add(mul(bandpass(noise(bd, rng), 800, 6000), env(bd, 0.001, 0.07)),
                            [s * 0.5 for s in mul(tone(0.4, 200, 160), env(bd, 0.001, 0.04))])
    out["note/hat"] = mul(highpass(noise(int(0.2 * RATE), rng), 6000), env(int(0.2 * RATE), 0.001, 0.03))
    out["random/minecart"] = mul(lowpass(noise(int(2.0 * RATE), rng), 500),
                                 [0.7 + 0.3 * math.sin(2 * math.pi * 6 * k / RATE) for k in range(int(2.0 * RATE))])
    return out


def main():
    count = 0
    for mat in MATERIALS:
        for i in (1, 2, 3):
            rng = random.Random(f"dig/{mat}{i}")
            write(f"dig/{mat}{i}", material(mat, rng, False))
            rng = random.Random(f"step/{mat}{i}")
            write(f"step/{mat}{i}", material(mat, rng, True), peak=0.6)
            count += 2
    for mob in MOBS:
        rng = random.Random(f"mob/{mob}")
        say, hurt, death = mob_sounds(mob, rng)
        for i, s in enumerate(say or [], 1):
            write(f"mob/{mob}/say{i}", s)
            count += 1
        for i, s in enumerate(hurt or [], 1):
            write(f"mob/{mob}/hurt{i}", s)
            count += 1
        if death is not None:
            write(f"mob/{mob}/death", death)
            count += 1
    for name, s in misc(random.Random("misc")).items():
        write(name, s)
        count += 1
    print(f"wrote {count} sounds to {OUT}")


if __name__ == "__main__":
    main()
