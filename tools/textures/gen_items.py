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
    "netherite": hexc("#5A4E56"),
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


def apple(base="#D02A1C"):
    pal = ramp(hexc(base), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16)
           if ((x - 7.5) / 5.5) ** 2 + ((y - 9) / 5) ** 2 < 1 and not (abs(x - 7.5) < 1 and y < 5)}, pal)
    s.add({(8, 2), (8, 3), (7, 4)}, HANDLE)
    s.add({(9, 2), (10, 2), (10, 1), (11, 1)}, ramp(hexc("#4C9A2A"), 5))
    return s.render()


def saddle():
    """A brown leather saddle from the side (M26.2): seat, raised back, stirrup strap."""
    pal = ramp(hexc("#8A4E26"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(6, 10) if not (y == 6 and 4 < x < 11)}, pal)
    s.add({(2, 4), (2, 5), (3, 5), (13, 3), (13, 4), (13, 5), (12, 5)}, pal)
    s.add({(7, y) for y in range(10, 14)} | {(8, y) for y in range(10, 14)}, ramp(hexc("#5A3418"), 5))
    s.add({(6, 13), (7, 14), (8, 14), (9, 13)}, ramp(hexc("#BFC4C8"), 5))
    return s.render()


def horse_armor(base):
    """Horse armor (M26.2): a plated horse head and neck from the side."""
    pal = ramp(hexc(base), 5, spread=0.4)
    s = Shape()
    s.add({(x, y) for x in range(3, 9) for y in range(4, 14) if x + 13 - y >= 3}, pal)    # neck
    s.add({(x, y) for x in range(7, 15) for y in range(3, 8) if not (x > 12 and y < 5)}, pal)  # head
    s.add({(5, 2), (6, 2), (6, 3)}, pal)                                                     # ear
    img = s.render()
    img.set(10, 5, (30, 30, 34, 255))  # eye hole
    return img


def chest_boat_item(colour, raft=False):
    """A boat with a chest standing in it (M26.2)."""
    img = boat_item(colour, raft)
    top = 7 if raft else 5
    for x in range(5, 11):
        for y in range(top - 4, top + 1):
            edge = x in (5, 10) or y in (top - 4, top)
            img.set(x, y, (92, 60, 26, 255) if edge else (168, 118, 54, 255))
    img.set(7, top - 2, (200, 200, 205, 255))
    img.set(8, top - 2, (200, 200, 205, 255))
    return img


def sweet_berries():
    """A cluster of round red berries on a green twig (M26.3)."""
    s = Shape()
    berry = ramp(hexc("#C8202E"), 5, spread=0.4)
    for cx, cy in ((5, 9), (9, 8), (7, 12), (10, 12)):
        s.add({(x, y) for x in range(16) for y in range(16) if (x - cx) ** 2 + (y - cy) ** 2 <= 4}, berry)
    s.add({(7, 3), (7, 4), (8, 5), (8, 6), (6, 2), (9, 2)}, ramp(hexc("#3E7A2A"), 5))
    return s.render()


def glow_berries():
    """Glow berries (M27.2): glowing orange berries hanging from a green vine."""
    s = Shape()
    berry = ramp(hexc("#F0A030"), 5, spread=0.45)
    for cx, cy in ((5, 11), (10, 10), (8, 13)):
        s.add({(x, y) for x in range(16) for y in range(16) if (x - cx) ** 2 + (y - cy) ** 2 <= 4}, berry)
    s.add({(7, 2), (7, 3), (8, 4), (8, 5), (7, 6), (6, 7), (9, 7), (5, 8), (10, 8)}, ramp(hexc("#4E7A2A"), 5))
    return s.render()


def trial_key():
    """A trial key (M27.4d): a copper-and-tuff key with a teal bow (ours)."""
    s = Shape()
    s.add({(x, y) for x in range(3, 8) for y in range(3, 8) if (x - 5) ** 2 + (y - 5) ** 2 <= 5}, ramp(hexc("#3AA0A0"), 5))
    s.add({(x, x + 1) for x in range(6, 13)} | {(x + 1, x + 1) for x in range(6, 12)}, ramp(hexc("#C07A50"), 5))
    s.add({(11, 9), (12, 10), (10, 12), (11, 13)}, ramp(hexc("#C07A50"), 5))
    return s.render()


def brush():
    """A brush (M27.5): a wooden handle, a copper band, a tuft of feather bristles."""
    s = Shape()
    s.add(line(3, 13, 9, 7, 2), HANDLE)
    s.add({(9, 6), (10, 7), (9, 7), (10, 6)}, ramp(hexc("#C07A50"), 5))
    s.add({(x, y) for x in range(10, 15) for y in range(1, 7) if x + y <= 17 and y + 14 - x >= 3}, ramp(hexc("#E8E4DC"), 5))
    return s.render()


SHERD_MARKS = {
    "angler": [(5, 9), (6, 8), (7, 7), (8, 7), (9, 8), (8, 10), (7, 10)], "archer": [(5, 5), (6, 6), (7, 7), (8, 8), (9, 9), (9, 6), (6, 9)],
    "arms_up": [(7, 5), (7, 6), (7, 7), (7, 8), (5, 6), (9, 6), (6, 10), (8, 10)], "blade": [(5, 10), (6, 9), (7, 8), (8, 7), (9, 6), (10, 5)],
    "brewer": [(6, 6), (8, 6), (5, 8), (6, 9), (7, 9), (8, 9), (9, 8)], "burn": [(7, 5), (6, 7), (8, 7), (5, 9), (7, 9), (9, 9)],
    "danger": [(7, 5), (6, 7), (8, 7), (5, 9), (6, 9), (7, 9), (8, 9), (9, 9)], "explorer": [(5, 5), (9, 5), (7, 7), (5, 9), (9, 9)],
    "flow": [(5, 7), (6, 6), (7, 7), (8, 8), (9, 7), (10, 6)], "friend": [(6, 6), (8, 6), (5, 8), (9, 8), (6, 9), (7, 10), (8, 9)],
    "guster": [(5, 6), (6, 5), (7, 6), (8, 7), (9, 8), (8, 9), (7, 9)], "heart": [(6, 6), (8, 6), (5, 7), (7, 7), (9, 7), (6, 8), (8, 8), (7, 9)],
    "heartbreak": [(6, 6), (9, 6), (5, 7), (10, 7), (6, 8), (9, 8), (7, 9)], "howl": [(6, 5), (6, 6), (7, 7), (8, 7), (9, 8), (9, 9)],
    "miner": [(5, 5), (6, 5), (7, 5), (8, 5), (6, 6), (7, 7), (7, 8), (7, 9)], "mourner": [(7, 5), (6, 6), (8, 6), (6, 8), (8, 8), (7, 9)],
    "plenty": [(5, 6), (6, 6), (7, 6), (8, 6), (9, 6), (6, 8), (8, 8), (7, 9)], "prize": [(7, 5), (6, 6), (8, 6), (5, 7), (9, 7), (7, 9)],
    "scrape": [(5, 9), (6, 8), (7, 9), (8, 8), (9, 9), (10, 8)], "sheaf": [(6, 5), (7, 5), (8, 5), (7, 6), (7, 7), (6, 9), (8, 9)],
    "shelter": [(7, 5), (6, 6), (8, 6), (5, 7), (9, 7), (6, 9), (8, 9)], "skull": [(6, 6), (8, 6), (5, 7), (9, 7), (6, 9), (7, 9), (8, 9)],
    "snort": [(5, 7), (6, 7), (7, 6), (8, 6), (9, 7), (10, 7), (7, 9)],
}


def sherd(name):
    """A pottery sherd (M27.5): a terracotta shard with its mark scratched in (ours)."""
    pal = ramp(hexc("#9A5A40"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(3, 13) if not (x + y < 6 or x - y > 7 or y - x > 8)}, pal)
    img = s.render()
    for x, y in SHERD_MARKS[name]:
        img.set(x, y, (60, 30, 22, 255))
    return img


def torchflower_seeds():
    """Torchflower seeds (M27.5c): a few orange-tipped seeds."""
    s = Shape()
    for cx, cy in ((5, 9), (9, 6), (10, 11)):
        s.add({(cx, cy), (cx + 1, cy), (cx, cy + 1), (cx + 1, cy + 1), (cx + 1, cy - 1)}, ramp(hexc("#6A8A3A"), 5))
        s.add({(cx + 2, cy - 1)}, ramp(hexc("#F08A20"), 5))
    return s.render()


def pitcher_pod():
    """A pitcher pod (M27.5c): a teal-and-purple bulb (ours)."""
    s = Shape()
    s.add({(x, y) for x in range(4, 12) for y in range(5, 13) if (x - 7.5) ** 2 / 14 + (y - 9) ** 2 / 16 < 1}, ramp(hexc("#3A8A8A"), 5))
    s.add({(7, 3), (8, 3), (7, 4), (8, 4)}, ramp(hexc("#8A4AA0"), 5))
    return s.render()


def horn():
    """A goat horn (M26.3): a ridged, curving cone."""
    s = Shape()
    pal = ramp(hexc("#C8B89A"), 5, spread=0.35)
    pts = set()
    for i in range(12):
        x, y = 3 + i, 13 - int(round(6 * (i / 11) ** 0.7)) - (i // 6)
        w = max(1, 3 - i // 4)
        for dy in range(-w, w + 1):
            pts.add((x, y + dy))
    s.add(pts, pal)
    img = s.render()
    for x in range(4, 14, 3):
        for y in range(16):
            c = img.get(x, y)
            if c[3]:
                img.set(x, y, pal[0])
    return img


def scute(base):
    """An armadillo scute (M26.3): a curved plate with ridges."""
    s = Shape()
    pal = ramp(hexc(base), 5, spread=0.35)
    s.add({(x, y) for x in range(3, 13) for y in range(4, 12) if ((x - 7.5) / 5) ** 2 + ((y - 8) / 4) ** 2 < 1}, pal)
    img = s.render()
    for x in (5, 8, 11):
        for y in range(5, 11):
            if img.get(x, y)[3]:
                img.set(x, y, pal[0])
    return img


def wolf_armor():
    """Wolf armor (M26.3): a scute-plated back piece with straps."""
    s = Shape()
    pal = ramp(hexc("#A8705E"), 5, spread=0.35)
    s.add({(x, y) for x in range(2, 14) for y in range(5, 10)}, pal)
    s.add({(3, y) for y in range(10, 13)} | {(12, y) for y in range(10, 13)}, ramp(hexc("#5A3A2A"), 5))
    img = s.render()
    for x in range(3, 14, 3):
        img.set(x, 6, pal[0])
        img.set(x, 7, pal[0])
    return img


def rabbit_foot():
    """A rabbit's foot (M26.3): a long tan foot with a dark tuft."""
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if 4 <= x <= 8 and 3 <= y <= 12}, ramp(hexc("#C8A070"), 5, spread=0.3))
    s.add({(x, 13) for x in range(4, 11)} | {(x, 12) for x in range(8, 12)}, ramp(hexc("#8A6A48"), 5))
    s.add({(5, 2), (6, 2), (7, 2), (6, 1)}, ramp(hexc("#6A4A30"), 5))
    return s.render()


def honey_bottle():
    """A glass bottle of golden honey (M26.3b)."""
    s = Shape()
    glass = ramp(hexc("#C8E0E8"), 5, spread=0.2)
    honey = ramp(hexc("#F0A020"), 5, spread=0.35)
    s.add({(x, y) for x in range(16) for y in range(16) if ((x - 7.5) / 4.5) ** 2 + ((y - 10) / 4.5) ** 2 < 1}, honey)
    s.add({(x, y) for x in range(6, 10) for y in range(3, 6)}, glass)
    s.add({(x, 2) for x in range(6, 10)}, ramp(hexc("#8A6A48"), 5))
    return s.render()


def wind_charge():
    """A swirl of pale wind (M26.4c)."""
    s = Shape()
    pal = ramp(hexc("#C8E0F4"), 5, spread=0.3)
    import math
    pts = set()
    for k in range(60):
        t = k / 60 * 2.5 * math.pi
        rr = 1.0 + t * 0.9
        pts.add((int(round(7.5 + math.cos(t) * rr)), int(round(7.5 + math.sin(t) * rr))))
    s.add(pts, pal)
    return s.render()


def harness(base):
    """A happy ghast's harness (M26.5b): a coloured strap frame with a goggle lens."""
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(3, 13) if x in (2, 3, 12, 13) or y in (3, 4, 11, 12)},
          ramp(hexc(base), 5, spread=0.35))
    s.add({(x, y) for x in range(6, 10) for y in range(6, 10)}, ramp(hexc("#8AC8E8"), 5, spread=0.2))
    return s.render()


def snowball():
    """A packed ball of snow (M26.5b)."""
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if (x - 7.5) ** 2 + (y - 8) ** 2 < 22}, ramp(hexc("#F4F8FC"), 5, spread=0.15))
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


def membrane():
    """Phantom membrane (M26.4a): a pale, ragged leathery sheet (ours)."""
    pal = ramp(hexc("#C8C0B4"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(3, 13) if (x * 7 + y * 3) % 11 != 0 and not (y < 5 and x > 9)}, pal)
    s.add({(x, 3 + x // 3) for x in range(2, 13)}, ramp(hexc("#8A8278"), 5))
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


def crossbow():
    """A crossbow (M24.4, ours): a wooden stock along the diagonal, an iron-tipped bow
    across it and the string drawn back."""
    wood = ramp(hexc("#7A4E28"), 5, spread=0.35)
    iron = ramp(hexc("#B8B8B8"), 5, spread=0.3)
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 14)} | {(x, 16 - x) for x in range(4, 14)}, wood)  # the stock
    s.add({(x, x - 2) for x in range(4, 13)}, iron)  # the bow limbs
    img = s.render()
    for t in range(6, 11):
        img.set(t, t + 1, (220, 220, 220, 255))
    return img


def totem():
    """Totem of Undying (M24.5, ours): a small golden figure with green eyes, arms out."""
    gold = ramp(hexc("#E8C040"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(5, 11) for y in range(2, 7)}, gold)    # head
    s.add({(x, y) for x in range(6, 10) for y in range(7, 14)}, gold)   # body
    s.add({(x, 8) for x in range(2, 14)} | {(x, 9) for x in range(3, 13)}, gold)  # arms
    img = s.render()
    img.set(6, 4, (40, 160, 70, 255))
    img.set(9, 4, (40, 160, 70, 255))
    return img


def arrow():
    # A diagonal shaft, flint head (upper right), feather fletching (lower left).
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 12)}, ramp(hexc("#8A6A42"), 5))
    s.add({(11, 4), (12, 3), (13, 2), (12, 2), (13, 3), (11, 3), (12, 4)}, ramp(hexc("#5A5A60"), 5))
    s.add({(2, 13), (3, 13), (2, 12), (4, 14), (3, 14), (1, 12), (2, 14)}, ramp(hexc("#E8E8E8"), 5))
    return s.render()


def arrow_variant(head):
    """M28.4b: an arrow with a coloured head (spectral: glowing yellow; tipped: the base
    with a white head that the potion tints - drawn by a separate overlay)."""
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 12)}, ramp(hexc("#8A6A42"), 5))
    s.add({(2, 13), (3, 13), (2, 12), (4, 14), (3, 14), (1, 12), (2, 14)}, ramp(hexc("#E8E8E8"), 5))
    if head:
        s.add({(11, 4), (12, 3), (13, 2), (12, 2), (13, 3), (11, 3), (12, 4)}, ramp(hexc(head), 5))
    return s.render()


def arrow_head_overlay():
    img = Img(16, 16, CLEAR)
    for (x, y) in ((11, 4), (12, 3), (13, 2), (12, 2), (13, 3), (11, 3), (12, 4)):
        img.set(x, y, (255, 255, 255, 255))
    return img


def firework_rocket():
    """M28.4c: a red paper tube with a cone, a stick below."""
    s = Shape()
    s.add({(x, y) for x in (6, 7, 8, 9) for y in range(4, 11)}, ramp(hexc("#C83A2E"), 5, spread=0.3))
    s.add({(7, 2), (8, 2), (7, 3), (8, 3), (6, 3), (9, 3)}, ramp(hexc("#E8E0C8"), 5))
    s.add({(7, y) for y in range(11, 15)} | {(8, y) for y in range(11, 15)}, ramp(hexc("#9A7A4E"), 5))
    img = s.render()
    for y in (5, 8):
        for x in (6, 7, 8, 9):
            img.set(x, y, hexc("#F2E8C8"))
    return img


def firework_star():
    """M28.4c: a rough grey ball of gunpowder."""
    return lump("firework_star", "#6A6A6A", "#9A9A9A", size=4.6)


def mace():
    """M28.4d: a heavy dark-steel head on a short wrapped handle."""
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 10)}, ramp(hexc("#6A5A48"), 5))
    s.add({(x, y) for x in range(9, 15) for y in range(1, 7) if (x - 11.5) ** 2 + (y - 3.5) ** 2 < 8.5},
          ramp(hexc("#5A6068"), 5, spread=0.3))
    img = s.render()
    for (x, y) in ((10, 2), (13, 2), (10, 5), (13, 5)):
        img.set(x, y, hexc("#9AA2AA"))
    return img


def spear(base):
    """M28.4e: a long shaft with a leaf-shaped head (upper right)."""
    s = Shape()
    s.add({(x, 15 - x) for x in range(1, 11)}, ramp(hexc("#8A6A42"), 5))
    s.add({(10, 4), (11, 3), (12, 2), (13, 1), (11, 4), (12, 3), (13, 2), (10, 3), (12, 1), (11, 2)},
          ramp(hexc(base), 5, spread=0.35))
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


def dried_kelp():
    # A dark, wrinkled leaf, wavy along its length (M25.1).
    pal = ramp(hexc("#3E4A26"), 5, spread=0.4)
    s = Shape()
    pts = set()
    for y in range(2, 15):
        cx = 8 + round(1.6 * math.sin(y * 0.9))
        for x in range(cx - 2, cx + 2):
            pts.add((x, y))
    s.add(pts, pal)
    img = s.render()
    for y in range(3, 14, 3):
        cx = 8 + round(1.6 * math.sin(y * 0.9))
        img.set(cx - 1, y, hexc("#262E16"))
    return img


def fish_item(body, belly, cooked=False, fin=None, stripes=None, spots=None):
    """A fish lying diagonally, head at the top right (M25.2)."""
    top = ramp(hexc(body), 5, spread=0.35)
    low = ramp(hexc(belly), 5, spread=0.25)
    s = Shape()
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - y) / 2.0) ** 2 / 2.2 ** 2 + ((x + y - 15) / 2.0) ** 2 / 5.6 ** 2 < 1}
    s.add({p for p in pts if p[0] - p[1] >= 0}, top)
    s.add({p for p in pts if p[0] - p[1] < 0}, low)
    tail = {(x, y) for x in range(16) for y in range(16) if x + y <= 6 and abs(x - y) <= 3 - (x + y) * 0.0 and x + y >= 2}
    s.add(tail, ramp(hexc(fin or body), 5, spread=0.3))
    img = s.render()
    if stripes:
        for k in range(-2, 3, 2):
            for t in range(-2, 3):
                img.set(7 + k + t, 8 + k - t, hexc(stripes)) if img.get(7 + k + t, 8 + k - t)[3] else None
    if spots:
        rng = random.Random(body)
        for _ in range(6):
            x, y = rng.randrange(4, 13), rng.randrange(3, 12)
            if img.get(x, y)[3]:
                img.set(x, y, hexc(spots))
    if not cooked:
        img.set(11, 4, hexc("#141414"))  # the eye
    return img


def ink_sac(base):
    pal = ramp(hexc(base), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if ((x - 7.5) / 5.0) ** 2 + ((y - 9) / 5.5) ** 2 < 1}, pal)
    s.add({(x, y) for x in range(6, 10) for y in range(2, 5)}, pal)
    img = s.render()
    img.set(6, 7, pal[4])
    img.set(7, 6, pal[4])
    return img


def fishing_rod():
    wood = ramp(hexc("#8A6236"), 5, spread=0.3)
    s = Shape()
    s.add({(x, 15 - x) for x in range(1, 14)} | {(x + 1, 15 - x) for x in range(1, 13)}, wood)
    img = s.render()
    for y in range(2, 13):  # the line hanging from the tip
        img.set(14, y, (230, 230, 230, 255))
    img.set(14, 13, (180, 180, 190, 255))  # the hook
    img.set(13, 13, (180, 180, 190, 255))
    return img


def boat_item(colour, raft=False):
    """A boat seen from the side (M25.2b): a hull with a darker rim; a raft is flat."""
    pal = ramp(hexc(colour), 5, spread=0.35)
    s = Shape()
    if raft:
        s.add({(x, y) for x in range(1, 15) for y in range(8, 12)}, pal)
    else:
        s.add({(x, y) for x in range(16) for y in range(6, 12) if abs(x - 7.5) <= 7.5 - (y - 6) * 0.6}, pal)
    img = s.render()
    for x in range(16):
        if img.get(x, 6 if not raft else 8)[3]:
            img.set(x, 6 if not raft else 8, pal[0])
    return img


def trident():
    """A three-pronged spear, diagonal (M25.3): a teal-grey shaft and prongs."""
    metal = ramp(hexc("#5AA89C"), 5, spread=0.35)
    s = Shape()
    shaft = {(x, 15 - x) for x in range(1, 11)} | {(x + 1, 15 - x) for x in range(1, 10)}
    prongs = {(11, 4), (12, 3), (13, 2), (14, 1), (10, 3), (10, 2), (10, 1), (12, 5), (13, 5), (14, 5)}
    s.add(shaft | prongs, metal)
    return s.render()


def bone_meal():
    rng = random.Random("bone_meal")
    pal = ramp(hexc("#ECECE4"), 5, spread=0.2)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.85}
    s = Shape()
    s.add(pts, pal)
    return s.render()


ARMOR = {"leather": "#8A5530", "copper": "#D9804F", "golden": "#F2CF3C", "iron": "#D6D6D6", "diamond": "#45DCCB",
         "netherite": "#5A4E56"}


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


def dial(rim, face):
    """A round case (rim) with a face inside: the base of compasses and clocks."""
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if (x - 7.5) ** 2 + (y - 7.5) ** 2 < 6.6 ** 2},
          ramp(hexc(rim), 5, spread=0.3))
    img = s.render()
    f = ramp(hexc(face), 5, spread=0.15)
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if d < 4.6 ** 2:
                img.set(x, y, f[3] if x + y < 14 else f[2])
    return img


def compass(frame, frames=32, recovery=False):
    """M28.2a: a needle turned to frame/frames of a full turn (frame 0 points up, as
    vanilla's compass_00 points straight ahead), red tip and grey tail; the recovery
    compass is a dark case with a cyan needle."""
    img = dial("#5A5E66" if recovery else "#8A8E96", "#2E3E44" if recovery else "#D8D2C0")
    tip, tail = (hexc("#40E0E8"), hexc("#20707A")) if recovery else (hexc("#D02A1C"), hexc("#60646C"))
    a = 2 * math.pi * frame / frames
    dx, dy = math.sin(a), -math.cos(a)
    for k in range(1, 9):
        t = k / 2.0
        for sign, col in ((1, tip), (-1, tail)):
            x, y = 7.5 + sign * dx * t, 7.5 + sign * dy * t
            if t <= 4.0:
                img.set(int(round(x - 0.01)), int(round(y - 0.01)), col)
    img.set(7, 7, hexc("#202020"))
    return img


def clock(frame, frames=64):
    """M28.2a: a gold case whose window shows the sky turning: frame 0 is noon (the sun
    at the top), half way round midnight (the moon); vanilla's clock turns the same way."""
    img = dial("#D8A830", "#5A8AD8")
    a = 2 * math.pi * frame / frames
    day, night = ramp(hexc("#6AA0F0"), 5, spread=0.1), ramp(hexc("#1A2050"), 5, spread=0.1)
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if d >= 4.6 ** 2:
                continue
            # The face's half toward the sun is day sky, the other half night.
            ang = math.atan2(x - 7.5, -(y - 7.5)) - a
            up = math.cos(ang)
            img.set(x, y, day[3] if up > 0.0 else night[2])
    for body, col, ang in ((0, "#F8E040", a), (1, "#E8E8F0", a + math.pi)):  # sun, moon
        cx, cy = 7.5 + math.sin(ang) * 2.6, 7.5 - math.cos(ang) * 2.6
        for (ox, oy) in ((0, 0), (1, 0), (0, 1), (1, 1)):
            x, y = int(cx - 0.5) + ox, int(cy - 0.5) + oy
            if (x - 7.5) ** 2 + (y - 7.5) ** 2 < 4.6 ** 2:
                img.set(x, y, hexc(col))
    # The case's marker at the top (vanilla: the pointer the sky turns under).
    img.set(7, 1, hexc("#7A5410"))
    img.set(8, 1, hexc("#7A5410"))
    return img


def map_item(filled=False):
    """M28.2b: a sheet of parchment; the filled map shows land, water and a path."""
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(2, 14)}, ramp(hexc("#E8DCB0"), 5, spread=0.2))
    img = s.render()
    rng = random.Random("map")
    for y in range(3, 13):
        for x in range(3, 13):
            if filled:
                water = (x - 9) ** 2 + (y - 9) ** 2 < 10
                img.set(x, y, hexc("#6A8AD0" if water else ("#7AA050" if rng.random() < 0.7 else "#5A8040")))
            elif rng.random() < 0.08:
                img.set(x, y, hexc("#D2C496"))
    if filled:
        for x, y in ((4, 4), (5, 5), (6, 5), (7, 6)):
            img.set(x, y, hexc("#8A6A3A"))
    return img


def writable_book(written=False):
    """M28.2c: a book with a quill (book and quill) or with writing on its cover."""
    img = book()
    if written:
        for x in range(4, 10):
            img.set(x, 5, hexc("#F0E0B0"))
            img.set(x, 8, hexc("#F0E0B0"))
        return img
    for k in range(7):  # the quill across it
        img.set(12 - k, 1 + k, hexc("#F4F4F4") if k < 4 else hexc("#2A2A2A"))
        if k < 4:
            img.set(13 - k, 1 + k, hexc("#D8D8D8"))
    return img


def frame_item(inner, picture=False):
    """M28.3a: a wooden frame around leather (item frames; glowing ink for the glow frame)
    or a small landscape (paintings)."""
    s = Shape()
    s.add({(x, y) for x in range(2, 14) for y in range(2, 14)}, ramp(hexc("#A07A48"), 5, spread=0.3))
    img = s.render()
    f = ramp(hexc(inner), 5, spread=0.15)
    for y in range(4, 12):
        for x in range(4, 12):
            if picture:
                c = "#8AB8E8" if y < 7 else ("#F2D46A" if (x - 9) ** 2 + (y - 5) ** 2 < 2 else "#5E8A3C")
                if y < 7 and (x - 9) ** 2 + (y - 5) ** 2 < 2:
                    c = "#F2D46A"
                img.set(x, y, hexc(c))
            else:
                img.set(x, y, f[3] if (x + y) % 3 else f[2])
    return img


def armor_stand_item():
    """M28.3b: a wooden stand on a stone plate: pole, shoulder bar, hips."""
    s = Shape()
    wood = ramp(hexc("#B8945F"), 5, spread=0.3)
    s.add({(7, y) for y in range(2, 13)} | {(8, y) for y in range(2, 13)}, wood)
    s.add({(x, 4) for x in range(3, 13)} | {(x, 5) for x in range(3, 13)}, wood)
    s.add({(x, 9) for x in range(5, 11)}, wood)
    s.add({(x, y) for x in range(3, 13) for y in (13, 14)}, ramp(hexc("#9A9A9A"), 5, spread=0.25))
    return s.render()


def lead_item():
    """M28.3c: a coil of rope with a loop at its end."""
    s = Shape()
    pts = set()
    for k in range(60):
        a = k / 60 * 2 * math.pi * 1.5
        r = 3.2 + 1.2 * math.sin(k * 0.4)
        pts.add((int(round(7 + r * math.cos(a))), int(round(8 + r * math.sin(a) * 0.8))))
    pts |= {(11 + i, 3 - i // 2) for i in range(3)} | {(12, 4), (13, 3)}
    s.add(pts, ramp(hexc("#9A7A4E"), 5, spread=0.3))
    return s.render()


DYES = [("white", "#F9FFFE"), ("orange", "#F9801D"), ("magenta", "#C74EBD"), ("light_blue", "#3AB3DA"),
        ("yellow", "#FED83D"), ("lime", "#80C71F"), ("pink", "#F38BAA"), ("gray", "#474F52"),
        ("light_gray", "#9D9D97"), ("cyan", "#169C9C"), ("purple", "#8932B8"), ("blue", "#3C44AA"),
        ("brown", "#835432"), ("green", "#5E7C16"), ("red", "#B02E26"), ("black", "#1D1D21")]


def banner_item(colour):
    """M28.3d: a cloth hanging from a crossbar on a pole, in the dye's colour."""
    s = Shape()
    wood = ramp(hexc("#9A7A4E"), 5, spread=0.3)
    s.add({(x, 1) for x in range(3, 13)}, wood)
    s.add({(7, y) for y in range(2, 15)}, wood)
    s.add({(x, y) for x in range(4, 12) for y in range(2, 13) if not (y == 12 and x % 2)}, ramp(hexc(colour), 5, spread=0.2))
    return s.render()


PATTERN_EMBLEMS = {  # (M28.3d) a small fixed emblem per pattern item, 5x5
    "field_masoned": ["#####", "#.#.#", "#####", ".#.#.", "#####"],
    "bordure_indented": ["#.#.#", ".....", "#...#", ".....", "#.#.#"],
    "creeper": ["#####", "#.#.#", "#####", "#...#", "#.#.#"],
    "skull": [".###.", "#####", "#.#.#", "#####", ".#.#."],
    "flower": ["..#..", ".###.", "##.##", ".###.", "..#.."],
    "mojang": ["..#..", "#####", ".###.", ".#.#.", "#...#"],  # (our own star for "Thing")
    "globe": [".###.", "#.#.#", "###.#", "#.###", ".###."],
    "piglin": [".....", "#####", "#.#.#", "#####", "....."],
    "flow": ["####.", "...#.", ".#.#.", ".###.", "....."],
    "guster": [".###.", "#...#", "#.#.#", "#..#.", ".#..."],
}


def pattern_item(seed):
    """M28.3d: a banner pattern: paper with its stencilled emblem."""
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(2, 14)}, ramp(hexc("#E8E0C8"), 5, spread=0.15))
    img = s.render()
    for j, row in enumerate(PATTERN_EMBLEMS[seed]):
        for i, ch in enumerate(row):
            if ch == "#":
                img.set(5 + i, 5 + j, hexc("#6A5A48"))
    return img


def ghast_tear():
    pal = ramp(hexc("#C8E4EE"), 5, spread=0.3)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16)
           if (y >= 8 and math.hypot(x - 7.5, y - 10) < 4) or (y < 8 and abs(x - 7.5) < (y - 2) * 0.6)}, pal)
    img = s.render()
    img.set(6, 9, hexc("#FFFFFF"))
    img.set(6, 10, hexc("#FFFFFF"))
    return img


def blaze_rod(base="#F0A020", glint="#FFE070"):
    pal = ramp(hexc(base), 5, spread=0.45)
    s = Shape()
    s.add({(x, 15 - x) for x in range(3, 13)} | {(x + 1, 15 - x) for x in range(3, 12)}, pal)
    img = s.render()
    for x in range(4, 12, 2):
        img.set(x, 15 - x, hexc(glint))
    return img


def blaze_powder():
    rng = random.Random("blaze_powder")
    pal = ramp(hexc("#E89018"), 5, spread=0.45)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.8}
    s = Shape()
    s.add(pts, pal)
    img = s.render()
    for (x, y) in pts:
        if rng.random() < 0.15:
            img.set(x, y, hexc("#FFE060"))
    return img


def magma_cream():
    rng = random.Random("magma_cream")
    pal = ramp(hexc("#3A2410"), 5, spread=0.35)
    s = Shape()
    pts = {(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 8) < 5.2}
    s.add(pts, pal)
    img = s.render()
    for (x, y) in pts:
        if (x * 3 + y * 5) % 7 == 0 or rng.random() < 0.15:
            img.set(x, y, hexc("#F08020") if rng.random() < 0.7 else hexc("#FFD040"))
    return img


def fire_charge():
    rng = random.Random("fire_charge")
    pal = ramp(hexc("#40302A"), 5, spread=0.35)
    s = Shape()
    pts = {(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 7.5) < 5}
    s.add(pts, pal)
    img = s.render()
    for (x, y) in pts:
        if rng.random() < 0.3:
            img.set(x, y, hexc("#E86018") if rng.random() < 0.6 else hexc("#F8C030"))
    return img


def minecart_item():
    # A grey open box seen from the side with two wheels.
    rng = random.Random("minecart")
    img = Img(16, 16, CLEAR)
    iron = ramp(hexc("#7C7C84"), 5, spread=0.3)
    for y in range(5, 11):
        for x in range(1, 15):
            if y == 5 or y == 10 or x in (1, 14):
                img.set(x, y, iron[rng.randrange(1, 4)])
            elif y > 7:
                img.set(x, y, iron[0])
    for cx in (4, 11):
        for (dx, dy) in ((0, 0), (1, 0), (0, 1), (1, 1)):
            img.set(cx + dx, 11 + dy, hexc("#303034"))
    return img


def elytra_item():
    # Two grey-violet wings hanging from a shoulder bar (worn on the back).
    rng = random.Random("elytra")
    img = Img(16, 16, CLEAR)
    pal = ramp(hexc("#8A82A0"), 5, spread=0.3)
    for y in range(2, 15):
        span = max(1, 7 - abs(y - 6) // 2)
        for x in range(span):
            for xx in (7 - x, 8 + x):
                if y > 2 or x < 6:
                    img.set(xx, y, pal[rng.randrange(0, 5)] if (x + y) % 5 else pal[0])
    for x in range(7, 9):
        for y in range(2, 15):
            img.set(x, y, CLEAR)
    return img


def end_crystal_item():
    # A glass cube outline around a pink core.
    rng = random.Random("end_crystal")
    img = Img(16, 16, CLEAR)
    glass = ramp(hexc("#B8C8D8"), 5, spread=0.2)
    core = ramp(hexc("#C050B8"), 5, spread=0.35)
    for y in range(2, 14):
        for x in range(2, 14):
            if x in (2, 13) or y in (2, 13) or x == y or x + y == 15:
                img.set(x, y, glass[rng.randrange(1, 4)])
    for y in range(5, 11):
        for x in range(5, 11):
            img.set(x, y, core[rng.randrange(0, 5)])
    return img


def bottle_pixels():
    body = {(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 10.5) < 4.6}
    neck = {(x, y) for x in (6, 7, 8, 9) for y in range(3, 7)}
    return body, neck


def bottle(splash=False, filled=True):
    # Glass outline with a cork; the liquid is the separate overlay (tinted per potion).
    img = Img(16, 16, CLEAR)
    body, neck = bottle_pixels()
    if splash:  # a squat, rounder bottle
        body = {(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 10) < 5.2}
    shape = body | neck
    for (x, y) in shape:
        edge = any((x + dx, y + dy) not in shape for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if edge:
            img.set(x, y, (205, 225, 240, 255))
        elif not filled or y < 8:
            img.set(x, y, (225, 240, 250, 90))
    for x in (6, 7, 8, 9):
        img.set(x, 2, (140, 100, 60, 255))
        img.set(x, 3, (120, 84, 50, 255))
    img.set(6, 9, (255, 255, 255, 220))  # a glint
    img.set(6, 10, (255, 255, 255, 180))
    return img


def potion_overlay():
    # White liquid where the bottle body is (lower part), multiplied by the potion colour.
    img = Img(16, 16, CLEAR)
    body, _ = bottle_pixels()
    for (x, y) in body:
        inner = all((x + dx, y + dy) in body for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if inner and y >= 8:
            v = 255 if (x + y) % 5 else 220
            img.set(x, y, (v, v, v, 255))
    return img


def sugar():
    rng = random.Random("sugar")
    pal = ramp(hexc("#F4F4F0"), 5, spread=0.15)
    pts = {(x, y) for x in range(16) for y in range(16)
           if ((x - 8) / 5.5) ** 2 + ((y - 10) / 3.5) ** 2 < 1 and rng.random() < 0.85}
    s = Shape()
    s.add(pts, pal)
    return s.render()


def fermented_spider_eye():
    pal = ramp(hexc("#8A4A3A"), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 8) < 5}, pal)
    img = s.render()
    for x, y in ((5, 6), (9, 6), (7, 9), (8, 9), (6, 11)):
        img.set(x, y, hexc("#C86A4A"))
    for x, y in ((4, 4), (10, 5), (11, 9)):
        img.set(x, y, hexc("#E0C8A0"))  # sugar specks
    return img


def golden_carrot():
    img = carrot()
    for y in range(16):
        for x in range(16):
            r, g, b, a = img.get(x, y)
            if a and r > g:  # the orange root turns gold
                img.set(x, y, (min(255, r + 30), min(255, g + 70), max(0, b - 10), a))
    return img


# Trim template accents (M23.6): one colour per pattern so the 18 templates differ.
TRIM_ACCENTS = {
    "sentry": "#9A9A9A", "dune": "#E0C878", "coast": "#5AA0C8", "wild": "#5A9A3A", "ward": "#2A6070",
    "eye": "#7AD8C0", "vex": "#B0B8C8", "tide": "#4AB0A0", "snout": "#E8B83A", "rib": "#E8E0D0",
    "spire": "#B080C0", "wayfinder": "#C88A5A", "shaper": "#B85A3A", "silence": "#3A4A60",
    "raiser": "#C89A6A", "host": "#A87A5A", "flow": "#6AC8E0", "bolt": "#D8B040"}


def smithing_template(accent, base="#3A3540"):
    """A slate tablet (our drawing) with a glyph in the pattern's colour."""
    rng = random.Random("template_" + accent)
    pal = ramp(hexc(base), 5, spread=0.3)
    acc = ramp(hexc(accent), 5, spread=0.35)
    s = Shape()
    s.add({(x, y) for x in range(3, 13) for y in range(1, 15) if not ((x in (3, 12)) and (y in (1, 14)))}, pal)
    img = s.render()
    for _ in range(14):  # the glyph: a small random rune, mirrored for symmetry
        x, y = rng.randint(5, 7), rng.randint(3, 12)
        for px in (x, 15 - x):
            img.set(px, y, acc[3] if y < 8 else acc[2])
    return img


def music_disc(label):
    """A black record with grooves and a coloured label (our drawing)."""
    img = Img(16, 16, CLEAR)
    lab = ramp(hexc(label), 5, spread=0.3)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if d < 7.2:
                c = (24, 24, 28, 255) if int(d * 2) % 3 else (48, 48, 56, 255)
                if d < 3.0:
                    c = lab[3] if d > 1.0 else (16, 16, 16, 255)
                if 6.5 <= d < 7.2:
                    c = (10, 10, 12, 255)
                img.set(x, y, c)
    for (x, y) in ((4, 4), (5, 4), (4, 5)):  # a shine
        img.set(x, y, (110, 110, 120, 255))
    return img


DISC_LABELS = {"13": "#E8D040", "cat": "#60D040", "blocks": "#E05030", "chirp": "#C03028", "far": "#90E060",
               "mall": "#8060D0", "mellohi": "#E0A0E0", "stal": "#303030", "strad": "#F0F0F0", "ward": "#208040",
               "11": "#606060", "wait": "#40A0E0", "pigstep": "#C06030", "otherside": "#40A8C0"}


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
    # Building materials (M23.1): bricks are small ingot-like bars, clay a soft lump.
    items["brick"] = ingot("#B4553C")
    items["nether_brick"] = ingot("#4E2228")
    items["clay_ball"] = lump("clay_ball", "#A0A6B6", "#C4C8D4", size=5)
    # Dyes and iron nuggets (M23.2): powder lumps in the dye colours (wiki: Dye).
    dyes = {"white": "#E8ECEC", "orange": "#F0801E", "magenta": "#C04EB8", "light_blue": "#3CB0DA",
            "yellow": "#F6D03C", "lime": "#80C020", "pink": "#EE8AA8", "gray": "#4A5154",
            "light_gray": "#9C9C96", "cyan": "#18989A", "purple": "#8832B4", "blue": "#3C44A8",
            "brown": "#82542E", "green": "#5E7A18", "red": "#AE2E26", "black": "#24242A"}
    for colour, base in dyes.items():
        items[f"{colour}_dye"] = lump(f"{colour}_dye", base, size=4.5)
    for colour, base in dyes.items():  # (M26.5b) harnesses: a strap in the colour, with goggles
        items[f"{colour}_harness"] = harness(base)
    items["iron_nugget"] = lump("iron_nugget", "#C8C8C8", "#F0F0F0", size=3.2)
    items["honeycomb"] = lump("honeycomb", "#E8A824", "#F8D860", size=5)  # (M23.4b: waxes copper)
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
    items["rabbit"] = meat("rabbit", "#E8A0A0", "#F8E0D8")  # (M26.3)
    items["cooked_rabbit"] = meat("cooked_rabbit", "#A8683E", "#D8A870", marbled=False)
    items["rabbit_hide"] = lump("rabbit_hide", "#A88458", "#D8B888", seed=7, size=6.0)
    items["rabbit_foot"] = rabbit_foot()
    items["goat_horn"] = horn()
    items["armadillo_scute"] = scute("#B87A6A")
    items["wolf_armor"] = wolf_armor()
    items["sweet_berries"] = sweet_berries()
    items["glow_berries"] = glow_berries()  # (M27.2)
    items["honey_bottle"] = honey_bottle()
    items["wind_charge"] = wind_charge()  # (M26.4c)
    items["snowball"] = snowball()  # (M26.5b)
    items["amethyst_shard"] = gem("#A87AE0", "emerald")  # (M26.5a)
    items["breeze_rod"] = blaze_rod("#8AB0E0", "#E0F0FF")
    items["phantom_membrane"] = membrane()  # (M26.4a; was missing)
    items["resin_brick"] = ingot("#E0702C")  # (M27.1c)
    items["echo_shard"] = gem("#1E6E78", "emerald")  # (M27.3b)
    items["trial_key"] = trial_key()  # (M27.4d)
    items["brush"] = brush()  # (M27.5)
    items["torchflower_seeds"] = torchflower_seeds()  # (M27.5c)
    items["pitcher_pod"] = pitcher_pod()
    for name in SHERD_MARKS:
        items[name + "_pottery_sherd"] = sherd(name)
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
    # Navigation and writing (M28.2): animated frames, as vanilla's compass_00.. names.
    for f in range(32):
        items[f"compass_{f:02d}"] = compass(f)
        items[f"recovery_compass_{f:02d}"] = compass(f, recovery=True)
    for f in range(64):
        items[f"clock_{f:02d}"] = clock(f)
    items["map"] = map_item()
    items["filled_map"] = map_item(filled=True)
    items["writable_book"] = writable_book()
    items["written_book"] = writable_book(written=True)
    items["item_frame"] = frame_item("#7A4A2A")  # (M28.3a)
    items["glow_item_frame"] = frame_item("#3AB8A0")
    items["painting"] = frame_item("#5E8A3C", picture=True)
    items["armor_stand"] = armor_stand_item()  # (M28.3b)
    items["lead"] = lead_item()  # (M28.3c)
    loaded = crossbow()  # (M28.4a) a crossbow with an arrow on the string: shaft, head, fletching
    for k in range(7):
        loaded.set(5 + k, 12 - k, hexc("#F0F0F0"))
    for (x, y) in ((12, 4), (13, 3), (12, 3), (13, 4)):
        loaded.set(x, y, hexc("#9AD8F0"))
    for (x, y) in ((4, 12), (4, 13), (5, 13)):
        loaded.set(x, y, hexc("#D04030"))
    items["crossbow_arrow"] = loaded
    for dye, colour in DYES:  # (M28.3d)
        items[f"{dye}_banner"] = banner_item(colour)
    for pat in ("field_masoned", "bordure_indented", "creeper", "skull", "flower", "mojang", "globe", "piglin",
                "flow", "guster"):
        items[f"{pat}_banner_pattern"] = pattern_item(pat)
    items["enchanted_book"] = book(True)
    # Nether mobs (M19.2).
    items["ghast_tear"] = ghast_tear()
    items["blaze_rod"] = blaze_rod()
    items["blaze_powder"] = blaze_powder()
    items["magma_cream"] = magma_cream()
    items["gold_nugget"] = lump("gold_nugget", "#F2CF3C", "#FFF4A0", size=3.4)
    items["fire_charge"] = fire_charge()
    # Brewing (M19.4).
    items["glass_bottle"] = bottle(filled=False)
    items["potion"] = bottle()
    items["splash_potion"] = bottle(splash=True)
    items["potion_overlay"] = potion_overlay()
    # (M28.4b) a lingering potion: the splash bottle with a long neck; dragon's breath
    lingering = bottle(splash=True)
    for y in (1, 2):
        for x in (7, 8):
            lingering.set(x, y, (205, 225, 240, 255))
    items["lingering_potion"] = lingering
    breath = bottle()
    for (x, y) in {(x, y) for x in range(16) for y in range(16) if math.hypot(x - 7.5, y - 10.5) < 3.6}:
        breath.set(x, y, (225, 120, 200, 255) if (x + y) % 3 else (250, 180, 230, 255))
    items["dragon_breath"] = breath
    items["tipped_arrow_base"] = arrow_variant(None)
    items["tipped_arrow_head"] = arrow_head_overlay()
    items["spectral_arrow"] = arrow_variant("#F4D040")
    items["firework_rocket"] = firework_rocket()  # (M28.4c)
    items["firework_star"] = firework_star()
    items["mace"] = mace()  # (M28.4d)
    cake = Shape()  # (M28.5a) a cake: white icing over a sponge
    cake.add({(x, y) for x in range(2, 14) for y in range(6, 13)}, ramp(hexc("#C8884A"), 5, spread=0.25))
    cake.add({(x, y) for x in range(2, 14) for y in range(4, 7)}, ramp(hexc("#F4F0EA"), 5, spread=0.1))
    cake_img = cake.render()
    for (x, y) in ((4, 5), (8, 4), (11, 5), (6, 7), (10, 8)):
        cake_img.set(x, y, hexc("#D0302A"))
    items["cake"] = cake_img
    for mat, base in (("wooden", "#A07A48"), ("stone", "#8E8E8E"), ("copper", "#D9804F"), ("iron", "#D6D6D6"),
                      ("golden", "#F2CF3C"), ("diamond", "#45DCCB"), ("netherite", "#5A4E56")):  # (M28.4e)
        items[f"{mat}_spear"] = spear(base)
    ok = trial_key()
    for y in range(16):
        for x in range(16):
            c = ok.get(x, y)
            if c[3]:
                ok.set(x, y, (max(0, c[0] - 90), min(255, c[1] + 10), min(255, c[2] + 30), 255))
    items["ominous_trial_key"] = ok
    items["sugar"] = sugar()
    items["fermented_spider_eye"] = fermented_spider_eye()
    items["golden_carrot"] = golden_carrot()
    items["glowstone_dust"] = lump("glowstone_dust", "#E8C060", "#FFF0A0", size=4.5)
    # The End (M20.1).
    items["chorus_fruit"] = lump("chorus_fruit", "#7A4A82", "#C89AD2", size=5.2)
    items["end_crystal"] = end_crystal_item()
    items["elytra"] = elytra_item()
    items["minecart"] = minecart_item()
    items["slime_ball"] = lump("slime_ball", "#6CC060", "#B8F0A8", size=4.8)
    items["shulker_shell"] = lump("shulker_shell", "#946894", "#C8A0C8", size=5.8)
    items["popped_chorus_fruit"] = lump("popped_chorus_fruit", "#A882B4", "#EEDDF4", size=5.2)
    # Netherite and smithing (M23.6).
    for disc, label in DISC_LABELS.items():
        items[f"music_disc_{disc}"] = music_disc(label)
    items["nether_star"] = gem("#F4F0E0", "diamond")
    items["heart_of_the_sea"] = lump("heart_of_the_sea", "#2A6AC8", "#7AE0F0", size=5.4)
    items["nautilus_shell"] = lump("nautilus_shell", "#E8D8C0", "#B07850", size=5.6)
    items["prismarine_shard"] = gem("#5AA898", "emerald")
    items["prismarine_crystals"] = lump("prismarine_crystals", "#9AD8C8", "#F0FFF8", size=4.6)
    items["golden_apple"] = apple("#F2C83C")  # (M24.3)
    items["crossbow"] = crossbow()  # (M24.4)
    items["totem_of_undying"] = totem()  # (M24.5)
    items["ominous_bottle"] = bottle(filled=True)  # (M24.4; tinted dark below)
    items["dried_kelp"] = dried_kelp()  # (M25.1)
    # M25.2: fish, ink, fish buckets, the fishing rod
    items["cod"] = fish_item("#9C8460", "#D2C6A8")
    items["cooked_cod"] = fish_item("#C89A62", "#E8D2A8", cooked=True)
    items["salmon"] = fish_item("#A8463A", "#D07A68", fin="#4E7A60")
    items["cooked_salmon"] = fish_item("#C8743E", "#E8A878", cooked=True, fin="#8A5A3A")
    items["tropical_fish"] = fish_item("#E87A2A", "#F2F2F2", stripes="#F8F8F8")
    items["pufferfish"] = fish_item("#E2BC3A", "#F2E6B0", spots="#7A5428")
    items["ink_sac"] = ink_sac("#2A2A36")
    items["glow_ink_sac"] = ink_sac("#2ACCB0")
    for fish, colour in (("cod", "#9C8460"), ("salmon", "#A8463A"), ("tropical_fish", "#E87A2A"),
                         ("pufferfish", "#E2BC3A")):
        img = bucket("#3C6EE6")
        img.set(7, 5, hexc(colour))
        img.set(8, 5, hexc(colour))
        img.set(9, 6, hexc(colour))
        items[f"{fish}_bucket"] = img
    for mob, colour in (("axolotl", "#F4A8C8"), ("tadpole", "#4A3A2A")):  # (M26.3c)
        img = bucket("#3C6EE6")
        for (x, y) in ((6, 5), (7, 5), (8, 5), (9, 6)):
            img.set(x, y, hexc(colour))
        items[f"{mob}_bucket"] = img
    items["fishing_rod"] = fishing_rod()
    items["trident"] = trident()  # (M25.3)
    items["turtle_scute"] = lump("turtle_scute", "#4E9A3A", "#7EC060", size=5.0)  # (M25.3b)
    items["turtle_helmet"] = armor("helmet", "#4E9A3A")
    BOAT_WOODS = (("oak", "#B8945F"), ("spruce", "#7A5A34"), ("birch", "#D7C185"), ("jungle", "#B88764"),
                         ("acacia", "#BA6337"), ("dark_oak", "#4F3218"), ("mangrove", "#773636"),
                         ("cherry", "#E7B7AE"), ("pale_oak", "#E5DACD"))
    for wood, colour in BOAT_WOODS:
        items[f"{wood}_boat"] = boat_item(colour)
    items["bamboo_raft"] = boat_item("#C9B758", raft=True)
    for wood, colour in BOAT_WOODS:
        items[f"{wood}_chest_boat"] = chest_boat_item(colour)
    items["bamboo_chest_raft"] = chest_boat_item("#C9B758", raft=True)
    items["saddle"] = saddle()
    for mat, base in (("leather", "#9A5A30"), ("iron", "#C8CCD0"), ("golden", "#F0C83C"), ("diamond", "#4ADCD0")):
        items[f"{mat}_horse_armor"] = horse_armor(base)
    items["netherite_ingot"] = ingot("#4A4048")
    items["netherite_scrap"] = lump("netherite_scrap", "#5E4A44", "#8A6E62", size=5.2)
    items["netherite_upgrade_smithing_template"] = smithing_template("#7A5A50")
    for pattern, accent in TRIM_ACCENTS.items():
        items[f"{pattern}_armor_trim_smithing_template"] = smithing_template(accent)
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for a 4x preview sheet")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    items = all_items()
    # The ominous bottle's liquid: a dark teal instead of the potion overlay's white.
    ob = items["ominous_bottle"]
    for y in range(16):
        for x in range(16):
            c = ob.get(x, y)
            if c[3] and c[0] > 150 and c[1] > 150 and c[2] > 150 and (x, y) not in ((7, 2), (8, 2)):
                ob.set(x, y, (40, 90, 80, 255))
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
