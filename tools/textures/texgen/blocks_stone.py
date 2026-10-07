"""Batch 1: stone family, terrain, soils, snow/ice, ores, minerals.

Base colours are our own picks from descriptions of each material.
"""
import math

from . import materials as M
from .core import (CLEAR, N, Img, frame, grey, hexc, mix, pick, ramp, rgba, scale, shift,
                   stack_frames, tint)
from .registry import add

PLAINS_TINT = (0x91, 0xBD, 0x59)

# --- Palettes (dark -> light) ----------------------------------------------------------
STONE = [(74, 75, 82, 255), (96, 97, 103, 255), (116, 116, 120, 255), (134, 133, 135, 255),
         (156, 154, 150, 255)]
COBBLE = [(58, 58, 66, 255), (88, 88, 94, 255), (112, 111, 114, 255), (134, 132, 132, 255),
          (162, 159, 152, 255)]
MORTAR = (44, 44, 52, 255)
DIRT = [(74, 50, 36, 255), (98, 68, 46, 255), (122, 86, 58, 255), (143, 104, 70, 255),
        (166, 124, 86, 255)]
PEBBLE = [(98, 96, 100, 255), (136, 134, 132, 255)]
GRASS_GREY = [grey(98), grey(126), grey(152), grey(178), grey(206)]
GRASS_GREEN = [tint(c, PLAINS_TINT) for c in GRASS_GREY]
SAND = [(196, 178, 122, 255), (214, 198, 144, 255), (226, 212, 160, 255), (236, 225, 176, 255),
        (248, 240, 200, 255)]
BEDROCK = [(22, 22, 26, 255), (48, 48, 54, 255), (80, 80, 86, 255), (116, 116, 120, 255),
           (160, 160, 164, 255)]
MOSS = ramp(hexc("5f7a2c"))
GRANITE = ramp(hexc("9c6a58"))
DIORITE = ramp(hexc("bdbbbb"), spread=0.22)
ANDESITE = ramp(hexc("838584"))
DEEPSLATE = ramp(hexc("4e4e56"), spread=0.22)
TUFF = ramp(hexc("6c6d66"))
CALCITE = ramp(hexc("dcdedc"), spread=0.16)
DRIPSTONE = ramp(hexc("866b5c"))
BRICK = ramp(hexc("965444"))
BRICK_MORTAR = hexc("8e8a84")
SANDSTONE = ramp(hexc("d6cca0"), spread=0.18)
RED_SANDSTONE = ramp(hexc("b5611f"), spread=0.24)
RED_SAND = ramp(hexc("be6621"), spread=0.22)
MUD = ramp(hexc("3d3a3e"), spread=0.2)
PACKED_MUD = ramp(hexc("8e6a4f"))
MUD_BRICK = ramp(hexc("896750"))
OBSIDIAN = ramp(hexc("1b1528"), spread=0.22)
CRYING = ramp(hexc("8a2be2"), spread=0.3)
GRAVEL = ramp(hexc("857d7c"), spread=0.32)
CLAY = ramp(hexc("a0a6b3"), spread=0.2)
COARSE = ramp(hexc("775538"))
PODZOL = ramp(hexc("5c3f18"))
MYCELIUM = ramp(hexc("6f6369"))
MYCELIUM_SPORE = ramp(hexc("9a7fa6"))
PATH = ramp(hexc("8a6b3a"), spread=0.24)
FARMLAND = ramp(hexc("5d3d23"))
FARMLAND_WET = ramp(hexc("3b2412"))
SNOW = ramp(hexc("eef8f8"), spread=0.1)
ICE = ramp(hexc("8fb4f8"), spread=0.25)
PACKED_ICE = ramp(hexc("8bb2f6"), spread=0.22)
BLUE_ICE = ramp(hexc("74a5f5"), spread=0.25)
POWDER = ramp(hexc("f6fbfb"), spread=0.08)
ROOT = ramp(hexc("8a6a4a"))

ORE = {
    "coal": ramp(hexc("2e2e30"), spread=0.25),
    "iron": ramp(hexc("d4ab8e")),
    "copper": ramp(hexc("e07e4c")),
    "gold": ramp(hexc("f5d43c")),
    "redstone": ramp(hexc("e01414")),
    "emerald": ramp(hexc("2fd86b")),
    "lapis": ramp(hexc("2d55c8")),
    "diamond": ramp(hexc("5ae3dc")),
}
ORE_SHAPE = {"coal": "nugget", "iron": "nugget", "copper": "vein", "gold": "nugget",
             "redstone": "vein", "emerald": "gem", "lapis": "nugget", "diamond": "gem"}

AMETHYST = ramp(hexc("8a63c4"), spread=0.3)
QUARTZ = ramp(hexc("e8e6e1"), spread=0.14)


def reg(name, fn, mcmeta=None):
    add(name, fn, "stone & terrain", mcmeta)


# --- Stones ----------------------------------------------------------------------------

def _stone(rng):
    return M.stone(rng, STONE)


def _cobble(rng):
    return M.cells(rng, COBBLE, 8, MORTAR)


reg("stone", _stone)
reg("cobblestone", _cobble)
reg("mossy_cobblestone", lambda r: M.mossy(_cobble(r), r, MOSS, 0.38))
reg("smooth_stone", lambda r: M.polished(r, STONE, noise=4))
reg("smooth_stone_slab_side", lambda r: _slab_side(r, STONE))
reg("stone_bricks", lambda r: M.bricks(r, STONE, MORTAR, rows=4, brick_w=8))
reg("mossy_stone_bricks", lambda r: M.mossy(M.bricks(r, STONE, MORTAR), r, MOSS, 0.4))
reg("cracked_stone_bricks", lambda r: M.cracked(M.bricks(r, STONE, MORTAR), r, STONE, 3))
reg("chiseled_stone_bricks", lambda r: M.chiseled(r, STONE, "ring"))


def _slab_side(rng, pal):
    img = M.polished(rng, pal, noise=4)
    for x in range(N):  # seam between the two half-slabs
        img.set(x, 7, pal[0])
        img.set(x, 8, pal[4])
    return img


def _speckled(pal, dark_flecks, light_flecks, base_fn=None):
    def paint(rng):
        img = M.mottled(rng, pal)
        M.speckle(img, rng, dark_flecks, 14)
        M.speckle(img, rng, light_flecks, 10)
        return img
    return paint


reg("granite", _speckled(GRANITE, [GRANITE[0], hexc("6e3f33")], [GRANITE[4], hexc("d9b0a0")]))
reg("polished_granite", lambda r: M.polished(r, GRANITE))
reg("diorite", _speckled(DIORITE, [hexc("5c5c5e"), DIORITE[0]], [DIORITE[4]]))
reg("polished_diorite", lambda r: M.polished(r, DIORITE))
reg("andesite", _speckled(ANDESITE, [ANDESITE[0]], [ANDESITE[4], hexc("b8bab9")]))
reg("polished_andesite", lambda r: M.polished(r, ANDESITE))

# Deepslate: layered rock; top shows the layer ends as a swirl.
reg("deepslate", lambda r: M.pits(M.banded(r, DEEPSLATE, horizontal=False), r, DEEPSLATE, 4))
reg("deepslate_top", lambda r: _swirl(r, DEEPSLATE))
reg("cobbled_deepslate", lambda r: M.cells(r, DEEPSLATE, 9, scale(DEEPSLATE[0], 0.7)))
reg("polished_deepslate", lambda r: M.polished(r, DEEPSLATE))
reg("deepslate_bricks", lambda r: M.bricks(r, DEEPSLATE, scale(DEEPSLATE[0], 0.7), brick_w=8))
reg("cracked_deepslate_bricks",
    lambda r: M.cracked(M.bricks(r, DEEPSLATE, scale(DEEPSLATE[0], 0.7)), r, DEEPSLATE, 3))
reg("deepslate_tiles", lambda r: M.tiles(r, DEEPSLATE, scale(DEEPSLATE[0], 0.7), size=4))
reg("cracked_deepslate_tiles",
    lambda r: M.cracked(M.tiles(r, DEEPSLATE, scale(DEEPSLATE[0], 0.7), size=4), r, DEEPSLATE))
reg("chiseled_deepslate", lambda r: M.chiseled(r, DEEPSLATE, "steps"))

REINFORCED = ramp(hexc("4a5052"), spread=0.25)
SCULK_TEAL = ramp(hexc("0f8a8a"), spread=0.3)


def _reinforced(rng, face):
    img = M.tiles(rng, DEEPSLATE, scale(DEEPSLATE[0], 0.6), size=8)
    frame(img, REINFORCED[4], REINFORCED[0])
    frame(img, REINFORCED[3], REINFORCED[1], inset=1)
    if face == "top":
        for i in range(5, 11):
            img.set(i, 7, SCULK_TEAL[3])
            img.set(7, i, SCULK_TEAL[3])
        img.set(7, 7, SCULK_TEAL[4])
    elif face == "side":
        for y in range(3, 13):
            img.set(7, y, SCULK_TEAL[2])
            img.set(8, y, SCULK_TEAL[1])
    return img


for face in ("top", "side", "bottom"):
    reg(f"reinforced_deepslate_{face}", lambda r, f=face: _reinforced(r, f))


def _swirl(rng, pal):
    img = Img()
    c = 7.5
    for y in range(N):
        for x in range(N):
            a = math.atan2(y - c, x - c)
            r = math.hypot(x - c, y - c)
            v = (r * 0.9 + a * 1.3 + rng.uniform(-0.3, 0.3)) % 3
            img.set(x, y, pal[1] if v < 1 else pal[2] if v < 2 else pal[3])
    return M.pits(img, rng, pal, 4)


reg("tuff", lambda r: M.pits(M.speckle(M.mottled(r, TUFF), r, [TUFF[4]], 12), r, TUFF, 6))
reg("polished_tuff", lambda r: M.polished(r, TUFF))
reg("tuff_bricks", lambda r: M.bricks(r, TUFF, scale(TUFF[0], 0.7), brick_w=8))
reg("chiseled_tuff", lambda r: M.chiseled(r, TUFF, "diamond"))
reg("chiseled_tuff_top", lambda r: M.polished(r, TUFF, noise=2))
reg("chiseled_tuff_bricks", lambda r: M.chiseled(r, TUFF, "cross"))
reg("chiseled_tuff_bricks_top", lambda r: M.tiles(r, TUFF, scale(TUFF[0], 0.7), size=8))
reg("calcite", lambda r: M.speckle(M.mottled(r, CALCITE), r, [CALCITE[0]], 8))
reg("dripstone_block", lambda r: M.pits(M.speckle(M.banded(r, DRIPSTONE, horizontal=True, wobble=3.5), r,
                                                  [DRIPSTONE[0], DRIPSTONE[4]], 14), r, DRIPSTONE, 6))
reg("bricks", lambda r: M.bricks(r, BRICK, BRICK_MORTAR, rows=4, brick_w=8))


# --- Sandstone (top / side / bottom / cut / chiseled) ---------------------------------

def _sandstone_side(rng, pal):
    img = M.grainy(rng, pal, dark=6, bright=6)
    for x in range(N):   # a hard band near the top, finer layering below
        img.set(x, 3, pal[0] if x % 5 else pal[1])
        img.set(x, 4, pal[4])
        img.set(x, 10, pal[1])
    return img


def _sandstone_top(rng, pal):
    return M.grainy(rng, pal, dark=8, bright=8)


def _sandstone_bottom(rng, pal):
    img = M.grainy(rng, pal, dark=12, bright=4)
    return M.pits(img, rng, pal, 4, light_rim=False)


for prefix, pal in (("sandstone", SANDSTONE), ("red_sandstone", RED_SANDSTONE)):
    reg(prefix, lambda r, p=pal: _sandstone_side(r, p))
    reg(f"{prefix}_top", lambda r, p=pal: _sandstone_top(r, p))
    reg(f"{prefix}_bottom", lambda r, p=pal: _sandstone_bottom(r, p))
    reg(f"cut_{prefix}", lambda r, p=pal: M.bricks(r, p, p[0], rows=2, brick_w=16, grit=6))
    reg(f"chiseled_{prefix}", lambda r, p=pal: M.chiseled(r, p, "sun"))

reg("mud", lambda r: M.speckle(M.mottled(r, MUD), r, [MUD[4]], 10))
reg("packed_mud", lambda r: M.pits(M.mottled(r, PACKED_MUD), r, PACKED_MUD, 6))
reg("mud_bricks", lambda r: M.bricks(r, MUD_BRICK, scale(MUD_BRICK[0], 0.75), rows=4, brick_w=8))
reg("obsidian", lambda r: M.speckle(M.mottled(r, OBSIDIAN), r, [OBSIDIAN[4], hexc("5a3d8a")], 10))


def _crying(rng):
    img = M.speckle(M.mottled(rng, OBSIDIAN), rng, [OBSIDIAN[4]], 6)
    for _ in range(5):  # glowing tear streaks
        x, y = rng.randrange(N), rng.randrange(N)
        for i in range(rng.randrange(2, 5)):
            img.set(x, y + i, CRYING[4] if i == 0 else CRYING[3])
    return img


reg("crying_obsidian", _crying)
reg("bedrock", lambda r: M.speckle(M.speckle(M.cells(r, BEDROCK, 7, BEDROCK[0], shades=(1, 2, 3, 3, 4),
                                                       edge_width=0.55, grit=0), r, [BEDROCK[0]], 14),
                                   r, [BEDROCK[4]], 8))


# --- Loose blocks ----------------------------------------------------------------------

reg("sand", lambda r: M.grainy(r, SAND))
reg("red_sand", lambda r: M.grainy(r, RED_SAND))
reg("gravel", lambda r: M.cells(r, GRAVEL, 14, GRAVEL[0], shades=(1, 2, 3, 3), edge_width=0.6, grit=8))
reg("clay", lambda r: M.speckle(M.mottled(r, CLAY, octaves=((2, 0.6), (8, 0.4))), r, [CLAY[0]], 6))


def _suspicious(base_fn, stage):
    """Brushable block: an object shows through more at each stage (0..3)."""
    def paint(rng):
        img = base_fn(rng)
        # A pottery shard peeking out: a dark hollow, then more of the shard per stage.
        hollow = hexc("4a3a2a")
        shard = [hexc("6e3a24"), hexc("a5583a"), hexc("d98a5e")]
        for y in range(6, 11):
            for x in range(5, 11):
                if abs(x - 7.5) + abs(y - 8) < 4:
                    img.set(x, y, hollow)
        width = [1, 2, 3, 4][stage]
        for y in range(7, 7 + 1 + stage):
            for x in range(8 - width, 8 + width):
                img.set(x, y, shard[2] if y == 7 else shard[1])
        for x in range(8 - width, 8 + width):
            img.set(x, 7 + 1 + stage, shard[0])
        return img
    return paint


for i in range(4):
    reg(f"suspicious_sand_{i}", _suspicious(lambda r: M.grainy(r, SAND), i))
    reg(f"suspicious_gravel_{i}",
        _suspicious(lambda r: M.cells(r, GRAVEL, 14, GRAVEL[0], shades=(1, 2, 3, 3), edge_width=0.6), i))


# --- Soils and grass ---------------------------------------------------------------------

def _dirt(rng):
    return M.soil(rng, DIRT, PEBBLE)


reg("dirt", _dirt)
reg("coarse_dirt", lambda r: M.speckle(M.cells(r, COARSE, 12, COARSE[0], shades=(1, 2, 3), edge_width=0.5),
                                      r, [PEBBLE[1]], 6))


def _rooted(rng):
    img = M.soil(rng, DIRT, PEBBLE, clods=6)
    for _ in range(4):  # pale roots threading through
        x, y = rng.randrange(N), rng.randrange(N)
        for _ in range(rng.randrange(4, 7)):
            img.set(x, y, ROOT[3])
            img.set(x, y + 1, ROOT[1])
            x += rng.choice((1, 1, 0))
            y += rng.choice((-1, 0, 1))
    return img


reg("rooted_dirt", _rooted)
reg("grass_block_top", lambda r: M.blades(r, GRASS_GREY))


def _grass_side(rng):
    # Vanilla: dirt + biome-tinted overlay. We bake plains green (known deviation).
    img, depth = M.side_with_cap(_dirt(__import__("random").Random("grass_block_side/dirt")), rng,
                                 GRASS_GREEN)
    for x in range(N):
        img.set(x, depth[x], DIRT[0])  # shadow cast on the dirt
    return img


reg("grass_block_side", _grass_side)


def _grass_overlay(rng):
    """Greyscale fringe only (transparent below), for tinted side rendering."""
    full, depth = M.side_with_cap(Img(), rng, GRASS_GREY)
    out = Img()
    for x in range(N):
        for y in range(depth[x]):
            out.set(x, y, full.get(x, y))
    return out


reg("grass_block_side_overlay", _grass_overlay)


def _snowy_side(rng):
    img, depth = M.side_with_cap(_dirt(__import__("random").Random("grass_block_snow/dirt")), rng, SNOW,
                                 depth=(2, 3, 3, 4), drips=3)
    for x in range(N):
        img.set(x, depth[x], DIRT[0])
    return img


reg("grass_block_snow", _snowy_side)
reg("podzol_top", lambda r: M.speckle(M.soil(r, PODZOL, clods=14), r, [hexc("3b6b2a")], 8))
reg("podzol_side", lambda r: M.side_with_cap(_dirt(r), r, PODZOL, depth=(2, 3, 3), drips=3)[0])


def _mycelium_top(rng):
    img = M.soil(rng, MYCELIUM, clods=8)
    return M.speckle(img, rng, [MYCELIUM_SPORE[4], MYCELIUM_SPORE[3]], 12)


reg("mycelium_top", _mycelium_top)
reg("mycelium_side", lambda r: M.side_with_cap(_dirt(r), r, MYCELIUM, depth=(2, 3, 3, 4), drips=4)[0])
reg("dirt_path_top", lambda r: M.pits(M.speckle(M.mottled(r, PATH), r, [PATH[0], PATH[4]], 12), r, PATH, 4))


def _path_side(rng):
    img = _dirt(rng)
    for x in range(N):
        img.set(x, 0, CLEAR)  # path blocks are 15/16 tall: top row unused
        img.set(x, 1, PATH[3])
        img.set(x, 2, PATH[1])
    return img


reg("dirt_path_side", _path_side)


def _farmland(rng, pal):
    """Tilled soil: broken, slightly wavy furrows (dark trench, lit ridge above)."""
    img = M.soil(rng, pal, clods=6)
    for row in range(4):
        y0 = row * 4 + 2
        x = 0
        while x < N:
            run = rng.randrange(3, 7)
            dy = rng.choice((0, 0, 1))
            for k in range(run):
                img.set(x + k, y0 + dy, pal[0])
                img.set(x + k, y0 + dy - 1, pal[4] if k % 2 else pal[3])
            x += run + rng.choice((1, 2))
    return img


reg("farmland", lambda r: _farmland(r, FARMLAND))
reg("farmland_moist", lambda r: _farmland(r, FARMLAND_WET))
reg("moss_block", lambda r: M.blades(r, MOSS, count=20))

# --- Snow and ice ----------------------------------------------------------------------

reg("snow", lambda r: M.speckle(M.mottled(r, SNOW, 2, 4), r, [SNOW[1]], 8))
reg("powder_snow", lambda r: M.speckle(M.mottled(r, POWDER, 2, 4), r, [POWDER[1], POWDER[0]], 14))
reg("ice", lambda r: M.ice(r, ICE, alpha=200))
reg("packed_ice", lambda r: M.cracked(M.ice(r, PACKED_ICE), r, PACKED_ICE, 2))
reg("blue_ice", lambda r: M.cracked(M.ice(r, BLUE_ICE), r, BLUE_ICE, 3))
for i in range(4):
    reg(f"frosted_ice_{i}", lambda r, i=i: M.cracked(M.ice(r, ICE, alpha=200), r, ICE, 1 + i * 2))


# --- Ores and mineral blocks -----------------------------------------------------------

for mineral, pal in ORE.items():
    reg(f"{mineral}_ore", lambda r, p=pal, m=mineral: M.ore(_stone(__import__("random").Random(f"ore/{m}")),
                                                           r, p, 5, ORE_SHAPE[m]))
    reg(f"deepslate_{mineral}_ore",
        lambda r, p=pal, m=mineral: M.ore(M.banded(__import__("random").Random(f"dore/{m}"), DEEPSLATE,
                                                   horizontal=False), r, p, 5, ORE_SHAPE[m]))

reg("coal_block", lambda r: M.mineral_block(r, ramp(hexc("232325"), spread=0.2)))
reg("lapis_block", lambda r: M.mineral_block(r, ramp(hexc("2a4fae"))))
reg("redstone_block", lambda r: M.mineral_block(r, ramp(hexc("b0180a"), spread=0.3)))
reg("iron_block", lambda r: M.metal_block(r, ramp(hexc("d8d8d8"), spread=0.25)))
reg("gold_block", lambda r: M.metal_block(r, ramp(hexc("f2cc3a"), spread=0.3)))
reg("netherite_block", lambda r: M.metal_block(r, ramp(hexc("443e40"), spread=0.25), rivets=False))
reg("diamond_block", lambda r: M.gem_block(r, ramp(hexc("62e8de"), spread=0.3)))
reg("emerald_block", lambda r: M.gem_block(r, ramp(hexc("2ac85a"), spread=0.3)))
reg("raw_iron_block", lambda r: M.raw_block(r, ramp(hexc("c49a7c"))))
reg("raw_copper_block", lambda r: M.raw_block(r, ramp(hexc("9a694f"))))
reg("raw_gold_block", lambda r: M.raw_block(r, ramp(hexc("ddaa2e"))))

# --- Amethyst ----------------------------------------------------------------------------

def _amethyst_block(rng):
    return M.cells(rng, AMETHYST, 9, AMETHYST[0], shades=(1, 2, 3, 3), edge_width=0.6, grit=6)


def _budding(rng):
    img = _amethyst_block(rng)
    for k in range(4):  # small bright crystal starts
        x, y = (k % 2) * 8 + rng.randrange(2, 6), (k // 2) * 8 + rng.randrange(2, 6)
        img.set(x, y, AMETHYST[4])
        img.set(x + 1, y, AMETHYST[3])
        img.set(x, y + 1, AMETHYST[1])
    return img


def _crystal(rng, height, spikes):
    """Cross-model crystal cluster growing up from the bottom (transparent)."""
    img = Img()
    xs = [8] if spikes == 1 else [5, 8, 11][:spikes]
    for i, cx in enumerate(xs):
        h = height - (i % 2) * 2
        for y in range(N - h, N):
            w = max(0, (y - (N - h)) // 3)
            for x in range(cx - w, cx + w + 1):
                img.set(x, y, AMETHYST[3] if x < cx else AMETHYST[2] if x == cx else AMETHYST[1])
        img.set(cx, N - h, AMETHYST[4])
    return img


reg("amethyst_block", _amethyst_block)
reg("budding_amethyst", _budding)
reg("small_amethyst_bud", lambda r: _crystal(r, 5, 1))
reg("medium_amethyst_bud", lambda r: _crystal(r, 8, 2))
reg("large_amethyst_bud", lambda r: _crystal(r, 11, 3))
reg("amethyst_cluster", lambda r: _crystal(r, 14, 3))

# --- Quartz ------------------------------------------------------------------------------

reg("quartz_block_side", lambda r: M.polished(r, QUARTZ, noise=2))
reg("quartz_block_top", lambda r: M.polished(r, QUARTZ, noise=3))
reg("quartz_block_bottom", lambda r: M.speckle(M.mottled(r, QUARTZ, 2, 3), r, [QUARTZ[1]], 6))
reg("chiseled_quartz_block", lambda r: M.chiseled(r, QUARTZ, "steps"))
reg("chiseled_quartz_block_top", lambda r: M.chiseled(r, QUARTZ, "ring"))


def _pillar(rng, pal):
    img = M.mottled(rng, pal, 2, 3)
    for y in range(N):
        for x in (0, 5, 10):
            img.set(x, y, pal[4])
            img.set(x + 4, y, pal[1])
    return img


reg("quartz_pillar", lambda r: _pillar(r, QUARTZ))
reg("quartz_pillar_top", lambda r: M.chiseled(r, QUARTZ, "diamond"))
reg("quartz_bricks", lambda r: M.bricks(r, QUARTZ, QUARTZ[1], rows=4, brick_w=8, grit=4))


# --- Pointed dripstone (cross model, transparent) ----------------------------------------

def _drip(rng, direction, part):
    """Spike pieces: tip (point), frustum, middle, base (widest). `up` points up."""
    widths = {"tip": [0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3],
              "tip_merge": [2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2],
              "frustum": [2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4],
              "middle": [4] * 16,
              "base": [4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6]}[part]
    img = Img()
    for row in range(N):
        w = widths[row]
        y = row if direction == "up" else N - 1 - row
        for x in range(8 - w, 8 + w):
            img.set(x, y, DRIPSTONE[3] if x < 7 else DRIPSTONE[2] if x < 9 else DRIPSTONE[1])
        if w:
            img.set(8 - w, y, DRIPSTONE[4])
            img.set(8 + w - 1, y, DRIPSTONE[0])
    return img


for direction in ("up", "down"):
    for part in ("tip", "tip_merge", "frustum", "middle", "base"):
        reg(f"pointed_dripstone_{direction}_{part}", lambda r, d=direction, p=part: _drip(r, d, p))
