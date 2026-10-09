"""Batch 4: plants, flowers, crops, gourds, underwater life, corals, lush caves.

Greyscale entries are tinted in game (grass/foliage colours); see PREVIEW_TINT.
"""
import random

from . import materials as M
from . import plants as P
from .core import CLEAR, N, Img, frame, grey, hexc, mix, ramp, scale, stack_frames
from .registry import add, animated


def reg(name, fn, mcmeta=None):
    add(name, fn, "plants", mcmeta)


STEM = ramp(hexc("4c8a2a"))
GREY = [grey(70), grey(104), grey(140), grey(176), grey(212)]
YELLOW_C, DARK_C = hexc("f2c230"), hexc("3a2a1a")

# --- Flowers -----------------------------------------------------------------------------
FLOWERS = {
    "dandelion": (ramp(hexc("f2d22a")), hexc("e0a010"), "round"),
    "poppy": (ramp(hexc("d8241e")), DARK_C, "round"),
    "blue_orchid": (ramp(hexc("2a9ae0")), hexc("1a5aa0"), "star"),
    "allium": (ramp(hexc("b46ae0")), None, "cluster"),
    "azure_bluet": (ramp(hexc("eef2f2"), spread=0.15), YELLOW_C, "small"),
    "red_tulip": (ramp(hexc("d8301e")), None, "tulip"),
    "orange_tulip": (ramp(hexc("f0841c")), None, "tulip"),
    "white_tulip": (ramp(hexc("eceeee"), spread=0.15), None, "tulip"),
    "pink_tulip": (ramp(hexc("ee9ac0")), None, "tulip"),
    "oxeye_daisy": (ramp(hexc("f4f4f0"), spread=0.15), YELLOW_C, "daisy"),
    "cornflower": (ramp(hexc("4060e0")), hexc("20308a"), "spiky"),
    "lily_of_the_valley": (ramp(hexc("f4f4f0"), spread=0.15), None, "bells"),
    "wither_rose": (ramp(hexc("2a2622"), spread=0.25), hexc("101010"), "round"),
    "torchflower": (ramp(hexc("f08a2a"), spread=0.3), hexc("f8d070"), "flame"),
    "closed_eyeblossom": (ramp(hexc("6a6a6e")), hexc("3a3a3e"), "tulip"),
    "open_eyeblossom": (ramp(hexc("e8e8ec"), spread=0.15), hexc("f07820"), "eye"),
    "golden_dandelion": (ramp(hexc("ffd84a"), spread=0.35), hexc("b87a08"), "round"),  # (26.1)
}
for name, (petal, center, shape) in FLOWERS.items():
    stem_pal = ramp(hexc("3a4a3a")) if name == "wither_rose" else STEM
    reg(name, lambda r, p=petal, c=center, s=shape, st=stem_pal: P.flower(r, st, p, c, s))

TALL = {
    "sunflower": (ramp(hexc("f4cc20")), hexc("6a4a1a"), "sun"),
    "lilac": (ramp(hexc("cc8ae0")), None, "bush"),
    "rose_bush": (ramp(hexc("d02a30")), None, "bush"),
    "peony": (ramp(hexc("eeaad0")), None, "bush"),
    "pitcher_plant": (ramp(hexc("4a8ab0")), hexc("8a4ae0"), "pitcher"),
}
for name, (petal, center, shape) in TALL.items():
    reg(f"{name}_top", lambda r, p=petal, c=center, s=shape: P.tall_flower(r, STEM, p, c, "top", s))
    reg(f"{name}_bottom", lambda r, p=petal, c=center, s=shape: P.tall_flower(r, STEM, p, c, "bottom", s))


def _sun_face(rng, back):
    img = Img()
    P.disc(img, 8, 8, 6, ramp(hexc("6a4a1a")) if not back else STEM)
    if not back:
        for a in range(16):
            import math
            ang = a * math.pi / 8
            img.set(round(8 + math.cos(ang) * 7), round(8 + math.sin(ang) * 7), hexc("f4cc20"))
            img.set(round(8 + math.cos(ang) * 6.5), round(8 + math.sin(ang) * 6.5), hexc("f8e070"))
    return img


reg("sunflower_front", lambda r: _sun_face(r, False))
reg("sunflower_back", lambda r: _sun_face(r, True))

# --- Grasses, ferns, bushes (greyscale, tinted) ----------------------------------------
reg("short_grass", lambda r: P.grass_cross(r, GREY, height=10))
reg("tall_grass_bottom", lambda r: P.grass_cross(r, GREY, height=16, blades=9))
reg("tall_grass_top", lambda r: P.grass_cross(r, GREY, height=12, blades=6))
reg("fern", lambda r: P.fern(r, GREY, top=4))
reg("large_fern_bottom", lambda r: P.fern(r, GREY, top=0))
reg("large_fern_top", lambda r: P.fern(r, GREY, top=3))
reg("dead_bush", lambda r: P.coral(r, ramp(hexc("7a5a32")), "plant"))
# (M28.5a: the 1.21.5 plants) a leafy bush (tinted), dry grasses, a firefly bush with
# glowing dots, a cactus flower, wildflowers and leaf litter (laid flat).
reg("bush", lambda r: P.grass_cross(r, GREY, height=9, blades=11))
DRY = ramp(hexc("c8aa6a"), spread=0.3)
reg("short_dry_grass", lambda r: P.grass_cross(r, DRY, height=8, blades=6))
reg("tall_dry_grass", lambda r: P.grass_cross(r, DRY, height=14, blades=8))


def _firefly_bush(rng):
    img = P.grass_cross(rng, ramp(hexc("4a6a3a"), spread=0.3), height=13, blades=10)
    for _ in range(6):
        img.set(rng.randrange(2, 14), rng.randrange(2, 12), hexc("e8f060"))
    return img


reg("firefly_bush", _firefly_bush)
reg("cactus_flower", lambda r: P.flower(r, ramp(hexc("5a9a3a")), ramp(hexc("e868a8")), hexc("f8c8e0"), "round"))
WILD = [hexc("f2e04a"), hexc("f8f8f0"), hexc("f0c040")]
reg("wildflowers", lambda r: M.speckle(Img(), r, WILD, 34))
LITTER = [hexc("9a6a2a"), hexc("b8803a"), hexc("7a5222"), hexc("c89a4a")]
reg("leaf_litter", lambda r: M.speckle(Img(), r, LITTER, 70))

# --- Mushrooms ----------------------------------------------------------------------------
MUSH_STEM = ramp(hexc("d8d0c0"), spread=0.15)
BROWN_CAP, RED_CAP = ramp(hexc("8e6a4a")), ramp(hexc("c8241e"))
reg("brown_mushroom", lambda r: P.mushroom(r, BROWN_CAP, MUSH_STEM, flat=True))
reg("red_mushroom", lambda r: P.mushroom(r, RED_CAP, MUSH_STEM, spots=hexc("f4f0e8")))
reg("brown_mushroom_block", lambda r: P.mushroom_block(r, BROWN_CAP))
reg("red_mushroom_block", lambda r: P.mushroom_block(r, RED_CAP, hexc("f4f0e8")))
reg("mushroom_stem", lambda r: M.speckle(M.mottled(r, MUSH_STEM, 2, 3), r, [MUSH_STEM[1]], 10))
reg("mushroom_block_inside", lambda r: M.speckle(M.mottled(r, ramp(hexc("d4b890")), 2, 3), r,
                                                 [hexc("b89a70")], 8))

# --- Crops --------------------------------------------------------------------------------
WHEAT_STALK, WHEAT_GRAIN = ramp(hexc("5a9a2a")), ramp(hexc("d8b040"))
for s in range(8):
    stalk = WHEAT_STALK if s < 6 else ramp(mix(hexc("5a9a2a"), hexc("c8a040"), (s - 5) / 2))
    reg(f"wheat_stage{s}", lambda r, s=s, st=stalk: P.crop(r, s, 8, st, WHEAT_GRAIN, produce_at=5))
for s in range(4):
    reg(f"carrots_stage{s}", lambda r, s=s: P.root_crop(r, s, 4, STEM, ramp(hexc("f08020"))))
    reg(f"potatoes_stage{s}", lambda r, s=s: P.root_crop(r, s, 4, STEM, ramp(hexc("c8a050"))))
    reg(f"beetroots_stage{s}", lambda r, s=s: P.root_crop(r, s, 4, ramp(hexc("4a8a3a")), ramp(hexc("a01a2a"))))
    reg(f"sweet_berry_bush_stage{s}",
        lambda r, s=s: P.crop(r, s, 4, ramp(hexc("3a6a3a")), ramp(hexc("d02040")) if s >= 2 else None, 2, 12))
for s in range(2):
    reg(f"torchflower_crop_stage{s}", lambda r, s=s: P.crop(r, s, 3, STEM, ramp(hexc("f08a2a")), 1, 10))
for s in range(5):
    reg(f"pitcher_crop_bottom_stage_{s}", lambda r, s=s: P.crop(r, s, 5, ramp(hexc("4a8ab0")), None, None, 16))
    reg(f"pitcher_crop_top_stage_{s}",
        lambda r, s=s: P.crop(r, max(0, s - 2), 3, ramp(hexc("4a8ab0")), ramp(hexc("8a4ae0")) if s >= 3 else None,
                              2, 12) if s >= 2 else Img())
COCOA = ramp(hexc("a0602a"))
for s in range(3):
    def _cocoa(r, s=s):
        img = Img()
        w, h = 2 + s, 3 + s * 2
        for y in range(4, 4 + h):
            for x in range(8 - w, 8 + w):
                img.set(x, y, COCOA[3] if x < 8 else COCOA[1])
        for y in range(1, 4):
            img.set(8, y, hexc("4a7a2a"))
        return img
    reg(f"cocoa_stage{s}", _cocoa)


def _gourd_stem(rng, attached):
    img = Img()
    for y in range(4, N):
        img.set(7, y, GREY[2])
        img.set(8, y, GREY[1])
    if attached:
        for x in range(9, 16):
            img.set(x, 5, GREY[3])
    return img


for g in ("melon", "pumpkin"):
    reg(f"{g}_stem", lambda r: _gourd_stem(r, False))
    reg(f"attached_{g}_stem", lambda r: _gourd_stem(r, True))

PUMPKIN = ramp(hexc("d8781a"))
MELON = ramp(hexc("6a9a22"))
GOURD_STALK = ramp(hexc("6a5a2a"))
reg("pumpkin_side", lambda r: P.gourd_side(r, PUMPKIN))
reg("pumpkin_top", lambda r: P.gourd_top(r, PUMPKIN, GOURD_STALK))
reg("carved_pumpkin", lambda r: P.carved_face(P.gourd_side(random.Random("carved/base"), PUMPKIN), PUMPKIN))
reg("jack_o_lantern", lambda r: P.carved_face(P.gourd_side(random.Random("carved/base"), PUMPKIN), PUMPKIN,
                                              hexc("ffd85a")))
reg("melon_side", lambda r: P.gourd_side(r, MELON))
reg("melon_top", lambda r: P.gourd_top(r, MELON, GOURD_STALK))

# --- Other plant blocks --------------------------------------------------------------------
CACTUS = ramp(hexc("4a8a2a"))
reg("cactus_side", lambda r: P.cactus_side(r, CACTUS))
reg("cactus_top", lambda r: P.cactus_top(r, CACTUS))
reg("cactus_bottom", lambda r: P.cactus_top(r, ramp(hexc("8aa060"))))
reg("sugar_cane", lambda r: P.cane(r, GREY))
reg("vine", lambda r: P.vine(r, GREY))
reg("glow_lichen", lambda r: P.vine(r, ramp(hexc("6ac0a8"), spread=0.3), density=34))
reg("lily_pad", lambda r: P.lily_pad(r, GREY))
HAY, HAY_BAND = ramp(hexc("c8a020")), ramp(hexc("8a3a1a"))
reg("hay_block_side", lambda r: P.bundle(r, HAY, HAY_BAND))
reg("hay_block_top", lambda r: P.bundle_top(r, HAY))
KELP_DRY = ramp(hexc("3a4a2a"))
reg("dried_kelp_side", lambda r: P.bundle(r, KELP_DRY, ramp(hexc("6a5a3a"))))
reg("dried_kelp_top", lambda r: P.bundle_top(r, KELP_DRY))
reg("dried_kelp_bottom", lambda r: P.bundle_top(r, KELP_DRY))
SPONGE = ramp(hexc("d0c040"))
reg("sponge", lambda r: P.sponge(r, SPONGE))
reg("wet_sponge", lambda r: P.sponge(r, ramp(hexc("a09a30")), wet=True))

# Bamboo plant
BAMBOO = ramp(hexc("5a8a2a"))
reg("bamboo_stalk", lambda r: P.cane(r, BAMBOO))
reg("bamboo_large_leaves", lambda r: P.vine(r, BAMBOO, density=40))
reg("bamboo_small_leaves", lambda r: P.vine(r, BAMBOO, density=20))
reg("bamboo_singleleaf", lambda r: P.ribbon(r, BAMBOO, tip=True))
reg("bamboo_stage0", lambda r: P.crop(r, 0, 3, BAMBOO))

# --- Underwater ---------------------------------------------------------------------------
KELP = ramp(hexc("4a8a2a"))
SEAGRASS = ramp(hexc("3a8a3a"))
reg("kelp", lambda r: P.ribbon(r, KELP, tip=True))
reg("kelp_plant", lambda r: P.ribbon(r, KELP))
reg("seagrass", lambda r: P.grass_cross(r, SEAGRASS, height=13))
reg("tall_seagrass_bottom", lambda r: P.grass_cross(r, SEAGRASS, height=16, blades=8))
reg("tall_seagrass_top", lambda r: P.grass_cross(r, SEAGRASS, height=11, blades=6))
reg("sea_pickle", lambda r: P.eggs(r, ramp(hexc("6a8a2a")), 3))
TURTLE = ramp(hexc("e8e4c8"), spread=0.15)
reg("turtle_egg", lambda r: P.eggs(r, TURTLE, 1))
reg("turtle_egg_slightly_cracked", lambda r: P.eggs(r, TURTLE, 1, 2))
reg("turtle_egg_very_cracked", lambda r: P.eggs(r, TURTLE, 1, 4))
reg("frogspawn", lambda r: P.eggs(r, ramp(hexc("5a6a7a")), 4))

CORALS = {"tube": "3050d0", "brain": "d050a0", "bubble": "a030c0", "fire": "c82a2a", "horn": "e0c830"}
DEAD = ramp(hexc("8a8480"), spread=0.22)
for name, hexstr in CORALS.items():
    pal = ramp(hexc(hexstr), spread=0.3)
    for shape, suffix in (("plant", "coral"), ("block", "coral_block"), ("fan", "coral_fan")):
        reg(f"{name}_{suffix}", lambda r, p=pal, s=shape: P.coral(r, p, s))
        reg(f"dead_{name}_{suffix}", lambda r, s=shape: P.coral(r, DEAD, s))


# Prismarine: animated colour drift (teal -> blue-green -> teal).
def _prismarine_frames(rng):
    base = M.tiles(random.Random("prismarine/layout"), ramp(hexc("5aa89a")), hexc("2a5a54"), size=4)
    frames = []
    for i, k in enumerate((0.0, 0.25, 0.5, 0.25)):
        target = (60, 120, 170, 255)
        frames.append(base.map(lambda c, k=k: mix(c, target, k * 0.5) if c[3] else c))
    return stack_frames(frames)


reg("prismarine", _prismarine_frames, animated(40))
reg("prismarine_bricks", lambda r: M.bricks(r, ramp(hexc("62ae9e")), hexc("3a6a62"), brick_w=8))
reg("dark_prismarine", lambda r: M.tiles(r, ramp(hexc("33584c")), hexc("1e3a32"), size=8))


def _sea_lantern(rng):
    frames = []
    glow = ramp(hexc("cfe8e0"), spread=0.2)
    for i in range(5):
        img = M.tiles(random.Random("sea_lantern/layout"), glow, hexc("7aa8a0"), size=8)
        for k in range(4):  # moving light glints
            x = (i * 3 + k * 4) % 16
            img.set(x, (k * 4 + 2) % 16, (255, 255, 255, 255))
        frames.append(img)
    return stack_frames(frames)


reg("sea_lantern", _sea_lantern, animated(5))

# --- Lush caves, azalea, dripleaf, pale garden ---------------------------------------------
LUSH = ramp(hexc("5a8a30"))
BERRY = ramp(hexc("e8a020"), spread=0.3)
reg("cave_vines", lambda r: P.hanging(r, LUSH, BERRY, tip=True))
reg("cave_vines_lit", lambda r: P.hanging(r, LUSH, BERRY, lit=True, tip=True))
reg("cave_vines_plant", lambda r: P.hanging(r, LUSH, BERRY))
reg("cave_vines_plant_lit", lambda r: P.hanging(r, LUSH, BERRY, lit=True))
reg("hanging_roots", lambda r: P.hanging(r, ramp(hexc("a07a5a"))))
SPORE = ramp(hexc("e070a0"))
reg("spore_blossom", lambda r: P.coral(r, SPORE, "fan"))
reg("spore_blossom_base", lambda r: P.lily_pad(r, LUSH))
AZALEA_LEAF = ramp(hexc("5a8a2a"))
FLOWER_PINK = ramp(hexc("d070c0"))


def _flowering(img_fn, rng):
    img = img_fn(rng)
    for _ in range(8):
        x, y = rng.randrange(16), rng.randrange(16)
        if img.get(x, y)[3]:
            img.set(x, y, FLOWER_PINK[3])
            img.set(x + 1, y, FLOWER_PINK[2])
    return img


reg("azalea_leaves", lambda r: M.leaves(r, AZALEA_LEAF))
reg("flowering_azalea_leaves", lambda r: _flowering(lambda q: M.leaves(q, AZALEA_LEAF), r))
reg("azalea_top", lambda r: M.leaves(r, AZALEA_LEAF, holes=0.08))
reg("flowering_azalea_top", lambda r: _flowering(lambda q: M.leaves(q, AZALEA_LEAF, holes=0.08), r))
reg("azalea_side", lambda r: M.sapling(r, ramp(hexc("6a5030")), AZALEA_LEAF, "round"))
reg("flowering_azalea_side", lambda r: _flowering(lambda q: M.sapling(q, ramp(hexc("6a5030")), AZALEA_LEAF), r))
reg("azalea_plant", lambda r: P.crop(r, 2, 3, ramp(hexc("6a5030"))))
DRIPLEAF = ramp(hexc("6aa02a"))
reg("big_dripleaf_top", lambda r: P.lily_pad(r, DRIPLEAF))
reg("big_dripleaf_side", lambda r: M.leaves(r, DRIPLEAF, holes=0.0, clusters=4))
reg("big_dripleaf_stem", lambda r: P.ribbon(r, DRIPLEAF))
reg("big_dripleaf_tip", lambda r: P.ribbon(r, DRIPLEAF, tip=True))
reg("small_dripleaf_top", lambda r: P.lily_pad(r, DRIPLEAF))
reg("small_dripleaf_side", lambda r: P.ribbon(r, DRIPLEAF, tip=True))
reg("small_dripleaf_stem_top", lambda r: P.ribbon(r, DRIPLEAF, tip=True))
reg("small_dripleaf_stem_bottom", lambda r: P.ribbon(r, DRIPLEAF))
PETALS = ramp(hexc("f0a8c8"))
reg("pink_petals", lambda r: M.speckle(Img(), r, [PETALS[3], PETALS[2], PETALS[4]], 40))
reg("pink_petals_stem", lambda r: P.crop(r, 0, 4, STEM))
PALE_MOSS = ramp(hexc("8a9488"), spread=0.25)
reg("pale_moss_block", lambda r: M.blades(r, PALE_MOSS, count=20))
reg("pale_moss_carpet", lambda r: M.blades(r, PALE_MOSS, count=24))
reg("pale_hanging_moss", lambda r: P.hanging(r, PALE_MOSS))
reg("pale_hanging_moss_tip", lambda r: P.hanging(r, PALE_MOSS, tip=True))
