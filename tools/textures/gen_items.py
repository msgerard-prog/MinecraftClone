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
