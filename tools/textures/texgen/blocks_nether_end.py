"""Batch 5: Nether and End blocks. Base colours are our own picks."""
import math
import random

from . import materials as M
from . import plants as P
from .core import N, Img, frame, hexc, mix, ramp, scale, stack_frames
from .registry import add, animated


def reg(name, fn, mcmeta=None):
    add(name, fn, "nether & end", mcmeta)


NETHERRACK = ramp(hexc("6e3434"), spread=0.28)
NETHER_BRICK = ramp(hexc("2e171b"), spread=0.25)
RED_NETHER_BRICK = ramp(hexc("4a0c10"), spread=0.28)
WART = ramp(hexc("7a0a08"), spread=0.3)
WARPED_WART = ramp(hexc("177a7a"), spread=0.3)
SOUL_SAND = ramp(hexc("513e32"))
SOUL_SOIL = ramp(hexc("4b392e"))
GLOWSTONE = ramp(hexc("d8b878"), spread=0.32)
BASALT = ramp(hexc("4a494e"), spread=0.25)
BLACKSTONE = ramp(hexc("2c252b"), spread=0.25)
GOLD = ramp(hexc("f2c23a"), spread=0.3)
CRIMSON = ramp(hexc("8a2222"), spread=0.3)
WARPED = ramp(hexc("2b7266"), spread=0.3)
SHROOM = ramp(hexc("f0924a"), spread=0.3)
QUARTZ = ramp(hexc("ece8e2"), spread=0.15)
DEBRIS = ramp(hexc("5a3e36"), spread=0.25)

# --- Netherrack & bricks ------------------------------------------------------------------
reg("netherrack", lambda r: M.pits(M.mottled(r, NETHERRACK), r, NETHERRACK, 8))
reg("nether_bricks", lambda r: M.bricks(r, NETHER_BRICK, scale(NETHER_BRICK[0], 0.6), rows=4, brick_w=8))
reg("cracked_nether_bricks",
    lambda r: M.cracked(M.bricks(r, NETHER_BRICK, scale(NETHER_BRICK[0], 0.6)), r, NETHER_BRICK, 3))
reg("chiseled_nether_bricks", lambda r: M.chiseled(r, NETHER_BRICK, "cross"))
reg("red_nether_bricks", lambda r: M.bricks(r, RED_NETHER_BRICK, scale(RED_NETHER_BRICK[0], 0.6)))
reg("nether_wart_block", lambda r: M.speckle(M.mottled(r, WART), r, [WART[4], WART[0]], 18))
reg("warped_wart_block", lambda r: M.speckle(M.mottled(r, WARPED_WART), r, [WARPED_WART[4], WARPED_WART[0]], 18))


def _soul(rng, pal):
    """Soul sand/soil: dark hollows (bevelled inwards) in grainy ground."""
    img = M.grainy(rng, pal, dark=8, bright=6)
    for k in range(5):
        x, y = (k % 3) * 5 + rng.randrange(1, 3), (k // 3) * 8 + rng.randrange(1, 4)
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 1)):
            img.set(x + dx, y + dy, pal[0])
        img.set(x + 1, y + 2, pal[4])
        img.set(x + 2, y + 2, pal[3])
    return img


reg("soul_sand", lambda r: _soul(r, SOUL_SAND))
reg("soul_soil", lambda r: M.speckle(M.mottled(r, SOUL_SOIL), r, [SOUL_SOIL[0], SOUL_SOIL[4]], 16))


def _glowstone(rng):
    """Gold base with bevelled bright crystal clusters and a few dark flecks."""
    img = M.mottled(rng, GLOWSTONE, 1, 3)
    for k in range(9):
        x = (k % 3) * 5 + rng.randrange(0, 3)
        y = (k // 3) * 5 + rng.randrange(0, 3)
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 1), (1, 2)):
            img.set(x + dx, y + dy, GLOWSTONE[3])
        img.set(x, y, (255, 246, 210, 255))          # lit corner
        img.set(x + 2, y + 2, GLOWSTONE[1])          # shadow corner
        img.set(x + 1, y + 1, GLOWSTONE[4])
    M.speckle(img, rng, [GLOWSTONE[0]], 8)
    return img


reg("glowstone", _glowstone)


def _magma(rng):
    base = M.cells(random.Random("magma/layout"), ramp(hexc("3a1e14")), 9, hexc("1a0c08"),
                   shades=(1, 2, 2, 3), edge_width=0.8, grit=0)
    glow = [hexc("8a2a0a"), hexc("d0501a"), hexc("f8902a"), hexc("ffd060")]
    frames = []
    for f in range(3):
        img = base.copy()
        for y in range(N):
            for x in range(N):
                if img.get(x, y) == hexc("1a0c08"):  # cracks between rocks glow
                    img.set(x, y, glow[(x + y + f) % 3 + (1 if (x * y + f) % 7 == 0 else 0)])
        frames.append(img)
    return stack_frames(frames)


reg("magma", _magma, animated(8))


# --- Basalt / blackstone ---------------------------------------------------------------------

def _columns(rng, pal):
    img = Img()
    widths = [3, 4, 3, 3, 3]
    x = 0
    for w in widths:
        for k in range(w):
            for y in range(N):
                if x + k < N:
                    img.set(x + k, y, pal[0] if k == 0 else pal[3] if k == 1 else pal[2])
        x += w
    M.speckle(img, rng, [pal[1], pal[4]], 12)
    return img


reg("basalt_side", lambda r: _columns(r, BASALT))
reg("basalt_top", lambda r: M.cells(r, BASALT, 5, BASALT[0], shades=(1, 2, 3)))
reg("polished_basalt_side", lambda r: M.polished(r, BASALT))
reg("polished_basalt_top", lambda r: M.chiseled(r, BASALT, "ring"))
reg("smooth_basalt", lambda r: M.speckle(M.mottled(r, BASALT, 1, 2), r, [BASALT[3]], 8))
reg("blackstone", lambda r: M.pits(M.mottled(r, BLACKSTONE), r, BLACKSTONE, 6))
reg("blackstone_top", lambda r: M.cells(r, BLACKSTONE, 7, BLACKSTONE[0], shades=(1, 2, 3)))
reg("polished_blackstone", lambda r: M.polished(r, BLACKSTONE))
reg("polished_blackstone_bricks",
    lambda r: M.bricks(r, BLACKSTONE, scale(BLACKSTONE[0], 0.6), rows=4, brick_w=8))
reg("cracked_polished_blackstone_bricks",
    lambda r: M.cracked(M.bricks(r, BLACKSTONE, scale(BLACKSTONE[0], 0.6)), r, BLACKSTONE, 3))
reg("chiseled_polished_blackstone", lambda r: M.chiseled(r, BLACKSTONE, "sun"))
reg("gilded_blackstone", lambda r: M.ore(M.pits(M.mottled(random.Random("gilded/base"), BLACKSTONE), r,
                                                BLACKSTONE, 4), r, GOLD, 6, "nugget"))

# --- Nylium, fungi, roots, vines, wart ------------------------------------------------------
reg("crimson_nylium", lambda r: M.blades(r, CRIMSON, count=22))
reg("warped_nylium", lambda r: M.blades(r, WARPED, count=22))
reg("crimson_nylium_side",
    lambda r: M.side_with_cap(M.pits(M.mottled(random.Random("nyl/c"), NETHERRACK), r, NETHERRACK, 6), r, CRIMSON)[0])
reg("warped_nylium_side",
    lambda r: M.side_with_cap(M.pits(M.mottled(random.Random("nyl/w"), NETHERRACK), r, NETHERRACK, 6), r, WARPED)[0])
reg("crimson_fungus", lambda r: P.mushroom(r, CRIMSON, ramp(hexc("d8a070")), spots=hexc("f0c040")))
reg("warped_fungus", lambda r: P.mushroom(r, WARPED, ramp(hexc("d8a070")), spots=hexc("f08a30")))
reg("crimson_roots", lambda r: P.grass_cross(r, CRIMSON, height=11))
reg("warped_roots", lambda r: P.grass_cross(r, WARPED, height=11))
reg("crimson_roots_pot", lambda r: P.grass_cross(r, CRIMSON, height=8, blades=5))
reg("warped_roots_pot", lambda r: P.grass_cross(r, WARPED, height=8, blades=5))
reg("nether_sprouts", lambda r: P.grass_cross(r, WARPED, height=6, blades=6))
reg("weeping_vines", lambda r: P.hanging(r, CRIMSON, tip=True))
reg("weeping_vines_plant", lambda r: P.hanging(r, CRIMSON))


def _flip(img):
    out = Img(img.w, img.h)
    for y in range(img.h):
        for x in range(img.w):
            out.set(x, img.h - 1 - y, img.get(x, y))
    return out


reg("twisting_vines", lambda r: _flip(P.hanging(r, WARPED, tip=True)))
reg("twisting_vines_plant", lambda r: _flip(P.hanging(r, WARPED)))
reg("shroomlight", lambda r: M.speckle(M.cells(r, SHROOM, 9, SHROOM[1], shades=(2, 3, 3), edge_width=0.6),
                                      r, [(255, 230, 170, 255)], 10))
WART_RED = ramp(hexc("a01a1a"), spread=0.3)
for s in range(3):
    reg(f"nether_wart_stage{s}", lambda r, s=s: P.crop(r, s, 3, WART_RED, WART_RED if s == 2 else None, 2, 8 + s * 2))

# --- Respawn anchor, lodestone ------------------------------------------------------------
CRYING = ramp(hexc("8a2be2"), spread=0.3)
OBSIDIAN = ramp(hexc("1b1528"), spread=0.22)


def _anchor_side(rng, charge):
    img = M.speckle(M.mottled(random.Random("anchor/side"), OBSIDIAN), rng, [OBSIDIAN[4]], 6)
    frame(img, OBSIDIAN[4], OBSIDIAN[0])
    for k in range(4):  # one glowing bar per charge level
        x = 3 + k * 3
        lit = k < charge
        for y in range(4, 12):
            img.set(x, y, CRYING[3] if lit else OBSIDIAN[0])
            img.set(x + 1, y, CRYING[2] if lit else OBSIDIAN[1])
    return img


def _anchor_top(rng, on):
    img = M.polished(random.Random("anchor/top"), OBSIDIAN, noise=3)
    for y in range(5, 11):
        for x in range(5, 11):
            d = max(abs(x - 7.5), abs(y - 7.5))
            img.set(x, y, (CRYING[4] if d < 1.5 else CRYING[2]) if on else OBSIDIAN[0])
    return img


for c in range(5):
    reg(f"respawn_anchor_side{c}", lambda r, c=c: _anchor_side(r, c))
reg("respawn_anchor_top", lambda r: _anchor_top(r, True))
reg("respawn_anchor_top_off", lambda r: _anchor_top(r, False))
reg("respawn_anchor_bottom", lambda r: M.polished(r, OBSIDIAN))
LODE = ramp(hexc("8a8a8e"), spread=0.25)
reg("lodestone_side", lambda r: M.bricks(r, LODE, scale(LODE[0], 0.7), rows=4, brick_w=8))


def _lode_top(rng):
    img = M.polished(rng, LODE)
    for i in range(3, 13):
        img.set(i, 7, hexc("5a5a62"))
        img.set(7, i, hexc("5a5a62"))
    img.set(7, 7, hexc("d8d8e0"))
    return img


reg("lodestone_top", _lode_top)


# --- Portal, ores, debris -------------------------------------------------------------------
def _portal(rng):
    purple = ramp(hexc("7a2ad0"), spread=0.35)
    frames = []
    for f in range(8):
        img = Img()
        for y in range(N):
            for x in range(N):
                a = math.atan2(y - 7.5, x - 7.5)
                r = math.hypot(x - 7.5, y - 7.5)
                v = math.sin(r * 0.9 - a * 2 + f * math.pi / 4)
                img.set(x, y, purple[1 + int((v + 1) * 1.49)][:3] + (200,))
        frames.append(img)
    return stack_frames(frames)


reg("nether_portal", _portal, animated(2))
reg("nether_gold_ore", lambda r: M.ore(M.pits(M.mottled(random.Random("ngo/base"), NETHERRACK), r, NETHERRACK, 4),
                                       r, GOLD, 7, "nugget"))
reg("nether_quartz_ore", lambda r: M.ore(M.pits(M.mottled(random.Random("nqo/base"), NETHERRACK), r, NETHERRACK, 4),
                                         r, QUARTZ, 6, "vein"))


def _debris_side(rng):
    img = M.mottled(rng, DEBRIS)
    for y in range(N):  # swirling ridged lines
        for x in range(N):
            if (x + int(2 * math.sin(y * 0.8))) % 5 == 0:
                img.set(x, y, DEBRIS[0])
            elif (x + int(2 * math.sin(y * 0.8))) % 5 == 1:
                img.set(x, y, DEBRIS[3])
    frame(img, DEBRIS[4], DEBRIS[0])
    return img


reg("ancient_debris_side", _debris_side)
reg("ancient_debris_top", lambda r: M.log_end(r, DEBRIS, ramp(hexc("3a2622"))))

# --- End ------------------------------------------------------------------------------------
END_STONE = ramp(hexc("dbdc9a"), spread=0.22)
PURPUR = ramp(hexc("a77ba7"), spread=0.25)
CHORUS = ramp(hexc("5d395d"), spread=0.3)
reg("end_stone", lambda r: M.pits(M.speckle(M.mottled(r, END_STONE), r, [END_STONE[0]], 10), r, END_STONE, 6))
reg("end_stone_bricks", lambda r: M.bricks(r, END_STONE, scale(END_STONE[0], 0.75), rows=4, brick_w=8))
reg("purpur_block", lambda r: M.tiles(r, PURPUR, scale(PURPUR[0], 0.7), size=8))


def _purpur_pillar(rng):
    img = M.mottled(rng, PURPUR, 2, 3)
    for y in range(N):
        for x in (2, 7, 12):
            img.set(x, y, PURPUR[4])
            img.set(x + 2, y, PURPUR[1])
    return img


reg("purpur_pillar", _purpur_pillar)
reg("purpur_pillar_top", lambda r: M.chiseled(r, PURPUR, "diamond"))


def _end_rod(rng):
    img = Img()
    for y in range(1, 15):
        img.set(7, y, (250, 248, 240, 255))
        img.set(8, y, (220, 214, 200, 255))
    for x in range(6, 10):
        img.set(x, 15, hexc("8a6a4a"))
    return img


reg("end_rod", _end_rod)
reg("chorus_plant", lambda r: M.speckle(M.cells(r, CHORUS, 8, CHORUS[0], shades=(1, 2, 3)), r, [CHORUS[4]], 8))
reg("chorus_flower", lambda r: M.speckle(M.cells(r, ramp(hexc("9a6ab0"), spread=0.3), 6, CHORUS[1],
                                                 shades=(2, 3, 3)), r, [(230, 200, 240, 255)], 10))
reg("chorus_flower_dead", lambda r: M.cells(r, ramp(hexc("6a5a6a")), 6, CHORUS[0], shades=(1, 2, 3)))
reg("dragon_egg", lambda r: M.speckle(M.mottled(r, ramp(hexc("120c16"), spread=0.15)), r,
                                      [hexc("7a3ab0"), hexc("4a2a70")], 16))
FRAME_TOP = ramp(hexc("a8c0a0"), spread=0.25)


def _portal_frame(rng, face):
    img = M.speckle(M.mottled(rng, END_STONE), rng, [END_STONE[0]], 8)
    if face == "top":
        img = M.polished(rng, FRAME_TOP)
        for y in range(4, 12):
            for x in range(4, 12):
                img.set(x, y, hexc("1a2a2a"))
    elif face == "side":
        for x in range(N):
            img.set(x, 3, hexc("2a6a6a"))
            img.set(x, 4, hexc("4a9a90"))
    else:  # eye
        img = Img()
        for y in range(4, 12):
            for x in range(4, 12):
                d = math.hypot(x - 7.5, y - 7.5)
                if d < 3.8:
                    img.set(x, y, hexc("1a6a4a") if d > 1.6 else hexc("0a2a1a"))
        img.set(6, 6, hexc("a0f0c0"))
    return img


reg("end_portal_frame_top", lambda r: _portal_frame(r, "top"))
reg("end_portal_frame_side", lambda r: _portal_frame(r, "side"))
reg("end_portal_frame_eye", lambda r: _portal_frame(r, "eye"))
