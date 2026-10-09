"""Batch 6: workstations, storage, redstone, copper, sculk, fluids, fire, cracks, misc.
Base colours are our own picks."""
import math
import random

from . import materials as M
from . import plants as P
from . import utility as U
from .core import CLEAR, N, Img, frame, grey, hexc, mix, ramp, scale, stack_frames
from .registry import add, animated


def reg(name, fn, mcmeta=None):
    add(name, fn, "utility & redstone", mcmeta)


OAK = [(98, 70, 38, 255), (132, 100, 56, 255), (158, 124, 72, 255), (180, 145, 88, 255),
       (204, 170, 108, 255)]
SPRUCE = ramp(hexc("6f5131"))
DARK_OAK = ramp(hexc("432b14"), spread=0.22)
BIRCH = ramp(hexc("c4b27b"), spread=0.2)
STONE = [(74, 75, 82, 255), (96, 97, 103, 255), (116, 116, 120, 255), (134, 133, 135, 255),
         (156, 154, 150, 255)]
COBBLE = [(58, 58, 66, 255), (88, 88, 94, 255), (112, 111, 114, 255), (134, 132, 132, 255),
          (162, 159, 152, 255)]
SMOOTH = ramp(hexc("9e9ea0"), spread=0.2)
IRON = U.IRON
DARK_IRON = ramp(hexc("3a3a40"), spread=0.25)
RED = ramp(hexc("c8201a"), spread=0.3)
GOLDP = ramp(hexc("f2c23a"), spread=0.3)
R = random.Random  # shorthand for fixed sub-seeds

# --- Crafting & workstations -----------------------------------------------------------
reg("crafting_table_top", lambda r: U.workbench_top(r, OAK))
reg("crafting_table_side", lambda r: U.tool_icons(U.framed(M.planks(r, OAK), OAK), r, ["saw", "hammer"]))
reg("crafting_table_front", lambda r: U.tool_icons(U.framed(M.planks(r, OAK), OAK), r, ["pick", "hammer"]))
reg("furnace_top", lambda r: U.stone_top(r, COBBLE))
reg("furnace_side", lambda r: M.bricks(r, COBBLE, COBBLE[0], grit=6))
reg("furnace_front", lambda r: U.furnace_front(r, COBBLE, False))
reg("furnace_front_on", lambda r: U.furnace_front(r, COBBLE, True))
reg("blast_furnace_top", lambda r: U.framed(M.tiles(r, SMOOTH, SMOOTH[0], size=8), SMOOTH))
reg("blast_furnace_side", lambda r: U.framed(M.bricks(r, SMOOTH, DARK_IRON[1]), DARK_IRON))
reg("blast_furnace_front", lambda r: U.furnace_front(r, SMOOTH, False, (3, 8, 12, 12)))
reg("blast_furnace_front_on", lambda r: U.furnace_front(r, SMOOTH, True, (3, 8, 12, 12)))
reg("smoker_top", lambda r: U.framed(M.polished(r, COBBLE), DARK_OAK))
reg("smoker_side", lambda r: U.framed(M.planks(r, DARK_OAK), DARK_OAK))
reg("smoker_bottom", lambda r: M.polished(r, COBBLE))
reg("smoker_front", lambda r: U.furnace_front(r, DARK_OAK, False, (5, 8, 10, 12)))
reg("smoker_front_on", lambda r: U.furnace_front(r, DARK_OAK, True, (5, 8, 10, 12)))


def _map_top(rng):
    img = U.framed(M.planks(rng, DARK_OAK), DARK_OAK)
    paper = ramp(hexc("e8dcb8"), spread=0.15)
    for y in range(3, 13):
        for x in range(3, 13):
            img.set(x, y, paper[2])
    for _ in range(6):  # map lines
        x, y = rng.randrange(4, 12), rng.randrange(4, 12)
        img.set(x, y, hexc("5a8ac8"))
        img.set(x + 1, y, hexc("6aa040"))
    return img


reg("cartography_table_top", _map_top)
for i in (1, 2, 3):
    reg(f"cartography_table_side{i}", lambda r, i=i: U.framed(M.planks(r, DARK_OAK if i != 2 else SPRUCE), DARK_OAK))
reg("fletching_table_top", lambda r: U.framed(M.planks(r, BIRCH), BIRCH))
reg("fletching_table_side", lambda r: U.framed(M.planks(r, BIRCH), OAK))
reg("fletching_table_front", lambda r: U.tool_icons(U.framed(M.planks(r, BIRCH), OAK), r, ["arrow", "arrow"]))
reg("smithing_table_top", lambda r: U.framed(M.metal_block(r, DARK_IRON, rivets=True), DARK_IRON))
reg("smithing_table_side", lambda r: U.framed(M.planks(r, DARK_OAK), DARK_IRON))
reg("smithing_table_front", lambda r: U.tool_icons(U.framed(M.planks(r, DARK_OAK), DARK_IRON), r, ["hammer", "hammer"]))
reg("smithing_table_bottom", lambda r: M.planks(r, DARK_OAK))


def _loom(rng, face):
    img = U.framed(M.planks(rng, OAK), OAK)
    if face == "front":
        for x in range(3, 13):
            for y in range(3, 13):
                img.set(x, y, hexc("e8e4dc") if x % 2 else hexc("c8b8a0"))
    elif face == "top":
        for x in range(2, 14, 2):
            for y in range(2, 14):
                img.set(x, y, hexc("e8e4dc"))
    return img


for face in ("top", "side", "front", "bottom"):
    reg(f"loom_{face}", lambda r, f=face: _loom(r, f))


def _stonecutter_saw(rng):
    frames = []
    for f in range(4):
        img = Img()
        for x in range(N):
            for y in range(6, 10):
                img.set(x, y, IRON[2] if (x + f + (y == 6)) % 2 else IRON[3])
        frames.append(img)
    return stack_frames(frames)


reg("stonecutter_top", lambda r: U.framed(M.polished(r, STONE), IRON))
reg("stonecutter_side", lambda r: U.framed(M.planks(r, OAK), STONE))
reg("stonecutter_bottom", lambda r: M.polished(r, STONE))
reg("stonecutter_saw", _stonecutter_saw, animated(2))
reg("grindstone_side", lambda r: U.framed(M.polished(r, SMOOTH), SMOOTH))
reg("grindstone_round", lambda r: M.cells(r, SMOOTH, 8, SMOOTH[0], shades=(1, 2, 3)))
reg("grindstone_pivot", lambda r: M.planks(r, DARK_OAK))
ANVIL = ramp(hexc("4a4a50"), spread=0.25)
reg("anvil", lambda r: U.framed(M.metal_block(r, ANVIL, rivets=False), ANVIL))
reg("anvil_top", lambda r: M.metal_block(r, ANVIL, rivets=False))
reg("chipped_anvil_top", lambda r: M.cracked(M.metal_block(r, ANVIL, rivets=False), r, ANVIL, 2))
reg("damaged_anvil_top", lambda r: M.cracked(M.metal_block(r, ANVIL, rivets=False), r, ANVIL, 5))
OBSIDIAN = ramp(hexc("1b1528"), spread=0.22)
DIAMOND = ramp(hexc("5ae3dc"), spread=0.3)


def _enchant(rng, face):
    if face == "top":
        img = Img(fill=hexc("8a2020"))  # red cloth with a gold-edged book
        frame(img, hexc("b03030"), hexc("5a1010"))
        for y in range(5, 11):
            for x in range(3, 13):
                img.set(x, y, hexc("e8dcb8") if x != 8 else hexc("8a6a3a"))
        return img
    img = M.speckle(M.mottled(rng, OBSIDIAN), rng, [OBSIDIAN[4]], 6)
    if face == "side":
        for x in range(N):
            img.set(x, 3, DIAMOND[3])
            img.set(x, 4, DIAMOND[1])
    return img


for face in ("top", "side", "bottom"):
    reg(f"enchanting_table_{face}", lambda r, f=face: _enchant(r, f))


def _brewing(rng, base):
    img = Img()
    if base:
        return M.polished(rng, COBBLE)
    for y in range(2, 14):
        img.set(7, y, hexc("d8c060"))
        img.set(8, y, hexc("a08a30"))
    for bx in (3, 12):
        for y in range(9, 14):
            img.set(bx, y, hexc("c8d8e8"))
    return img


reg("brewing_stand", lambda r: _brewing(r, False))
reg("brewing_stand_base", lambda r: _brewing(r, True))
CAULDRON = ramp(hexc("3a3a40"), spread=0.25)
reg("cauldron_side", lambda r: U.framed(M.metal_block(r, CAULDRON, rivets=True), CAULDRON))
reg("cauldron_top", lambda r: U.window_grid(U.framed(M.metal_block(r, CAULDRON), CAULDRON), CAULDRON, 2, 2, 13, 13, 12))
reg("cauldron_bottom", lambda r: M.metal_block(r, CAULDRON))
reg("cauldron_inner", lambda r: M.metal_block(r, scale_pal(CAULDRON, 0.8), rivets=False))


def scale_pal(pal, k):
    return [scale(c, k) for c in pal]


COMPOST = ramp(hexc("5a3a1e"))
reg("composter_side", lambda r: U.framed(M.planks(r, SPRUCE), SPRUCE))
reg("composter_top", lambda r: U.window_grid(U.framed(M.planks(r, SPRUCE), SPRUCE), SPRUCE, 2, 2, 13, 13, 12))
reg("composter_bottom", lambda r: M.planks(r, SPRUCE))
reg("composter_compost", lambda r: M.soil(r, COMPOST, clods=12))
reg("composter_ready", lambda r: M.speckle(M.soil(r, COMPOST, clods=8), r, [hexc("f0f0e8")], 16))
reg("barrel_side", lambda r: _barrel_side(r))


def _barrel_side(rng):
    img = M.planks(rng, SPRUCE)
    for y in (2, 13):
        for x in range(N):
            img.set(x, y, DARK_IRON[2])
            img.set(x, y + 1, DARK_IRON[1])
    return img


reg("barrel_top", lambda r: U.framed(M.planks(r, SPRUCE), DARK_IRON))
reg("barrel_bottom", lambda r: U.framed(M.planks(r, SPRUCE), DARK_IRON))
reg("barrel_top_open", lambda r: U.window_grid(U.framed(M.planks(r, SPRUCE), DARK_IRON), SPRUCE, 2, 2, 13, 13, 12))
HIVE = ramp(hexc("c89a50"))
HONEY = ramp(hexc("f0a020"), spread=0.3)


def _hive_front(rng, honey, nest):
    img = U.framed(M.planks(rng, HIVE) if not nest else M.cells(rng, ramp(hexc("d8b060")), 8, HIVE[0]), HIVE)
    for y in range(9, 12):
        for x in range(6, 10):
            img.set(x, y, hexc("201810"))
    if honey:
        for x, y in ((4, 3), (5, 3), (5, 4), (11, 5), (11, 6), (10, 5)):
            img.set(x, y, HONEY[3])
    return img


reg("beehive_front", lambda r: _hive_front(r, False, False))
reg("beehive_front_honey", lambda r: _hive_front(r, True, False))
reg("beehive_side", lambda r: U.framed(M.planks(r, HIVE), HIVE))
reg("beehive_end", lambda r: M.log_end(r, HIVE, HIVE))
reg("bee_nest_front", lambda r: _hive_front(r, False, True))
reg("bee_nest_front_honey", lambda r: _hive_front(r, True, True))
reg("bee_nest_side", lambda r: M.cells(r, ramp(hexc("d8b060")), 8, HIVE[0]))
reg("bee_nest_top", lambda r: M.cells(r, ramp(hexc("d8b060")), 6, HIVE[0]))
reg("bee_nest_bottom", lambda r: M.cells(r, ramp(hexc("c8a050")), 6, HIVE[0]))
BOOKS = [hexc("8a2a20"), hexc("2a4a8a"), hexc("2a6a3a"), hexc("6a4a2a"), hexc("8a6a20"), hexc("4a2a6a")]
reg("bookshelf", lambda r: U.books(r, OAK, BOOKS))
reg("chiseled_bookshelf_top", lambda r: U.framed(M.planks(r, OAK), OAK))
reg("chiseled_bookshelf_side", lambda r: M.planks(r, OAK))
def _empty_shelves(rng):
    # (M29.5) the shelves' dark back wall shows where there are no books (vanilla: opaque)
    img = U.window_grid(U.framed(M.planks(rng, OAK), OAK), OAK, 1, 1, 14, 7, 5)
    for y in range(16):
        for x in range(16):
            if img.get(x, y)[3] == 0:
                img.set(x, y, OAK[0] if (x + y * 3) % 7 else (40, 26, 14, 255))
    return img


reg("chiseled_bookshelf_empty", _empty_shelves)
reg("chiseled_bookshelf_occupied", lambda r: U.books(r, OAK, BOOKS))
reg("lectern_top", lambda r: U.framed(M.planks(r, OAK), OAK))
reg("lectern_sides", lambda r: U.framed(M.planks(r, OAK), OAK))
reg("lectern_front", lambda r: U.books(r, OAK, BOOKS[:2]))
reg("lectern_base", lambda r: M.planks(r, OAK))
reg("jukebox_side", lambda r: U.framed(M.planks(r, DARK_OAK), DARK_OAK))


def _jukebox_top(rng):
    img = U.framed(M.planks(rng, DARK_OAK), DARK_OAK)
    for x in range(4, 12):
        img.set(x, 7, hexc("101010"))
        img.set(x, 8, hexc("3a3a3a"))
    return img


reg("jukebox_top", _jukebox_top)
reg("note_block", lambda r: U.framed(M.planks(r, DARK_OAK), DARK_OAK))
TNT_RED = ramp(hexc("c82a20"), spread=0.3)


def _tnt(rng, face):
    if face == "side":
        img = Img()
        for x in range(N):
            for y in range(N):
                img.set(x, y, TNT_RED[2] if x % 4 else TNT_RED[1])
        for x in range(N):
            for y in range(6, 10):
                img.set(x, y, hexc("eceae4"))
        for x in range(4, 12, 2):  # an original "danger" mark: black diamonds
            img.set(x, 7, hexc("1a1a1a"))
            img.set(x + 1, 8, hexc("1a1a1a"))
        return img
    img = U.framed(M.planks(rng, TNT_RED), TNT_RED)
    if face == "top":
        img.set(7, 7, hexc("2a2a2a"))
        img.set(8, 8, hexc("2a2a2a"))
    return img


for face in ("top", "side", "bottom"):
    reg(f"tnt_{face}", lambda r, f=face: _tnt(r, f))
for face in ("top", "side", "bottom"):
    reg(f"bell_{face}", lambda r: M.metal_block(r, GOLDP, rivets=False))
SCAFFOLD = ramp(hexc("b8a050"), spread=0.22)
reg("scaffolding_top", lambda r: U.window_grid(U.framed(M.planks(r, SCAFFOLD), SCAFFOLD), SCAFFOLD, 2, 2, 13, 13, 4))
reg("scaffolding_side", lambda r: U.window_grid(U.framed(M.planks(r, SCAFFOLD), SCAFFOLD), SCAFFOLD, 2, 2, 13, 13, 6))
reg("scaffolding_bottom", lambda r: U.window_grid(U.framed(M.planks(r, SCAFFOLD), SCAFFOLD), SCAFFOLD, 2, 2, 13, 13, 12))


def _ladder(rng):
    img = Img()
    for y in range(N):
        for x, c in ((2, OAK[3]), (3, OAK[1]), (12, OAK[3]), (13, OAK[1])):
            img.set(x, y, c)
    for y in range(1, N, 4):
        for x in range(4, 12):
            img.set(x, y, OAK[3])
            img.set(x, y + 1, OAK[1])
    return img


reg("ladder", _ladder)


def _bars(rng, pal=IRON):
    img = Img()
    for x in range(1, N, 4):
        for y in range(N):
            img.set(x, y, pal[3])
            img.set(x + 1, y, pal[1])
    for y in (0, 15):
        for x in range(N):
            img.set(x, y, pal[2])
    return img


reg("iron_bars", _bars)
reg("iron_door_top", lambda r: M.door(r, IRON, "top", "grid"))
reg("iron_door_bottom", lambda r: M.door(r, IRON, "bottom", "panel"))
reg("iron_trapdoor", lambda r: M.trapdoor(r, IRON, "grid"))


def _chain(rng, pal=DARK_IRON):
    img = Img()
    for y in range(N):
        if y % 4 < 3:
            img.set(7, y, pal[3])
            img.set(8, y, pal[1])
        else:
            img.set(6, y, pal[2])
            img.set(9, y, pal[2])
    return img


reg("chain", _chain)
reg("iron_chain", _chain)  # (M29.6: 1.21.9 renamed the chain)


def _lantern(rng, flame, metal=DARK_IRON):
    img = Img()
    for y in range(3, 12):
        for x in range(5, 11):
            edge = x in (5, 10) or y in (3, 11)
            img.set(x, y, metal[2] if edge else flame[3 if (x + y) % 2 else 4])
    for x in range(6, 10):
        img.set(x, 2, metal[3])
    return img


reg("lantern", lambda r: _lantern(r, ramp(hexc("f0a040"), spread=0.3)))
reg("soul_lantern", lambda r: _lantern(r, ramp(hexc("40d8e0"), spread=0.3)))


def _torch(rng, flame, stick=OAK, lit=True):
    img = Img()
    for y in range(8, 16):
        img.set(7, y, stick[3])
        img.set(8, y, stick[1])
    if lit:
        img.set(7, 7, flame[3])
        img.set(8, 7, flame[2])
        img.set(7, 6, flame[4])
        img.set(8, 6, flame[3])
    else:
        img.set(7, 7, hexc("4a1010"))
        img.set(8, 7, hexc("3a0a0a"))
    return img


reg("torch", lambda r: _torch(r, ramp(hexc("f0a030"), spread=0.3)))
reg("soul_torch", lambda r: _torch(r, ramp(hexc("40d8e0"), spread=0.3)))
reg("redstone_torch", lambda r: _torch(r, RED))
reg("redstone_torch_off", lambda r: _torch(r, RED, lit=False))


def _pot(rng):
    img = Img()
    clay = ramp(hexc("9a5038"))
    for y in range(10, 16):
        for x in range(5, 11):
            img.set(x, y, clay[3] if x < 8 else clay[1])
    for x in range(4, 12):
        img.set(x, 10, clay[4])
    return img


reg("flower_pot", _pot)
reg("honey_block_top", lambda r: M.ice(r, HONEY, alpha=210, cracks=0))
reg("honey_block_side", lambda r: M.ice(r, HONEY, alpha=210, cracks=0))
reg("honey_block_bottom", lambda r: M.ice(r, HONEY, alpha=210, cracks=0))
reg("honeycomb_block", lambda r: M.tiles(r, HONEY, HONEY[0], size=4))
SLIME = ramp(hexc("6ac050"), spread=0.25)


def _slime(rng):
    img = M.ice(rng, SLIME, alpha=200, cracks=0)
    for y in range(4, 12):
        for x in range(4, 12):
            img.set(x, y, SLIME[3])
    frame(img, SLIME[4], SLIME[1], inset=4)
    return img


reg("slime_block", _slime)
BONE = ramp(hexc("e4dcc4"), spread=0.16)
reg("bone_block_side", lambda r: M.stripped(r, BONE))
reg("bone_block_top", lambda r: M.log_end(r, BONE, BONE, rim=False))


def _cobweb(rng):
    img = Img()
    for a in range(8):
        ang = a * math.pi / 4
        for t in range(8):
            img.set(round(7.5 + math.cos(ang) * t), round(7.5 + math.sin(ang) * t), (230, 230, 230, 220))
    for r in (3, 6):
        for a in range(0, 360, 12):
            t = math.radians(a)
            img.set(round(7.5 + math.cos(t) * r), round(7.5 + math.sin(t) * r), (210, 210, 210, 200))
    return img


reg("cobweb", _cobweb)
reg("spawner", lambda r: U.window_grid(Img(fill=DARK_IRON[2]), DARK_IRON, 0, 0, 15, 15, 3))


def _beacon(rng):
    img = M.glass(rng, ramp(hexc("a8e8e8")), 140)
    for y in range(4, 12):
        for x in range(4, 12):
            img.set(x, y, (240, 255, 255, 255) if max(abs(x - 7.5), abs(y - 7.5)) < 2 else (120, 220, 220, 255))
    return img


reg("beacon", _beacon)
reg("conduit", lambda r: M.tiles(r, ramp(hexc("a07a50")), hexc("5a4030"), size=4))
FROG = {"ochre": "e8c860", "verdant": "a8e090", "pearlescent": "e8b8e0"}
for name, hx in FROG.items():
    pal = ramp(hexc(hx), spread=0.25)
    reg(f"{name}_froglight_top", lambda r, p=pal: U.framed(M.tiles(r, p, p[1], size=8), p))
    reg(f"{name}_froglight_side", lambda r, p=pal: M.stripped(r, p))


def _target(rng, top):
    img = M.blades(rng, ramp(hexc("e8dcc0"), spread=0.15), count=8) if not top else Img(fill=hexc("eae0c8"))
    for y in range(N):
        for x in range(N):
            d = max(abs(x - 7.5), abs(y - 7.5))
            if int(d) in (1, 5):
                img.set(x, y, RED[2])
            elif d < 1:
                img.set(x, y, RED[3])
    return img


reg("target_top", lambda r: _target(r, True))
reg("target_side", lambda r: _target(r, False))
COPPER = ramp(hexc("c87050"), spread=0.28)


def _rod(rng, on, pal=COPPER):
    img = Img()
    for y in range(3, 16):
        img.set(7, y, pal[3] if not on else (250, 250, 220, 255))
        img.set(8, y, pal[1] if not on else (200, 220, 255, 255))
    for x in range(6, 10):
        img.set(x, 2, pal[4])
    return img


reg("lightning_rod", lambda r: _rod(r, False))
reg("lightning_rod_on", lambda r: _rod(r, True))
# (M29.5) the aged rods of 1.21.9, in the copper stages' colours
for _stage, _hex in (("exposed", "a8806a"), ("weathered", "6d9a72"), ("oxidized", "4fa08a")):
    reg(_stage + "_lightning_rod", lambda r, h=_hex: _rod(r, False, ramp(hexc(h), spread=0.28)))


def _daylight(rng, inverted):
    img = U.framed(M.planks(rng, OAK), OAK)
    glass = hexc("3a4a6a") if inverted else hexc("d8e0f0")
    for y in range(4, 12):
        for x in range(2, 14):
            img.set(x, y, glass if (x + y) % 4 else scale(glass, 0.8))
    return img


reg("daylight_detector_top", lambda r: _daylight(r, False))
reg("daylight_detector_inverted_top", lambda r: _daylight(r, True))
reg("daylight_detector_side", lambda r: M.planks(r, OAK))

# --- Sniffer egg -------------------------------------------------------------------------
EGG = ramp(hexc("a84a3a"), spread=0.25)
for crack, n in (("not_cracked", 0), ("slightly_cracked", 3), ("very_cracked", 6)):
    for face in ("top", "bottom", "north", "south", "east", "west"):
        reg(f"sniffer_egg_{crack}_{face}",
            lambda r, n=n: M.cracked(M.speckle(M.mottled(r, EGG), r, [hexc("3a7a6a")], 10), r, EGG, n) if n
            else M.speckle(M.mottled(r, EGG), r, [hexc("3a7a6a")], 10))

# --- Redstone ----------------------------------------------------------------------------
DUST = [grey(60), grey(110), grey(160), grey(210), grey(250)]
reg("redstone_dust_dot", lambda r: U.dust("dot", DUST))
reg("redstone_dust_line0", lambda r: U.dust("line0", DUST))
reg("redstone_dust_line1", lambda r: U.dust("line1", DUST))
reg("redstone_dust_overlay", lambda r: Img())


def _diode(rng, kind, on):
    img = M.polished(rng, SMOOTH, noise=2)
    torch = RED[4] if on else hexc("5a1010")
    pts = [(7, 3), (7, 12)] if kind == "repeater" else [(4, 3), (11, 3), (7, 12)]
    for x, y in pts:
        img.set(x, y, torch)
        img.set(x + 1, y, scale(torch, 0.8))
    for y in range(4, 12):
        img.set(7, y, RED[2] if on else hexc("6a2020"))
    return img


reg("repeater", lambda r: _diode(r, "repeater", False))
reg("repeater_on", lambda r: _diode(r, "repeater", True))
reg("comparator", lambda r: _diode(r, "comparator", False))
reg("comparator_on", lambda r: _diode(r, "comparator", True))


def _lever(rng):
    img = Img()
    for y in range(3, 12):
        img.set(7, y, OAK[3])
        img.set(8, y, OAK[1])
    return img


reg("lever", _lever)
TIES = ramp(hexc("6a4a2a"))
reg("rail", lambda r: U.rail(r, TIES, IRON))
reg("rail_corner", lambda r: U.rail(r, TIES, IRON, corner=True))
for name, off, on in (("powered_rail", hexc("6a2020"), RED[4]), ("detector_rail", hexc("5a3a3a"), RED[3]),
                      ("activator_rail", hexc("5a2a2a"), RED[3])):
    metal = GOLDP if name == "powered_rail" else IRON
    reg(name, lambda r, c=off, m=metal: U.rail(r, TIES, m, powered=c))
    reg(f"{name}_on", lambda r, c=on, m=metal: U.rail(r, TIES, m, powered=c))


def _machine_face(rng, kind):
    img = M.bricks(R(f"machine/{kind}"), COBBLE, COBBLE[0], grit=6)
    frame(img, COBBLE[4], COBBLE[0])
    if kind in ("dispenser", "dropper"):
        holes = [(6, 6), (7, 6), (8, 6), (9, 6), (6, 9), (9, 9), (7, 10), (8, 10)] if kind == "dispenser" else \
            [(x, y) for x in range(6, 10) for y in range(6, 10)]
        for x, y in holes:
            img.set(x, y, hexc("141414"))
    elif kind == "vertical":
        for y in range(5, 11):
            for x in range(5, 11):
                if abs(x - 7.5) + abs(y - 7.5) < 4:
                    img.set(x, y, hexc("141414"))
    return img


reg("dispenser_front", lambda r: _machine_face(r, "dispenser"))
reg("dispenser_front_vertical", lambda r: _machine_face(r, "vertical"))
reg("dropper_front", lambda r: _machine_face(r, "dropper"))
reg("dropper_front_vertical", lambda r: _machine_face(r, "vertical"))


def _observer(rng, face, on=False):
    img = M.bricks(R("observer"), COBBLE, COBBLE[0], grit=4)
    frame(img, COBBLE[4], COBBLE[0])
    if face == "front":
        for (x0, y0) in ((3, 5), (10, 5)):  # an original pair of square "eyes"
            for y in range(y0, y0 + 3):
                for x in range(x0, x0 + 3):
                    img.set(x, y, hexc("101010"))
        for x in range(4, 12):
            img.set(x, 11, hexc("101010"))
    elif face == "back":
        for y in range(6, 10):
            for x in range(6, 10):
                img.set(x, y, RED[4] if on else hexc("4a1010"))
    elif face == "top":
        for y in range(N):
            img.set(7, y, RED[2] if on else hexc("4a1010"))
    return img


reg("observer_front", lambda r: _observer(r, "front"))
reg("observer_back", lambda r: _observer(r, "back"))
reg("observer_back_on", lambda r: _observer(r, "back", True))
reg("observer_side", lambda r: _observer(r, "side"))
reg("observer_top", lambda r: _observer(r, "top"))


def _piston(rng, face):
    if face in ("top", "top_sticky"):
        img = U.framed(M.planks(rng, OAK), OAK)
        if face == "top_sticky":
            for y in range(3, 13):
                for x in range(3, 13):
                    img.set(x, y, SLIME[2] if (x + y) % 3 else SLIME[3])
        return img
    if face == "inner":
        return M.polished(rng, STONE)
    img = M.bricks(rng, COBBLE, COBBLE[0], grit=4)
    if face == "side":
        for y in range(0, 4):
            for x in range(N):
                img.set(x, y, OAK[3] if y == 0 else OAK[2])
        for y in range(4, N):
            img.set(7, y, IRON[3])
            img.set(8, y, IRON[1])
    else:
        for y in range(5, 11):
            for x in range(5, 11):
                img.set(x, y, IRON[2])
    return img


for face in ("top", "top_sticky", "side", "bottom", "inner"):
    reg(f"piston_{face}", lambda r, f=face: _piston(r, f))
reg("hopper_outside", lambda r: M.metal_block(r, DARK_IRON, rivets=True))
reg("hopper_inside", lambda r: M.metal_block(r, scale_pal(DARK_IRON, 0.8), rivets=False))
reg("hopper_top", lambda r: U.window_grid(M.metal_block(r, DARK_IRON), DARK_IRON, 2, 2, 13, 13, 12))
LAMP = ramp(hexc("8a5a30"))
LAMP_ON = ramp(hexc("f0c070"), spread=0.3)
reg("redstone_lamp", lambda r: U.lamp(r, LAMP, False))
reg("redstone_lamp_on", lambda r: U.lamp(r, LAMP_ON, True))


def _tripwire(rng):
    img = Img()
    for x in range(N):
        img.set(x, 7, (220, 220, 220, 200))
    return img


reg("tripwire", _tripwire)


def _hook(rng):
    img = Img()
    for y in range(6, 14):
        img.set(7, y, OAK[2])
        img.set(8, y, OAK[1])
    for x in range(6, 10):
        img.set(x, 5, IRON[3])
    return img


reg("tripwire_hook", _hook)

# --- Sculk -------------------------------------------------------------------------------
SCULK = ramp(hexc("0c2a35"), spread=0.25)
SCULK_GLOW = ramp(hexc("2ad8d0"), spread=0.3)


def _sculk_frames(rng):
    base = M.speckle(M.mottled(R("sculk/base"), SCULK), rng, [SCULK[0]], 10)
    frames = []
    spots = [(rng.randrange(N), rng.randrange(N)) for _ in range(8)]
    for f in range(4):
        img = base.copy()
        for k, (x, y) in enumerate(spots):
            if (k + f) % 2 == 0:
                img.set(x, y, SCULK_GLOW[3])
                img.set(x + 1, y, SCULK_GLOW[2])
        frames.append(img)
    return stack_frames(frames)


reg("sculk", _sculk_frames, animated(10))
reg("sculk_vein", lambda r: P.vine(r, SCULK, density=20))


def _sculk_face(rng, glow_bars=0, top_ring=False, bloom=False):
    img = M.speckle(M.mottled(rng, SCULK), rng, [SCULK[0]], 8)
    frame(img, SCULK[3], SCULK[0])
    if top_ring:
        for y in range(4, 12):
            for x in range(4, 12):
                if int(max(abs(x - 7.5), abs(y - 7.5))) == 2:
                    img.set(x, y, SCULK_GLOW[4 if bloom else 2])
    for k in range(glow_bars):
        for y in range(5, 11):
            img.set(4 + k * 4, y, SCULK_GLOW[3])
    return img


reg("sculk_catalyst_top", lambda r: _sculk_face(r, top_ring=True))
reg("sculk_catalyst_top_bloom", lambda r: _sculk_face(r, top_ring=True, bloom=True))
reg("sculk_catalyst_side", lambda r: _sculk_face(r, glow_bars=2))
reg("sculk_catalyst_side_bloom", lambda r: _sculk_face(r, glow_bars=3))
reg("sculk_catalyst_bottom", lambda r: M.polished(r, ramp(hexc("6a6a70"))))
reg("sculk_sensor_top", lambda r: _sculk_face(r, top_ring=True))
reg("sculk_sensor_side", lambda r: _sculk_face(r, glow_bars=1))
reg("sculk_sensor_bottom", lambda r: _sculk_face(r))
reg("sculk_sensor_tendril_inactive", lambda r: P.grass_cross(r, SCULK_GLOW[:2] + SCULK[2:], height=8, blades=4))
reg("sculk_sensor_tendril_active", lambda r: P.grass_cross(r, SCULK_GLOW, height=8, blades=4))
reg("calibrated_sculk_sensor_top", lambda r: _sculk_face(r, top_ring=True, bloom=True))
reg("calibrated_sculk_sensor_amethyst", lambda r: P.grass_cross(r, ramp(hexc("8a63c4"), spread=0.3), height=9, blades=4))
reg("calibrated_sculk_sensor_input_side", lambda r: _sculk_face(r, glow_bars=2))
reg("sculk_shrieker_top", lambda r: _sculk_face(r, top_ring=True))
reg("sculk_shrieker_side", lambda r: _sculk_face(r, glow_bars=1))
reg("sculk_shrieker_bottom", lambda r: M.polished(r, ramp(hexc("6a6a70"))))
reg("sculk_shrieker_inner_top", lambda r: _sculk_face(r, top_ring=True))
reg("sculk_shrieker_can_summon_inner_top", lambda r: _sculk_face(r, top_ring=True, bloom=True))

# --- Crafter, trial spawner, vault ----------------------------------------------------
CRAFTER = ramp(hexc("6a6a70"), spread=0.25)


def _crafter(rng, face, state=""):
    img = U.framed(M.tiles(R("crafter"), CRAFTER, CRAFTER[0], size=8), CRAFTER)
    if face == "top":
        for k in (5, 10):
            for i in range(2, 14):
                img.set(k, i, CRAFTER[0])
                img.set(i, k, CRAFTER[0])
    if state:
        img.set(7, 7, RED[4] if state == "triggered" else GOLDP[4])
        img.set(8, 8, RED[3] if state == "triggered" else GOLDP[3])
    return img


reg("crafter_top", lambda r: _crafter(r, "top"))
reg("crafter_top_crafting", lambda r: _crafter(r, "top", "crafting"))
reg("crafter_top_triggered", lambda r: _crafter(r, "top", "triggered"))
reg("crafter_bottom", lambda r: _crafter(r, "bottom"))
for side in ("north", "south", "east", "west"):
    reg(f"crafter_{side}", lambda r: _crafter(r, "side"))
    reg(f"crafter_{side}_crafting", lambda r: _crafter(r, "side", "crafting"))
    reg(f"crafter_{side}_triggered", lambda r: _crafter(r, "side", "triggered"))
TRIAL = ramp(hexc("4a4a52"), spread=0.25)
TRIAL_GLOW = ramp(hexc("f0a030"), spread=0.3)
for state, lit in (("inactive", False), ("active", True), ("ejecting_reward", True)):
    reg(f"trial_spawner_top_{state}", lambda r, lit=lit: U.window_grid(
        U.framed(M.metal_block(r, TRIAL), TRIAL_GLOW if lit else TRIAL), TRIAL, 2, 2, 13, 13, 4))
    reg(f"trial_spawner_side_{state}", lambda r, lit=lit: U.window_grid(
        U.framed(Img(fill=TRIAL[2]), TRIAL_GLOW if lit else TRIAL), TRIAL, 1, 1, 14, 14, 3))
reg("trial_spawner_bottom", lambda r: M.metal_block(r, TRIAL))
for name, lit in (("vault_side_off", False), ("vault_side_on", True), ("vault_front_off", False),
                  ("vault_front_on", True), ("vault_front_ejecting", True)):
    reg(name, lambda r, lit=lit: U.window_grid(U.framed(M.metal_block(r, TRIAL), TRIAL_GLOW if lit else TRIAL),
                                              TRIAL, 3, 3, 12, 12, 9))
reg("vault_top", lambda r: M.metal_block(r, TRIAL))
reg("vault_bottom", lambda r: M.metal_block(r, TRIAL))

# --- Copper (four weathering stages) -----------------------------------------------------
STAGES = {"": "c87050", "exposed_": "a8806a", "weathered_": "6a9a7a", "oxidized_": "50a88a"}
for prefix, hx in STAGES.items():
    pal = ramp(hexc(hx), spread=0.28)
    block = "copper_block" if prefix == "" else f"{prefix}copper"
    reg(block, lambda r, p=pal: M.speckle(M.metal_block(r, p, rivets=False), r, [p[1]], 8))
    reg(f"{prefix}cut_copper", lambda r, p=pal: M.tiles(r, p, p[0], size=8))
    reg(f"{prefix}chiseled_copper", lambda r, p=pal: M.chiseled(r, p, "steps"))
    reg(f"{prefix}copper_grate", lambda r, p=pal: U.window_grid(U.framed(Img(fill=p[2]), p), p, 1, 1, 14, 14, 3))
    for suffix, lit, powered in (("", False, False), ("_lit", True, False), ("_powered", False, True),
                                 ("_lit_powered", True, True)):
        def bulb(r, p=pal, lit=lit, powered=powered):
            img = U.framed(M.metal_block(r, p, rivets=False), p)
            core = (255, 220, 140, 255) if lit else p[0]
            for y in range(4, 12):
                for x in range(4, 12):
                    img.set(x, y, core if abs(x - 7.5) + abs(y - 7.5) < 4 else img.get(x, y))
            if powered:
                img.set(7, 1, RED[4])
                img.set(8, 1, RED[3])
            return img
        reg(f"{prefix}copper_bulb{suffix}", bulb)
    reg(f"{prefix}copper_door_top", lambda r, p=pal: M.door(r, p, "top", "window"))
    reg(f"{prefix}copper_door_bottom", lambda r, p=pal: M.door(r, p, "bottom", "panel"))
    reg(f"{prefix}copper_trapdoor", lambda r, p=pal: M.trapdoor(r, p, "window"))

# --- Fluids, fire, campfire, cracks ----------------------------------------------------
# Tinted blue in game. Water stays subtle (narrow range) and see-through: the
# "pop" rule is relaxed here - a contrasty water texture tiles visibly.
WATER = [grey(178), grey(188), grey(198), grey(210), grey(226)]
LAVA = [hexc("a8300a"), hexc("d8501a"), hexc("f07a20"), hexc("f8a830"), hexc("ffd860")]
reg("water_still", lambda r: U.fluid_frames(r, WATER, 16, alpha=150), animated(2))
reg("water_flow", lambda r: U.fluid_frames(r, WATER, 16, flow=True, alpha=150), animated(1))
reg("water_overlay", lambda r: U.fluid_frames(r, WATER, 1, alpha=150))
reg("lava_still", lambda r: U.fluid_frames(r, LAVA, 20), animated(2))
reg("lava_flow", lambda r: U.fluid_frames(r, LAVA, 16, flow=True), animated(2))
FIRE = [hexc("7a1a0a"), hexc("c83a10"), hexc("f07a1a"), hexc("f8b030"), hexc("fff0a0")]
SOUL_FIRE = [hexc("0a3a4a"), hexc("1a7a8a"), hexc("2ac0d0"), hexc("6ae8f0"), hexc("d8ffff")]
for i in (0, 1):
    reg(f"fire_{i}", lambda r: U.fire_frames(r, FIRE), animated(2))
    reg(f"soul_fire_{i}", lambda r: U.fire_frames(r, SOUL_FIRE), animated(2))
reg("campfire_fire", lambda r: U.fire_frames(r, FIRE), animated(2))
reg("soul_campfire_fire", lambda r: U.fire_frames(r, SOUL_FIRE), animated(2))
CAMP_LOG = [(40, 29, 18, 255), (62, 46, 28, 255), (86, 64, 38, 255), (108, 82, 50, 255),
            (128, 100, 62, 255)]
reg("campfire_log", lambda r: M.bark(r, CAMP_LOG))


def _campfire_log_lit(rng, flame):
    img = M.bark(rng, CAMP_LOG)
    for _ in range(10):
        img.set(rng.randrange(N), rng.randrange(N), flame[3])
    return img


reg("campfire_log_lit", lambda r: _campfire_log_lit(r, FIRE))
reg("soul_campfire_log_lit", lambda r: _campfire_log_lit(r, SOUL_FIRE))
for s in range(10):
    reg(f"destroy_stage_{s}", lambda r, s=s: U.crack_stage(s))


# --- The Copper Age (M29.6, 1.21.9; wiki: Copper Bars, Copper Chain, Copper Lantern,
# Copper Torch): copper metal in its four oxidation colours; copper fire burns green.
COPPER_STAGES = {"": "c87050", "exposed_": "a8806a", "weathered_": "6d9a72", "oxidized_": "4fa08a"}
COPPER_FLAME = ramp(hexc("78e070"), spread=0.3)
for _st, _hx in COPPER_STAGES.items():
    reg(_st + "copper_bars", lambda r, h=_hx: _bars(r, ramp(hexc(h), spread=0.28)))
    reg(_st + "copper_chain", lambda r, h=_hx: _chain(r, ramp(hexc(h), spread=0.28)))
    reg(_st + "copper_lantern", lambda r, h=_hx: _lantern(r, COPPER_FLAME, ramp(hexc(h), spread=0.28)))
reg("copper_torch", lambda r: _torch(r, COPPER_FLAME))


# --- Test blocks (M29.7, 1.21.5 game tests; wiki: Test Block): a framed panel per mode.
def _test_block(rng, colour, mark):
    img = Img()
    pal = ramp(hexc(colour), spread=0.25)
    for y in range(N):
        for x in range(N):
            edge = x in (0, 15) or y in (0, 15)
            img.set(x, y, pal[1] if edge else pal[2 if (x + y) % 5 else 3])
    for (x, y) in mark:
        img.set(x, y, (240, 240, 240, 255))
    return img


_CHECK = [(4, 8), (5, 9), (6, 10), (7, 9), (8, 8), (9, 7), (10, 6), (11, 5)]
_CROSS = [(i, i) for i in range(4, 12)] + [(i, 15 - i) for i in range(4, 12)]
_ARROW = [(x, 8) for x in range(4, 12)] + [(9, 6), (10, 7), (9, 10), (10, 9)]
_LINES = [(x, y) for y in (5, 8, 11) for x in range(4, 12)]
reg("test_block_start", lambda r: _test_block(r, "3a7ad0", _ARROW))
reg("test_block_log", lambda r: _test_block(r, "8a8a8a", _LINES))
reg("test_block_fail", lambda r: _test_block(r, "c03030", _CROSS))
reg("test_block_accept", lambda r: _test_block(r, "30a040", _CHECK))
reg("test_instance_block", lambda r: _test_block(r, "6a4a9a", _LINES[:8]))
