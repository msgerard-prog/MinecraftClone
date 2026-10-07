#!/usr/bin/env python3
"""Generate the particle sprites (M22.3): original art in the game's pixel style
(docs/art-style.md). Drawn on an 8x8 grid like vanilla's particles and scaled 2x into
16x16 cells of the block atlas (the entity renderer samples particles from it).

Writes assets/minecraft/textures/block/particle_*.png:
  particle_generic_0..7  white round puffs, 0 = smallest (smoke, poof, explosions
                         and portal sparks tint them and step through the sizes)
  particle_flame         a small flame (torches, furnaces, fire)
  particle_lava          an ember (lava pops)
  particle_crit          a four-point spark (critical hits)
  particle_effect        a sparkle (potions)
  particle_splash_0..3   rain splash droplets
  particle_drip          a hanging drop (tinted water blue / lava orange)

Deterministic. Usage: tools/textures/gen_particles.py
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import Img, encode_png  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"
CLEAR = (0, 0, 0, 0)


def scaled(small):
    """8x8 -> 16x16, nearest (chunky pixels like the rest of the art)."""
    img = Img(16, 16, CLEAR)
    for y in range(16):
        for x in range(16):
            img.set(x, y, small.get(x // 2, y // 2))
    return img


def generic(i):
    """A soft round puff whose radius grows with i (0..7)."""
    img = Img(8, 8, CLEAR)
    r = 0.6 + i * 0.5
    for y in range(8):
        for x in range(8):
            d = math.hypot(x - 3.5, y - 3.5)
            if d <= r:
                edge = d > r - 1.0
                v = 210 if edge else 255
                img.set(x, y, (v, v, v, 255))
    return img


def from_rows(rows, palette):
    img = Img(8, 8, CLEAR)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != ".":
                img.set(x, y, palette[ch])
    return img


FLAME = from_rows([
    "........",
    "...y....",
    "...yy...",
    "..yoy...",
    "..ooy...",
    "..oro...",
    "...o....",
    "........"], {"y": (255, 240, 140, 255), "o": (255, 170, 40, 255), "r": (230, 90, 20, 255)})

LAVA = from_rows([
    "........",
    "........",
    "...yo...",
    "..yoor..",
    "..oorr..",
    "...rr...",
    "........",
    "........"], {"y": (255, 230, 120, 255), "o": (250, 140, 30, 255), "r": (200, 60, 10, 255)})

CRIT = from_rows([
    "........",
    "...w....",
    "...w....",
    ".wwgww..",
    "...w....",
    "...w....",
    "........",
    "........"], {"w": (255, 255, 255, 255), "g": (230, 230, 230, 255)})

EFFECT = from_rows([
    "........",
    "...w....",
    "..wgw...",
    ".wg.gw..",
    "..wgw...",
    "...w....",
    "........",
    "........"], {"w": (255, 255, 255, 255), "g": (200, 200, 200, 255)})

DRIP = from_rows([
    "........",
    "........",
    "...w....",
    "..wwg...",
    "..wgg...",
    "...g....",
    "........",
    "........"], {"w": (255, 255, 255, 255), "g": (210, 210, 210, 255)})


def splash(i):
    """Droplets flying apart: frame 0 tight, 3 spread."""
    img = Img(8, 8, CLEAR)
    c = (200, 220, 255, 255)
    s = 1 + i
    for dx, dy in ((-s, 0), (s, 0), (0, -s), (-s + 1, -1), (s - 1, -1)):
        x, y = 3 + dx, 5 + dy - (i // 2)
        if 0 <= x < 8 and 0 <= y < 8:
            img.set(x, y, c)
    return img


def main():
    out = {f"particle_generic_{i}": generic(i) for i in range(8)}
    out.update({"particle_flame": FLAME, "particle_lava": LAVA, "particle_crit": CRIT,
                "particle_effect": EFFECT, "particle_drip": DRIP})
    out.update({f"particle_splash_{i}": splash(i) for i in range(4)})
    for name, small in out.items():
        (OUT / f"{name}.png").write_bytes(encode_png(scaled(small)))
    print(f"wrote {len(out)} particle sprites to {OUT}")


if __name__ == "__main__":
    main()
