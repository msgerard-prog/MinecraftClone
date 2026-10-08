#!/usr/bin/env python3
"""Generate banner pattern masks: original shapes, white on transparent, 16x32 (the flag,
top half then bottom half), tinted by the dye when drawn.

Writes assets/minecraft/textures/clone_banner/<pattern>.png for every pattern in
src/world/Banners.h plus "base" (the whole flag). Our own layout (not vanilla's 64x64
entity sheet), so vanilla packs don't replace them. Deterministic.
Usage: tools/textures/gen_banners.py
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.core import CLEAR, Img, encode_png  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/clone_banner"
W, H = 16, 32
ON = (255, 255, 255, 255)


def tri_wave(x, period, depth):
    return depth * (1 - abs((x % period) / (period / 2) - 1))


def icon(rows):
    """A centred pixel icon from text rows ('#' set), scaled 2x."""
    pts = set()
    h, w = len(rows), len(rows[0])
    ox, oy = (W - 2 * w) // 2, (H - 2 * h) // 2
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "#":
                for dx in range(2):
                    for dy in range(2):
                        pts.add((ox + 2 * i + dx, oy + 2 * j + dy))
    return lambda x, y: (x, y) in pts


CREEPER = ["######", "#.##.#", "######", "##..##", "#....#", "#.##.#"]
SKULL = [".####.", "######", "#..#.#", "######", ".####.", ".#.#.."]
FLOWER = ["..#...", ".###..", "##.##.", ".###..", "..#...", "......"]
THING = ["#....#", "##..##", "#.##.#", "#....#", "#....#", "......"]  # our own emblem for "mojang"
GLOBE = [".####.", "#.##.#", "####.#", "#.####", "#.##.#", ".####."]
PIGLIN = ["......", "######", "#.##.#", "#.##.#", "######", "......"]
FLOW = ["#####.", "....#.", ".##.#.", ".#..#.", ".####.", "......"]
GUSTER = [".###..", "#...#.", "#.#.#.", "#..##.", ".#....", "..###."]


def predicate(name):
    u = lambda x: (x + 0.5) / W  # noqa: E731
    v = lambda y: (y + 0.5) / H  # noqa: E731
    third, sq = 1 / 3, 6 / 16
    p = {
        "base": lambda x, y: True,
        "square_bottom_left": lambda x, y: u(x) < sq and v(y) > 1 - sq * W / H * 1.25,
        "square_bottom_right": lambda x, y: u(x) > 1 - sq and v(y) > 1 - sq * W / H * 1.25,
        "square_top_left": lambda x, y: u(x) < sq and v(y) < sq * W / H * 1.25,
        "square_top_right": lambda x, y: u(x) > 1 - sq and v(y) < sq * W / H * 1.25,
        "stripe_bottom": lambda x, y: v(y) > 1 - third,
        "stripe_top": lambda x, y: v(y) < third,
        "stripe_left": lambda x, y: u(x) < third,
        "stripe_right": lambda x, y: u(x) > 1 - third,
        "stripe_center": lambda x, y: third < u(x) < 1 - third,
        "stripe_middle": lambda x, y: third < v(y) < 1 - third,
        "stripe_downright": lambda x, y: abs(x - y * W / H) < 2.2,
        "stripe_downleft": lambda x, y: abs((W - 1 - x) - y * W / H) < 2.2,
        "small_stripes": lambda x, y: (x // 2) % 2 == 1,
        "cross": lambda x, y: abs(x - y * W / H) < 1.6 or abs((W - 1 - x) - y * W / H) < 1.6,
        "straight_cross": lambda x, y: 6 <= x <= 9 or 13 <= y <= 18,
        "triangle_bottom": lambda x, y: y >= H - 1 - (W / 2 - abs(x + 0.5 - W / 2)),
        "triangle_top": lambda x, y: y <= (W / 2 - abs(x + 0.5 - W / 2)),
        "triangles_bottom": lambda x, y: y >= H - 1 - tri_wave(x + 2, 4, 3),
        "triangles_top": lambda x, y: y <= tri_wave(x + 2, 4, 3),
        "diagonal_left": lambda x, y: x + y * W / H < W - 0.5,
        "diagonal_up_right": lambda x, y: x - y * W / H > -0.5,
        "diagonal_up_left": lambda x, y: x - y * W / H < 0.5,
        "diagonal_right": lambda x, y: x + y * W / H > W - 1.5,
        "circle": lambda x, y: (x + 0.5 - W / 2) ** 2 + (y + 0.5 - H / 2) ** 2 < 4.2 ** 2,
        "rhombus": lambda x, y: abs(x + 0.5 - W / 2) / 6 + abs(y + 0.5 - H / 2) / 11 < 1,
        "half_vertical": lambda x, y: u(x) < 0.5,
        "half_horizontal": lambda x, y: v(y) < 0.5,
        "half_vertical_right": lambda x, y: u(x) > 0.5,
        "half_horizontal_bottom": lambda x, y: v(y) > 0.5,
        "border": lambda x, y: x < 1 or x > W - 2 or y < 1 or y > H - 2,
        # Gradients are dithered (the cutout pass has no partial alpha).
        "gradient": lambda x, y: (y / H) < ((x * 7 + y * 3) % 8 + 0.5) / 8 * 1.05,
        "gradient_up": lambda x, y: ((H - 1 - y) / H) < ((x * 7 + y * 3) % 8 + 0.5) / 8 * 1.05,
        "bricks": lambda x, y: y % 4 == 3 or (x + (4 if (y // 4) % 2 else 0)) % 8 == 0,
        "curly_border": lambda x, y: min(x, W - 1 - x, y, H - 1 - y) < 1.5 + math.sin((x + y) * 1.2),
        "creeper": icon(CREEPER),
        "skull": icon(SKULL),
        "flower": icon(FLOWER),
        "mojang": icon(THING),
        "globe": icon(GLOBE),
        "piglin": icon(PIGLIN),
        "flow": icon(FLOW),
        "guster": icon(GUSTER),
    }
    return p[name]


PATTERNS = ["square_bottom_left", "square_bottom_right", "square_top_left", "square_top_right", "stripe_bottom",
            "stripe_top", "stripe_left", "stripe_right", "stripe_center", "stripe_middle", "stripe_downright",
            "stripe_downleft", "small_stripes", "cross", "straight_cross", "triangle_bottom", "triangle_top",
            "triangles_bottom", "triangles_top", "diagonal_left", "diagonal_up_right", "diagonal_up_left",
            "diagonal_right", "circle", "rhombus", "half_vertical", "half_horizontal", "half_vertical_right",
            "half_horizontal_bottom", "border", "gradient", "gradient_up", "bricks", "curly_border", "creeper",
            "skull", "flower", "mojang", "globe", "piglin", "flow", "guster"]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in ["base"] + PATTERNS:
        f = predicate(name)
        img = Img(W, H, CLEAR)
        for y in range(H):
            for x in range(W):
                if f(x, y):
                    img.set(x, y, ON)
        (OUT / f"{name}.png").write_bytes(encode_png(img))
    print(f"wrote {len(PATTERNS) + 1} banner masks to {OUT}")


if __name__ == "__main__":
    main()
