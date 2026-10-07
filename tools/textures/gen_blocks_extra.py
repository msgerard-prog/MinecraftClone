#!/usr/bin/env python3
"""Block textures for blocks whose vanilla look comes from an entity model (chests):
original art in the game's pixel style (docs/art-style.md), as plain block faces.

Writes assets/minecraft/textures/block/<name>.png (16x16): chest_top, chest_side,
chest_front (wooden planks with a dark frame, the front with an iron latch).
Usage: tools/textures/gen_blocks_extra.py
"""
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import Img, encode_png, hexc, ramp  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"


def planks(rng, img, pal, lid_line=None):
    for y in range(16):
        for x in range(16):
            r = rng.random()
            c = pal[2] if r > 0.35 else pal[3] if r > 0.15 else pal[1]
            if y in (4, 9, 14):  # board seams
                c = pal[0]
            img.set(x, y, c)
    for i in range(16):  # dark frame
        for x, y in ((i, 0), (i, 15), (0, i), (15, i)):
            img.set(x, y, pal[0])
    if lid_line is not None:
        for x in range(16):
            img.set(x, lid_line, hexc("#3A2410"))


def chest(face):
    rng = random.Random("chest_" + face)
    img = Img(16, 16, (0, 0, 0, 255))
    pal = ramp(hexc("#A26A2E"), 5, spread=0.3)
    planks(rng, img, pal, lid_line=None if face == "top" else 5)
    if face == "front":  # the latch
        iron = ramp(hexc("#C8C8C8"), 5, spread=0.3)
        for y in range(4, 8):
            for x in range(7, 9):
                img.set(x, y, iron[2] if y < 7 else iron[1])
    return img


def bed(face):
    """Red bed (our drawing): blanket on top, a white pillow at the head, wooden frame
    sides; ends show the frame."""
    rng = random.Random("bed_" + face)
    img = Img(16, 16, (0, 0, 0, 255))
    red = ramp(hexc("#B0242A"), 5, spread=0.3)
    wood = ramp(hexc("#8A5A30"), 5, spread=0.3)
    white = ramp(hexc("#E8E8E0"), 5, spread=0.15)
    for y in range(16):
        for x in range(16):
            r = rng.random()
            i = 2 if r > 0.3 else 3 if r > 0.15 else 1
            if face in ("head_top", "foot_top"):
                pillow = face == "head_top" and 3 <= x <= 12 and 1 <= y <= 6
                c = white[i] if pillow else red[i]
                if x in (0, 15):
                    c = red[0]
            else:  # sides / ends: blanket band on top, frame below
                c = red[i] if y < 7 else wood[i]
                if y == 7 or (y >= 7 and x in (0, 15)):
                    c = wood[0]
            img.set(x, y, c)
    return img


def experience_orb():
    """A white-cored ball (tinted green/yellow by the renderer when drawn)."""
    img = Img(16, 16, (0, 0, 0, 0))
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if d < 3.0:
                img.set(x, y, (255, 255, 255, 255))
            elif d < 4.5:
                img.set(x, y, (200, 200, 200, 255))
            elif d < 5.2 and (x + y) % 2 == 0:
                img.set(x, y, (140, 140, 140, 255))
    return img


def main():
    for face in ("top", "side", "front"):
        (OUT / f"chest_{face}.png").write_bytes(encode_png(chest(face)))
    for face in ("head_top", "foot_top", "side", "end"):
        (OUT / f"red_bed_{face}.png").write_bytes(encode_png(bed(face)))
    (OUT / "experience_orb.png").write_bytes(encode_png(experience_orb()))
    print("wrote chest, bed and orb textures")


if __name__ == "__main__":
    main()
