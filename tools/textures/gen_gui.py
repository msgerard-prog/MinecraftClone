#!/usr/bin/env python3
"""Generate the GUI textures: original art in the game's pixel style (docs/art-style.md).

Writes, in vanilla's resource-pack layout (so a pack or the user's jar overrides it):
  assets/minecraft/textures/font/ascii.png               128x128, 16x16 cells of 8x8,
                                                         code point = row*16 + col;
                                                         advance = ink width + 1
  assets/minecraft/textures/gui/sprites/hud/hotbar.png            182x22, 9 slots
  assets/minecraft/textures/gui/sprites/hud/hotbar_selection.png  24x23
Deterministic. Usage: tools/textures/gen_gui.py [--preview DIR]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import CLEAR, Img, encode_png  # noqa: E402
from texgen.font5x7 import G  # noqa: E402

ROOT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures"
WHITE = (255, 255, 255, 255)


def font():
    img = Img(128, 128, CLEAR)
    for ch, rows in G.items():
        code = ord(ch)
        ox, oy = (code % 16) * 8, (code // 16) * 8
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                if c == "#":
                    img.set(ox + x, oy + y, WHITE)
    return img


def hotbar():
    """Nine 20x20 slots in a 182x22 bar: dark translucent wells, bevelled frame."""
    img = Img(182, 22, CLEAR)
    edge_dark, edge_light = (24, 24, 28, 255), (150, 150, 158, 255)
    well, well_light, well_dark = (58, 58, 64, 190), (92, 92, 100, 200), (36, 36, 40, 200)
    for x in range(182):
        img.set(x, 0, edge_dark)
        img.set(x, 21, edge_dark)
    for y in range(22):
        img.set(0, y, edge_dark)
        img.set(181, y, edge_dark)
    for i in range(9):
        sx = 1 + i * 20
        for y in range(1, 21):
            for x in range(sx, sx + 20):
                lx, ly = x - sx, y - 1
                if lx == 0 or ly == 0:
                    c = edge_light if (lx == 0 and ly == 0) else well_dark
                elif lx == 19 or ly == 19:
                    c = well_light
                else:
                    c = well
                img.set(x, y, c)
    return img


def selection():
    """A bright 24x23 frame (open at the bottom row like the bar's lower edge)."""
    img = Img(24, 23, CLEAR)
    outer, inner = (20, 20, 20, 255), (236, 236, 236, 255)
    for y in range(23):
        for x in range(24):
            ring = min(x, y, 23 - x, 22 - y)
            if ring == 0:
                img.set(x, y, outer)
            elif ring in (1, 2):
                img.set(x, y, inner if ring == 1 else (176, 176, 176, 255))
    return img


HEART = [  # 9x9: '#' outline, 'r' fill, 'h' highlight
    ".##...##.",
    "#rr#.#rr#",
    "#hrr#rrr#",
    "#hrrrrrr#",
    "#rrrrrrr#",
    ".#rrrrr#.",
    "..#rrr#..",
    "...#r#...",
    "....#....",
]
DRUMSTICK = [  # 9x9: '#' outline, 'm' meat, 'l' light, 'b' bone
    "..####...",
    ".#mmmm#..",
    "#mllmmm#.",
    "#lmmmmm#.",
    "#mmmmmm#.",
    ".#mmmm##.",
    "..####b#.",
    "......#b#",
    ".......#.",
]


def icon(rows, colors, keep=lambda x, y: True):
    img = Img(9, 9, CLEAR)
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c != "." and keep(x, y):
                img.set(x, y, colors.get(c, colors["#"]))
    return img


BUBBLE = [  # 9x9: '#' outline, 'b' water, 'h' highlight
    "..#####..",
    ".#bbbbb#.",
    "#bhhbbbb#",
    "#bhbbbbb#",
    "#bbbbbbb#",
    "#bbbbbbb#",
    "#bbbbbbb#",
    ".#bbbbb#.",
    "..#####..",
]


CHESTPLATE = [  # 9x9: '#' outline, 'a' plate, 'h' highlight (our drawing)
    "##.....##",
    "#a#...#a#",
    "#aa###aa#",
    "#ahaaaaa#",
    ".#haaaa#.",
    ".#aaaaa#.",
    ".#aaaaa#.",
    ".#aaaaa#.",
    ".#######.",
]


def survival_icons():
    out = {}
    red = {"#": (30, 8, 10, 255), "r": (214, 34, 36, 255), "h": (255, 160, 150, 255)}
    out["hud/heart/full.png"] = icon(HEART, red)
    out["hud/heart/half.png"] = icon(HEART, red, lambda x, y: x <= 4 or HEART[y][x] == "#")
    grey = {"#": (24, 24, 24, 255), "r": (60, 60, 60, 255), "h": (60, 60, 60, 255)}
    out["hud/heart/container.png"] = icon(HEART, grey)
    meat = {"#": (40, 20, 8, 255), "m": (176, 96, 44, 255), "l": (230, 150, 90, 255), "b": (236, 228, 210, 255)}
    out["hud/food_full.png"] = icon(DRUMSTICK, meat)
    out["hud/food_half.png"] = icon(DRUMSTICK, meat, lambda x, y: x >= 4 or DRUMSTICK[y][x] == "#")
    dark = {"#": (24, 24, 24, 255), "m": (58, 50, 44, 255), "l": (58, 50, 44, 255), "b": (70, 66, 60, 255)}
    out["hud/food_empty.png"] = icon(DRUMSTICK, dark)
    water = {"#": (20, 40, 110, 255), "b": (70, 140, 230, 255), "h": (210, 235, 255, 255)}
    out["hud/air.png"] = icon(BUBBLE, water)
    # Bursting: a broken ring (our own drawing).
    out["hud/air_bursting.png"] = icon(BUBBLE, water, lambda x, y: BUBBLE[y][x] == "#" and (x + y) % 3 != 0)
    plate = {"#": (30, 30, 34, 255), "a": (198, 198, 206, 255), "h": (250, 250, 255, 255)}
    out["hud/armor_full.png"] = icon(CHESTPLATE, plate)
    out["hud/armor_half.png"] = icon(CHESTPLATE, plate, lambda x, y: x <= 4 or CHESTPLATE[y][x] == "#")
    empty = {"#": (30, 30, 34, 255), "a": (60, 60, 64, 255), "h": (60, 60, 64, 255)}
    out["hud/armor_empty.png"] = icon(CHESTPLATE, empty)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for 4x previews")
    args = ap.parse_args()
    files = {
        **{"gui/sprites/" + k: v for k, v in survival_icons().items()},
        "font/ascii.png": font(),
        "gui/sprites/hud/hotbar.png": hotbar(),
        "gui/sprites/hud/hotbar_selection.png": selection(),
    }
    for rel, img in files.items():
        path = ROOT / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(encode_png(img))
        print(f"wrote {path} ({img.w}x{img.h})")
    if args.preview:
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)
        for rel, img in files.items():
            big = Img(img.w * 4, img.h * 4, (90, 120, 90, 255))
            for y in range(big.h):
                for x in range(big.w):
                    c = img.get(x // 4, y // 4)
                    if c[3]:
                        a = c[3] / 255
                        bg = big.get(x, y)
                        big.set(x, y, tuple(int(c[k] * a + bg[k] * (1 - a)) for k in range(3)))
            (out / ("gui-" + rel.replace("/", "_"))).write_bytes(encode_png(big))


if __name__ == "__main__":
    main()
