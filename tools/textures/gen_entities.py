#!/usr/bin/env python3
"""Generate mob textures: original art in the game's pixel style (docs/art-style.md).

Writes assets/minecraft/textures/entity/clone/<mob>.png, 64x64, box-UV layouts that
match the cuboid models in src/rendering/MobModels.cpp (keep both in sync):
  zombie  the humanoid skin layout (wiki: Skin): head 8x8x8 @ (0,0), body 8x12x4 @
          (16,16), right arm 4x12x4 @ (40,16), right leg 4x12x4 @ (0,16),
          left leg @ (16,48), left arm @ (32,48)
  cow     our layout: head 8x8x6 @ (0,0), horn 1x3x1 @ (22,0), body 10x10x18 @ (0,16),
          leg 4x12x4 @ (0,48), udder 4x6x1 @ (16,48)
Box UV for a w x h x d box at (u, v): top (u+d, v) w*d, bottom (u+d+w, v) w*d,
right side (u, v+d) d*h, front (u+d, v+d) w*h, left side (u+d+w, v+d) d*h,
back (u+2d+w, v+d) w*h.
Our own file names (entity/clone/...) so a vanilla resource pack never maps its
differently laid-out textures onto our models.
Usage: tools/textures/gen_entities.py [--preview DIR]
"""
import argparse
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import CLEAR, Img, encode_png, hexc, ramp  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/entity/clone"


def box_faces(u, v, w, h, d):
    """name -> (x0, y0, width, height) of each face region."""
    return {
        "top": (u + d, v, w, d),
        "bottom": (u + d + w, v, w, d),
        "right": (u, v + d, d, h),
        "front": (u + d, v + d, w, h),
        "left": (u + d + w, v + d, d, h),
        "back": (u + 2 * d + w, v + d, w, h),
    }


def paint(img, region, pal, rng, noise=0.35):
    x0, y0, w, h = region
    for y in range(h):
        for x in range(w):
            r = rng.random()
            i = 2 if r > noise else (1 if r < noise / 2 else 3)
            img.set(x0 + x, y0 + y, pal[i])


def zombie():
    rng = random.Random("zombie")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#5A8C3C"), 5, spread=0.25)
    shirt = ramp(hexc("#2E8C8C"), 5, spread=0.25)
    pants = ramp(hexc("#4A3C8C"), 5, spread=0.25)
    for name, f in box_faces(0, 0, 8, 8, 8).items():
        paint(img, f, skin, rng)
    # Face: dark eye sockets, a mouth.
    fx, fy = 8, 8
    for x, y in ((1, 3), (2, 3), (5, 3), (6, 3)):
        img.set(fx + x, fy + y, (24, 30, 20, 255))
    for x in range(2, 6):
        img.set(fx + x, fy + 6, skin[0])
    for f in box_faces(16, 16, 8, 12, 4).values():
        paint(img, f, shirt, rng)
    for u, v in ((40, 16), (32, 48)):  # arms: sleeves at the top, skin below
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), skin, rng)
            if name not in ("top", "bottom"):
                paint(img, (x0, y0, w, 4), shirt, rng)
    for u, v in ((0, 16), (16, 48)):  # legs: trousers, dark shoes
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), pants, rng)
            if name not in ("top", "bottom"):
                paint(img, (x0, y0 + h - 2, w, 2), ramp(hexc("#3A3A3A"), 5), rng)
    return img


def cow():
    rng = random.Random("cow")
    img = Img(64, 64, CLEAR)
    brown = ramp(hexc("#5C3A22"), 5, spread=0.25)
    white = ramp(hexc("#E8E4DA"), 5, spread=0.15)
    pink = ramp(hexc("#D89A9A"), 5, spread=0.2)
    horn = ramp(hexc("#C8C0B0"), 5, spread=0.2)

    def patchy(region, seed):
        r2 = random.Random(seed)
        x0, y0, w, h = region
        cx, cy = r2.uniform(0, w), r2.uniform(0, h)
        rad = r2.uniform(2.0, 4.5)
        for y in range(h):
            for x in range(w):
                pal = white if (x - cx) ** 2 + (y - cy) ** 2 < rad * rad else brown
                r = r2.random()
                img.set(x0 + x, y0 + y, pal[2 if r > 0.3 else (1 if r < 0.15 else 3)])

    for name, f in box_faces(0, 0, 8, 8, 6).items():
        patchy(f, "head" + name)
    fx, fy = 6, 6  # front of the head: eyes and a pink snout
    for x, y in ((1, 2), (6, 2)):
        img.set(fx + x, fy + y, (20, 16, 14, 255))
    for y in range(5, 8):
        for x in range(2, 6):
            img.set(fx + x, fy + y, pink[2])
    img.set(fx + 3, fy + 6, pink[0])
    img.set(fx + 4, fy + 6, pink[0])
    for f in box_faces(22, 0, 1, 3, 1).values():
        paint(img, f, horn, rng)
    for name, f in box_faces(0, 16, 10, 10, 18).items():
        patchy(f, "body" + name)
    for name, (x0, y0, w, h) in box_faces(0, 48, 4, 12, 4).items():
        patchy((x0, y0, w, h), "leg" + name)
        if name not in ("top", "bottom"):  # hooves
            paint(img, (x0, y0 + h - 2, w, 2), ramp(hexc("#3A2E26"), 5), rng)
    for f in box_faces(16, 48, 4, 6, 1).values():
        paint(img, f, pink, rng)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for 4x previews")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    images = {"zombie": zombie(), "cow": cow()}
    for name, img in images.items():
        (OUT / f"{name}.png").write_bytes(encode_png(img))
        print(f"wrote {OUT / name}.png")
    if args.preview:
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)
        for name, img in images.items():
            big = Img(256, 256, (120, 120, 120, 255))
            for y in range(256):
                for x in range(256):
                    c = img.get(x // 4, y // 4)
                    if c[3]:
                        big.set(x, y, c)
            (out / f"entity-{name}.png").write_bytes(encode_png(big))


if __name__ == "__main__":
    main()
