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


def skeleton(name="skeleton", colour="#C8C8C0"):
    # Bone white, dark eye sockets. Head 8x8x8 @ (0,0), body 8x12x4 @ (16,16), arm
    # 2x12x2 @ (40,16), leg 2x12x2 @ (0,16). (M26.4a: wither skeletons are charcoal.)
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    bone = ramp(hexc(colour), 5, spread=0.2)
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


def spider(name="spider", colour="#3A3028"):
    # Dark brown-black hairy body, red eyes. Head 8x8x8 @ (0,0), thorax 6x6x6 @ (32,0),
    # abdomen 10x8x12 @ (0,16), leg 16x2x2 @ (0,40). (M26.4a: cave spiders dark teal.)
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    body = ramp(hexc(colour), 5, spread=0.35)
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


def villager():
    """Villager (M24.1, our layout): head 8x10x8 @ (0,0), nose 2x4x2 @ (24,0), body
    8x12x6 @ (16,20), crossed arms 8x4x4 @ (0,40), leg 4x12x4 @ (0,48)."""
    rng = random.Random("villager")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#B98A65"), 5, spread=0.2)
    robe = ramp(hexc("#6A4A30"), 5, spread=0.25)
    for name, f in box_faces(0, 0, 8, 10, 8).items():
        paint(img, f, skin, rng, noise=0.2)
    fx, fy = 8, 8  # the face: a unibrow, green eyes, a mouth line
    for x in range(1, 7):
        img.set(fx + x, fy + 3, (60, 40, 30, 255))
    for x, c in ((2, (255, 255, 255, 255)), (3, (40, 140, 60, 255)), (4, (40, 140, 60, 255)), (5, (255, 255, 255, 255))):
        img.set(fx + x, fy + 4, c)
    for x in range(3, 5):
        img.set(fx + x, fy + 8, skin[0])
    for f in box_faces(24, 0, 2, 4, 2).values():
        paint(img, f, skin, rng, noise=0.15)
    for f in box_faces(16, 20, 8, 12, 6).values():
        paint(img, f, robe, rng)
    for name, f in box_faces(0, 40, 8, 4, 4).items():  # arms: sleeves, hands in the middle
        paint(img, f, robe, rng)
        if name == "front":
            paint(img, (f[0] + 2, f[1], 4, f[3]), skin, rng, noise=0.15)
    for f in box_faces(0, 48, 4, 12, 4).values():
        paint(img, f, ramp(hexc("#4A3A2A"), 5, spread=0.25), rng)
    return img


def zombie_villager():
    """A villager's build in zombie green, with dark eye sockets (M24.3)."""
    rng = random.Random("zombie_villager")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#5A8C3C"), 5, spread=0.25)
    robe = ramp(hexc("#4A3A2A"), 5, spread=0.25)
    for f in box_faces(0, 0, 8, 10, 8).values():
        paint(img, f, skin, rng, noise=0.3)
    fx, fy = 8, 8
    for x in range(1, 7):
        img.set(fx + x, fy + 3, skin[0])
    for x, c in ((2, (24, 30, 20, 255)), (3, (160, 30, 30, 255)), (4, (160, 30, 30, 255)), (5, (24, 30, 20, 255))):
        img.set(fx + x, fy + 4, c)
    for f in box_faces(24, 0, 2, 4, 2).values():
        paint(img, f, skin, rng, noise=0.2)
    for f in box_faces(16, 20, 8, 12, 6).values():
        paint(img, f, robe, rng)
    for name, f in box_faces(0, 40, 8, 4, 4).items():
        paint(img, f, robe, rng)
        if name == "front":
            paint(img, (f[0] + 2, f[1], 4, f[3]), skin, rng, noise=0.2)
    for f in box_faces(0, 48, 4, 12, 4).values():
        paint(img, f, ramp(hexc("#3A3A2A"), 5, spread=0.25), rng)
    return img


def iron_golem():
    """Iron golem (M24.3, our layout): head 8x10x8 @ (0,0), nose 2x4x2 @ (32,0), arm
    4x30x6 @ (40,0), leg 6x16x5 @ (0,18), body 18x12x11 @ (0,41) (the waist reuses it)."""
    rng = random.Random("iron_golem")
    img = Img(64, 64, CLEAR)
    iron = ramp(hexc("#C8C0B8"), 5, spread=0.25)
    for f in box_faces(0, 0, 8, 10, 8).values():
        paint(img, f, iron, rng)
    fx, fy = 8, 8  # brow, red eyes
    for x in range(0, 8):
        img.set(fx + x, fy + 3, iron[0])
    for x in (2, 5):
        img.set(fx + x, fy + 4, (200, 40, 30, 255))
    for f in box_faces(32, 0, 2, 4, 2).values():
        paint(img, f, iron, rng)
    for f in box_faces(40, 0, 4, 30, 6).values():
        paint(img, f, iron, rng)
    for f in box_faces(0, 18, 6, 16, 5).values():
        paint(img, f, iron, rng)
    for name, (x0, y0, w, h) in box_faces(0, 41, 18, 12, 11).items():
        paint(img, (x0, y0, w, h), iron, rng)
        if name in ("front", "back"):  # vines over the chest
            for _ in range(10):
                img.set(x0 + rng.randrange(w), y0 + rng.randrange(h), (70, 120, 40, 255))
    return img


def witch():
    """Witch (M24.4): the villager layout in a purple robe, plus a hat - brim 10x1x10 @
    (24,40), cone 7x4x7 @ (24,51), tip 4x3x4 @ (40,0) - and a wart on the nose."""
    img = villager()
    rng = random.Random("witch")
    robe = ramp(hexc("#4A2A5A"), 5, spread=0.25)
    hat = ramp(hexc("#2A2A30"), 5, spread=0.25)
    for f in box_faces(16, 20, 8, 12, 6).values():
        paint(img, f, robe, rng)
    for name, f in box_faces(0, 40, 8, 4, 4).items():
        paint(img, f, robe, rng)
        if name == "front":
            paint(img, (f[0] + 2, f[1], 4, f[3]), ramp(hexc("#B98A65"), 5, spread=0.2), rng, noise=0.15)
    for u, v, w, h, d in ((24, 40, 10, 1, 10), (24, 51, 7, 4, 7), (40, 0, 4, 3, 4)):
        for f in box_faces(u, v, w, h, d).values():
            paint(img, f, hat, rng)
    img.set(25, 3, (60, 140, 50, 255))  # the wart (on the nose's front)
    return img


def wandering_trader():
    """Wandering trader (M24.4): the villager layout in a blue robe with a gold trim."""
    img = villager()
    rng = random.Random("wandering_trader")
    robe = ramp(hexc("#2A4A9A"), 5, spread=0.25)
    for name, (x0, y0, w, h) in box_faces(16, 20, 8, 12, 6).items():
        paint(img, (x0, y0, w, h), robe, rng)
        if name in ("front", "back"):
            paint(img, (x0, y0 + h - 2, w, 1), ramp(hexc("#D8B040"), 5), rng)
    for name, f in box_faces(0, 40, 8, 4, 4).items():
        paint(img, f, robe, rng)
        if name == "front":
            paint(img, (f[0] + 2, f[1], 4, f[3]), ramp(hexc("#B98A65"), 5, spread=0.2), rng, noise=0.15)
    return img


def pillager():
    """Pillager (M24.4): an illager - the villager's head in grey skin, a dark tunic,
    arms 4x12x4 @ (40,16) (held forward with a crossbow), legs as the villager's."""
    rng = random.Random("pillager")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#8A8A86"), 5, spread=0.2)
    cloth = ramp(hexc("#3A3A40"), 5, spread=0.25)
    for f in box_faces(0, 0, 8, 10, 8).values():
        paint(img, f, skin, rng, noise=0.2)
    fx, fy = 8, 8  # a heavy brow, dark eyes, a frown
    for x in range(1, 7):
        img.set(fx + x, fy + 3, (40, 40, 40, 255))
    for x in (2, 5):
        img.set(fx + x, fy + 4, (30, 30, 30, 255))
    for x in range(2, 6):
        img.set(fx + x, fy + 7, skin[0])
    for f in box_faces(24, 0, 2, 4, 2).values():
        paint(img, f, skin, rng, noise=0.15)
    for f in box_faces(16, 20, 8, 12, 6).values():
        paint(img, f, cloth, rng)
    for name, (x0, y0, w, h) in box_faces(40, 16, 4, 12, 4).items():
        paint(img, (x0, y0, w, h), cloth, rng)
        if name not in ("top", "bottom"):
            paint(img, (x0, y0 + h - 3, w, 3), skin, rng, noise=0.15)
    for f in box_faces(0, 48, 4, 12, 4).values():
        paint(img, f, ramp(hexc("#2A2A2E"), 5, spread=0.25), rng)
    return img


def vindicator():
    """Vindicator (M24.5): the pillager's build in a dark grey-blue coat."""
    img = pillager()
    rng = random.Random("vindicator")
    cloth = ramp(hexc("#2A3440"), 5, spread=0.25)
    for f in box_faces(16, 20, 8, 12, 6).values():
        paint(img, f, cloth, rng)
    for name, (x0, y0, w, h) in box_faces(40, 16, 4, 12, 4).items():
        paint(img, (x0, y0, w, h), cloth, rng)
        if name not in ("top", "bottom"):
            paint(img, (x0, y0 + h - 3, w, 3), ramp(hexc("#8A8A86"), 5, spread=0.2), rng, noise=0.15)
    return img


def evoker():
    """Evoker (M24.5): an illager in a long black robe with gold trim (the villager layout,
    arms crossed)."""
    img = villager()
    rng = random.Random("evoker")
    skin = ramp(hexc("#8A8A86"), 5, spread=0.2)
    robe = ramp(hexc("#202024"), 5, spread=0.25)
    for f in box_faces(0, 0, 8, 10, 8).values():
        paint(img, f, skin, rng, noise=0.2)
    fx, fy = 8, 8
    for x in range(1, 7):
        img.set(fx + x, fy + 3, (40, 40, 40, 255))
    for x in (2, 5):
        img.set(fx + x, fy + 4, (30, 30, 30, 255))
    for f in box_faces(24, 0, 2, 4, 2).values():
        paint(img, f, skin, rng, noise=0.15)
    for name, (x0, y0, w, h) in box_faces(16, 20, 8, 12, 6).items():
        paint(img, (x0, y0, w, h), robe, rng)
        if name in ("front", "back"):
            paint(img, (x0 + w // 2, y0, 1, h), ramp(hexc("#D8B040"), 5), rng)
    for name, f in box_faces(0, 40, 8, 4, 4).items():
        paint(img, f, robe, rng)
        if name == "front":
            paint(img, (f[0] + 2, f[1], 4, f[3]), skin, rng, noise=0.15)
    return img


def vex():
    """Vex (M24.5, our layout): head 5x5x5 @ (0,0), body 3x5x2 @ (0,10), arm 1x5x1 @
    (20,0), tail 2x3x2 @ (0,17), wing 6x5x1 @ (24,10)."""
    rng = random.Random("vex")
    img = Img(64, 64, CLEAR)
    body = ramp(hexc("#B8C8D8"), 5, spread=0.2)
    wing = ramp(hexc("#E8F0F8"), 5, spread=0.15)
    for u, v, w, h, d in ((0, 0, 5, 5, 5), (0, 10, 3, 5, 2), (20, 0, 1, 5, 1), (0, 17, 2, 3, 2)):
        for f in box_faces(u, v, w, h, d).values():
            paint(img, f, body, rng, noise=0.2)
    for f in box_faces(24, 10, 6, 5, 1).values():
        paint(img, f, wing, rng, noise=0.15)
    img.set(6, 7, (40, 40, 60, 255))  # eyes on the face (front: x 5..10, y 5..10)
    img.set(8, 7, (40, 40, 60, 255))
    return img


def ravager():
    """Ravager (M24.5, our layout): body 12x14x18 @ (0,0), head 10x10x10 @ (0,32), horn
    2x6x2 @ (40,32), leg 6x12x6 @ (40,40)."""
    rng = random.Random("ravager")
    img = Img(64, 64, CLEAR)
    hide = ramp(hexc("#5A564E"), 5, spread=0.25)
    for f in box_faces(0, 0, 12, 14, 18).values():
        paint(img, f, hide, rng)
    for name, f in box_faces(0, 32, 10, 10, 10).items():
        paint(img, f, hide, rng)
        if name == "front":  # eyes and a dark muzzle
            img.set(f[0] + 2, f[1] + 3, (220, 200, 120, 255))
            img.set(f[0] + 7, f[1] + 3, (220, 200, 120, 255))
            paint(img, (f[0] + 2, f[1] + 6, 6, 3), ramp(hexc("#2A2824"), 5), rng)
    for f in box_faces(40, 32, 2, 6, 2).values():
        paint(img, f, ramp(hexc("#D8D0B8"), 5, spread=0.15), rng)
    for f in box_faces(40, 40, 6, 12, 6).values():
        paint(img, f, hide, rng)
    return img


def villager_apron():
    """The profession robe over the body (8x18x6 @ (16,20), inflated): greyscale cloth
    the game tints per profession, with a darker belt."""
    rng = random.Random("villager_apron")
    img = Img(64, 64, CLEAR)
    cloth = ramp(hexc("#D8D8D8"), 5, spread=0.2)
    for name, (x0, y0, w, h) in box_faces(16, 20, 8, 18, 6).items():
        paint(img, (x0, y0, w, h), cloth, rng, noise=0.25)
        if name not in ("top", "bottom"):
            paint(img, (x0, y0 + 6, w, 1), ramp(hexc("#707070"), 5), rng)
    return img


def fish(name, body, head, tail, fins, base, belly, eye=(16, 16, 20, 255), stripes=None):
    """A fish (M25.2): boxes (u, v, w, h, d); the body darker on top, a pale belly, an eye
    on each side of the head (or the body's front end), optional dark stripes."""
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    top = ramp(hexc(base), 5, spread=0.3)
    low = ramp(hexc(belly), 5, spread=0.2)
    for box in (body, head, tail) + tuple(fins):
        if box is None:
            continue
        u, v, w, h, d = box
        for fname, (x0, y0, fw, fh) in box_faces(u, v, w, h, d).items():
            for y in range(fh):
                for x in range(fw):
                    pal = low if (fname == "bottom" or (fname in ("left", "right", "front", "back") and y >= fh * 0.6)) else top
                    r = rng.random()
                    img.set(x0 + x, y0 + y, pal[2 if r > 0.35 else (1 if r < 0.17 else 3)])
    if stripes:
        u, v, w, h, d = body
        for fname in ("left", "right"):
            x0, y0, fw, fh = box_faces(u, v, w, h, d)[fname]
            for x in range(1, fw, 3):
                for y in range(fh - 1):
                    img.set(x0 + x, y0 + y, hexc(stripes))
    eyebox = head or body
    u, v, w, h, d = eyebox
    for fname in ("left", "right"):
        x0, y0, fw, fh = box_faces(u, v, w, h, d)[fname]
        img.set(x0 + (fw - 2 if fname == "left" else 1), y0 + 1, eye)
    return img


def tropical_fish():
    """Greyscale (tinted by the fish's two colours): body and tail light, a pattern layer
    (same box) with stripes and alpha between them."""
    rng = random.Random("tropical_fish")
    img = Img(64, 64, CLEAR)
    light = ramp(hexc("#E8E8E8"), 5, spread=0.12)
    for u, v, w, h, d in ((0, 0, 2, 5, 6), (0, 32, 1, 5, 4)):
        for f in box_faces(u, v, w, h, d).values():
            paint(img, f, light, rng, noise=0.2)
    for fname, (x0, y0, fw, fh) in box_faces(0, 16, 2, 5, 6).items():  # the pattern: two bands
        for y in range(fh):
            for x in range(fw):
                if (x // 2) % 2 == 0 and fname in ("left", "right", "top"):
                    img.set(x0 + x, y0 + y, light[2])
    for f in box_faces(16, 32, 1, 2, 4).values():
        paint(img, f, light, rng, noise=0.2)
    x0, y0, fw, fh = box_faces(0, 0, 2, 5, 6)["left"]
    img.set(x0 + fw - 2, y0 + 1, (16, 16, 20, 255))
    x0, y0, fw, fh = box_faces(0, 0, 2, 5, 6)["right"]
    img.set(x0 + 1, y0 + 1, (16, 16, 20, 255))
    return img


def pufferfish():
    rng = random.Random("pufferfish")
    img = Img(64, 64, CLEAR)
    yellow = ramp(hexc("#E8C23A"), 5, spread=0.25)
    pale = ramp(hexc("#F2E6B0"), 5, spread=0.15)
    spine = ramp(hexc("#D8D8C8"), 5, spread=0.15)
    for fname, f in box_faces(0, 0, 8, 8, 8).items():
        paint(img, f, pale if fname == "bottom" else yellow, rng)
        x0, y0, fw, fh = f
        for k in range(4):  # brown spots
            img.set(x0 + rng.randrange(fw), y0 + rng.randrange(fh), (120, 84, 40, 255))
    face(img, box_faces(0, 0, 8, 8, 8)["front"], ((1, 2), (6, 2)), (20, 20, 20, 255),
         tuple(((x, 6), (150, 90, 60, 255)) for x in range(3, 5)))
    for f in box_faces(0, 20, 1, 4, 3).values():
        paint(img, f, yellow, rng)
    for box in ((32, 0, 1, 2, 2), (32, 4, 2, 1, 2)):
        for f in box_faces(*box).values():
            paint(img, f, spine, rng)
    return img


def squid(name, base, spot):
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    body = ramp(hexc(base), 5, spread=0.3)
    for fname, f in box_faces(0, 0, 12, 16, 12).items():
        paint(img, f, body, rng, noise=0.4)
        x0, y0, fw, fh = f
        for k in range(6):  # spots
            img.set(x0 + rng.randrange(fw), y0 + rng.randrange(fh), hexc(spot))
    for fname in ("front", "back", "left", "right"):  # eyes low on every side
        x0, y0, fw, fh = box_faces(0, 0, 12, 16, 12)[fname]
        for x in (3, 8):
            img.set(x0 + x, y0 + fh - 4, (240, 240, 230, 255))
            img.set(x0 + x, y0 + fh - 3, (20, 20, 30, 255))
    for f in box_faces(48, 0, 2, 10, 2).values():
        paint(img, f, body, rng, noise=0.4)
    return img


def drowned():
    """The zombie layout (M25.3) in sea colours: teal-grey skin, a torn blue-green shirt,
    darker trousers, glowing cyan eyes."""
    rng = random.Random("drowned")
    img = Img(64, 64, CLEAR)
    skin = ramp(hexc("#4E8C86"), 5, spread=0.3)
    shirt = ramp(hexc("#3C7A6A"), 5, spread=0.3)
    pants = ramp(hexc("#4A5A6E"), 5, spread=0.25)
    for f in box_faces(0, 0, 8, 8, 8).values():
        paint(img, f, skin, rng, noise=0.45)
    fx, fy = 8, 8
    for x, y in ((1, 3), (2, 3), (5, 3), (6, 3)):
        img.set(fx + x, fy + y, (80, 230, 230, 255))
    for x in range(2, 6):
        img.set(fx + x, fy + 6, skin[0])
    for f in box_faces(16, 16, 8, 12, 4).values():
        paint(img, f, shirt, rng, noise=0.45)
    for u, v in ((40, 16), (32, 48)):
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), skin, rng, noise=0.45)
            if name not in ("top", "bottom"):
                paint(img, (x0, y0, w, 3), shirt, rng)
    for u, v in ((0, 16), (16, 48)):
        for name, (x0, y0, w, h) in box_faces(u, v, 4, 12, 4).items():
            paint(img, (x0, y0, w, h), pants, rng)
    return img


def boxed(name, boxes, eyes=None, shell=None):
    """Boxes painted from (top colour, belly colour) palettes (M25.3b dolphin, turtle): the
    belly colour on bottoms and the lower part of the sides; eyes on the eye box's sides
    (or front); a shell gets a darker plate pattern on top."""
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    for (u, v, w, h, d), top, belly in boxes:
        tp = ramp(hexc(top), 5, spread=0.3)
        bp = ramp(hexc(belly), 5, spread=0.2) if belly else tp
        for fname, (x0, y0, fw, fh) in box_faces(u, v, w, h, d).items():
            for y in range(fh):
                for x in range(fw):
                    low = fname == "bottom" or (fname in ("left", "right", "front", "back") and y >= fh * 0.6)
                    pal = bp if low else tp
                    r = rng.random()
                    img.set(x0 + x, y0 + y, pal[2 if r > 0.35 else (1 if r < 0.17 else 3)])
    if shell:
        x0, y0, fw, fh = box_faces(*shell)["top"]
        for y in range(fh):
            for x in range(fw):
                if x % 4 == 0 or y % 5 == 0:
                    img.set(x0 + x, y0 + y, (40, 70, 30, 255))
    if eyes:
        for fname in ("left", "right"):
            x0, y0, fw, fh = box_faces(*eyes)[fname]
            img.set(x0 + (fw - 2 if fname == "left" else 1), y0 + 1, (20, 20, 24, 255))
    return img


def guardian(name, body, spike):
    """A spiny prismarine-coloured body with plates, one eye (M25.5): body 12x12x16 @ (0,0),
    eye 2x2x1 @ (56,0), tail 4x4x8 / 3x3x7 / 2x2x6 @ (0,30) (24,30) (44,30), spikes @ (56,4)."""
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    bp = ramp(hexc(body), 5, spread=0.35)
    for box in ((0, 0, 12, 12, 16), (0, 30, 4, 4, 8), (24, 30, 3, 3, 7), (44, 30, 2, 2, 6)):
        for (x0, y0, w, h) in box_faces(*box).values():
            for y in range(h):
                for x in range(w):
                    r = rng.random()
                    c = bp[2 if r > 0.35 else (1 if r < 0.17 else 3)]
                    if (x // 3 + y // 3) % 3 == 0:
                        c = bp[0]  # darker plates
                    img.set(x0 + x, y0 + y, c)
    for (x0, y0, w, h) in box_faces(56, 0, 2, 2, 1).values():  # the eye
        for y in range(h):
            for x in range(w):
                img.set(x0 + x, y0 + y, (240, 230, 200, 255))
    fx, fy, _, _ = box_faces(56, 0, 2, 2, 1)["front"]
    img.set(fx, fy + 1, (40, 30, 60, 255))
    for box in ((56, 4, 1, 4, 1), (56, 10, 4, 1, 1)):
        for f in box_faces(*box).values():
            paint(img, f, ramp(hexc(spike), 5, spread=0.2), rng)
    return img


def pet(name, boxes, eyebox, nosebox, base="#E4E4E4", collar=None, stripes=False, spots=False, extra=()):
    """Pets (M26.1): light fur (the game tints wolves, cats and parrots by variant), dark
    eyes on the head's front, a dark nose; ocelots keep their own yellow with spots."""
    rng = random.Random(name)
    img = Img(64, 64, CLEAR)
    fur = ramp(hexc(base), 5, spread=0.25)
    for (u, v, w, h, d) in boxes:
        for fname, (x0, y0, fw, fh) in box_faces(u, v, int(round(w + 0.49)), int(round(h + 0.49)), int(round(d + 0.49))).items():
            for y in range(fh):
                for x in range(fw):
                    r = rng.random()
                    c = fur[2 if r > 0.35 else (1 if r < 0.17 else 3)]
                    if stripes and (x + y) % 5 == 0:
                        c = fur[0]
                    if spots and rng.random() < 0.12:
                        c = (70, 50, 30, 255)
                    img.set(x0 + x, y0 + y, c)
    for box, colour in extra:
        u, v, w, h, d = box
        for f in box_faces(u, v, max(1, int(round(w))), max(1, int(round(h))), max(1, int(round(d)))).values():
            paint(img, f, ramp(hexc(colour), 5, spread=0.2), rng)
    u, v, w, h, d = eyebox
    fx, fy, fw, fh = box_faces(u, v, int(w), int(h), int(d))["front"]
    img.set(fx, fy + 1, (24, 20, 20, 255))
    img.set(fx + fw - 1, fy + 1, (24, 20, 20, 255))
    if nosebox:
        u, v, w, h, d = nosebox
        fx, fy, fw, fh = box_faces(u, v, int(w), int(h), int(d))["front"]
        img.set(fx + fw // 2, fy, (30, 24, 24, 255))
    if collar:
        u, v, w, h, d = collar
        for f in box_faces(u, v, int(round(w)), int(h), max(1, int(round(d)))).values():
            paint(img, f, ramp(hexc("#E8E8E8"), 5, spread=0.1), rng, noise=0.1)
    return img


def mount_gear():
    """Mount gear (M26.2), one texture for every mount: a leather saddle @ (0,0), chest
    packs @ (36,0), horse armor plates @ (0,16) (light: tinted by its material), a
    carpet @ (0,48) (white wool: tinted by its dye)."""
    rng = random.Random("mount_gear")
    img = Img(64, 64, CLEAR)
    leather = ramp(hexc("#7A4424"), 5, spread=0.3)
    for y in range(0, 12):
        for x in range(0, 36):
            img.set(x, y, leather[0] if y in (0, 11) else leather[2 if rng.random() > 0.3 else 3])
    wood = ramp(hexc("#A8742E"), 5, spread=0.3)
    for y in range(0, 16):
        for x in range(36, 58):
            c = wood[2 if rng.random() > 0.3 else 1]
            if y % 4 == 0:
                c = wood[0]
            img.set(x, y, c)
    for (x, y) in ((46, 6), (47, 6), (46, 7), (47, 7)):
        img.set(x, y, (200, 200, 205, 255))  # a latch
    metal = ramp(hexc("#E8E8E8"), 5, spread=0.2)
    for y in range(16, 48):
        for x in range(64):
            c = metal[2 if rng.random() > 0.25 else 1]
            if (x % 8 == 0) or (y % 6 == 0):
                c = metal[3]  # plate seams
            img.set(x, y, c)
    wool = ramp(hexc("#F0F0F0"), 5, spread=0.15)
    for y in range(48, 64):
        for x in range(64):
            img.set(x, y, wool[2 if rng.random() > 0.3 else 1])
    return img


def boat():
    """Greyscale planks (tinted per wood, M25.2b): bottom 10x1x14 @ (0,0), sides 1x3x14 @
    (0,16), ends 8x3x1 @ (0,36) (the model is drawn at twice its size); plank seams."""
    rng = random.Random("boat")
    img = Img(64, 64, CLEAR)
    wood = ramp(hexc("#D0D0D0"), 5, spread=0.22)
    for box in ((0, 0, 10, 1, 14), (0, 16, 1, 3, 14), (0, 36, 8, 3, 1)):
        for (x0, y0, w, h) in box_faces(*box).values():
            for y in range(h):
                for x in range(w):
                    r = rng.random()
                    c = wood[2 if r > 0.3 else (1 if r < 0.15 else 3)]
                    if (x + 1) % 3 == 0 and w >= 8:
                        c = wood[0]
                    img.set(x0 + x, y0 + y, c)
    return img


def trader_llama():
    """A creamy llama under the trader's blue and gold blanket (top and upper sides)."""
    img = pet("trader_llama", LLAMA_BOXES, (40, 0, 6, 5, 6), None, base="#D8C8A0")
    rng = random.Random("trader_blanket")
    blue = ramp(hexc("#2E4E9A"), 5, spread=0.25)
    faces = box_faces(0, 36, 12, 10, 18)
    x0, y0, w, h = faces["top"]
    paint(img, (x0, y0, w, h), blue, rng)
    for name in ("left", "right", "front", "back"):
        x0, y0, w, h = faces[name]
        paint(img, (x0, y0, w, 4), blue, rng)
        for x in range(w):
            img.set(x0 + x, y0 + 4, (230, 180, 50, 255))  # gold trim
    return img


def panda():
    """A panda (M26.3): white body and head, black legs, ears, shoulder band and eye patches."""
    rng = random.Random("panda")
    img = pet("panda", [(0, 35, 13, 11, 18), (20, 0, 9, 8, 7), (52, 4, 4, 3, 2)], (20, 0, 9, 8, 7), (52, 4, 4, 3, 2),
              base="#F0F0EC")
    black = ramp(hexc("#24242A"), 5, spread=0.2)
    for box in ((0, 0, 5, 9, 5), (52, 0, 3, 3, 1)):
        for f in box_faces(*box).values():
            paint(img, f, black, rng)
    body = box_faces(0, 35, 13, 11, 18)
    for name in ("left", "right"):  # a dark band over the shoulders
        x0, y0, w, h = body[name]
        paint(img, (x0 + w - 5, y0, 4, h), black, rng)
    x0, y0, w, h = body["top"]
    paint(img, (x0, y0 + h - 5, w, 4), black, rng)
    fx, fy, fw, fh = box_faces(20, 0, 9, 8, 7)["front"]
    for (ex, ey) in ((1, 2), (2, 2), (1, 3), (2, 3), (6, 2), (7, 2), (6, 3), (7, 3)):  # eye patches
        img.set(fx + ex, fy + ey, black[2])
    img.set(fx + 2, fy + 2, (250, 250, 250, 255))
    img.set(fx + 6, fy + 2, (250, 250, 250, 255))
    return img


def bee():
    """A bee (M26.3b): yellow body with dark stripes, pale see-through wings, a dark
    stinger and antennae."""
    rng = random.Random("bee")
    img = Img(64, 64, CLEAR)
    yellow = ramp(hexc("#F0C030"), 5, spread=0.25)
    dark = ramp(hexc("#2A2018"), 5, spread=0.2)
    for name, (x0, y0, w, h) in box_faces(0, 0, 7, 7, 10).items():
        for y in range(h):
            for x in range(w):
                along = x if name in ("left", "right") else (y if name in ("top", "bottom") else -1)
                c = yellow[2 if rng.random() > 0.3 else 1]
                if along >= 0 and along % 4 == 2:
                    c = dark[2]
                img.set(x0 + x, y0 + y, c)
    fx, fy, fw, fh = box_faces(0, 0, 7, 7, 10)["front"]
    for (ex, ey) in ((1, 2), (1, 3), (5, 2), (5, 3)):
        img.set(fx + ex, fy + ey, (20, 20, 30, 255))
    for (x0, y0, w, h) in box_faces(0, 18, 8, 1, 6).values():
        for y in range(h):
            for x in range(w):
                img.set(x0 + x, y0 + y, (220, 235, 245, 170))
    for box in ((34, 0, 1, 1, 2), (34, 4, 1, 2, 3), (16, 18, 7, 2, 1)):
        for f in box_faces(*box).values():
            paint(img, f, dark, rng)
    return img


HORSE_BOXES = [(0, 0, 4, 11, 4), (16, 0, 5, 5, 10), (46, 0, 3, 10, 4), (0, 15, 4, 10, 6), (20, 15, 2, 10, 3),
               (30, 15, 2, 3, 1), (0, 32, 10, 10, 22)]
LLAMA_BOXES = [(0, 0, 4, 11, 4), (16, 0, 6, 12, 6), (40, 0, 6, 5, 6), (16, 18, 2, 3, 2), (24, 18, 2, 4, 2),
               (0, 36, 12, 10, 18)]


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
              "slime": slime(), "villager": villager(), "villager_apron": villager_apron(),
              "zombie_villager": zombie_villager(), "iron_golem": iron_golem(),
              "witch": witch(), "wandering_trader": wandering_trader(),
              "pillager": pillager(), "vindicator": vindicator(), "evoker": evoker(), "vex": vex(),
              "ravager": ravager(),
              # M25.2: water mobs (layouts match MobModels.cpp's kCod, kSalmon, ...)
              "cod": fish("cod", (0, 0, 2, 4, 7), (0, 12, 2, 3, 3), (0, 20, 1, 4, 5), [(20, 0, 1, 1, 4)],
                          "#A08860", "#D8CDB0"),
              "salmon": fish("salmon", (0, 0, 3, 5, 9), (0, 16, 2, 4, 3), (0, 24, 1, 5, 6),
                             [(24, 0, 1, 1, 5), (24, 8, 1, 1, 3)], "#9A3A30", "#C88070", stripes="#5E8A6A"),
              "tropical_fish": tropical_fish(), "pufferfish": pufferfish(),
              "squid": squid("squid", "#3A5070", "#5A7898"), "glow_squid": squid("glow_squid", "#1E8C8A", "#9AFFE8"),
              "boat": boat(), "drowned": drowned(),
              "dolphin": boxed("dolphin", [((0, 0, 8, 7, 13), "#7E8E9E", "#C8D0D8"), ((0, 22, 6, 5, 5), "#7E8E9E", "#C8D0D8"),
                                           ((24, 22, 2, 2, 4), "#8E9EAE", "#D8E0E8"), ((42, 0, 1, 4, 4), "#6E7E8E", None),
                                           ((0, 34, 10, 1, 6), "#6E7E8E", None), ((32, 34, 3, 1, 3), "#6E7E8E", None)],
                               eyes=(0, 22, 6, 5, 5)),
              "turtle": boxed("turtle", [((0, 0, 12, 5, 14), "#3E7A30", "#C8C080"), ((0, 20, 10, 1, 12), "#C8C080", None),
                                         ((0, 34, 4, 3, 4), "#8AB050", None), ((16, 34, 3, 1, 3), "#8AB050", None),
                                         ((28, 34, 3, 1, 2), "#8AB050", None)],
                              eyes=(0, 34, 4, 3, 4), shell=(0, 0, 12, 5, 14)),
              "guardian": guardian("guardian", "#5E9A8C", "#D88A40"),
              "elder_guardian": guardian("elder_guardian", "#C8C4B0", "#8A7A9A"),
              # M26.1 pets: light fur the game tints per variant; dark eyes and noses
              "wolf": pet("wolf", [(0, 0, 6, 6, 9), (32, 0, 6, 6, 4), (0, 16, 3, 3, 4), (16, 16, 2, 2, 1),
                                   (0, 24, 2, 8, 2), (10, 24, 2, 2, 6)], (32, 0, 6, 6, 4), (0, 16, 3, 3, 4),
                          collar=(32, 12, 6.6, 6, 1.2)),
              "cat": pet("cat", [(0, 0, 4, 4, 13), (36, 0, 5, 4, 4), (0, 18, 3, 2, 1), (10, 18, 1, 1, 2),
                                 (0, 22, 2, 6, 2), (10, 22, 1, 1, 8)], (36, 0, 5, 4, 4), (0, 18, 3, 2, 1),
                         collar=(36, 10, 5.4, 4, 1), stripes=True),
              "ocelot": pet("ocelot", [(0, 0, 4, 4, 13), (36, 0, 5, 4, 4), (0, 18, 3, 2, 1), (10, 18, 1, 1, 2),
                                       (0, 22, 2, 6, 2), (10, 22, 1, 1, 8)], (36, 0, 5, 4, 4), (0, 18, 3, 2, 1),
                            base="#E0B860", spots=True),
              "parrot": pet("parrot", [(0, 0, 3, 6, 3), (12, 0, 2, 3, 2), (0, 10, 1, 4, 2.5), (8, 10, 2, 3, 1)],
                            (12, 0, 2, 3, 2), None, extra=[((20, 0, 1, 1.5, 1.5), "#E8B030"), ((14, 10, 0.5, 4, 0.5), "#606060")]),
              # M26.2 mounts: horses and llamas light (tinted by coat), donkeys, mules,
              # trader llamas and camels in their own colours; manes and tails darker
              "horse": pet("horse", HORSE_BOXES, (16, 0, 5, 5, 10), None,
                           extra=[((20, 15, 2, 10, 3), "#3A2A20"), ((46, 0, 3, 10, 4), "#3A2A20")]),
              "donkey": pet("donkey", HORSE_BOXES, (16, 0, 5, 5, 10), None, base="#8A8078",
                            extra=[((20, 15, 2, 10, 3), "#4A4440"), ((46, 0, 3, 10, 4), "#4A4440")]),
              "mule": pet("mule", HORSE_BOXES, (16, 0, 5, 5, 10), None, base="#6A4630",
                          extra=[((20, 15, 2, 10, 3), "#2E2018"), ((46, 0, 3, 10, 4), "#2E2018")]),
              "llama": pet("llama", LLAMA_BOXES, (40, 0, 6, 5, 6), None),
              "trader_llama": trader_llama(),
              "camel": pet("camel", [(0, 0, 4, 18, 4), (16, 0, 5, 12, 5), (36, 0, 6, 6, 8), (48, 14, 2, 2, 1),
                                     (0, 34, 12, 10, 20)], (36, 0, 6, 6, 8), None, base="#C8A060",
                           extra=[((16, 17, 8, 5, 8), "#B08A50"), ((0, 22, 2, 8, 2), "#6A5030")]),
              "mount_gear": mount_gear(),
              "bee": bee(),
              # M26.4a monsters
              "cave_spider": spider("cave_spider", "#1E3A44"),
              "wither_skeleton": skeleton("wither_skeleton", "#3A3A3C"),
              "silverfish": pet("silverfish", [(0, 0, 4, 3, 8), (24, 0, 3, 2, 2), (24, 4, 2, 2, 3)], (24, 0, 3, 2, 2), None,
                                base="#9A9AA0", stripes=True),
              "breeze": pet("breeze", [(0, 0, 8, 8, 8), (32, 0, 6, 10, 6), (0, 16, 2, 8, 2)], (0, 0, 8, 8, 8), None,
                            base="#B8D8F0", extra=[((0, 16, 2, 8, 2), "#6A8AC8"), ((32, 0, 6, 10, 6), "#D8ECFA")]),
              "wither": pet("wither", [(0, 0, 8, 8, 8), (32, 0, 6, 6, 6), (0, 16, 20, 3, 3), (0, 22, 3, 10, 3),
                                       (24, 22, 11, 2, 2), (12, 22, 3, 6, 3)], (0, 0, 8, 8, 8), None, base="#2A2A2E",
                            extra=[((32, 0, 6, 6, 6), "#26262A")]),
              "phantom": pet("phantom", [(0, 0, 5, 3, 9), (28, 0, 7, 3, 5), (0, 12, 10, 1, 9), (40, 12, 3, 2, 6)],
                             (28, 0, 7, 3, 5), None, base="#3A4A78", extra=[((0, 12, 10, 1, 9), "#5A6A98")]),
              # M26.3c: frogs and axolotls light (tinted by kind / colour), tadpoles dark
              "frog": pet("frog", [(0, 0, 7, 3, 9), (32, 0, 7, 3, 6), (0, 12, 3, 2, 2), (10, 12, 3, 3, 4),
                                   (24, 12, 2, 3, 2)], (0, 12, 3, 2, 2), None, base="#E8E8E0",
                          extra=[((32, 0, 7, 3, 6), "#F0F0E8")]),
              "tadpole": pet("tadpole", [(0, 0, 3, 2, 3), (0, 6, 1, 2, 7)], (0, 0, 3, 2, 3), None, base="#4A3A2A"),
              "axolotl": pet("axolotl", [(0, 0, 8, 4, 10), (36, 0, 8, 5, 5), (24, 14, 3, 1, 4), (0, 20, 1, 5, 12)],
                             (36, 0, 8, 5, 5), None, base="#F0F0F0", extra=[((0, 14, 10, 3, 1), "#E0D0D8")]),
              # M26.3 wildlife: rabbits and foxes light (tinted by kind), the rest in their own colours
              "rabbit": pet("rabbit", [(0, 0, 5, 5, 7), (24, 0, 4, 4, 4), (40, 0, 1, 4, 1), (0, 12, 2, 2, 4),
                                       (12, 12, 1, 3, 1)], (24, 0, 4, 4, 4), (24, 0, 4, 4, 4),
                            extra=[((44, 0, 2, 2, 1), "#F4F4F4")]),
              "fox": pet("fox", [(0, 0, 6, 6, 9), (30, 0, 8, 6, 6), (12, 15, 2, 2, 1), (26, 15, 4, 4, 8)],
                         (30, 0, 8, 6, 6), (0, 15, 3, 3, 3),
                         extra=[((0, 15, 3, 3, 3), "#F0EAE0"), ((18, 15, 2, 3, 2), "#2A2220")]),
              "polar_bear": pet("polar_bear", [(0, 33, 12, 11, 20), (0, 0, 5, 10, 5), (20, 0, 7, 7, 7),
                                               (48, 6, 2, 2, 1)], (20, 0, 7, 7, 7), (48, 0, 4, 3, 3),
                                base="#EEEEE8", extra=[((48, 0, 4, 3, 3), "#E0E0D8")]),
              "panda": panda(),
              "goat": pet("goat", [(0, 40, 9, 8, 16), (0, 0, 3, 9, 3), (12, 0, 5, 7, 6), (42, 0, 1, 4, 2)],
                          (12, 0, 5, 7, 6), None, base="#E8E4DA", extra=[((34, 0, 2, 6, 2), "#8A8478")]),
              "armadillo": pet("armadillo", [(0, 0, 7, 6, 9), (32, 0, 3, 3, 4), (46, 0, 1, 2, 1), (0, 15, 2, 3, 2),
                                             (8, 15, 1, 1, 4)], (32, 0, 3, 3, 4), None, base="#E8B0A0",
                               extra=[((0, 0, 7, 6, 9), "#A8685A"), ((8, 15, 1, 1, 4), "#A8685A")])}
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
