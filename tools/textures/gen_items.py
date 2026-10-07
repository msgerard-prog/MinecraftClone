#!/usr/bin/env python3
"""Generate item textures: original art in the game's pixel style (docs/art-style.md).

Writes assets/minecraft/textures/item/<name>.png (16x16, transparent background).
Shapes are drawn by code (no tracing): a filled silhouette per item, then shading
(light on upper-left edges, dark on lower-right) and a dark outline around it.
Deterministic. Usage: tools/textures/gen_items.py [--preview DIR]
"""
import argparse
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import CLEAR, Img, encode_png, hexc, ramp  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/item"

MATERIALS = {
    "wooden": hexc("#A07A48"),
    "stone": hexc("#8E8E8E"),
    "iron": hexc("#D6D6D6"),
    "golden": hexc("#F2CF3C"),
    "diamond": hexc("#45DCCB"),
    "copper": hexc("#D9804F"),
}
HANDLE = ramp(hexc("#7C5A30"), 5)


class Shape:
    """Pixels of an item: name -> palette (ramp) and a set of filled (x, y)."""

    def __init__(self):
        self.parts = []  # (pixels set, ramp)

    def add(self, pixels, pal):
        self.parts.append((set(pixels), pal))

    def render(self, outline=None):
        img = Img(16, 16, CLEAR)
        filled = {}
        for pixels, pal in self.parts:  # later parts draw over earlier ones
            for p in pixels:
                filled[p] = pal
        for (x, y), pal in filled.items():
            if not (0 <= x < 16 and 0 <= y < 16):
                continue
            up_left = (x - 1, y) not in filled or (x, y - 1) not in filled
            down_right = (x + 1, y) not in filled or (x, y + 1) not in filled
            i = 2
            if up_left and not down_right:
                i = 3
            elif down_right and not up_left:
                i = 1
            img.set(x, y, pal[i])
        # Outline: the darkest shade of the neighbouring part, one pixel around.
        for y in range(16):
            for x in range(16):
                if (x, y) in filled:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    n = (x + dx, y + dy)
                    if n in filled:
                        img.set(x, y, outline or filled[n][0])
                        break
        return img


def line(x0, y0, x1, y1, width=1):
    pts = set()
    n = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
    for i in range(n + 1):
        t = i / n
        x, y = round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t)
        for w in range(width):
            pts.add((x + w, y))
    return pts


def handle(top=(9, 6)):
    # Diagonal stick from the lower left up to the head.
    return line(3, 13, top[0], top[1], width=2)


def tool(kind, mat):
    pal = ramp(MATERIALS[mat], 5, spread=0.35)
    s = Shape()
    if kind == "sword":
        s.add(line(3, 13, 5, 11, 2), HANDLE)  # grip
        s.add({(3, 10), (4, 10), (5, 12), (6, 12), (4, 11), (6, 11), (2, 9), (7, 13)}, pal)  # guard
        s.add(line(6, 9, 13, 2, 2), pal)  # blade
        s.add({(14, 1), (13, 1), (14, 2)}, pal)
    else:
        s.add(handle(), HANDLE)
        if kind == "pickaxe":
            arc = set()
            for x in range(2, 15):
                y = round(1 + 0.11 * (x - 8.5) ** 2)
                arc |= {(x, y), (x, y + 1)}
            s.add(arc, pal)
        elif kind == "axe":
            s.add({(x, y) for x in range(8, 13) for y in range(1, 7) if (x - 8) + abs(y - 3.5) < 5.5}, pal)
        elif kind == "shovel":
            s.add({(x, y) for x in range(8, 15) for y in range(0, 8)
                   if (x - 11.5) ** 2 + (y - 3.5) ** 2 < 9.5 and x - y > 3}, pal)
        elif kind == "hoe":
            s.add({(x, y) for x in range(6, 13) for y in (2, 3)}, pal)
            s.add({(11, 4), (12, 4)}, pal)
    return s.render()


def stick():
    s = Shape()
    s.add(line(3, 13, 12, 4, 2), HANDLE)
    return s.render()


def ingot(base):
    pal = ramp(hexc(base), 5, spread=0.4)
    s = Shape()
    top = {(x + (10 - y) // 2, y) for y in (7, 8) for x in range(4, 12)}
    front = {(x, y) for y in (9, 10, 11) for x in range(2 + (11 - y) // 2, 13 - (y - 9))}
    s.add(front, pal)
    img = s.render()
    light = Shape()
    light.add(top | front, pal)
    img = light.render()
    for (x, y) in top:  # the top face catches the light
        if 0 <= x < 16:
            img.set(x, y, pal[4] if (x + y) % 5 else pal[3])
    return img


def gem(base, kind):
    pal = ramp(hexc(base), 5, spread=0.45)
    s = Shape()
    if kind == "diamond":
        pts = {(x, y) for x in range(16) for y in range(3, 14)
               if abs(x - 7.5) + max(0, y - 6) * 1.1 <= 6.5 and abs(x - 7.5) <= 6 - max(0, 5 - y)}
    else:  # emerald: tall hexagon
        pts = {(x, y) for x in range(4, 12) for y in range(1, 15)
               if abs(x - 7.5) <= 3.5 - max(0, abs(y - 7.5) - 4.5)}
    s.add(pts, pal)
    img = s.render()
    for (x, y) in pts:  # facet highlights
        if y < 7 and (x + y) % 4 == 0:
            img.set(x, y, pal[4])
    return img


def lump(name, base, speck=None, seed=0, size=5.5):
    rng = random.Random(name)
    pal = ramp(hexc(base), 5, spread=0.3)
    pts = set()
    for _ in range(5):
        cx, cy, r = 8 + rng.uniform(-2.5, 2.5), 8 + rng.uniform(-2, 2.5), rng.uniform(2.5, size - 1)
        pts |= {(x, y) for x in range(16) for y in range(16) if (x - cx) ** 2 + (y - cy) ** 2 < r * r}
    s = Shape()
    s.add(pts, pal)
    img = s.render()
    if speck:
        sp = hexc(speck)
        for (x, y) in pts:
            if rng.random() < 0.12:
                img.set(x, y, sp)
    return img


def redstone():
    rng = random.Random("redstone")
    pal = ramp(hexc("#C4140C"), 5, spread=0.35)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.85}
    s = Shape()
    s.add(pts, pal)
    return s.render()


def flint():
    pal = ramp(hexc("#4A4A4E"), 5, spread=0.4)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if y >= 2 and y <= 13 and abs(x - 8) <= (y - 2) * 0.5 + 1 and x - y < 4}, pal)
    return s.render()


def apple():
    pal = ramp(hexc("#D02A1C"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16)
           if ((x - 7.5) / 5.5) ** 2 + ((y - 9) / 5) ** 2 < 1 and not (abs(x - 7.5) < 1 and y < 5)}, pal)
    s.add({(8, 2), (8, 3), (7, 4)}, HANDLE)
    s.add({(9, 2), (10, 2), (10, 1), (11, 1)}, ramp(hexc("#4C9A2A"), 5))
    return s.render()


def meat(name, base, fat, marbled=True):
    rng = random.Random(name)
    pal = ramp(hexc(base), 5, spread=0.35)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 7.5) / 6.0) ** 2 + ((y - 8.5) / 4.5) ** 2 < 1 and x + y > 6}
    s = Shape()
    s.add(pts, pal)
    img = s.render()
    fatc = hexc(fat)
    for (x, y) in pts:  # a fat rim and marbling
        if (x + 1, y) not in pts or (marbled and rng.random() < 0.08):
            img.set(x, y, fatc)
    return img


def leather():
    pal = ramp(hexc("#8A5530"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(3, 13)
           if not ((x in (2, 13)) and (y in (3, 12))) and not (x == 7 and y in (3, 12))}, pal)
    return s.render()


def flint_and_steel():
    # A curved steel striker (an open ring) and a flint chip beside it.
    steel = ramp(hexc("#B8B8C0"), 5, spread=0.4)
    s = Shape()
    ring = {(x, y) for x in range(16) for y in range(16)
            if 2.2 <= math.hypot(x - 6, y - 6) <= 4.2 and not (x > 6 and y > 6)}
    s.add(ring, steel)
    s.add({(x, y) for x in range(16) for y in range(16)
           if 8 <= x <= 13 and 8 <= y <= 13 and abs(x - y) <= 2 + (x + y) % 2}, ramp(hexc("#4A4A4E"), 5, spread=0.4))
    return s.render()


def ender_eye():
    # A round green-teal eye with a dark slit pupil.
    pal = ramp(hexc("#3E9A7A"), 5, spread=0.45)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 7.5) < 5.6}, pal)
    img = s.render()
    for y in range(4, 12):
        img.set(7, y, hexc("#10261E"))
        img.set(8, y, hexc("#10261E"))
    img.set(5, 5, pal[4])
    img.set(6, 4, pal[4])
    return img


def quartz():
    # Two white crystal prisms, pointed tops.
    pal = ramp(hexc("#E8E2D8"), 5, spread=0.25)
    s = Shape()
    s.add({(x, y) for x in range(4, 9) for y in range(3, 14) if y >= 3 + abs(x - 6)}, pal)
    s.add({(x, y) for x in range(8, 13) for y in range(6, 14) if y >= 6 + abs(x - 10)}, pal)
    return s.render()


def bucket(fill=None):
    # An iron pail seen from the side, with a handle; optionally full of something.
    iron = ramp(hexc("#B4B4BC"), 5, spread=0.4)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(5, 15) if abs(x - 7.5) <= 5.5 - (y - 5) * 0.25}, iron)
    s.add({(x, y) for x in range(16) for y in range(1, 5) if abs(x - 7.5) in (5.5,) or (y == 1 and 3 <= x <= 12)},
          iron)
    img = s.render()
    if fill:
        pal = ramp(hexc(fill), 5, spread=0.3)
        for y in (5, 6, 7):  # the surface seen over the rim
            for x in range(3, 13):
                if abs(x - 7.5) <= 5.0 - (y - 5) * 0.25:
                    img.set(x, y, pal[4] if (x + y) % 4 == 0 else pal[3] if y == 5 else pal[2])
    return img


def wheat():
    # A bundle of golden stalks with grain heads, tied in the middle.
    gold = ramp(hexc("#D8B048"), 5, spread=0.35)
    s = Shape()
    for x0 in (4, 7, 10):
        s.add({(x0 + (y - 8) // 4 * (1 if x0 > 7 else -1 if x0 < 7 else 0), y) for y in range(6, 15)}, gold)
        s.add({(x0 + dx, y) for y in range(1, 6) for dx in (0, 1) if (y + dx) % 2 == 0 or y < 5}, gold)
    s.add({(x, 10) for x in range(4, 12)}, ramp(hexc("#8A6A2A"), 5))
    return s.render()


def seeds():
    # A few small seeds scattered.
    pal = ramp(hexc("#5C8A2A"), 5, spread=0.35)
    s = Shape()
    for cx, cy in ((5, 5), (10, 6), (7, 9), (11, 11), (4, 11)):
        s.add({(cx, cy), (cx + 1, cy), (cx, cy + 1)}, pal)
    return s.render()


def carrot():
    orange = ramp(hexc("#F08A1C"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if 3 <= x + y - 6 <= 14 and abs((x - y) - 0) <= 3 - (x + y - 6) // 6 and x + y >= 11}, orange)
    s.add({(11, 2), (12, 2), (12, 3), (13, 3), (10, 3), (11, 4), (13, 1), (10, 1)}, ramp(hexc("#4C9A2A"), 5))
    return s.render()


def feather():
    white = ramp(hexc("#EAEAEA"), 5, spread=0.25)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if abs((x + y) - 15) <= 2 and 2 <= x <= 12 and abs(x - y) <= 9}, white)
    s.add({(x, 15 - x) for x in range(2, 6)}, ramp(hexc("#9A9A9A"), 5))
    return s.render()


def egg():
    pal = ramp(hexc("#E8D2A8"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if ((x - 7.5) / 4.2) ** 2 + ((y - 8.5) / (5.6 if y < 8.5 else 4.8)) ** 2 < 1}, pal)
    return s.render()


def shears():
    iron = ramp(hexc("#C8C8D0"), 5, spread=0.4)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if 2 <= x <= 9 and abs(y - (x + 1)) <= 1 and y <= 10}, iron)
    s.add({(x, y) for x in range(16) for y in range(16) if 6 <= x <= 13 and abs(y - (15 - x + 1)) <= 1 and y >= 2 and x >= 6 and y <= 9}, iron)
    s.add({(x, y) for x in range(16) for y in range(16) if 1.4 <= math.hypot(x - 4, y - 12) <= 2.6}, ramp(hexc("#4A4A50"), 5))
    s.add({(x, y) for x in range(16) for y in range(16) if 1.4 <= math.hypot(x - 11, y - 12) <= 2.6}, ramp(hexc("#4A4A50"), 5))
    return s.render()


def bow():
    # A curved wooden bow (upper-left to lower-right) with a taut string.
    wood = ramp(hexc("#8A5A2E"), 5, spread=0.35)
    s = Shape()
    pts = set()
    for t in range(0, 41):
        a = t / 40.0
        x = 2 + a * 11
        y = 2 + a * 11
        bulge = math.sin(a * math.pi) * 3.2
        pts.add((int(round(x + bulge * 0.7)), int(round(y - bulge * 0.7))))
    s.add(pts, wood)
    img = s.render()
    for t in range(2, 13):
        img.set(t, t, (220, 220, 220, 255))
    return img


def arrow():
    # A diagonal shaft, flint head (upper right), feather fletching (lower left).
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 12)}, ramp(hexc("#8A6A42"), 5))
    s.add({(11, 4), (12, 3), (13, 2), (12, 2), (13, 3), (11, 3), (12, 4)}, ramp(hexc("#5A5A60"), 5))
    s.add({(2, 13), (3, 13), (2, 12), (4, 14), (3, 14), (1, 12), (2, 14)}, ramp(hexc("#E8E8E8"), 5))
    return s.render()


def bone():
    pal = ramp(hexc("#E8E2CC"), 5, spread=0.25)
    s = Shape()
    s.add({(x, 15 - x) for x in range(4, 12)} | {(x + 1, 15 - x) for x in range(4, 11)}, pal)
    for cx, cy in ((3, 12), (4, 13), (12, 3), (11, 2)):
        s.add({(cx, cy), (cx + 1, cy), (cx, cy + 1), (cx + 1, cy + 1)}, pal)
    return s.render()


def gunpowder():
    rng = random.Random("gunpowder")
    pal = ramp(hexc("#5A5A5A"), 5, spread=0.4)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.8}
    s = Shape()
    s.add(pts, pal)
    return s.render()


def string_item():
    pal = ramp(hexc("#EEEEEE"), 5, spread=0.2)
    s = Shape()
    pts = set()
    for t in range(60):
        a = t / 60 * 4 * math.pi
        pts.add((int(8 + math.cos(a) * (2 + t / 20)), int(8 + math.sin(a) * (2 + t / 20))))
    s.add(pts, pal)
    return s.render()


def spider_eye():
    pal = ramp(hexc("#9A2A3A"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 8) < 5}, pal)
    img = s.render()
    for x, y in ((6, 6), (9, 6), (7, 9), (8, 9)):
        img.set(x, y, hexc("#E04050"))
    return img


def ender_pearl():
    pal = ramp(hexc("#1E5A50"), 5, spread=0.45)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 7.5) < 5.6}, pal)
    img = s.render()
    for x, y in ((5, 5), (6, 5), (5, 6)):
        img.set(x, y, hexc("#7AD0B8"))
    return img


def potato(base, spots, name):
    rng = random.Random(name)
    pal = ramp(hexc(base), 5, spread=0.3)
    s = Shape()
    pts = {(x, y) for x in range(16) for y in range(16) if ((x - 7.5) / 5.2) ** 2 + ((y - 8.5) / 4.0) ** 2 < 1}
    s.add(pts, pal)
    img = s.render()
    for (x, y) in pts:
        if rng.random() < 0.06:
            img.set(x, y, hexc(spots))
    return img


def beetroot():
    pal = ramp(hexc("#A8243A"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 9.5) < 4.6 or (x == 8 and y > 13)}, pal)
    s.add({(6, 3), (7, 4), (9, 3), (8, 4), (7, 2), (10, 2), (8, 5)}, ramp(hexc("#4C9A2A"), 5))
    return s.render()


def beetroot_seeds():
    pal = ramp(hexc("#B89A5A"), 5, spread=0.35)
    s = Shape()
    for cx, cy in ((5, 6), (10, 5), (8, 9), (11, 11), (4, 11)):
        s.add({(cx, cy), (cx + 1, cy), (cx, cy + 1), (cx + 1, cy + 1)}, pal)
    return s.render()


def bread():
    pal = ramp(hexc("#C08840"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if ((x - 7.5) / 6.5) ** 2 + ((y - 9) / 3.6) ** 2 < 1}, pal)
    img = s.render()
    for x in (5, 8, 11):
        img.set(x, 7, hexc("#E8C070"))
        img.set(x - 1, 8, hexc("#E8C070"))
    return img


def bone_meal():
    rng = random.Random("bone_meal")
    pal = ramp(hexc("#ECECE4"), 5, spread=0.2)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.85}
    s = Shape()
    s.add(pts, pal)
    return s.render()


ARMOR = {"leather": "#8A5530", "copper": "#D9804F", "golden": "#F2CF3C", "iron": "#D6D6D6", "diamond": "#45DCCB"}


def armor(piece, base):
    pal = ramp(hexc(base), 5, spread=0.35)
    s = Shape()
    if piece == "helmet":  # a dome with a face opening
        pts = {(x, y) for x in range(2, 14) for y in range(3, 11)
               if not (5 <= x <= 10 and y >= 7) and not (y == 3 and x in (2, 13))}
    elif piece == "chestplate":  # shoulders, body, a neck notch
        pts = {(x, y) for x in range(1, 15) for y in range(2, 15)
               if not (5 <= x <= 10 and y <= 4) and not ((x <= 3 or x >= 12) and y >= 7)}
    elif piece == "leggings":  # waist band and two legs
        pts = {(x, y) for x in range(3, 13) for y in range(2, 15) if y <= 5 or not (6 <= x <= 9)}
    else:  # boots
        pts = {(x, y) for x in range(1, 15) for y in range(6, 14)
               if (x <= 6 or x >= 9) and not (y < 10 and (x in (1, 2) or x in (13, 14)))}
    s.add(pts, pal)
    return s.render()


def shield():
    wood = ramp(hexc("#8A6038"), 5, spread=0.3)
    iron = ramp(hexc("#B8B8C0"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(1, 15) if y < 11 or abs(x - 7.5) < (15 - y) * 1.1}, wood)
    s.add({(x, y) for x in range(3, 13) for y in range(1, 15)
           if (y in (1, 2) or x in (3, 12)) and (y < 11 or abs(x - 7.5) < (15 - y) * 1.1)}, iron)
    s.add({(7, 7), (8, 7), (7, 8), (8, 8)}, iron)
    return s.render()


def paper():
    pal = ramp(hexc("#EEEEE4"), 5, spread=0.15)
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(2, 14) if not (x >= 11 and y >= 12)}, pal)
    img = s.render()
    for y in (5, 7, 9):
        for x in range(5, 11):
            img.set(x, y, hexc("#B8B8B0"))
    return img


def book(enchanted=False):
    cover = ramp(hexc("#7A3E20" if not enchanted else "#5A2A8A"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(2, 14)}, cover)
    s.add({(x, y) for x in range(11, 13) for y in range(3, 13)}, ramp(hexc("#EEEEE4"), 5, spread=0.1))
    img = s.render()
    if enchanted:
        for x, y in ((5, 4), (8, 6), (6, 9), (9, 11), (4, 12)):
            img.set(x, y, hexc("#E8B8FF"))
    else:
        for x in range(4, 10):
            img.set(x, 5, hexc("#C8A050"))
    return img


def all_items():
    items = {}
    for mat in MATERIALS:
        for kind in ("pickaxe", "axe", "shovel", "sword", "hoe"):
            items[f"{mat}_{kind}"] = tool(kind, mat)
    items["stick"] = stick()
    items["iron_ingot"] = ingot("#D8D8D8")
    items["gold_ingot"] = ingot("#F2CF3C")
    items["copper_ingot"] = ingot("#D47A4E")
    items["diamond"] = gem("#45DCCB", "diamond")
    items["emerald"] = gem("#2FC062", "emerald")
    items["coal"] = lump("coal", "#2A2A2C", "#5A5A60")
    items["charcoal"] = lump("charcoal", "#3A2A1E", "#62503C")
    items["raw_iron"] = lump("raw_iron", "#C8A88E", "#E6D2BE")
    items["raw_gold"] = lump("raw_gold", "#E2B43A", "#FFF080")
    items["raw_copper"] = lump("raw_copper", "#C46A42", "#4EAA86")
    items["lapis_lazuli"] = lump("lapis", "#2848B0", "#E2C24A", size=5)
    items["redstone"] = redstone()
    items["flint"] = flint()
    items["apple"] = apple()
    items["beef"] = meat("beef", "#C8323A", "#F0D0C8")
    items["cooked_beef"] = meat("cooked_beef", "#6A3A22", "#B07040", marbled=False)
    items["rotten_flesh"] = meat("rotten_flesh", "#8A6A3A", "#5A8A3A")
    items["leather"] = leather()
    items["flint_and_steel"] = flint_and_steel()
    items["ender_eye"] = ender_eye()
    items["quartz"] = quartz()
    items["bucket"] = bucket()
    items["water_bucket"] = bucket("#3C6EE6")
    items["lava_bucket"] = bucket("#E8661A")
    items["milk_bucket"] = bucket("#F4F4F0")
    # Farm animals and their food (M16.3).
    items["wheat"] = wheat()
    items["wheat_seeds"] = seeds()
    items["carrot"] = carrot()
    items["porkchop"] = meat("porkchop", "#E89090", "#F8D8D0")
    items["cooked_porkchop"] = meat("cooked_porkchop", "#B8784A", "#E0B880", marbled=False)
    items["mutton"] = meat("mutton", "#B8323A", "#E8C8C0")
    items["cooked_mutton"] = meat("cooked_mutton", "#7A4026", "#C08A54", marbled=False)
    items["chicken"] = meat("chicken", "#F0C0B0", "#F8E0D8", marbled=False)
    items["cooked_chicken"] = meat("cooked_chicken", "#C88A48", "#E8B868", marbled=False)
    items["feather"] = feather()
    items["egg"] = egg()
    items["shears"] = shears()
    items["bow"] = bow()
    items["arrow"] = arrow()
    items["bone"] = bone()
    items["gunpowder"] = gunpowder()
    items["string"] = string_item()
    items["spider_eye"] = spider_eye()
    items["ender_pearl"] = ender_pearl()
    items["potato"] = potato("#C8A050", "#8A6A30", "potato")
    items["baked_potato"] = potato("#D89838", "#F0C860", "baked_potato")
    items["poisonous_potato"] = potato("#A8B048", "#5A7A20", "poisonous_potato")
    items["beetroot"] = beetroot()
    items["beetroot_seeds"] = beetroot_seeds()
    items["bread"] = bread()
    items["bone_meal"] = bone_meal()
    for mat, base in ARMOR.items():
        for piece in ("helmet", "chestplate", "leggings", "boots"):
            items[f"{mat}_{piece}"] = armor(piece, base)
    items["shield"] = shield()
    items["paper"] = paper()
    items["book"] = book()
    items["enchanted_book"] = book(True)
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for a 4x preview sheet")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    items = all_items()
    for name, img in items.items():
        (OUT / f"{name}.png").write_bytes(encode_png(img))
    print(f"wrote {len(items)} item textures to {OUT}")
    if args.preview:
        cols, cell = 8, 16 * 4 + 4
        rows = (len(items) + cols - 1) // cols
        sheet = Img(cols * cell + 4, rows * cell + 4, (150, 150, 150, 255))
        for i, (name, img) in enumerate(sorted(items.items())):
            ox, oy = 4 + (i % cols) * cell, 4 + (i // cols) * cell
            for y in range(64):
                for x in range(64):
                    c = img.get(x // 4, y // 4)
                    if c[3]:
                        sheet.set(ox + x, oy + y, c)
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)
        (out / "items.png").write_bytes(encode_png(sheet))


if __name__ == "__main__":
    main()
