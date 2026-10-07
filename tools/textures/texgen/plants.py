"""Plant painters: shapes on transparent backgrounds (cross models) and plant blocks."""
import math

from .core import CLEAR, N, Img, fbm, frame, pick, rgba, value_noise, voronoi
from .materials import cells, mottled, speckle

TRANSPARENT = CLEAR


def stem(img, rng, pal, x=7, top=8, bottom=15, leaves=True, wobble=True):
    """A plant stalk with a pair of side leaves."""
    cx = x
    for y in range(bottom, top - 1, -1):
        img.set(cx, y, pal[2])
        img.set(cx + 1, y, pal[1])
        if wobble and rng.random() < 0.15:
            cx += rng.choice((-1, 1))
            cx = max(5, min(9, cx))
    if leaves:
        ly = rng.randrange(top + 2, bottom - 1)
        for k in range(3):
            img.set(cx - 1 - k, ly - k // 2, pal[3] if k < 2 else pal[2])
            img.set(cx + 2 + k, ly + 1 - k // 2, pal[2] if k < 2 else pal[1])
    return cx


def disc(img, cx, cy, r, pal, ring=None):
    """Bevelled round head (lit top-left)."""
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            d = math.hypot(x - cx, y - cy)
            if d <= r + 0.3:
                c = pal[3] if (x - cx) + (y - cy) < -r * 0.6 else pal[1] if (x - cx) + (y - cy) > r * 0.6 else pal[2]
                img.set(x, y, c)
    if ring:
        img.set(cx, cy, ring)


def flower(rng, stem_pal, petal, center, shape="round"):
    img = Img()
    top = {"tall": 4, "cluster": 5}.get(shape, 6)
    cx = stem(img, rng, stem_pal, top=top + 2)
    hx, hy = cx, top
    if shape == "round":                 # poppy-like cup of petals
        disc(img, hx, hy, 2, petal, center)
    elif shape == "daisy":               # petals around a centre
        for dx, dy in ((0, -2), (2, 0), (0, 2), (-2, 0), (1, -1), (1, 1), (-1, 1), (-1, -1)):
            img.set(hx + dx, hy + dy, petal[3] if dy < 0 else petal[2])
        img.set(hx, hy, center)
        img.set(hx + 1, hy, center)
    elif shape == "cluster":             # ball of tiny florets
        for dy in range(-3, 3):
            for dx in range(-3, 3):
                if dx * dx + dy * dy <= 7 and rng.random() < 0.85:
                    img.set(hx + dx, hy + dy, petal[rng.choice((1, 2, 3, 4))])
    elif shape == "tulip":               # closed cup with pointed tips
        for dy in range(0, 4):
            for dx in range(-2, 2):
                img.set(hx + dx, hy + dy - 1, petal[3] if dx < 0 else petal[1])
        for dx in (-2, 0):
            img.set(hx + dx, hy - 2, petal[4])
    elif shape == "bells":               # drooping bells along a bent stem
        for k, (bx, by) in enumerate(((hx - 3, hy + 1), (hx - 1, hy - 1), (hx + 2, hy), (hx + 3, hy + 3))):
            img.set(bx, by, petal[4])
            img.set(bx, by + 1, petal[2])
            img.set(bx + 1, by + 1, petal[1])
    elif shape == "star":                # spiky open flower
        for a in range(6):
            ang = a * math.pi / 3 + 0.3
            for r in (1, 2):
                img.set(round(hx + math.cos(ang) * r), round(hy + math.sin(ang) * r),
                        petal[3] if r == 1 else petal[2])
        img.set(hx, hy, center)
    elif shape == "spiky":               # cornflower-like fringe
        for a in range(10):
            ang = a * math.pi / 5
            img.set(round(hx + math.cos(ang) * 2.4), round(hy + math.sin(ang) * 2.4), petal[3])
        disc(img, hx, hy, 1, petal, center)
    elif shape == "flame":               # torchflower: upward tongues
        for dy in range(4):
            w = 2 - dy // 2
            for dx in range(-w, w + 1):
                img.set(hx + dx, hy + 1 - dy, petal[4 - min(3, dy)])
        img.set(hx, hy + 2, center)
    elif shape == "eye":                 # eyeblossom: petals around a dark "eye"
        disc(img, hx, hy, 2, petal)
        img.set(hx, hy, center)
        img.set(hx + 1, hy, center)
    elif shape == "small":               # bluet: several tiny white flowers
        for bx, by in ((hx - 2, hy + 1), (hx + 1, hy), (hx, hy + 3), (hx + 3, hy + 2)):
            for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
                img.set(bx + dx, by + dy, petal[3])
            img.set(bx, by, center)
    return img


def tall_flower(rng, stem_pal, petal, center, part, shape="round"):
    img = Img()
    if part == "bottom":
        stem(img, rng, stem_pal, top=0, bottom=15)
        for y in (4, 9):
            for k in range(3):
                img.set(5 - k, y + k // 2, stem_pal[3])
                img.set(10 + k, y + 1 + k // 2, stem_pal[2])
        return img
    stem(img, rng, stem_pal, top=8, bottom=15, leaves=False)
    if shape == "bush":                  # lilac/rose bush/peony: clusters of blooms
        for bx, by in ((5, 4), (9, 3), (7, 7), (11, 7), (4, 8)):
            disc(img, bx, by, 2, petal, center)
    elif shape == "pitcher":
        for y in range(2, 10):
            w = 2 + (y > 4)
            for x in range(8 - w, 8 + w):
                img.set(x, y, petal[3] if x < 8 else petal[1])
        for x in range(5, 11):
            img.set(x, 2, center)
    else:                                # sunflower stalk top
        disc(img, 8, 5, 3, petal, center)
    return img


def grass_cross(rng, pal, height=11, blades=7, top_half=False):
    """Grass/fern tufts (greyscale for tinting)."""
    img = Img()
    for k in range(blades):
        x = rng.randrange(2, 14)
        h = rng.randrange(height - 4, height + 1)
        lean = rng.choice((-1, 0, 1))
        for i in range(h):
            y = N - 1 - i if not top_half else N - 1 - i
            xx = x + (lean * i) // 5
            img.set(xx, y, pal[3] if i > h - 3 else pal[2] if i > h // 2 else pal[1])
    return img


def fern(rng, pal, top=2):
    img = Img()
    for k, x in enumerate((4, 8, 11)):
        for y in range(top + k, N):
            img.set(x, y, pal[2])
            if (y - top) % 2 == 0:
                img.set(x - 1, y, pal[3])
                img.set(x + 1, y + 1, pal[1])
    return img


def mushroom(rng, cap, stem_pal, spots=None, flat=False):
    img = Img()
    for y in range(9, 15):
        img.set(7, y, stem_pal[3])
        img.set(8, y, stem_pal[2])
    rows = [(5, 2), (6, 3), (7, 4), (8, 4)] if not flat else [(7, 4), (8, 5), (9, 5)]
    for y, w in rows:
        for x in range(8 - w, 8 + w):
            img.set(x, y, cap[3] if y == rows[0][0] else cap[2] if x < 8 else cap[1])
    if spots:
        for x, y in ((6, 6), (9, 7), (5, 8)):
            img.set(x, y, spots)
    return img


def mushroom_block(rng, cap, spots=None):
    img = mottled(rng, cap, 1, 3)
    if spots:
        for k in range(6):
            x, y = (k % 3) * 5 + rng.randrange(1, 3), (k // 3) * 8 + rng.randrange(1, 5)
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                img.set(x + dx, y + dy, spots)
    return img


def crop(rng, stage, stages, stalk, produce=None, produce_at=None, height_full=14):
    """Crop rows: several stalks that grow taller each stage; the final stage(s)
    show the produce (grain heads, roots, berries...)."""
    img = Img()
    grow = (stage + 1) / stages
    h = max(2, int(height_full * grow))
    for x in (2, 5, 9, 12):
        hh = h - rng.randrange(0, 2)
        for i in range(hh):
            y = N - 1 - i
            img.set(x, y, stalk[2] if i % 3 else stalk[3])
            if i > 2 and i % 3 == 0:
                img.set(x + 1, y - 1, stalk[1])
        if produce and stage >= (produce_at if produce_at is not None else stages - 1):
            top = N - hh
            for k in range(3):
                img.set(x, top + k, produce[3] if k == 0 else produce[2])
                img.set(x + 1, top + k + 1, produce[1])
    return img


def root_crop(rng, stage, stages, leaf, root):
    """Leafy tops; the last stage shows the root peeking out (carrot/potato/beet)."""
    img = Img()
    h = 3 + int(7 * (stage + 1) / stages)
    for x in (3, 7, 11):
        for i in range(h):
            img.set(x + (i % 2), N - 3 - i, leaf[2 if i % 2 else 3])
        if stage == stages - 1:
            img.set(x, N - 2, root[3])
            img.set(x + 1, N - 2, root[2])
            img.set(x, N - 1, root[1])
    return img


def gourd_side(rng, pal):
    """Pumpkin/melon side: vertical ribs with lit and shaded edges."""
    img = Img()
    for x in range(N):
        k = x % 4
        for y in range(N):
            img.set(x, y, (pal[1], pal[3], pal[2], pal[2])[k])
    speckle(img, rng, [pal[4]], 6)
    for x in range(N):
        img.set(x, 0, pal[0])
        img.set(x, N - 1, pal[0])
    return img


def gourd_top(rng, pal, stalk):
    img = Img()
    for y in range(N):
        for x in range(N):
            d = max(abs(x - 7.5), abs(y - 7.5))
            img.set(x, y, pal[3] if int(d) % 3 == 0 else pal[2])
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            img.set(7 + dx, 7 + dy, stalk[2])
    img.set(7, 7, stalk[4])
    return img


def carved_face(base, pal, glow=None):
    """An original carved face (round eyes, wide wavy grin)."""
    img = base.copy()
    inside = glow or pal[0]
    for x, y in ((3, 4), (4, 4), (3, 5), (4, 5), (11, 4), (12, 4), (11, 5), (12, 5)):
        img.set(x, y, inside)
    img.set(7, 7, inside)
    img.set(8, 7, inside)
    for x in range(3, 13):
        y = 10 + (1 if x % 4 in (1, 2) else 0)
        img.set(x, y, inside)
        img.set(x, y + 1, inside)
    return img


def cactus_side(rng, pal):
    img = Img()
    for x in range(1, 15):
        for y in range(N):
            img.set(x, y, pal[2] if x % 4 else pal[1])
            if x % 4 == 1:
                img.set(x, y, pal[3])
    for k in range(10):  # spines
        x, y = rng.randrange(2, 14), rng.randrange(N)
        img.set(x, y, (230, 230, 200, 255))
    return img


def cactus_top(rng, pal):
    img = Img()
    for y in range(1, 15):
        for x in range(1, 15):
            d = max(abs(x - 7.5), abs(y - 7.5))
            img.set(x, y, pal[3] if int(d) % 2 else pal[2])
    frame(img, pal[3], pal[1], inset=1)
    return img


def cane(rng, pal):
    img = Img()
    for x in (3, 8, 12):
        for y in range(N):
            img.set(x, y, pal[3] if y % 5 else pal[1])
            img.set(x + 1, y, pal[2] if y % 5 else pal[0])
        ly = rng.randrange(N)
        img.set(x - 1, ly, pal[3])
        img.set(x - 2, ly - 1, pal[2])
    return img


def vine(rng, pal, density=26):
    img = Img()
    for k in range(3):
        x = rng.randrange(N)
        for y in range(N):
            img.set(x, y, pal[1])
            if rng.random() < 0.5:
                x += rng.choice((-1, 1))
    for _ in range(density):
        x, y = rng.randrange(N), rng.randrange(N)
        img.set(x, y, pal[3])
        img.set(x + 1, y, pal[2])
    return img


def lily_pad(rng, pal):
    img = Img()
    for y in range(N):
        for x in range(N):
            d = math.hypot(x - 7.5, y - 7.5)
            notch = x > 7 and abs(y - 7.5) < (x - 7) * 0.3
            if d < 7 and not notch:
                img.set(x, y, pal[3] if d < 2 else pal[2] if (x + y) % 5 else pal[1])
    return img


def bundle(rng, pal, band):
    """Bound bundle (hay/dried kelp sides): vertical strands with two binding bands."""
    img = Img()
    for x in range(N):
        for y in range(N):
            img.set(x, y, pal[[1, 3, 2, 2, 3][x % 5]])
    speckle(img, rng, [pal[4], pal[0]], 10)
    for y in (3, 12):
        for x in range(N):
            img.set(x, y, band[2] if x % 2 else band[3])
    return img


def bundle_top(rng, pal):
    img = Img()
    for y in range(N):
        for x in range(N):
            img.set(x, y, pal[2 + ((x * 7 + y * 3) % 5 == 0)] if (x + y) % 3 else pal[1])
    frame(img, pal[3], pal[0])
    return img


def sponge(rng, pal, wet=False):
    img = mottled(rng, pal, 2, 3)
    for _ in range(14):  # pores
        x, y = rng.randrange(N), rng.randrange(N)
        img.set(x, y, pal[0])
        img.set(x + 1, y + 1, pal[4])
    if wet:
        for _ in range(5):
            img.set(rng.randrange(N), rng.randrange(N), (90, 140, 220, 255))
    return img


def ribbon(rng, pal, tip=False):
    """Kelp/seagrass blade."""
    img = Img()
    x = 7
    top = 3 if tip else 0
    for y in range(N - 1, top - 1, -1):
        img.set(x, y, pal[2])
        img.set(x + 1, y, pal[3])
        if y % 4 == 0:
            img.set(x - 1, y, pal[3])
            img.set(x + 2, y + 1, pal[1])
        if rng.random() < 0.25:
            x = max(5, min(9, x + rng.choice((-1, 1))))
    return img


def coral(rng, pal, shape):
    """Coral plant (branching), fan (spread fan) or block (porous)."""
    if shape == "block":
        img = cells(rng, pal, 10, pal[0], shades=(1, 2, 3), edge_width=0.6, grit=8)
        for _ in range(12):
            img.set(rng.randrange(N), rng.randrange(N), pal[4])
        return img
    img = Img()
    if shape == "fan":
        for a in range(-4, 5):
            ang = a * 0.28
            for r in range(2, 8):
                img.set(round(8 + math.sin(ang) * r), round(13 - math.cos(ang) * r),
                        pal[3] if r % 2 else pal[2])
        return img
    # branching plant
    def branch(x, y, depth):
        for i in range(3):
            img.set(x, y - i, pal[2] if i else pal[1])
        if depth > 0:
            branch(x - 2, y - 3, depth - 1)
            branch(x + 2, y - 3, depth - 1)
        else:
            img.set(x, y - 3, pal[4])
    branch(8, 15, 2)
    return img


def eggs(rng, pal, count, crack=0):
    img = Img()
    spots = [(4, 11), (10, 12), (7, 6), (11, 6)][:count]
    for ex, ey in spots:
        for dy in range(-2, 2):
            for dx in range(-1, 2):
                img.set(ex + dx, ey + dy, pal[3] if dx < 1 and dy < 0 else pal[2])
        img.set(ex + 1, ey - 1, (120, 160, 90, 255))
        for c in range(crack):
            img.set(ex - 1 + c, ey + (c % 2) - 1, pal[0])
    return img


def hanging(rng, pal, berries=None, lit=False, tip=False):
    """Hanging vines/roots growing down from the top edge."""
    img = Img()
    for x in (3, 7, 11):
        length = rng.randrange(10, 16) if not tip else rng.randrange(6, 10)
        xx = x
        for y in range(length):
            img.set(xx, y, pal[2] if y % 3 else pal[3])
            if rng.random() < 0.2:
                xx += rng.choice((-1, 1))
        if berries and rng.random() < 0.8:
            by = rng.randrange(3, max(4, length - 1))
            img.set(xx + 1, by, berries[4] if lit else berries[2])
            img.set(xx + 1, by + 1, berries[3] if lit else berries[1])
    return img


def animate(frames_fn, count):
    from .core import stack_frames
    return stack_frames([frames_fn(i) for i in range(count)])
