#!/usr/bin/env python3
"""Generate mob textures: original art in the game's pixel style (docs/art-style.md).

Writes assets/minecraft/textures/entity/clone/<mob>.png, 64x64, box-UV layouts that
match the cuboid models in src/rendering/MobModels.cpp (keep both in sync):
  zombie  the humanoid skin layout (wiki: Skin): head 8x8x8 @ (0,0), body 8x12x4 @
          (16,16), right arm 4x12x4 @ (40,16), right leg 4x12x4 @ (0,16),
          left leg @ (16,48), left arm @ (32,48)
  cow     our layout: head 8x8x6 @ (0,0), horn 1x3x1 @ (22,0), body 10x10x18 @ (0,16),
          leg 4x12x4 @ (0,48) (the udder region is painted but not modelled yet)
  sheep   head 6x6x8 @ (0,0), body 8x8x16 @ (0,16), leg 4x12x4 @ (0,40); sheep_wool
          the same layout (drawn inflated, tinted by the dye colour)
  pig     head 8x8x8 @ (0,0), snout 4x3x1 @ (32,0), body 10x8x16 @ (0,16), leg 4x6x4 @ (0,40)
  chicken body 6x6x8 @ (0,0), head 4x6x3 @ (28,0), beak 4x2x2 @ (42,0), wattle
          2x2x1 @ (42,4), wing 1x4x6 @ (0,16), leg 1x4x2 @ (16,16)
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


def face(img, region, eyes, eye_col, extra=()):
    x0, y0, _, _ = region
    for x, y in eyes:
        img.set(x0 + x, y0 + y, eye_col)
    for (x, y), c in extra:
        img.set(x0 + x, y0 + y, c)


def sheep():
    # Skin under the wool: pale grey-pink; face darker. Layout: head 6x6x8 @ (0,0),
    # body 8x8x16 @ (0,16), leg 4x12x4 @ (0,40). The wool layer has the same layout.
    rng = random.Random("sheep")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#C8A8A0"), 5, spread=0.2)
    facec = ramp(hexc("#D8C4B4"), 5, spread=0.2)
    for name, f in box_faces(0, 0, 6, 6, 8).items():
        paint(img, f, facec if name == "front" else skin, rng)
    face(img, box_faces(0, 0, 6, 6, 8)["front"], ((1, 2), (4, 2)), (30, 24, 24, 255),
         (((2, 4), skin[0]), ((3, 4), skin[0])))
    for f in box_faces(0, 16, 8, 8, 16).values():
        paint(img, f, skin, rng)
    for name, f in box_faces(0, 40, 4, 12, 4).items():
        paint(img, f, skin, rng)
        if name not in ("top", "bottom"):
            paint(img, (f[0], f[1] + f[3] - 2, f[2], 2), ramp(hexc("#5A4A44"), 5), rng)
    return img


def sheep_wool():
    # Wool, light grey so the renderer's dye tint colours it; curly noise.
    rng = random.Random("wool")
    img = Img(64, 64, CLEAR)
    wool = ramp(hexc("#E6E6E6"), 5, spread=0.18)
    for name, f in box_faces(0, 0, 6, 6, 8).items():
        if name != "front":  # the face stays bare
            paint(img, f, wool, rng, noise=0.5)
    for f in box_faces(0, 16, 8, 8, 16).values():
        paint(img, f, wool, rng, noise=0.5)
    for name, (x0, y0, w, h) in box_faces(0, 40, 4, 12, 4).items():
        if name in ("top", "bottom"):
            continue
        paint(img, (x0, y0, w, 6), wool, rng, noise=0.5)  # only the upper legs (the box is 6 tall)
    return img


def pig():
    rng = random.Random("pig")
    img = Img(64, 64, CLEAR)
    pink = ramp(hexc("#EAA0A0"), 5, spread=0.2)
    snout = ramp(hexc("#D88888"), 5, spread=0.2)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, pink, rng)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 3), (6, 3)), (24, 16, 16, 255),
         (((0, 3), (240, 240, 240, 255)), ((7, 3), (240, 240, 240, 255))))
    for f in box_faces(32, 0, 4, 3, 1).values():
        paint(img, f, snout, rng)
    fx, fy, _, _ = box_faces(32, 0, 4, 3, 1)["front"]
    img.set(fx + 1, fy + 1, (120, 60, 60, 255))
    img.set(fx + 2, fy + 1, (120, 60, 60, 255))
    for f in box_faces(0, 16, 10, 8, 16).values():
        paint(img, f, pink, rng)
    for name, f in box_faces(0, 40, 4, 6, 4).items():
        paint(img, f, pink, rng)
        if name not in ("top", "bottom"):
            paint(img, (f[0], f[1] + f[3] - 1, f[2], 1), ramp(hexc("#8A5A50"), 5), rng)
    return img


def chicken():
    rng = random.Random("chicken")
    img = Img(64, 64, CLEAR)
    white = ramp(hexc("#F2F2EE"), 5, spread=0.12)
    yellow = ramp(hexc("#F2B83A"), 5, spread=0.2)
    red = ramp(hexc("#D02020"), 5, spread=0.2)
    for f in box_faces(0, 0, 6, 6, 8).values():
        paint(img, f, white, rng)
    for f in box_faces(28, 0, 4, 6, 3).values():
        paint(img, f, white, rng)
    face(img, box_faces(28, 0, 4, 6, 3)["front"], ((0, 1), (3, 1)), (20, 20, 20, 255))
    for f in box_faces(42, 0, 4, 2, 2).values():
        paint(img, f, yellow, rng)
    for f in box_faces(42, 4, 2, 2, 1).values():
        paint(img, f, red, rng)
    for f in box_faces(0, 16, 1, 4, 6).values():
        paint(img, f, white, rng)
    for f in box_faces(16, 16, 1, 4, 2).values():
        paint(img, f, yellow, rng)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for 4x previews")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    images = {"zombie": zombie(), "cow": cow(), "sheep": sheep(), "sheep_wool": sheep_wool(), "pig": pig(),
              "chicken": chicken()}
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
