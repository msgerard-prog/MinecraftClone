#!/usr/bin/env python3
"""Generate our own placeholder block textures (ADR 0004: no Mojang assets).

Writes 16x16 RGBA PNGs to assets/minecraft/textures/block/, named after the vanilla
sprite they stand in for, so a real resource pack can override them file for file.
Deterministic: each texture uses its own seeded RNG. Standard library only.

Usage: tools/textures/gen_placeholders.py            (regenerates all)
"""
import random
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"
N = 16

# Base colours (ours, picked to read like the vanilla material).
STONE = (125, 125, 125)
DIRT = (134, 96, 67)
GRASS_GRAY = (150, 150, 150)  # grass top is greyscale; the shader tints it (biome colour)
GRASS_SIDE_GREEN = (110, 160, 60)
PLANKS = (162, 130, 78)
BARK = (104, 82, 48)
LOG_RING = (176, 144, 88)
SAND = (219, 207, 163)
BEDROCK = (85, 85, 85)


def clamp(v):
    return max(0, min(255, int(v)))


def shade(rgb, k):
    return tuple(clamp(c * k) for c in rgb)


def noisy(rng, rgb, amount):
    return shade(rgb, 1.0 + rng.uniform(-amount, amount))


def image(fn):
    return [[fn(x, y) for x in range(N)] for y in range(N)]


def stone(rng):
    return image(lambda x, y: noisy(rng, STONE, 0.12) if rng.random() > 0.08 else shade(STONE, 0.78))


def cobblestone(rng):
    # Irregular cells: pick seed points, colour by nearest point, dark mortar on cell edges.
    pts = [(rng.uniform(0, N), rng.uniform(0, N), rng.uniform(0.85, 1.15)) for _ in range(9)]

    def nearest(x, y):
        best = []
        for px, py, k in pts:
            # wrap around so the texture tiles
            dx = min(abs(x - px), N - abs(x - px))
            dy = min(abs(y - py), N - abs(y - py))
            best.append((dx * dx + dy * dy, k))
        best.sort()
        return best

    def px(x, y):
        b = nearest(x + 0.5, y + 0.5)
        if b[1][0] ** 0.5 - b[0][0] ** 0.5 < 0.9:
            return shade(STONE, 0.55)
        return noisy(rng, shade(STONE, b[0][1]), 0.06)

    return image(px)


def dirt(rng):
    return image(lambda x, y: noisy(rng, DIRT, 0.14) if rng.random() > 0.1 else shade(DIRT, 0.75))


def grass_block_top(rng):
    return image(lambda x, y: noisy(rng, GRASS_GRAY, 0.15))


def grass_block_side(rng):
    # Vanilla draws dirt + a tinted overlay; we bake a fixed green fringe (deviation
    # listed in game-design.md until biome tinting of overlays exists).
    depth = [rng.choice((2, 3, 3, 4)) for _ in range(N)]
    return image(lambda x, y: noisy(rng, GRASS_SIDE_GREEN, 0.12) if y < depth[x]
                 else noisy(rng, DIRT, 0.14))


def oak_planks(rng):
    seams = {0: 5, 1: 12, 2: 3, 3: 9}  # vertical seam x per 4-row plank

    def px(x, y):
        if y % 4 == 3:
            return shade(PLANKS, 0.7)
        if x == seams[y // 4]:
            return shade(PLANKS, 0.8)
        return noisy(rng, PLANKS, 0.06)

    return image(px)


def oak_log(rng):
    stripe = [rng.uniform(0.8, 1.1) for _ in range(N)]
    return image(lambda x, y: noisy(rng, shade(BARK, stripe[x]), 0.06))


def oak_log_top(rng):
    def px(x, y):
        d = max(abs(x - 7.5), abs(y - 7.5))
        if d > 6.5:
            return noisy(rng, BARK, 0.06)
        ring = int(d) % 2
        return noisy(rng, shade(LOG_RING, 0.88 if ring else 1.0), 0.04)

    return image(px)


def sand(rng):
    return image(lambda x, y: noisy(rng, SAND, 0.05))


def bedrock(rng):
    return image(lambda x, y: shade(BEDROCK, rng.choice((0.5, 0.8, 1.0, 1.3, 1.6))))


TEXTURES = {
    "stone": stone,
    "cobblestone": cobblestone,
    "dirt": dirt,
    "grass_block_top": grass_block_top,
    "grass_block_side": grass_block_side,
    "oak_planks": oak_planks,
    "oak_log": oak_log,
    "oak_log_top": oak_log_top,
    "sand": sand,
    "bedrock": bedrock,
}


def write_png(path, pixels):
    raw = b"".join(b"\x00" + bytes(c for rgb in row for c in (*rgb, 255)) for row in pixels)

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))

    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in TEXTURES.items():
        rng = random.Random(name)  # stable per texture
        write_png(OUT / f"{name}.png", fn(rng))
    print(f"wrote {len(TEXTURES)} textures to {OUT}")


if __name__ == "__main__":
    main()
