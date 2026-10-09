"""Batch 2: every wood type — planks, logs, stripped logs, leaves, saplings, doors,
trapdoors, plus mangrove roots, bamboo and the Nether stems.

Base colours are our own picks from a description of each wood.
"""
import random

from . import materials as M
from .core import Img, grey, hexc, mix, ramp
from .registry import add

# Oak keeps the hand-tuned palettes from the first texture pass.
OAK_PLANKS = [(98, 70, 38, 255), (132, 100, 56, 255), (158, 124, 72, 255), (180, 145, 88, 255),
              (204, 170, 108, 255)]
OAK_BARK = [(40, 29, 18, 255), (62, 46, 28, 255), (86, 64, 38, 255), (108, 82, 50, 255),
            (128, 100, 62, 255)]
OAK_END = [(132, 100, 56, 255), (158, 124, 72, 255), (184, 150, 92, 255), (206, 174, 112, 255),
           (220, 190, 130, 255)]

# name: planks, bark, log end ring, stripped, leaf (None = greyscale tinted),
#       sapling shape, door style
WOODS = {
    "oak": dict(planks=OAK_PLANKS, bark=OAK_BARK, end=OAK_END, stripped=ramp(hexc("b0874e")),
                leaf=None, sapling="round", door="window", trapdoor="window"),
    "spruce": dict(planks=ramp(hexc("6f5131")), bark=ramp(hexc("3d2a19"), spread=0.22),
                   end=ramp(hexc("80603a")), stripped=ramp(hexc("745a35")), leaf=None,
                   sapling="cone", door="boards", trapdoor="panel"),
    "birch": dict(planks=ramp(hexc("c4b27b"), spread=0.2), bark=ramp(hexc("d8d8cf"), spread=0.18),
                  end=ramp(hexc("cbb98a"), spread=0.2), stripped=ramp(hexc("c8b580"), spread=0.2),
                  leaf=None, sapling="round", door="grid", trapdoor="grid"),
    "jungle": dict(planks=ramp(hexc("a0734f")), bark=ramp(hexc("554320")), end=ramp(hexc("aa7850")),
                   stripped=ramp(hexc("ab8458")), leaf=None, sapling="tall", door="lattice",
                   trapdoor="lattice"),
    "acacia": dict(planks=ramp(hexc("aa5a32")), bark=ramp(hexc("675f55")), end=ramp(hexc("b05e34")),
                   stripped=ramp(hexc("ae5d3a")), leaf=None, sapling="flat", door="lattice",
                   trapdoor="grid"),
    "dark_oak": dict(planks=ramp(hexc("432b14"), spread=0.22), bark=ramp(hexc("3a2c18"), spread=0.2),
                     end=ramp(hexc("553a1e")), stripped=ramp(hexc("4a3420")), leaf=None,
                     sapling="round", door="panel", trapdoor="panel"),
    "mangrove": dict(planks=ramp(hexc("753630")), bark=ramp(hexc("4c3a28")), end=ramp(hexc("6e322d")),
                     stripped=ramp(hexc("7a3833")), leaf=None, sapling=None, door="window",
                     trapdoor="window"),
    "cherry": dict(planks=ramp(hexc("e2b2ac"), spread=0.22), bark=ramp(hexc("36212c"), spread=0.22),
                   end=ramp(hexc("d6a09a")), stripped=ramp(hexc("d89c96")),
                   leaf=ramp(hexc("eaa3c4"), spread=0.25), sapling="round", door="window",
                   trapdoor="window"),
    "pale_oak": dict(planks=ramp(hexc("e2d8d4"), spread=0.14), bark=ramp(hexc("5a524e"), spread=0.22),
                     end=ramp(hexc("ddd2cc"), spread=0.15), stripped=ramp(hexc("ddd3cf"), spread=0.14),
                     leaf=ramp(hexc("8a948a"), spread=0.25), sapling="round", door="grid",
                     trapdoor="grid"),
    # (M33.3a; 26.3) poplar: pale golden planks, grey-white bark with dark marks
    "poplar": dict(planks=ramp(hexc("d8b48a"), spread=0.16), bark=ramp(hexc("9a948a"), spread=0.2),
                   end=ramp(hexc("d4b088")), stripped=ramp(hexc("dcbc94"), spread=0.15),
                   leaf=ramp(hexc("e8b830"), spread=0.3), sapling="tall", door="grid", trapdoor="grid"),
}
LEAF_GREY = [grey(70), grey(100), grey(130), grey(160), grey(196)]
SAPLING_LEAF = {
    "oak": ramp(hexc("4f8a2c")), "spruce": ramp(hexc("3d6b3d")), "birch": ramp(hexc("6c9a40")),
    "jungle": ramp(hexc("3f8a1e")), "acacia": ramp(hexc("6b8a22")), "dark_oak": ramp(hexc("33621c")),
    "cherry": ramp(hexc("eaa3c4")), "pale_oak": ramp(hexc("8a948a")), "poplar": ramp(hexc("e8b830")),
}
BIRCH_MARK = [hexc("2a2622"), hexc("5a544c")]


def reg(name, fn, mcmeta=None):
    add(name, fn, "wood", mcmeta)


for wood, w in WOODS.items():
    bark_fn = (lambda r, w=w: M.birch_bark(r, w["bark"], BIRCH_MARK)) if wood == "birch" else \
        (lambda r, w=w: M.bark(r, w["bark"]))
    reg(f"{wood}_planks", lambda r, w=w: M.planks(r, w["planks"]))
    reg(f"{wood}_log", bark_fn)
    reg(f"{wood}_log_top", lambda r, w=w: M.log_end(r, w["end"], w["bark"]))
    reg(f"stripped_{wood}_log", lambda r, w=w: M.stripped(r, w["stripped"]))
    reg(f"stripped_{wood}_log_top", lambda r, w=w: M.log_end(r, w["end"], w["stripped"]))
    reg(f"{wood}_leaves", lambda r, w=w: M.leaves(r, w["leaf"] or LEAF_GREY))
    if w["sapling"]:
        reg(f"{wood}_sapling",
            lambda r, w=w, wood=wood: M.sapling(r, w["bark"], SAPLING_LEAF[wood], w["sapling"]))
    reg(f"{wood}_door_top", lambda r, w=w: M.door(r, w["planks"], "top", w["door"]))
    reg(f"{wood}_door_bottom", lambda r, w=w: M.door(r, w["planks"], "bottom", w["door"]))
    reg(f"{wood}_trapdoor", lambda r, w=w: M.trapdoor(r, w["planks"], w["trapdoor"]))


# (M33.3a) poplar leaves in three autumn colours
for _colour, _hex in (("red", "c8402a"), ("orange", "e07a24"), ("yellow", "e8b830")):
    reg(f"{_colour}_poplar_leaves", lambda r, h=_hex: M.leaves(r, ramp(hexc(h), spread=0.3)))


# --- Mangrove specials ---------------------------------------------------------------

MANGROVE_ROOT = ramp(hexc("4d3b2a"))
MUD = ramp(hexc("3d3a3e"), spread=0.2)
PROPAGULE = ramp(hexc("6e9a3a"))


def _propagule(rng, hanging):
    img = Img()
    top = 1 if hanging else 4
    for y in range(top, top + 9):          # the seed pod: a long green spindle
        w = 1 if y in (top, top + 8) else 2
        for x in range(8 - w, 8 + w):
            img.set(x, y, PROPAGULE[3] if x < 8 else PROPAGULE[1])
    for x in (6, 7, 8, 9):                 # leaf collar on top
        img.set(x, top - 1 if hanging else top, PROPAGULE[4])
    if hanging:
        img.set(7, 0, hexc("5a4630"))
        img.set(8, 0, hexc("5a4630"))
    else:
        for y in range(top + 9, 16):
            img.set(7, y, hexc("5a4630"))
    return img


reg("mangrove_propagule", lambda r: _propagule(r, False))
reg("mangrove_propagule_hanging", lambda r: _propagule(r, True))
reg("mangrove_roots_side", lambda r: M.roots(r, MANGROVE_ROOT))
reg("mangrove_roots_top", lambda r: M.roots(r, MANGROVE_ROOT, count=9))
reg("muddy_mangrove_roots_side",
    lambda r: M.roots(r, MANGROVE_ROOT, M.speckle(M.mottled(random.Random("mmr/s"), MUD), r, [MUD[4]], 8)))
reg("muddy_mangrove_roots_top",
    lambda r: M.roots(r, MANGROVE_ROOT, M.speckle(M.mottled(random.Random("mmr/t"), MUD), r, [MUD[4]], 8), 9))

# --- Bamboo ---------------------------------------------------------------------------

BAMBOO_GREEN = ramp(hexc("6b8a2a"))
BAMBOO_YELLOW = ramp(hexc("c1ad50"), spread=0.22)
reg("bamboo_block", lambda r: M.stalks(r, BAMBOO_GREEN))
reg("bamboo_block_top", lambda r: M.stalk_ends(r, BAMBOO_YELLOW, BAMBOO_GREEN))
reg("stripped_bamboo_block", lambda r: M.stalks(r, BAMBOO_YELLOW))
reg("stripped_bamboo_block_top", lambda r: M.stalk_ends(r, BAMBOO_YELLOW, BAMBOO_YELLOW))
reg("bamboo_planks", lambda r: M.stalks(r, BAMBOO_YELLOW, vertical=False))
reg("bamboo_mosaic", lambda r: M.tiles(r, BAMBOO_YELLOW, BAMBOO_YELLOW[0], size=8))
reg("bamboo_door_top", lambda r: M.door(r, BAMBOO_YELLOW, "top", "stalks"))
reg("bamboo_door_bottom", lambda r: M.door(r, BAMBOO_YELLOW, "bottom", "stalks"))
reg("bamboo_trapdoor", lambda r: M.trapdoor(r, BAMBOO_YELLOW, "stalks"))

# --- Nether stems (fungus "wood") ---------------------------------------------------

STEMS = {
    "crimson": dict(planks=ramp(hexc("6a2f47")), stem=ramp(hexc("5c1d22")), glow=ramp(hexc("d63a4f")),
                    inner=ramp(hexc("8f3a55")), stripped=ramp(hexc("8a3a55"))),
    "warped": dict(planks=ramp(hexc("2b6a63")), stem=ramp(hexc("3a3a4d")), glow=ramp(hexc("14b8a6")),
                   inner=ramp(hexc("2e8a80")), stripped=ramp(hexc("3a8e86"))),
}


def _stem(rng, s):
    img = M.bark(rng, s["stem"])
    for _ in range(5):  # glowing streaks running up the stem
        x, y = rng.randrange(16), rng.randrange(16)
        for i in range(rng.randrange(3, 7)):
            img.set(x, y + i, s["glow"][3] if i else s["glow"][4])
    return img


for name, s in STEMS.items():
    reg(f"{name}_stem", lambda r, s=s: _stem(r, s))
    reg(f"{name}_stem_top", lambda r, s=s: M.log_end(r, s["inner"], s["stem"]))
    reg(f"stripped_{name}_stem", lambda r, s=s: M.stripped(r, s["stripped"]))
    reg(f"stripped_{name}_stem_top", lambda r, s=s: M.log_end(r, s["inner"], s["stripped"]))
    reg(f"{name}_planks", lambda r, s=s: M.planks(r, s["planks"]))
    reg(f"{name}_door_top", lambda r, s=s: M.door(r, s["planks"], "top", "lattice"))
    reg(f"{name}_door_bottom", lambda r, s=s: M.door(r, s["planks"], "bottom", "lattice"))
    reg(f"{name}_trapdoor", lambda r, s=s: M.trapdoor(r, s["planks"], "lattice"))


# --- Shelves (M29.5/M29.6, 1.21.9; wiki: Shelf) ----------------------------------------
# A plank frame round a dark recessed band where its three items stand.
def _shelf(rng, pal):
    img = M.planks(rng, pal)
    for y in range(3, 13):
        for x in range(1, 15):
            img.set(x, y, pal[0] if y in (3, 12) else mix(pal[0], (0, 0, 0, 255), 0.35))
    for y in range(3, 13):
        img.set(0, y, pal[1])
        img.set(15, y, pal[1])
    return img


_SHELF_WOODS = {name: w["planks"] for name, w in WOODS.items()}
_SHELF_WOODS.update({"bamboo": BAMBOO_YELLOW, "crimson": STEMS["crimson"]["planks"],
                     "warped": STEMS["warped"]["planks"]})
for name, pal in _SHELF_WOODS.items():
    reg(f"{name}_shelf", lambda r, pal=pal: _shelf(r, pal))
