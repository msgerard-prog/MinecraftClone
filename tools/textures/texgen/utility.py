"""Painters for crafted/utility blocks: workstation faces, machine fronts, rails,
redstone parts, fluids, fire, crack overlays. Small "icon" helpers draw tools
and details on top of base materials."""
import math

from .core import CLEAR, N, Img, fbm, fill_rect, frame, line, mix, pick, rgba, stack_frames, value_noise
from .materials import bricks, cells, mottled, planks, polished, speckle

IRON = [(60, 60, 66, 255), (110, 110, 116, 255), (160, 160, 166, 255), (205, 205, 210, 255),
        (238, 238, 242, 255)]


def framed(base, pal):
    out = base.copy()
    frame(out, pal[4], pal[0])
    return out


def workbench_top(rng, wood, metal=IRON):
    """Crafting table top: planks with a 3x3 grid inlay."""
    img = planks(rng, wood)
    frame(img, wood[4], wood[0])
    for k in (5, 10):
        for i in range(2, 14):
            img.set(k, i, wood[0])
            img.set(i, k, wood[0])
    return img


def tool_icons(img, rng, tools, y0=3):
    """Hang small tool silhouettes on a face (saw, hammer, pick, axe, shears...)."""
    x = 2
    for t in tools:
        if t == "saw":
            for i in range(5):
                img.set(x + i, y0 + 2, IRON[3])
                img.set(x + i, y0 + 3, IRON[1] if i % 2 else IRON[2])
            img.set(x - 1, y0 + 2, (110, 70, 40, 255))
            img.set(x - 1, y0 + 3, (110, 70, 40, 255))
            x += 7
        elif t == "hammer":
            for i in range(5):
                img.set(x + 1, y0 + i, (120, 80, 45, 255))
            for i in range(3):
                img.set(x + i, y0, IRON[3])
            x += 5
        elif t == "pick":
            for i in range(5):
                img.set(x + 2, y0 + i, (120, 80, 45, 255))
            for i in range(5):
                img.set(x + i, y0 + (0 if 0 < i < 4 else 1), IRON[3])
            x += 6
        elif t == "arrow":
            for i in range(6):
                img.set(x + i, y0 + i, (140, 100, 60, 255))
            img.set(x, y0, (240, 240, 240, 255))
            img.set(x + 1, y0, (240, 240, 240, 255))
            img.set(x + 5, y0 + 5, IRON[3])
            x += 7
    return img


def furnace_front(rng, stone_pal, lit, opening=(4, 7, 11, 13), glow=None):
    img = bricks(rng, stone_pal, stone_pal[0], rows=4, brick_w=8, grit=6)
    frame(img, stone_pal[4], stone_pal[0])
    x0, y0, x1, y1 = opening
    glow = glow or [(255, 200, 60, 255), (250, 120, 30, 255), (190, 50, 20, 255)]
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if lit:
                img.set(x, y, glow[(y - y0 + (x % 2)) % 3] if y > y0 else glow[0])
            else:
                img.set(x, y, (20, 20, 22, 255))
    for x in range(x0 - 1, x1 + 2):
        img.set(x, y0 - 1, stone_pal[0])
        img.set(x, y1 + 1, stone_pal[4])
    return img


def stone_top(rng, pal):
    img = polished(rng, pal, noise=4)
    return img


def rail(rng, ties, metal, powered=None, corner=False):
    """Rail: wooden sleepers with two metal rails (straight or curved)."""
    img = Img()
    for y in range(0, N, 3):  # sleepers
        for x in range(2, 14):
            img.set(x, y, ties[2])
            img.set(x, y + 1, ties[1])
    if corner:
        for a in range(0, 90, 3):
            t = math.radians(a)
            for r, c in ((11, metal[3]), (5, metal[3])):
                img.set(round(15 - math.cos(t) * r), round(15 - math.sin(t) * r), c)
    else:
        for y in range(N):
            for x, c in ((3, metal[3]), (4, metal[1]), (11, metal[3]), (12, metal[1])):
                img.set(x, y, c)
    if powered:
        for y in range(1, N, 4):
            img.set(7, y, powered)
            img.set(8, y, powered)
    return img


def dust(kind, pal):
    """Redstone dust (greyscale; tinted by power level in game)."""
    img = Img()
    if kind == "dot":
        for y in range(5, 11):
            for x in range(5, 11):
                if abs(x - 7.5) + abs(y - 7.5) < 4:
                    img.set(x, y, pal[3] if (x + y) % 3 else pal[4])
    else:
        for y in range(N):
            for x in (6, 7, 8, 9):
                img.set(x, y, pal[3] if (x + y) % 3 else pal[4])
            if kind == "line1":
                img = img  # second line variant: same shape, offset noise
        if kind == "line1":
            rot = Img()
            for y in range(N):
                for x in range(N):
                    rot.set(y, x, img.get(x, y))
            img = rot
    return img


def fluid_frames(rng, pal, frames, flow=False, alpha=255):
    """Animated fluid: tileable noise scrolled per frame (down for flowing)."""
    n1 = value_noise(rng, 4, 16, 16)
    n2 = value_noise(rng, 8, 16, 16)
    out = []
    for f in range(frames):
        img = Img()
        for y in range(N):
            for x in range(N):
                oy = (y - f) % N if flow else y
                ox = x if flow else (x + f // 2) % N
                v = n1[oy][ox] * 0.6 + n2[(oy + f) % N][(ox - f) % N] * 0.4
                v += 0.08 * math.sin((x + y + f) * 0.8)
                img.set(x, y, rgba(pick(pal, v, 0, len(pal) - 1), alpha))
        out.append(img)
    return stack_frames(out)


def fire_frames(rng, pal, frames=8):
    out = []
    for f in range(frames):
        img = Img()
        n = value_noise(rng, 4, 16, 16)
        for x in range(N):
            h = 7 + int(6 * n[(f * 2) % N][x]) + (2 if x % 5 == 2 else 0)
            for i in range(h):
                y = N - 1 - i
                t = i / h
                c = pal[4] if t < 0.25 else pal[3] if t < 0.55 else pal[2] if t < 0.85 else pal[1]
                if rng.random() < 0.92:
                    img.set(x, y, c)
        out.append(img)
    return stack_frames(out)


def crack_stage(stage):
    """Block-breaking overlay: more crack lines each stage (grey with alpha)."""
    import random
    rng = random.Random("destroy")
    img = Img()
    lines = []
    for _ in range(14):
        x, y = rng.randrange(N), rng.randrange(N)
        seg = []
        for _ in range(rng.randrange(4, 9)):
            seg.append((x, y))
            x += rng.choice((-1, 0, 1))
            y += rng.choice((-1, 0, 1, 1))
        lines.append(seg)
    for seg in lines[: 1 + stage * 13 // 9]:
        for x, y in seg:
            img.set(x, y, (20, 20, 20, 200))
            if img.get(x + 1, y + 1)[3] == 0:
                img.set(x + 1, y + 1, (60, 60, 60, 120))
    return img


def window_grid(img, pal, x0, y0, x1, y1, step=3):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if (x - x0) % step == 0 or (y - y0) % step == 0:
                img.set(x, y, pal[0])
            else:
                img.set(x, y, CLEAR)
    return img


def lamp(rng, pal, lit):
    img = cells(rng, pal, 6, pal[0], shades=(2, 3, 3) if lit else (1, 2), edge_width=0.6, grit=0)
    frame(img, pal[4] if lit else pal[2], pal[0])
    return img


def books(rng, wood, colors):
    """Bookshelf face: two shelves of book spines between plank rails."""
    img = planks(rng, wood, boards=4)
    for shelf_y in (1, 9):
        x = 1
        while x < 15:
            w = rng.choice((1, 2, 2))
            c = rng.choice(colors)
            h = rng.randrange(5, 7)
            for dx in range(w):
                for y in range(shelf_y + (6 - h), shelf_y + 6):
                    if x + dx < 15:
                        img.set(x + dx, y, c if dx == 0 else mix(c, (0, 0, 0, 255), 0.25))
            x += w
    return img
