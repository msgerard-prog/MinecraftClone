"""Reusable material painters. Each returns a new Img and takes an explicit rng.

Palettes are ramps from core.ramp (index 0 = darkest). Painters follow
docs/art-style.md: crisp ramp steps, readable shapes, top-left light, tileable.
"""
import math

from .core import (CLEAR, N, Img, bevel, fbm, fill_rect, frame, mix, pick, ramp, rgba, scale,
                   shift, value_noise, voronoi)  # noqa: F401


# --- Stone-like surfaces ---------------------------------------------------------------

def mottled(rng, pal, lo=1, hi=3, octaves=((4, 0.55), (8, 0.45)), cut=(0.38, 0.62)):
    """Crisp mottling: noise thresholds choose 3 ramp shades (no blur)."""
    n = fbm(rng, octaves)
    img = Img()
    for y in range(N):
        for x in range(N):
            v = n[y][x]
            i = hi if v > cut[1] else lo if v < cut[0] else (lo + hi) // 2
            img.set(x, y, pal[i])
    return img


def pits(img, rng, pal, count=8, light_rim=True):
    """Recessed dark spots spread evenly; bottom-right rim catches the light."""
    dark, light = pal[0], pal[-1]
    for k in range(count):
        x = (k % 4) * 4 + rng.randrange(4)
        y = (k // 4) * (N // max(1, (count + 3) // 4)) + rng.randrange(4)
        cells = [(x, y)] + [(x + dx, y + dy) for dx, dy in
                            rng.sample([(1, 0), (0, 1), (1, 1), (-1, 0)], rng.choice((1, 2)))]
        cells = [(cx % N, cy % N) for cx, cy in cells]
        for cx, cy in cells:
            img.set(cx, cy, dark)
        if light_rim:
            for cx, cy in cells:
                for rx, ry in ((cx + 1, cy), (cx, cy + 1)):
                    if (rx % N, ry % N) not in cells:
                        img.set(rx, ry, light)
    return img


def speckle(img, rng, colors, count):
    for _ in range(count):
        img.set(rng.randrange(N), rng.randrange(N), rng.choice(colors))
    return img


def stone(rng, pal, spots=8):
    return pits(mottled(rng, pal), rng, pal, spots)


def grainy(rng, pal, dark=14, bright=10, octaves=((8, 0.5), (16, 0.5))):
    """Fine granular material (sand, concrete powder, gravel base)."""
    n = fbm(rng, octaves)
    img = Img()
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[y][x], 1, 3))
    speckle(img, rng, [pal[0]], dark)
    speckle(img, rng, [pal[-1]], bright)
    return img


def cells(rng, pal, count, gap, shades=(1, 2, 2, 3), edge_width=1.0, grit=12):
    """Rounded stones / chunks separated by dark gaps, each bevelled (cobblestone,
    bedrock, gravel...)."""
    ids, edge = voronoi(rng, count, edge_width)
    shade = {i: rng.choice(shades) for i in range(count)}
    mask = [[not edge[y][x] for x in range(N)] for y in range(N)]
    base = [[shade[ids[y][x]] for x in range(N)] for y in range(N)]
    img = Img(fill=gap)
    bevel(img, mask, pal, base)
    for _ in range(grit):
        x, y = rng.randrange(N), rng.randrange(N)
        if mask[y][x]:
            img.set(x, y, shift(pal, base[y][x], -1))
    return img


def bricks(rng, pal, mortar, rows=4, brick_w=8, stagger=None, grit=10, bevel_bricks=True):
    """Running-bond bricks. rows of height N/rows, mortar line at the bottom/right."""
    rh = N // rows
    stagger = brick_w // 2 if stagger is None else stagger
    mask = [[False] * N for _ in range(N)]
    base = [[2] * N for _ in range(N)]
    for r in range(rows):
        off = (r % 2) * stagger
        shade = {}
        for y in range(r * rh, (r + 1) * rh):
            for x in range(N):
                bx = (x - off) % N
                in_mortar = (y == (r + 1) * rh - 1) or (bx % brick_w == brick_w - 1)
                mask[y][x] = not in_mortar
                key = (bx // brick_w)
                shade.setdefault(key, rng.choice((1, 2, 2, 3)))
                base[y][x] = shade[key]
    img = Img(fill=mortar)
    if bevel_bricks:
        bevel(img, mask, pal, base)
    else:
        for y in range(N):
            for x in range(N):
                if mask[y][x]:
                    img.set(x, y, pal[base[y][x]])
    for _ in range(grit):
        x, y = rng.randrange(N), rng.randrange(N)
        if mask[y][x]:
            img.set(x, y, shift(pal, base[y][x], rng.choice((-1, 1))))
    return img


def tiles(rng, pal, gap, size=8, grit=8):
    """Square tiles in a grid (deepslate tiles, polished tiles)."""
    mask = [[(x % size != size - 1) and (y % size != size - 1) for x in range(N)]
            for y in range(N)]
    shade = {(x // size, y // size): rng.choice((1, 2, 2, 3)) for x in range(N) for y in range(N)}
    base = [[shade[(x // size, y // size)] for x in range(N)] for y in range(N)]
    img = Img(fill=gap)
    bevel(img, mask, pal, base)
    for _ in range(grit):
        x, y = rng.randrange(N), rng.randrange(N)
        if mask[y][x]:
            img.set(x, y, shift(pal, base[y][x], -1))
    return img


def polished(rng, pal, noise=6):
    """Smooth cut face with a bevelled 1px rim (polished granite, smooth stone...)."""
    img = mottled(rng, pal, 2, 2)
    for y in range(1, N - 1):
        for x in range(1, N - 1):
            if rng.random() < 0.18:
                img.set(x, y, pal[rng.choice((1, 3))])
    frame(img, pal[4], pal[0])
    speckle(img, rng, [pal[1]], noise)
    frame(img, pal[4], pal[0])
    return img


def chiseled(rng, pal, motif="ring"):
    """Polished block with an original carved motif (never a vanilla design)."""
    img = polished(rng, pal, noise=3)
    frame(img, pal[0], pal[3], inset=2)  # recessed panel: dark top/left, lit bottom/right
    c = (N - 1) / 2
    for y in range(3, N - 3):
        for x in range(3, N - 3):
            dx, dy = x - c, y - c
            r = math.hypot(dx, dy)
            on = False
            if motif == "ring":
                on = 3.0 < r < 4.3
            elif motif == "diamond":
                on = abs(abs(dx) + abs(dy) - 4) < 0.8
            elif motif == "cross":
                on = (abs(dx) < 1 or abs(dy) < 1) and r < 5
            elif motif == "sun":
                on = r < 2.2 or (abs(abs(dx) - abs(dy)) < 0.8 and 3 < r < 5.2)
            elif motif == "steps":
                on = max(abs(dx), abs(dy)) in (1.5, 3.5)
            if on:
                img.set(x, y, pal[0])
                img.set(x + 1, y + 1, pal[4]) if img.get(x + 1, y + 1) != pal[0] else None
    return img


def cracked(img, rng, pal, cracks=3):
    """Overlay jagged dark cracks with a light lower edge."""
    out = img.copy()
    for _ in range(cracks):
        x, y = rng.randrange(N), rng.randrange(N)
        for _ in range(rng.randrange(5, 9)):
            out.set(x, y, pal[0])
            out.set(x, y + 1, pal[3]) if out.get(x, y + 1) != pal[0] else None
            x += rng.choice((-1, 0, 1, 1))
            y += rng.choice((0, 1))
    return out


def mossy(img, rng, moss_pal, amount=0.42):
    """Moss patches growing over a surface (mossy cobblestone/bricks)."""
    out = img.copy()
    n = fbm(rng, ((4, 0.6), (8, 0.4)))
    for y in range(N):
        for x in range(N):
            if n[y][x] > 1 - amount:
                above_clear = n[(y - 1) % N][x] <= 1 - amount
                out.set(x, y, moss_pal[3] if above_clear else pick(moss_pal, n[y][x], 1, 2))
    return out


def banded(rng, pal, horizontal=True, wobble=1.2):
    """Layered rock (deepslate, sandstone sides): bands across the face."""
    n = value_noise(rng, 4)
    img = Img()
    band = [rng.choice((1, 2, 2, 3)) for _ in range(N)]
    for y in range(N):
        for x in range(N):
            a, b = (y, x) if horizontal else (x, y)
            k = int(a + (n[y][x] - 0.5) * wobble * 2) % N
            img.set(x, y, pal[band[k]])
    return img


# --- Ores, metals, gems --------------------------------------------------------------

def ore(base, rng, ore_pal, clusters=5, shape="nugget"):
    """Ore pieces embedded in a stone face. ore_pal: ramp of the mineral."""
    out = base.copy()
    spots = [((k % 3) * 5 + rng.randrange(1, 4), (k // 3) * 6 + rng.randrange(1, 5))
             for k in range(clusters)]
    for sx, sy in spots:
        if shape == "nugget":      # chunky 2x2-ish lumps
            pts = [(0, 0), (1, 0), (0, 1), (1, 1)] + rng.sample([(2, 0), (0, 2), (2, 1), (-1, 1)], 2)
        elif shape == "gem":       # small diamond shapes
            pts = [(0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)]
        else:                      # "vein": short diagonal streak
            pts = [(0, 0), (1, 1), (2, 1), (2, 2), (3, 3)][:rng.choice((3, 4, 5))]
        pts = [(sx + dx, sy + dy) for dx, dy in pts]
        for px, py in pts:
            out.set(px, py, ore_pal[2])
        for px, py in pts:   # bevel each piece
            if (px, py - 1) not in pts or (px - 1, py) not in pts:
                out.set(px, py, ore_pal[4])
            elif (px, py + 1) not in pts or (px + 1, py) not in pts:
                out.set(px, py, ore_pal[1])
        for px, py in pts:   # dark outline below/right so pieces sit in the rock
            for qx, qy in ((px + 1, py), (px, py + 1)):
                if (qx, qy) not in pts:
                    out.set(qx, qy, ore_pal[0])
    return out


def metal_block(rng, pal, rivets=True):
    """Storage block of refined metal: plate with bevel, diagonal sheen, rivets."""
    img = Img(fill=pal[2])
    for y in range(N):
        for x in range(N):
            d = (x + y) % 11
            if d in (0, 1) and 2 < x + y < 28:
                img.set(x, y, pal[3])
    frame(img, pal[4], pal[0])
    frame(img, pal[3], pal[1], inset=1)
    if rivets:
        for x, y in ((3, 3), (12, 3), (3, 12), (12, 12)):
            img.set(x, y, pal[4])
            img.set(x + 1, y + 1, pal[0])
    return img


def gem_block(rng, pal):
    """Cut gem storage block: faceted diagonal panes."""
    img = Img(fill=pal[2])
    for y in range(N):
        for x in range(N):
            u, v = x % 8, y % 8
            if u + v < 7:
                img.set(x, y, pal[3])
            elif u + v > 8:
                img.set(x, y, pal[1])
            if u == v:
                img.set(x, y, pal[4])
    frame(img, pal[4], pal[0])
    return img


def raw_block(rng, pal):
    """Raw ore block: lumpy nuggets packed together."""
    return cells(rng, pal, 9, pal[0], shades=(1, 2, 3), edge_width=0.7, grit=10)


def mineral_block(rng, pal):
    """Compressed mineral (coal, lapis, redstone blocks): grainy bevelled plate."""
    img = grainy(rng, pal, dark=10, bright=8)
    frame(img, pal[4], pal[0])
    return img


# --- Soils and loose blocks ----------------------------------------------------------

def soil(rng, pal, pebble=None, clods=10, pits_n=6):
    n = fbm(rng, ((4, 0.5), (8, 0.5)))
    img = Img()
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[y][x], 1, 3))
    for _ in range(clods):  # raised lumps: light pixel, dark below
        x, y = rng.randrange(N), rng.randrange(N)
        img.set(x, y, pal[4])
        img.set(x, y + 1, pal[0])
    speckle(img, rng, [pal[0]], pits_n)
    if pebble:
        for _ in range(2):
            x, y = rng.randrange(N), rng.randrange(N)
            img.set(x, y, pebble[1])
            img.set(x + 1, y, pebble[0])
    return img


def blades(rng, pal, count=26):
    """Grass/moss top seen from above: mottled with bright tips over dark bases."""
    n = fbm(rng, ((4, 0.4), (16, 0.6)))
    img = Img()
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[y][x], 1, 3))
    for _ in range(count):
        x, y = rng.randrange(N), rng.randrange(N)
        img.set(x, y, pal[4])
        img.set(x, y + 1, pal[0])
    return img


def side_with_cap(base, rng, cap_pal, depth=(3, 3, 4, 4, 5), drips=4):
    """A block side with a cap layer on top (grass, mycelium, podzol, snow)."""
    img = base.copy()
    d = [rng.choice(depth) for _ in range(N)]
    for x in rng.sample(range(N), drips):
        d[x] += rng.choice((1, 2))
    for x in range(N):
        for y in range(d[x]):
            img.set(x, y, cap_pal[3] if y == 0 else cap_pal[rng.choice((1, 2, 2, 3))])
        img.set(x, d[x] - 1, cap_pal[0])
    return img, d


def ice(rng, pal, alpha=255, cracks=2):
    """Clear-ish ice: smooth gradient bands with bright streaks."""
    img = Img()
    n = fbm(rng, ((2, 0.7), (4, 0.3)))
    for y in range(N):
        for x in range(N):
            img.set(x, y, rgba(pick(pal, n[y][x], 1, 3), alpha))
    for k in range(3):  # diagonal glints
        x0 = rng.randrange(N)
        for i in range(rng.randrange(3, 6)):
            img.set(x0 + i, k * 5 + 2 + i // 2, rgba(pal[4], alpha))
    for _ in range(cracks):
        x, y = rng.randrange(N), rng.randrange(N)
        for _ in range(4):
            img.set(x, y, rgba(pal[0], alpha))
            x += 1
            y += rng.choice((0, 1))
    return img


# --- Wood ------------------------------------------------------------------------------

def planks(rng, pal, boards=4):
    """Horizontal boards: lit top edge, dark gap, grain streaks, staggered end seams."""
    img = Img()
    bh = N // boards
    seams = [rng.randrange(3, 13) for _ in range(boards)]
    for b in range(boards):
        grain = value_noise(rng, 2)
        for row in range(bh):
            y = b * bh + row
            for x in range(N):
                if row == bh - 1:
                    img.set(x, y, pal[0])
                elif row == 0:
                    img.set(x, y, pal[4])
                else:
                    img.set(x, y, pick(pal, grain[y][x] + rng.uniform(-0.12, 0.12), 1, 3))
        sx = seams[b]
        for row in range(bh - 1):
            img.set(sx, b * bh + row, pal[0])
            img.set(sx + 1, b * bh + row, pal[4] if row == 0 else pal[3])
        for _ in range(2):
            gx, gy = rng.randrange(N), b * bh + rng.choice(range(1, bh - 1))
            for k in range(rng.randrange(3, 6)):
                if (gx + k) % N not in (sx, (sx + 1) % N):
                    img.set(gx + k, gy, pal[1])
    return img


def bark(rng, pal, ridge=(2, 3, 3, 4), horizontal=False):
    """Bark: ridges separated by dark crevices, lit ridge edges, breaks and flecks."""
    img = Img()
    x = 0
    cols = []
    while x < N:
        w = rng.choice(ridge)
        for k in range(w):
            if x + k < N:
                cols.append(0 if k == 0 else 3 if k == 1 else 2)
        x += w
    for y in range(N):
        for x in range(N):
            i = cols[x]
            img.set(x, y, pal[i]) if not horizontal else img.set(y, x, pal[i])
    for _ in range(7):
        bx, by = rng.randrange(N), rng.randrange(N)
        if img.get(bx, by) != pal[0]:
            img.set(bx, by, pal[1])
    for _ in range(6):
        fx, fy = rng.randrange(N), rng.randrange(N)
        if img.get(fx, fy) == pal[2]:
            img.set(fx, fy, pal[4])
    return img


def log_end(rng, ring_pal, bark_pal, rim=True):
    """Cut log end: rounded-square growth rings, pith, bark rim."""
    img = Img()
    for y in range(N):
        for x in range(N):
            dx, dy = abs(x - 7.5), abs(y - 7.5)
            d = 0.6 * max(dx, dy) + 0.4 * math.hypot(dx, dy)
            if rim and max(dx, dy) > 6.5:
                img.set(x, y, bark_pal[1] if (x + y) % 3 else bark_pal[0])
                continue
            ring = int(d + rng.uniform(-0.25, 0.25))
            img.set(x, y, ring_pal[3] if ring % 2 == 0 else ring_pal[1])
            if ring <= 0:
                img.set(x, y, ring_pal[0])
    if rim:
        for i in range(1, N - 1):
            if img.get(i, 1) != bark_pal[0]:
                img.set(i, 1, ring_pal[4])
            img.set(i, N - 2, ring_pal[0])
            img.set(N - 2, i, ring_pal[0])
    return img


def stripped(rng, pal):
    """Stripped log side: smooth long fibres along the length."""
    img = Img()
    n = value_noise(rng, 8)
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[(y // 4) % N][x] * 0.8 + 0.1, 1, 3))
    for _ in range(6):
        x, y = rng.randrange(N), rng.randrange(N)
        for i in range(rng.randrange(3, 7)):
            img.set(x, y + i, pal[1])
    for x in range(N):
        if rng.random() < 0.2:
            for y in range(N):
                if img.get(x, y) == pal[2]:
                    img.set(x, y, pal[3])
    return img


def birch_bark(rng, pal, mark):
    """Pale bark with dark horizontal marks and fine lines."""
    img = Img()
    n = value_noise(rng, 8)
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[y][x] * 0.6 + 0.3, 2, 4))
    for _ in range(6):
        x, y = rng.randrange(N), rng.randrange(N)
        w = rng.randrange(2, 6)
        for k in range(w):
            img.set(x + k, y, mark[0])
            if rng.random() < 0.5:
                img.set(x + k, y + 1, mark[1])
    for _ in range(8):
        img.set(rng.randrange(N), rng.randrange(N), pal[1])
    return img


def leaves(rng, pal, holes=0.16, clusters=11):
    """Leaf mass: bevelled leaf clumps with see-through gaps (cutout)."""
    img = Img(fill=pal[1])
    n = fbm(rng, ((4, 0.5), (8, 0.5)))
    for y in range(N):
        for x in range(N):
            img.set(x, y, pick(pal, n[y][x], 1, 3))
    for k in range(clusters):  # clumps: lit top-left, shadow bottom-right
        cx, cy = rng.randrange(N), rng.randrange(N)
        for dy in range(-1, 2):
            for dx in range(-1, 2):
                if abs(dx) + abs(dy) < 2:
                    img.set(cx + dx, cy + dy, pal[3])
        img.set(cx - 1, cy - 1, pal[4])
        img.set(cx + 1, cy + 1, pal[0])
    # See-through gaps between clumps: small clustered holes, not single-pixel noise.
    for _ in range(int(holes * 40)):
        x, y = rng.randrange(N), rng.randrange(N)
        for dx, dy in [(0, 0)] + rng.sample([(1, 0), (0, 1), (1, 1), (-1, 0)], rng.choice((1, 2))):
            img.set(x + dx, y + dy, CLEAR)
        img.set(x - 1, y - 1, pal[0])  # dark edge where the canopy opens up
    return img


def sapling(rng, trunk, leaf, shape="round"):
    """Young tree on a transparent background (cross model)."""
    img = Img()
    for y in range(9, N):  # trunk
        img.set(7, y, trunk[2])
        img.set(8, y, trunk[1])
    if shape == "cone":
        rows = [(2, 0), (3, 1), (4, 1), (5, 2), (6, 2), (7, 3), (8, 3), (9, 4), (10, 4)]
    elif shape == "flat":
        rows = [(3, 5), (4, 6), (5, 4), (7, 1), (8, 1)]
    elif shape == "tall":
        rows = [(1, 2), (2, 3), (3, 3), (4, 4), (5, 4), (6, 3), (7, 3), (8, 2), (9, 1)]
    else:  # round bush
        rows = [(2, 2), (3, 3), (4, 4), (5, 5), (6, 5), (7, 4), (8, 3), (9, 2)]
    for y, half in rows:
        for x in range(8 - half, 8 + half):
            if rng.random() < 0.88:
                img.set(x, y, leaf[rng.choice((1, 2, 2, 3))])
        img.set(8 - half, y, leaf[3]) if half else None
        img.set(8 + half - 1, y, leaf[1]) if half else None
    for _ in range(6):
        y, half = rng.choice(rows)
        if half:
            img.set(rng.randrange(8 - half, 8 + half), y, leaf[4])
    return img


def door(rng, pal, part, style):
    """Door half (top/bottom). Styles: window, panel, boards, lattice, grid, stalks.
    Transparent pixels are see-through openings."""
    img = Img(fill=pal[2])
    # Vertical boards as the base.
    for x in range(N):
        for y in range(N):
            img.set(x, y, pal[2] if (x // 4) % 2 == 0 else pal[3])
            if x % 4 == 3:
                img.set(x, y, pal[1])
    frame(img, pal[4], pal[0])
    if style == "window" and part == "top":
        for (x0, y0) in ((3, 3), (9, 3), (3, 9), (9, 9)):
            for y in range(y0, y0 + 4):
                for x in range(x0, x0 + 4):
                    img.set(x, y, CLEAR)
    elif style == "grid" and part == "top":
        for y in range(2, 14):
            for x in range(2, 14):
                if x % 4 != 1 and y % 4 != 1:
                    img.set(x, y, CLEAR)
    elif style == "lattice":
        for y in range(2, 14):
            for x in range(2, 14):
                if (x + y) % 4 == 0 or (x - y) % 4 == 0:
                    img.set(x, y, pal[0])
                elif (x + y) % 4 == 2 and (x - y) % 4 == 2 and part == "top":
                    img.set(x, y, CLEAR)
    elif style == "panel" or (style in ("window", "grid") and part == "bottom"):
        for (y0, y1) in ((2, 6), (9, 13)):
            frame_sub = [(x, y) for x in range(3, 13) for y in range(y0, y1 + 1)]
            for x, y in frame_sub:
                img.set(x, y, pal[2])
            for x in range(3, 13):
                img.set(x, y0, pal[0])
                img.set(x, y1, pal[4])
            for y in range(y0, y1 + 1):
                img.set(3, y, pal[0])
                img.set(12, y, pal[4])
    elif style == "stalks":
        for x in range(N):
            for y in range(N):
                img.set(x, y, pal[3] if x % 3 else pal[1])
            if x % 3 == 0:
                for y in range(0, N, 5):
                    img.set(x, y, pal[0])
        frame(img, pal[4], pal[0])
    if style == "boards":
        for y in (3, 12):  # iron straps
            for x in range(1, N - 1):
                img.set(x, y, (70, 70, 76, 255))
            img.set(2, y, (150, 150, 156, 255))
    if part == "bottom":  # handle
        img.set(12, 2, (60, 60, 66, 255))
        img.set(12, 3, (150, 150, 156, 255))
    return img


def trapdoor(rng, pal, style):
    img = Img(fill=pal[2])
    for y in range(N):
        for x in range(N):
            img.set(x, y, pal[2] if (y // 4) % 2 == 0 else pal[3])
            if y % 4 == 3:
                img.set(x, y, pal[1])
    frame(img, pal[4], pal[0])
    frame(img, pal[3], pal[1], inset=1)
    if style in ("window", "grid", "lattice"):
        for y in range(3, 13):
            for x in range(3, 13):
                hole = ((x % 5 in (3, 4)) and (y % 5 in (3, 4))) if style != "lattice" else \
                    ((x + y) % 4 == 0)
                if hole:
                    img.set(x, y, CLEAR)
    elif style == "panel":
        for x in range(4, 12):
            img.set(x, 4, pal[0])
            img.set(x, 11, pal[4])
        for y in range(4, 12):
            img.set(4, y, pal[0])
            img.set(11, y, pal[4])
    elif style == "stalks":
        for x in range(2, 14):
            for y in range(2, 14):
                img.set(x, y, pal[3] if x % 3 else pal[1])
    return img


def stalks(rng, pal, vertical=True):
    """Bamboo-like bundled stalks with nodes."""
    img = Img()
    for x in range(N):
        col = x % 4
        for y in range(N):
            c = (pal[1], pal[3], pal[2], pal[0])[col]
            img.set(x, y, c)
    for x0 in range(0, N, 4):
        ny = rng.randrange(N)
        for k in range(3):
            img.set(x0 + k, ny, pal[4] if k == 1 else pal[0])
    if not vertical:
        img = img.map(lambda c: c)
        rot = Img()
        for y in range(N):
            for x in range(N):
                rot.set(y, x, img.get(x, y))
        img = rot
    return img


def stalk_ends(rng, pal, rim):
    img = Img(fill=rim[0])
    for cy in range(2, N, 4):
        for cx in range(2, N, 4):
            for dy in (-1, 0):
                for dx in (-1, 0):
                    img.set(cx + dx, cy + dy, pal[3] if dx + dy < 0 else pal[1])
            img.set(cx, cy, rim[1])
    return img


def roots(rng, pal, background=None, count=7):
    """Tangled roots; transparent gaps unless a background image is given."""
    img = background.copy() if background else Img()
    for _ in range(count):
        x, y = rng.randrange(N), rng.randrange(N)
        horizontal = rng.random() < 0.5
        for _ in range(rng.randrange(8, 14)):
            img.set(x, y, pal[2])
            img.set(x + (0 if horizontal else 1), y + (1 if horizontal else 0), pal[0])
            img.set(x - (0 if horizontal else 1), y - (1 if horizontal else 0), pal[3])
            if horizontal:
                x += 1
                y += rng.choice((-1, 0, 0, 1))
            else:
                y += 1
                x += rng.choice((-1, 0, 0, 1))
    return img
