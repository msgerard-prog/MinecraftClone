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


def skeleton():
    # Bone white, dark eye sockets. Head 8x8x8 @ (0,0), body 8x12x4 @ (16,16), arm
    # 2x12x2 @ (40,16), leg 2x12x2 @ (0,16).
    rng = random.Random("skeleton")
    img = Img(64, 64, CLEAR)
    bone = ramp(hexc("#C8C8C0"), 5, spread=0.2)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, bone, rng)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 3), (2, 3), (5, 3), (6, 3), (1, 4), (2, 4), (5, 4), (6, 4)),
         (40, 40, 40, 255), tuple(((x, 6), (70, 70, 66, 255)) for x in range(2, 6)))
    for name, (x0, y0, w, h) in box_faces(16, 16, 8, 12, 4).items():
        paint(img, (x0, y0, w, h), bone, rng)
        if name in ("front", "back"):  # ribs: dark gaps
            for y in range(1, h - 2, 2):
                for x in range(1, w - 1):
                    img.set(x0 + x, y0 + y, bone[0])
    for u, v in ((40, 16), (0, 16)):
        for f in box_faces(u, v, 2, 12, 2).values():
            paint(img, f, bone, rng)
    return img


def creeper():
    # Mottled green, the famous dark face. Head 8x8x8 @ (0,0), body 8x12x4 @ (16,16),
    # leg 4x6x4 @ (0,16).
    rng = random.Random("creeper")
    img = Img(64, 64, CLEAR)
    green = ramp(hexc("#4CA83C"), 5, spread=0.45)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, green, rng, noise=0.5)
    dark = (16, 24, 16, 255)
    face(img, box_faces(0, 0, 8, 8, 8)["front"],
         ((1, 2), (2, 2), (1, 3), (2, 3), (5, 2), (6, 2), (5, 3), (6, 3), (3, 4), (4, 4), (2, 5), (3, 5), (4, 5),
          (5, 5), (2, 6), (5, 6), (2, 7), (5, 7)), dark)
    for f in box_faces(16, 16, 8, 12, 4).values():
        paint(img, f, green, rng, noise=0.5)
    for f in box_faces(0, 16, 4, 6, 4).values():
        paint(img, f, green, rng, noise=0.5)
    return img


def spider():
    # Dark brown-black hairy body, red eyes. Head 8x8x8 @ (0,0), thorax 6x6x6 @ (32,0),
    # abdomen 10x8x12 @ (0,16), leg 16x2x2 @ (0,40).
    rng = random.Random("spider")
    img = Img(64, 64, CLEAR)
    body = ramp(hexc("#3A3028"), 5, spread=0.35)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, body, rng, noise=0.5)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 3), (2, 3), (5, 3), (6, 3), (2, 2), (5, 2), (3, 4), (4, 4)),
         (200, 30, 30, 255))
    for f in box_faces(32, 0, 6, 6, 6).values():
        paint(img, f, body, rng, noise=0.5)
    for f in box_faces(0, 16, 10, 8, 12).values():
        paint(img, f, body, rng, noise=0.5)
    for f in box_faces(0, 40, 16, 2, 2).values():
        paint(img, f, body, rng, noise=0.5)
    return img


def enderman():
    # Near-black with a purple sheen, glowing purple eyes. Head 8x8x8 @ (0,0), body
    # 8x12x4 @ (16,16), limb 2x30x2 @ (0,16).
    rng = random.Random("enderman")
    img = Img(64, 64, CLEAR)
    black = ramp(hexc("#161618"), 5, spread=0.4)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, black, rng)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], (), (0, 0, 0, 255),
         tuple(((x, 4), (224, 120, 250, 255) if x in (1, 6) else (180, 60, 230, 255))
               for x in (0, 1, 2, 5, 6, 7)))
    for f in box_faces(16, 16, 8, 12, 4).values():
        paint(img, f, black, rng)
    for f in box_faces(0, 16, 2, 30, 2).values():
        paint(img, f, black, rng)
    return img


def ghast():
    # Pale grey-white with closed eyes and a frowning mouth. Body 16x16x16 @ (0,0),
    # tentacle 2x9x2 @ (0,32).
    rng = random.Random("ghast")
    img = Img(64, 64, CLEAR)
    white = ramp(hexc("#E8E8E8"), 5, spread=0.18)
    for f in box_faces(0, 0, 16, 16, 16).values():
        paint(img, f, white, rng, noise=0.25)
    dark = (90, 90, 96, 255)
    front = box_faces(0, 0, 16, 16, 16)["front"]
    pts = [(x, 6) for x in (3, 4, 5, 10, 11, 12)] + [(x, 7) for x in (2, 6, 9, 13)]
    pts += [(x, 11) for x in range(5, 11)] + [(4, 12), (11, 12), (5, 10), (10, 10)]
    face(img, front, pts, dark)
    for x in (3, 4, 11, 12):  # tear streaks
        img.set(front[0] + x, front[1] + 8, (180, 180, 190, 255))
    for f in box_faces(0, 32, 2, 9, 2).values():
        paint(img, f, white, rng, noise=0.25)
    return img


def blaze():
    # A glowing yellow-orange head with dark eyes, golden rods. Head 8x8x8 @ (0,0),
    # rod 2x8x2 @ (0,16).
    rng = random.Random("blaze")
    img = Img(64, 64, CLEAR)
    yellow = ramp(hexc("#F0C030"), 5, spread=0.4)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, yellow, rng, noise=0.5)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 3), (2, 3), (5, 3), (6, 3), (3, 6), (4, 6)), (60, 30, 10, 255))
    rod = ramp(hexc("#E8A020"), 5, spread=0.4)
    for f in box_faces(0, 16, 2, 8, 2).values():
        paint(img, f, rod, rng, noise=0.5)
    return img


def magma_cube():
    # Dark red-black with glowing orange seams, two yellow eyes. Body 8x8x8 @ (0,0).
    rng = random.Random("magma_cube")
    img = Img(64, 64, CLEAR)
    dark = ramp(hexc("#3A1810"), 5, spread=0.4)
    glow = ramp(hexc("#F07018"), 5, spread=0.3)
    for name, (x0, y0, w, h) in box_faces(0, 0, 8, 8, 8).items():
        paint(img, (x0, y0, w, h), dark, rng)
        for y in range(h):
            for x in range(w):
                if y % 3 == 1 and rng.random() < 0.6:
                    img.set(x0 + x, y0 + y, glow[3 if rng.random() < 0.5 else 2])
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 2), (2, 2), (5, 2), (6, 2)), (250, 220, 60, 255))
    return img


def zombified_piglin():
    # A piglin's wide pink head gone grey-green and rotten, a bare rib on the body,
    # golden loincloth. Head 10x8x8 @ (0,0), snout 4x3x1 @ (40,0), ear 1x5x4 @ (52,0),
    # body 8x12x4 @ (16,16), arms 4x12x4 @ (40,16) / (32,48), legs @ (0,16) / (16,48).
    rng = random.Random("zombified_piglin")
    img = Img(64, 64, CLEAR)
    pink = ramp(hexc("#D88A84"), 5, spread=0.25)
    rot = ramp(hexc("#7A9A6A"), 5, spread=0.25)
    bone = ramp(hexc("#E0DCC8"), 5, spread=0.15)
    gold = ramp(hexc("#E0B030"), 5, spread=0.3)
    for name, (x0, y0, w, h) in box_faces(0, 0, 10, 8, 8).items():
        for y in range(h):
            for x in range(w):
                pal = rot if rng.random() < 0.3 else pink
                img.set(x0 + x, y0 + y, pal[2 if rng.random() > 0.3 else 1])
    face(img, box_faces(0, 0, 10, 8, 8)["front"], ((2, 3), (7, 3)), (30, 20, 20, 255),
         (((3, 3), bone[3]), ((6, 3), (240, 240, 240, 255))))
    for f in box_faces(40, 0, 4, 3, 1).values():
        paint(img, f, pink, rng)
    img.set(41 + 1, 1 + 1, (90, 40, 40, 255))
    img.set(41 + 2, 1 + 1, (90, 40, 40, 255))
    for f in box_faces(52, 0, 1, 5, 4).values():
        paint(img, f, rot, rng)
    for name, (x0, y0, w, h) in box_faces(16, 16, 8, 12, 4).items():
        paint(img, (x0, y0, w, h), pink, rng)
        if name in ("front", "back"):
            for y in (3, 5, 7):
                for x in range(1, w - 1):
                    img.set(x0 + x, y0 + y, bone[2])
            paint(img, (x0, y0 + h - 3, w, 3), gold, rng)
    for u, v in ((40, 16), (32, 48)):
        for f in box_faces(u, v, 4, 12, 4).values():
            paint(img, f, pink if u == 40 else rot, rng)
    for u, v in ((0, 16), (16, 48)):
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), pink, rng)
            if name not in ("top", "bottom"):
                paint(img, (x0, y0 + h - 2, w, 2), ramp(hexc("#4A3A30"), 5), rng)
    return img


def piglin():
    # The healthy piglin: pink, brown leather and a gold belt; same layout as the
    # zombified one.
    rng = random.Random("piglin")
    img = Img(64, 64, CLEAR)
    pink = ramp(hexc("#E89C94"), 5, spread=0.2)
    leather = ramp(hexc("#6A4428"), 5, spread=0.3)
    gold = ramp(hexc("#E8B830"), 5, spread=0.3)
    for f in box_faces(0, 0, 10, 8, 8).values():
        paint(img, f, pink, rng)
    face(img, box_faces(0, 0, 10, 8, 8)["front"], ((2, 3), (7, 3)), (30, 20, 20, 255),
         (((3, 3), (240, 240, 240, 255)), ((6, 3), (240, 240, 240, 255)), ((1, 7), gold[3]), ((8, 7), gold[3])))
    for f in box_faces(40, 0, 4, 3, 1).values():
        paint(img, f, pink, rng)
    img.set(42, 2, (110, 50, 50, 255))
    img.set(43, 2, (110, 50, 50, 255))
    for f in box_faces(52, 0, 1, 5, 4).values():
        paint(img, f, pink, rng)
    for name, (x0, y0, w, h) in box_faces(16, 16, 8, 12, 4).items():
        paint(img, (x0, y0, w, h), leather, rng)
        if name in ("front", "back"):
            paint(img, (x0, y0 + h - 4, w, 2), gold, rng)
    for u, v in ((40, 16), (32, 48)):
        for f in box_faces(u, v, 4, 12, 4).values():
            paint(img, f, pink, rng)
    for u, v in ((0, 16), (16, 48)):
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), leather, rng)
            if name not in ("top", "bottom"):
                paint(img, (x0, y0 + h - 2, w, 2), ramp(hexc("#3A2A20"), 5), rng)
    return img


def hoglin():
    # A bristly brown-pink boar with pale tusks (drawn at half size, scaled 2x). Head
    # 7x6x9 @ (0,0), tusk 1x3x1 @ (40,0), body 8x7x12 @ (0,16), leg 3x6x3 @ (0,40).
    rng = random.Random("hoglin")
    img = Img(64, 64, CLEAR)
    hide = ramp(hexc("#A8644C"), 5, spread=0.35)
    mane = ramp(hexc("#D8B060"), 5, spread=0.3)
    for f in box_faces(0, 0, 7, 6, 9).values():
        paint(img, f, hide, rng, noise=0.5)
    face(img, box_faces(0, 0, 7, 6, 9)["front"], ((1, 1), (5, 1)), (20, 14, 10, 255),
         (((2, 4), (60, 30, 30, 255)), ((4, 4), (60, 30, 30, 255))))
    for f in box_faces(40, 0, 1, 3, 1).values():
        paint(img, f, ramp(hexc("#EEE8D8"), 5), rng)
    for name, (x0, y0, w, h) in box_faces(0, 16, 8, 7, 12).items():
        paint(img, (x0, y0, w, h), hide, rng, noise=0.5)
        if name == "top":
            paint(img, (x0 + 3, y0, 2, h), mane, rng)
    for f in box_faces(0, 40, 3, 6, 3).values():
        paint(img, f, hide, rng, noise=0.5)
    return img


def strider():
    # A red, wrinkled body on two long grey legs. Body 16x14x16 @ (0,0), leg 4x16x4 @
    # (0,32).
    rng = random.Random("strider")
    img = Img(64, 64, CLEAR)
    red = ramp(hexc("#9A2E2E"), 5, spread=0.3)
    for name, (x0, y0, w, h) in box_faces(0, 0, 16, 14, 16).items():
        paint(img, (x0, y0, w, h), red, rng)
        if name not in ("top", "bottom"):
            for y in range(2, h, 3):
                for x in range(w):
                    if rng.random() < 0.7:
                        img.set(x0 + x, y0 + y, red[1])
    face(img, box_faces(0, 0, 16, 14, 16)["front"], ((4, 5), (5, 5), (10, 5), (11, 5)), (250, 220, 120, 255),
         tuple(((x, 10), (40, 10, 10, 255)) for x in range(5, 11)))
    for f in box_faces(0, 32, 4, 16, 4).values():
        paint(img, f, ramp(hexc("#5A4A50"), 5, spread=0.25), rng)
    return img


def end_crystal():
    # Core 6x6x6 @ (0,0): pink-magenta; glass cube 8x8x8 @ (0,16): only its edges
    # (the rest clear, so the core shows); base 12x4x12 @ (0,40): dark bedrock grey.
    rng = random.Random("end_crystal")
    img = Img(64, 64, CLEAR)
    for f in box_faces(0, 0, 6, 6, 6).values():
        paint(img, f, ramp(hexc("#C050B8"), 5, spread=0.35), rng)
    glass = ramp(hexc("#B8C8D8"), 5, spread=0.2)
    for (x0, y0, w, h) in box_faces(0, 16, 8, 8, 8).values():
        for y in range(h):
            for x in range(w):
                if x in (0, w - 1) or y in (0, h - 1):
                    img.set(x0 + x, y0 + y, glass[rng.randrange(1, 4)])
    for f in box_faces(0, 40, 12, 4, 12).values():
        paint(img, f, ramp(hexc("#4A4A4E"), 5, spread=0.4), rng)
    return img


def ender_dragon():
    # Body 8x6x16 @ (0,0), neck 4x4x8 @ (0,22), head 6x5x8 @ (24,22) with purple eyes,
    # tail 3x3x8 @ (0,35), wing 20x1x10 @ (0,46): black scales, grey membranes.
    rng = random.Random("ender_dragon")
    img = Img(64, 64, CLEAR)
    scales = ramp(hexc("#1E1A22"), 5, spread=0.45)
    for (u, v, w, h, d) in ((0, 0, 8, 6, 16), (0, 22, 4, 4, 8), (24, 22, 6, 5, 8), (0, 35, 3, 3, 8)):
        for f in box_faces(u, v, w, h, d).values():
            paint(img, f, scales, rng)
    x0, y0, w, h = box_faces(24, 22, 6, 5, 8)["front"]
    for ex in (1, w - 2):
        img.set(x0 + ex, y0 + 1, (200, 80, 230, 255))
    membrane = ramp(hexc("#3A3440"), 5, spread=0.3)
    for name, f in box_faces(0, 46, 20, 1, 10).items():
        paint(img, f, membrane if name in ("top", "bottom") else scales, rng)
    return img


def shulker():
    # Lid 16x12x16 @ (0,0) and base 16x8x16 @ (0,28): purple shell plates; head 6x6x6 @
    # (0,52): pale yellow-green with dark eyes.
    rng = random.Random("shulker")
    img = Img(64, 64, CLEAR)
    shell = ramp(hexc("#946894"), 5, spread=0.35)
    for (u, v, w, h, d) in ((0, 0, 16, 12, 16), (0, 28, 16, 8, 16)):
        for name, f in box_faces(u, v, w, h, d).items():
            paint(img, f, shell, rng)
            x0, y0, fw, fh = f
            for x in range(fw): # plate seams
                if x % 4 == 0:
                    for y in range(fh):
                        img.set(x0 + x, y0 + y, shell[0])
    for f in box_faces(0, 52, 6, 6, 6).values():
        paint(img, f, ramp(hexc("#D8D890"), 5, spread=0.2), rng)
    x0, y0, _, _ = box_faces(0, 52, 6, 6, 6)["front"]
    for ex in (1, 4):
        img.set(x0 + ex, y0 + 2, (40, 30, 50, 255))
    return img


def minecart():
    # Floor 14x2x18 @ (0,0), long sides 2x6x18 @ (0,20), ends 10x6x2 @ (40,20): iron
    # plates with darker rivets along the edges.
    rng = random.Random("minecart")
    img = Img(64, 64, CLEAR)
    iron = ramp(hexc("#7C7C84"), 5, spread=0.3)
    for (u, v, w, h, d) in ((0, 0, 14, 2, 18), (0, 20, 2, 6, 18), (40, 20, 10, 6, 2)):
        for f in box_faces(u, v, w, h, d).values():
            paint(img, f, iron, rng)
            x0, y0, fw, fh = f
            for x in range(0, fw, 4):
                img.set(x0 + x, y0, iron[0])
    return img


def slime():
    # A green jelly cube with darker specks, two dark eyes and a mouth. Body 8x8x8 @ (0,0).
    rng = random.Random("slime")
    img = Img(64, 64, CLEAR)
    green = ramp(hexc("#6CC060"), 5, spread=0.3)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, green, rng)
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 2), (2, 2), (5, 2), (6, 2)), (30, 60, 30, 255),
         (((4, 5), (40, 80, 40, 255)),))
    return img


def projectiles():
    # The arrow seen from the side, 16 x 5 at (0, 0), tip at +x: fletching, shaft, head.
    img = Img(64, 64, CLEAR)
    shaft = ramp(hexc("#8A6A42"), 5)
    head = ramp(hexc("#6A6A72"), 5)
    feather = ramp(hexc("#E6E6E6"), 5)
    for x in range(2, 13):
        img.set(x, 2, shaft[2])
    for x, y in ((13, 1), (13, 2), (13, 3), (14, 2), (15, 2), (14, 1), (14, 3)):
        img.set(x, y, head[2 if y == 2 else 1])
    for x in range(0, 4):
        img.set(x, 0, feather[2])
        img.set(x, 4, feather[2])
        img.set(x + 1, 1, feather[3])
        img.set(x + 1, 3, feather[3])
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for 4x previews")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    images = {"zombie": zombie(), "cow": cow(), "sheep": sheep(), "sheep_wool": sheep_wool(), "pig": pig(),
              "chicken": chicken(), "projectiles": projectiles(), "skeleton": skeleton(), "creeper": creeper(),
              "spider": spider(), "enderman": enderman(), "ghast": ghast(), "blaze": blaze(),
              "magma_cube": magma_cube(), "zombified_piglin": zombified_piglin(), "piglin": piglin(),
              "hoglin": hoglin(), "strider": strider(),
              "end_crystal": end_crystal(), "ender_dragon": ender_dragon(),
              "shulker": shulker(), "minecart": minecart(),
              "slime": slime()}
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
