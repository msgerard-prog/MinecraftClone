"""Batch 6b: remaining block textures (cake, item frames, heavy core, creaking heart,
resin, ominous trial blocks, creative-only technical blocks)."""
import random

from . import materials as M
from . import utility as U
from .core import N, Img, frame, hexc, ramp, scale
from .registry import add


def reg(name, fn, mcmeta=None):
    add(name, fn, "misc", mcmeta)


CREAM = ramp(hexc("f2ece0"), spread=0.12)
SPONGE_CAKE = ramp(hexc("c8904a"))
BERRY = hexc("c8202a")


def _cake(rng, face):
    if face == "top":
        img = M.speckle(M.mottled(rng, CREAM, 2, 3), rng, [BERRY], 6)
        frame(img, CREAM[4], CREAM[1])
        return img
    if face == "bottom":
        return M.speckle(M.mottled(rng, SPONGE_CAKE, 1, 3), rng, [SPONGE_CAKE[0]], 10)
    img = M.speckle(M.mottled(rng, SPONGE_CAKE, 1, 3), rng, [SPONGE_CAKE[0]], 10)
    for x in range(N):  # icing on top, dripping a little
        for y in range(0, 3 + (x % 5 == 2)):
            img.set(x, y, CREAM[3])
    if face == "inner":
        for x in range(N):
            img.set(x, 8, hexc("e8d0a0"))
    return img


for face in ("top", "side", "bottom", "inner"):
    reg(f"cake_{face}", lambda r, f=face: _cake(r, f))
FRAME_WOOD = ramp(hexc("8a5a30"))
LEATHER = ramp(hexc("a06a3a"))
reg("item_frame", lambda r: U.framed(M.mottled(r, LEATHER, 1, 3), FRAME_WOOD))
reg("glow_item_frame", lambda r: U.framed(M.speckle(M.mottled(r, LEATHER, 1, 3), r, [hexc("f0e070")], 8),
                                          ramp(hexc("6aa8a0"))))
reg("heavy_core", lambda r: M.metal_block(r, ramp(hexc("4a4a56"), spread=0.25)))

PALE_BARK = ramp(hexc("5a524e"), spread=0.22)
RESIN = ramp(hexc("e07a2a"), spread=0.3)


def _heart(rng, top, active):
    img = M.bark(random.Random("heart/bark"), PALE_BARK) if not top else \
        M.log_end(random.Random("heart/top"), ramp(hexc("ddd2cc"), spread=0.15), PALE_BARK)
    core = RESIN if active else ramp(hexc("6a5a50"))
    for y in range(5, 11):
        for x in range(6, 10):
            img.set(x, y, core[3] if (x + y) % 2 else core[2])
    return img


reg("creaking_heart", lambda r: _heart(r, False, False))
reg("creaking_heart_top", lambda r: _heart(r, True, False))
reg("creaking_heart_active", lambda r: _heart(r, False, True))
reg("creaking_heart_top_active", lambda r: _heart(r, True, True))
reg("resin_block", lambda r: M.cells(r, RESIN, 9, RESIN[0], shades=(1, 2, 3, 3), edge_width=0.6))
reg("resin_bricks", lambda r: M.bricks(r, RESIN, scale(RESIN[0], 0.7), rows=4, brick_w=8))
reg("chiseled_resin_bricks", lambda r: M.chiseled(r, RESIN, "sun"))
reg("resin_clump", lambda r: M.speckle(Img(), r, [RESIN[3], RESIN[2], RESIN[4]], 28))

# Ominous trial spawner / vault: same build, cold blue glow.
TRIAL = ramp(hexc("4a4a52"), spread=0.25)
OMINOUS = ramp(hexc("3a8ae0"), spread=0.3)
for state, lit in (("inactive", False), ("active", True), ("ejecting_reward", True)):
    reg(f"trial_spawner_top_{state}_ominous", lambda r, lit=lit: U.window_grid(
        U.framed(M.metal_block(r, TRIAL), OMINOUS if lit else TRIAL), TRIAL, 2, 2, 13, 13, 4))
for state, lit in (("inactive", False), ("active", True)):
    reg(f"trial_spawner_side_{state}_ominous", lambda r, lit=lit: U.window_grid(
        U.framed(Img(fill=TRIAL[2]), OMINOUS if lit else TRIAL), TRIAL, 1, 1, 14, 14, 3))
for name, lit in (("vault_side_off_ominous", False), ("vault_side_on_ominous", True),
                  ("vault_front_off_ominous", False), ("vault_front_on_ominous", True),
                  ("vault_front_ejecting_ominous", True)):
    reg(name, lambda r, lit=lit: U.window_grid(U.framed(M.metal_block(r, TRIAL), OMINOUS if lit else TRIAL),
                                              TRIAL, 3, 3, 12, 12, 9))
reg("vault_top_ominous", lambda r: M.metal_block(r, TRIAL))
reg("vault_bottom_ominous", lambda r: M.metal_block(r, TRIAL))

# Creative-only technical blocks: plain, clearly "technical" designs.
TECH = {"command_block": "c08a5a", "repeating_command_block": "7a5ac8", "chain_command_block": "5aa88a"}


def _tech(rng, pal, face, mark):
    img = U.framed(M.tiles(random.Random(f"tech/{face}"), pal, pal[0], size=8), pal)
    if face in ("front", "back"):
        for y in range(5, 11):
            for x in range(5, 11):
                img.set(x, y, hexc("1a1a1a"))
        img.set(7, 7, mark)
        img.set(8, 8, mark)
    elif face == "conditional":
        for i in range(4, 12):
            img.set(i, 4, mark)
            img.set(4, i, mark)
    return img


for name, hx in TECH.items():
    pal = ramp(hexc(hx), spread=0.25)
    for face in ("front", "back", "side", "conditional"):
        reg(f"{name}_{face}", lambda r, p=pal, f=face: _tech(r, p, f, hexc("e0e0e0")))
STRUCT = ramp(hexc("5a5a6a"), spread=0.25)
for mode, color in (("", "e0e0e0"), ("_corner", "e8c040"), ("_data", "40c8e8"), ("_load", "60e060"),
                    ("_save", "e86060")):
    reg(f"structure_block{mode}", lambda r, c=color: _tech(r, STRUCT, "front", hexc(c)))
JIGSAW = ramp(hexc("6a5a8a"), spread=0.25)
for face in ("top", "bottom", "side", "lock"):
    reg(f"jigsaw_{face}", lambda r, f=face: _tech(r, JIGSAW, "front" if f == "top" else "side", hexc("e8c040")))
