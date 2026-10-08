#!/usr/bin/env python3
"""Generate painting textures: original art (docs/art-style.md), 16 pixels per block.

Writes assets/minecraft/textures/painting/<name>.png for every variant in
src/world/Paintings.h (names and sizes from the wiki; the pictures are ours: simple
landscapes, figures, still lifes and abstract patterns chosen by a seed from the name)
and back.png (the canvas back, shown from behind). Deterministic.
Usage: tools/textures/gen_paintings.py
"""
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import Img, encode_png, hexc  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/painting"

# (name, width, height) in blocks - kept in step with src/world/Paintings.h.
VARIANTS = [
    ("alban", 1, 1), ("aztec", 1, 1), ("aztec2", 1, 1), ("bomb", 1, 1), ("kebab", 1, 1), ("meditative", 1, 1),
    ("plant", 1, 1), ("wasteland", 1, 1), ("graham", 1, 2), ("prairie_ride", 1, 2), ("wanderer", 1, 2),
    ("courbet", 2, 1), ("creebet", 2, 1), ("pool", 2, 1), ("sea", 2, 1), ("sunset", 2, 1), ("baroque", 2, 2),
    ("bust", 2, 2), ("earth", 2, 2), ("fire", 2, 2), ("humble", 2, 2), ("match", 2, 2), ("skull_and_roses", 2, 2),
    ("stage", 2, 2), ("void", 2, 2), ("water", 2, 2), ("wind", 2, 2), ("wither", 2, 2), ("bouquet", 3, 3),
    ("cavebird", 3, 3), ("cotan", 3, 3), ("endboss", 3, 3), ("fern", 3, 3), ("owlemons", 3, 3), ("sunflowers", 3, 3),
    ("tides", 3, 3), ("backyard", 3, 4), ("pond", 3, 4), ("changing", 4, 2), ("fighters", 4, 2), ("finding", 4, 2),
    ("lowmist", 4, 2), ("passage", 4, 2), ("donkey_kong", 4, 3), ("skeleton", 4, 3), ("burning_skull", 4, 4),
    ("orb", 4, 4), ("pigscene", 4, 4), ("pointer", 4, 4), ("unpacked", 4, 4),
]
LANDSCAPE = {"wasteland", "sea", "sunset", "pool", "courbet", "creebet", "prairie_ride", "lowmist", "pond",
             "backyard", "tides", "passage", "changing", "finding", "fern", "cavebird", "wanderer"}
STILL = {"plant", "kebab", "bomb", "bouquet", "sunflowers", "cotan", "baroque", "unpacked"}
ABSTRACT = {"aztec", "aztec2", "void", "orb", "earth", "fire", "water", "wind"}

PALETTES = [  # sky/background, ground, accent, dark, light
    ["#7FA8D8", "#5E8A3C", "#E8C050", "#2E3A48", "#F2E8C8"],
    ["#E89A5A", "#7A5034", "#F2D46A", "#3A2430", "#FCE8C0"],
    ["#4A5A8A", "#3A6A5A", "#D8D8E8", "#1A1E30", "#B8C8E8"],
    ["#C8D8C0", "#8A9A5A", "#C0503A", "#3A3A2A", "#F4F0E0"],
    ["#5A3A6A", "#2A4A5A", "#E8A040", "#140C1A", "#E0C8F0"],
    ["#A8C8D0", "#C8B078", "#3A7AA8", "#3A3020", "#FFFFF0"],
]


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3)) + (255,)


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + (255,)


def painting(name, w, h):
    rng = random.Random(name)
    W, H = w * 16, h * 16
    pal = [hexc(c) for c in PALETTES[rng.randrange(len(PALETTES))]]
    sky, ground, accent, dark, light = pal
    img = Img(W, H, sky)

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            img.set(x, y, c)

    def disc(cx, cy, r, c):
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                    put(x, y, c)

    if name in LANDSCAPE:
        horizon = int(H * rng.uniform(0.45, 0.7))
        for y in range(H):  # sky gradient, then layered ground
            for x in range(W):
                if y < horizon:
                    img.set(x, y, mix(light, sky, y / max(1, horizon)))
        disc(rng.uniform(0.2, 0.8) * W, horizon * rng.uniform(0.25, 0.6), max(1.5, min(W, H) * 0.12), accent)
        for layer in range(3):
            base = horizon + layer * (H - horizon) // 3
            amp, freq, ph = rng.uniform(1, 3), rng.uniform(0.1, 0.3), rng.uniform(0, 6)
            col = shade(ground, 1.15 - layer * 0.18)
            for x in range(W):
                top = int(base - amp * (1 + math.sin(x * freq + ph)))
                for y in range(max(0, top), H):
                    img.set(x, y, col)
        for _ in range(w * h * 2):  # trees / rocks
            x, y = rng.randrange(W), rng.randrange(horizon - 2, H)
            disc(x, y, rng.uniform(0.8, 1.8), shade(ground, 0.6))
    elif name in STILL:
        table = int(H * 0.72)
        for y in range(table, H):
            for x in range(W):
                img.set(x, y, shade(dark, 1.6 if (x + y) % 5 else 1.4))
        vx = W / 2
        disc(vx, table - H * 0.12, max(1.5, min(W, H) * 0.14), shade(accent, 0.8))
        for _ in range(3 + w * h):  # flowers / fruit
            disc(vx + rng.uniform(-0.3, 0.3) * W, table - H * rng.uniform(0.3, 0.6),
                 rng.uniform(1.0, 2.2), mix(accent, light, rng.random() * 0.5))
    elif name in ABSTRACT:
        k = rng.randrange(3)
        for y in range(H):
            for x in range(W):
                if k == 0:  # rings
                    d = math.hypot(x + 0.5 - W / 2, y + 0.5 - H / 2)
                    c = [sky, accent, dark, light][int(d / 2) % 4]
                elif k == 1:  # zig-zag bands
                    c = [dark, accent, light, ground][((y + abs((x % 8) - 4)) // 3) % 4]
                else:  # checks
                    c = [accent, dark][((x // 4) + (y // 4)) % 2] if (x + y) % 7 else light
                img.set(x, y, c)
    else:  # a figure: a head and shoulders (or a creature) on a plain ground
        for y in range(H):
            for x in range(W):
                img.set(x, y, mix(sky, dark, y / H * 0.6))
        figures = max(1, w * h // 4)
        for f in range(figures):
            cx = W * (f + 0.5) / figures
            body = shade(ground, rng.uniform(0.7, 1.1))
            rb = min(W / figures * 0.4, H * 0.35)
            rh = rb * 0.55
            by, hy = H - rb * 0.55, H - rb * 0.55 - rb - rh * 0.5
            disc(cx, by, rb, body)
            disc(cx, hy, rh, mix(light, accent, 0.3))
            put(int(cx - rh * 0.35), int(hy), dark)
            put(int(cx + rh * 0.35), int(hy), dark)
    # Brush texture: a little noise, and a dark painted border.
    for y in range(H):
        for x in range(W):
            c = img.get(x, y)
            img.set(x, y, shade(c, 1.0 + rng.uniform(-0.05, 0.05)))
    for x in range(W):
        img.set(x, 0, shade(dark, 0.8))
        img.set(x, H - 1, shade(dark, 0.8))
    for y in range(H):
        img.set(0, y, shade(dark, 0.8))
        img.set(W - 1, y, shade(dark, 0.8))
    return img


def back():
    img = Img(16, 16, hexc("#B89A6A"))
    rng = random.Random("back")
    for y in range(16):
        for x in range(16):
            c = hexc("#B89A6A")
            img.set(x, y, shade(c, 0.92 + rng.random() * 0.12 - (0.1 if x in (0, 15) or y in (0, 15) else 0)))
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, w, h in VARIANTS:
        (OUT / f"{name}.png").write_bytes(encode_png(painting(name, w, h)))
    (OUT / "back.png").write_bytes(encode_png(back()))
    print(f"wrote {len(VARIANTS) + 1} painting textures to {OUT}")


if __name__ == "__main__":
    main()
