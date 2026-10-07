#!/usr/bin/env python3
"""Generate the sky textures: original art in the game's pixel style (docs/art-style.md).

Writes assets/minecraft/textures/environment/
  sun.png          32x32, drawn additively (black = no light)
  moon_phases.png  128x64, 4x2 phases of 32x32 (0 = full, then waning, new at 4,
                   waxing back), drawn additively

Deterministic (fixed seeds). Usage: tools/textures/gen_environment.py [--preview DIR]
"""
import argparse
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import Img, clamp, encode_png  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/environment"
BLACK = (0, 0, 0, 255)


def sun():
    """A square sun: hot white core, yellow body, a stepped orange halo."""
    rng = random.Random("sun")
    img = Img(32, 32, BLACK)
    for y in range(32):
        for x in range(32):
            # Chebyshev distance from the centre: square rings, pixel-art steps.
            d = max(abs(x - 15.5), abs(y - 15.5))
            if d < 4.5:
                c = (255, 255, 236)
            elif d < 7.5:
                c = (255, 247, 168)
            elif d < 9.5:
                c = (252, 222, 92)
            elif d < 11.5:
                c = (178, 128, 40)  # halo, dimmer (additive: darker = fainter)
            elif d < 13.5:
                c = (84, 54, 16)
            else:
                continue
            j = rng.uniform(-10, 6) if d >= 4.5 else 0  # a little shimmer
            img.set(x, y, (clamp(c[0] + j), clamp(c[1] + j), clamp(c[2] + j * 0.6), 255))
    return img


def moon_face(rng):
    """One 32x32 cell: a rounded square disc with craters, lit fully."""
    face = {}
    craters = [(rng.uniform(10, 22), rng.uniform(10, 22), rng.uniform(1.6, 3.2))
               for _ in range(6)]
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            # Superellipse: blocky but rounder than the sun.
            if (abs(dx) / 8.6) ** 4 + (abs(dy) / 8.6) ** 4 > 1.0:
                continue
            v = 214 + rng.uniform(-10, 10)
            for cx, cy, r in craters:
                dist = math.hypot(x - cx, y - cy)
                if dist < r:
                    v -= 52 if dist < r - 1 else 22
            # Soft limb darkening.
            v -= 26 * max(0.0, (max(abs(dx), abs(dy)) - 5.5) / 3.1)
            face[(x, y)] = v
    return face


def moon_phases():
    rng = random.Random("moon")
    face = moon_face(rng)
    img = Img(128, 64, BLACK)
    # Lit fraction across the disc for each phase; light comes from the right while
    # waning (phases 1-3) and from the left while waxing (5-7).
    for phase in range(8):
        ox, oy = (phase % 4) * 32, (phase // 4) * 32
        k = phase if phase <= 4 else 8 - phase  # 0 full .. 4 new
        for (x, y), v in face.items():
            u = (x - 6.9) / 17.2  # 0 at the left edge of the disc, 1 at the right
            u = u if phase <= 4 else 1.0 - u
            lit = u >= k / 4.0 if k < 4 else False
            if not lit:
                v *= 0.10  # earthshine: the dark side stays faintly visible
            b = clamp(v)
            img.set(ox + x, oy + y, (clamp(b * 0.96), clamp(b * 0.98), b, 255))
    return img


def end_portal():
    """The End portal's surface (block/end_portal.png, our own): a deep teal-black
    void with scattered pale stars. Vanilla draws it with a shader instead."""
    rng = random.Random("end_portal")
    img = Img(16, 16, (8, 14, 18, 255))
    for y in range(16):
        for x in range(16):
            v = rng.random()
            if v < 0.08:
                img.set(x, y, (12 + rng.randrange(20), 40 + rng.randrange(40), 44 + rng.randrange(40), 255))
            elif v < 0.12:
                img.set(x, y, (150 + rng.randrange(80), 220, 200 + rng.randrange(50), 255))
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for a 4x preview")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    images = {"sun": sun(), "moon_phases": moon_phases()}
    for name, img in images.items():
        (OUT / f"{name}.png").write_bytes(encode_png(img))
        print(f"wrote {OUT / name}.png ({img.w}x{img.h})")
    block = OUT.parent / "block" / "end_portal.png"
    block.write_bytes(encode_png(end_portal()))
    print(f"wrote {block}")
    if args.preview:
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)
        for name, img in images.items():
            big = Img(img.w * 4, img.h * 4)
            for y in range(big.h):
                for x in range(big.w):
                    big.set(x, y, img.get(x // 4, y // 4))
            (out / f"env-{name}.png").write_bytes(encode_png(big))


if __name__ == "__main__":
    main()
