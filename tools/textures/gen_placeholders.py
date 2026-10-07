#!/usr/bin/env python3
"""Generate our own block textures in Minecraft's style (docs/art-style.md).

Original art, never copied (ADR 0004): every texture is designed here from a
written description of the material, with our own palettes and shapes. Style goals:
16x16, crisp pixel shapes, a small palette ramp per material (cool shadows, warm
highlights), light from the top-left with 1px bevels, and seamless tiling.

Writes assets/minecraft/textures/block/<name>.png. Deterministic: each texture has
its own seeded RNG, so changing one never changes another.

Usage:
  tools/textures/gen_placeholders.py                 regenerate all
  tools/textures/gen_placeholders.py --preview OUT   also write a preview sheet
                                                     (8x scale + 3x3 tiling per texture)
"""
import argparse
import math
import random
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"
N = 16

# --- Palettes: dark -> light. Shadows lean cool/saturated, highlights warm. ---------
STONE = [(74, 75, 82), (96, 97, 103), (116, 116, 120), (134, 133, 135), (156, 154, 150)]
COBBLE = [(58, 58, 66), (88, 88, 94), (112, 111, 114), (134, 132, 132), (162, 159, 152)]
MORTAR = (44, 44, 52)
DIRT = [(74, 50, 36), (98, 68, 46), (122, 86, 58), (143, 104, 70), (166, 124, 86)]
PEBBLE = [(98, 96, 100), (136, 134, 132)]
# Grass top is greyscale: the game multiplies it by the biome colour (plains #91BD59).
GRASS_GREY = [(98, 98, 98), (126, 126, 126), (152, 152, 152), (178, 178, 178), (206, 206, 206)]
PLAINS_TINT = (0x91, 0xBD, 0x59)
PLANKS = [(98, 70, 38), (132, 100, 56), (158, 124, 72), (180, 145, 88), (204, 170, 108)]
BARK = [(40, 29, 18), (62, 46, 28), (86, 64, 38), (108, 82, 50), (128, 100, 62)]
LOG_END = [(132, 100, 56), (158, 124, 72), (184, 150, 92), (206, 174, 112)]
SAND = [(196, 178, 122), (214, 198, 144), (226, 212, 160), (236, 225, 176), (248, 240, 200)]
BEDROCK = [(22, 22, 26), (48, 48, 54), (80, 80, 86), (116, 116, 120), (160, 160, 164)]


def clamp(v):
    return max(0, min(255, int(round(v))))


def tint(rgb, t):
    return tuple(clamp(c * k / 255) for c, k in zip(rgb, t))


def blank(color=(0, 0, 0)):
    return [[color for _ in range(N)] for _ in range(N)]


def value_noise(rng, cells):
    """Tileable smooth noise in [0,1]: random lattice of `cells`^2, wrapped, smoothstep."""
    lattice = [[rng.random() for _ in range(cells)] for _ in range(cells)]
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            fx, fy = x * cells / N, y * cells / N
            x0, y0 = int(fx), int(fy)
            tx, ty = fx - x0, fy - y0
            tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
            x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
            a = lattice[y0][x0] + (lattice[y0][x1] - lattice[y0][x0]) * tx
            b = lattice[y1][x0] + (lattice[y1][x1] - lattice[y1][x0]) * tx
            out[y][x] = a + (b - a) * ty
    return out


def fbm(rng, octaves=((4, 0.65), (8, 0.35))):
    layers = [(value_noise(rng, c), w) for c, w in octaves]
    return [[sum(l[y][x] * w for l, w in layers) for x in range(N)] for y in range(N)]


def ramp(palette, t, lo=0, hi=None):
    """Map t in [0,1] to a discrete palette entry between indices lo..hi (crisp)."""
    hi = len(palette) - 1 if hi is None else hi
    i = lo + int(max(0.0, min(0.999, t)) * (hi - lo + 1))
    return palette[i]


def voronoi(rng, count, edge_width=1.0):
    """Tileable cells: returns (cell id grid, edge grid). Edge = within `edge_width`
    of a cell border (bigger = thicker gaps)."""
    pts = [(rng.uniform(0, N), rng.uniform(0, N)) for _ in range(count)]
    ids = [[0] * N for _ in range(N)]
    edge = [[False] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            d = []
            for i, (px, py) in enumerate(pts):
                dx = min(abs(x + 0.5 - px), N - abs(x + 0.5 - px))
                dy = min(abs(y + 0.5 - py), N - abs(y + 0.5 - py))
                d.append((dx * dx + dy * dy, i))
            d.sort()
            ids[y][x] = d[0][1]
            edge[y][x] = math.sqrt(d[1][0]) - math.sqrt(d[0][0]) < edge_width
    return ids, edge


def bevel(img, mask, palette, base_index):
    """Pixel-art bevel on shapes in `mask`: top-left rims light, bottom-right rims dark.
    `base_index[y][x]` is each pixel's palette index; rims shift it by +/-1."""
    for y in range(N):
        for x in range(N):
            if not mask[y][x]:
                continue
            up, left = mask[(y - 1) % N][x], mask[y][(x - 1) % N]
            down, right = mask[(y + 1) % N][x], mask[y][(x + 1) % N]
            i = base_index[y][x]
            if not up or not left:
                i += 1
            elif not down or not right:
                i -= 1
            img[y][x] = palette[max(0, min(len(palette) - 1, i))]


# --- Materials ------------------------------------------------------------------------

def stone(rng):
    # Crisp mottling: two noise thresholds pick light/mid/dark patches (no blur).
    n = fbm(rng, ((4, 0.55), (8, 0.45)))
    img = [[STONE[3] if n[y][x] > 0.62 else STONE[1] if n[y][x] < 0.38 else STONE[2]
            for x in range(N)] for y in range(N)]
    # Recessed dark spots: dark core, light rim on the bottom-right (light comes from
    # the top-left, so the far wall of a pit catches it).
    # Stratified: one spot per 4x8 strip so they spread evenly instead of clumping.
    for k in range(8):
        x = (k % 4) * 4 + rng.randrange(4)
        y = (k // 4) * 8 + rng.randrange(8)
        cells = [(x, y)] + [(x + dx, y + dy) for dx, dy in
                            rng.sample([(1, 0), (0, 1), (1, 1), (-1, 0)], rng.choice((1, 2)))]
        cells = [(cx % N, cy % N) for cx, cy in cells]
        for cx, cy in cells:
            img[cy][cx] = STONE[0]
        for cx, cy in cells:
            for rx, ry in ((cx + 1, cy), (cx, cy + 1)):
                rx, ry = rx % N, ry % N
                if (rx, ry) not in cells:
                    img[ry][rx] = STONE[4]
    return img


def cobblestone(rng):
    ids, edge = voronoi(rng, 8)
    shade = {i: rng.choice((1, 2, 2, 3)) for i in range(8)}
    mask = [[not edge[y][x] for x in range(N)] for y in range(N)]
    base = [[shade[ids[y][x]] for x in range(N)] for y in range(N)]
    img = blank(MORTAR)
    bevel(img, mask, COBBLE, base)
    # A little surface grit inside the stones.
    for _ in range(14):
        x, y = rng.randrange(N), rng.randrange(N)
        if mask[y][x]:
            img[y][x] = COBBLE[max(0, base[y][x] - 1)]
    return img


def dirt_pixels(rng):
    n = fbm(rng, ((4, 0.5), (8, 0.5)))
    img = [[ramp(DIRT, n[y][x], 1, 3) for x in range(N)] for y in range(N)]
    # Clods: a light pixel with a dark one below it (reads as a raised lump).
    for _ in range(10):
        x, y = rng.randrange(N), rng.randrange(N)
        img[y][x] = DIRT[4]
        img[(y + 1) % N][x] = DIRT[0]
    # Pits and two tiny pebbles.
    for _ in range(6):
        img[rng.randrange(N)][rng.randrange(N)] = DIRT[0]
    for _ in range(2):
        x, y = rng.randrange(N), rng.randrange(N)
        img[y][x] = PEBBLE[1]
        img[y][(x + 1) % N] = PEBBLE[0]
    return img


def dirt(rng):
    return dirt_pixels(rng)


def grass_top_grey(rng):
    n = fbm(rng, ((4, 0.4), (16, 0.6)))
    img = [[ramp(GRASS_GREY, n[y][x], 1, 3) for x in range(N)] for y in range(N)]
    # Blades: bright tip with a darker base below, scattered densely for texture.
    for _ in range(26):
        x, y = rng.randrange(N), rng.randrange(N)
        img[y][x] = GRASS_GREY[4]
        img[(y + 1) % N][x] = GRASS_GREY[0]
    return img


def grass_block_top(rng):
    return grass_top_grey(rng)


def grass_block_side(rng):
    # Vanilla draws dirt + a biome-tinted overlay; we bake plains green (known deviation),
    # using the same grey ramp x tint as the top so the edge colours match.
    img = dirt_pixels(random.Random("grass_block_side/dirt"))
    green = [tint(c, PLAINS_TINT) for c in GRASS_GREY]
    depth = [rng.choice((3, 3, 4, 4, 5)) for _ in range(N)]
    for x in rng.sample(range(N), 4):  # a few longer drips
        depth[x] += rng.choice((1, 2))
    for x in range(N):
        for y in range(depth[x]):
            img[y][x] = green[3] if y == 0 else green[rng.choice((1, 2, 2, 3))]
        img[depth[x] - 1][x] = green[0]  # dark lip where grass meets dirt
        img[depth[x]][x] = DIRT[0]       # shadow cast on the dirt
    return img


def oak_planks(rng):
    img = blank()
    seams = [rng.randrange(3, 13) for _ in range(4)]
    for board in range(4):
        grain = value_noise(random.Random(f"planks/{board}"), 2)
        for row in range(4):
            y = board * 4 + row
            for x in range(N):
                if row == 3:
                    img[y][x] = PLANKS[0]          # gap between boards
                elif row == 0:
                    img[y][x] = PLANKS[4]          # lit top edge of the board
                else:
                    t = grain[y][x] + rng.uniform(-0.12, 0.12)
                    img[y][x] = ramp(PLANKS, t, 1, 3)
        # Board end: dark seam with a lit pixel to its right (bevel).
        sx = seams[board]
        for row in range(3):
            img[board * 4 + row][sx] = PLANKS[0]
            img[board * 4 + row][(sx + 1) % N] = PLANKS[4] if row == 0 else PLANKS[3]
        # Grain streaks along the board.
        for _ in range(2):
            gx, gy = rng.randrange(N), board * 4 + rng.choice((1, 2))
            for k in range(rng.randrange(3, 6)):
                if (gx + k) % N not in (sx, (sx + 1) % N):
                    img[gy][(gx + k) % N] = PLANKS[1]
    return img


def oak_log(rng):
    img = blank()
    x = 0
    while x < N:  # vertical bark ridges, 2-4 px wide, separated by dark crevices
        width = rng.choice((2, 3, 3, 4))
        for y in range(N):
            for k in range(width):
                if x + k >= N:
                    break
                if k == 0:
                    c = BARK[0]                     # crevice
                elif k == 1:
                    c = BARK[3]                     # lit ridge edge
                else:
                    c = BARK[2]
                img[y][x + k] = c
        x += width
    # Horizontal breaks across ridges and lighter flecks.
    for _ in range(7):
        bx, by = rng.randrange(N), rng.randrange(N)
        for k in range(rng.choice((1, 2))):
            if img[by][(bx + k) % N] != BARK[0]:
                img[by][(bx + k) % N] = BARK[1]
    for _ in range(6):
        fx, fy = rng.randrange(N), rng.randrange(N)
        if img[fy][fx] == BARK[2]:
            img[fy][fx] = BARK[4]
    return img


def oak_log_top(rng):
    img = blank()
    for y in range(N):
        for x in range(N):
            # Rounded-square rings: mix of Chebyshev and Euclidean distance.
            dx, dy = abs(x - 7.5), abs(y - 7.5)
            d = 0.6 * max(dx, dy) + 0.4 * math.hypot(dx, dy)
            if max(dx, dy) > 6.5:
                img[y][x] = BARK[1] if (x + y) % 3 else BARK[0]  # bark rim
                continue
            ring = int(d + rng.uniform(-0.25, 0.25))
            img[y][x] = LOG_END[3] if ring % 2 == 0 else LOG_END[1]
            if ring <= 0:
                img[y][x] = LOG_END[0]                          # pith
    # Inner rim shadow on the bottom/right, highlight on the top/left of the wood.
    for i in range(1, N - 1):
        img[1][i] = LOG_END[3] if img[1][i] != BARK[0] else img[1][i]
        img[N - 2][i] = LOG_END[0]
        img[i][N - 2] = LOG_END[0]
    return img


def sand(rng):
    n = fbm(rng, ((8, 0.5), (16, 0.5)))
    img = [[ramp(SAND, n[y][x], 1, 3) for x in range(N)] for y in range(N)]
    for _ in range(16):  # grains: dark and bright single pixels
        img[rng.randrange(N)][rng.randrange(N)] = SAND[0]
    for _ in range(10):
        img[rng.randrange(N)][rng.randrange(N)] = SAND[4]
    return img


def bedrock(rng):
    # High contrast jumble: chunks from near-black to light grey, dark gaps between.
    ids, edge = voronoi(rng, 7, edge_width=0.55)
    shade = {i: rng.choice((1, 2, 3, 3, 4)) for i in range(7)}
    mask = [[not edge[y][x] for x in range(N)] for y in range(N)]
    base = [[shade[ids[y][x]] for x in range(N)] for y in range(N)]
    img = blank(BEDROCK[0])
    bevel(img, mask, BEDROCK, base)
    # Rough, chaotic surface: dark pits and bright flecks across the chunks.
    for _ in range(14):
        img[rng.randrange(N)][rng.randrange(N)] = BEDROCK[0]
    for _ in range(8):
        img[rng.randrange(N)][rng.randrange(N)] = BEDROCK[4]
    return img


TEXTURES = {
    "stone": stone,
    "cobblestone": cobblestone,
    "dirt": dirt,
    "grass_block_top": grass_block_top,
    "grass_block_side": grass_block_side,
    "oak_planks": oak_planks,
    "oak_log": oak_log,
    "oak_log_top": oak_log_top,
    "sand": sand,
    "bedrock": bedrock,
}


# --- PNG output -----------------------------------------------------------------------

def encode_png(width, height, rows):
    """rows: list of lists of RGB tuples."""
    raw = b"".join(b"\x00" + bytes(c for px in row for c in (*px, 255)) for row in rows)

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))

    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def preview(images, path):
    """Each texture: 8x scaled tile, then a 3x3 tiling at 4x (seam check), side by side.
    The grass top is shown tinted plains green, as in game."""
    scale_big, scale_tile, gap = 8, 4, 6
    cell_w = N * scale_big + gap + N * 3 * scale_tile
    height = max(N * scale_big, N * 3 * scale_tile)
    width = len(images) * (cell_w + gap * 2)
    rows = [[(255, 255, 255)] * width for _ in range(height)]
    for i, (name, img) in enumerate(images.items()):
        if name == "grass_block_top":
            img = [[tint(c, PLAINS_TINT) for c in row] for row in img]
        ox = i * (cell_w + gap * 2)
        for y in range(N * scale_big):
            for x in range(N * scale_big):
                rows[y][ox + x] = img[y // scale_big][x // scale_big]
        tx = ox + N * scale_big + gap
        for y in range(N * 3 * scale_tile):
            for x in range(N * 3 * scale_tile):
                rows[y][tx + x] = img[(y // scale_tile) % N][(x // scale_tile) % N]
    Path(path).write_bytes(encode_png(width, height, rows))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="write a preview sheet PNG here")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    images = {}
    for name, fn in TEXTURES.items():
        img = fn(random.Random(name))  # stable per texture
        images[name] = img
        (OUT / f"{name}.png").write_bytes(encode_png(N, N, img))
    print(f"wrote {len(TEXTURES)} textures to {OUT}")
    if args.preview:
        preview(images, args.preview)
        print(f"preview: {args.preview}")


if __name__ == "__main__":
    main()
