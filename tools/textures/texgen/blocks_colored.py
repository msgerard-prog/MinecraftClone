"""Batch 3: the 16 colours (wool, concrete, powder, terracotta, glazed terracotta,
stained glass, panes, shulker boxes, candles) plus their uncoloured versions.

The 16 dye colours below are our own picks.
"""
from . import materials as M
from .core import hexc, mix, ramp, scale
from .registry import add

DYES = {
    "white": "e6e9e9", "orange": "ee7518", "magenta": "b947b0", "light_blue": "3daed6",
    "yellow": "f5c22c", "lime": "6db51e", "pink": "ea8cab", "gray": "40464a",
    "light_gray": "8c8c85", "cyan": "178892", "purple": "7630a8", "blue": "373c9a",
    "brown": "70482a", "green": "526b1e", "red": "9f2a24", "black": "17181b",
}
TERRACOTTA_BASE = hexc("98604a")  # plain fired clay colour


def reg(name, fn, mcmeta=None):
    add(name, fn, "colored", mcmeta)


def palettes(hexstr):
    base = hexc(hexstr)
    clay = mix(base, TERRACOTTA_BASE, 0.45)        # terracotta: dye mixed into clay
    accent = ramp(mix(base, (255, 255, 255, 255), 0.45))
    return dict(pal=ramp(base), clay=ramp(scale(clay, 0.92), spread=0.18), accent=accent,
                dark=scale(base, 0.45))


# Motif per colour, cycled so neighbouring colours never share one.
MOTIFS = ("petal", "zigzag", "corner", "wave", "chain", "ring")

for i, (color, hexstr) in enumerate(DYES.items()):
    p = palettes(hexstr)
    reg(f"{color}_wool", lambda r, p=p: M.wool(r, p["pal"]))
    reg(f"{color}_concrete", lambda r, p=p: M.concrete(r, p["pal"]))
    reg(f"{color}_concrete_powder", lambda r, p=p: M.grainy(r, p["pal"], dark=12, bright=10))
    reg(f"{color}_terracotta", lambda r, p=p: M.terracotta(r, p["clay"]))
    reg(f"{color}_glazed_terracotta",
        lambda r, p=p, m=MOTIFS[i % len(MOTIFS)]: M.glazed(r, p["pal"], p["accent"], p["dark"], m))
    reg(f"{color}_stained_glass", lambda r, p=p: M.glass(r, p["pal"], 120))
    reg(f"{color}_stained_glass_pane_top", lambda r, p=p: M.pane_top(p["pal"]))
    reg(f"{color}_shulker_box", lambda r, p=p: M.shell(r, p["pal"]))
    reg(f"{color}_candle", lambda r, p=p: M.candle(r, p["pal"], False))
    reg(f"{color}_candle_lit", lambda r, p=p: M.candle(r, p["pal"], True))

GLASS = ramp(hexc("c8dce0"), spread=0.25)
TINTED = ramp(hexc("2c2433"), spread=0.25)
CANDLE = ramp(hexc("e8d6a6"), spread=0.2)
SHULKER = ramp(hexc("8e5f8e"))
reg("terracotta", lambda r: M.terracotta(r, ramp(TERRACOTTA_BASE, spread=0.18)))
reg("glass", lambda r: M.glass(r, GLASS, 0))
reg("glass_pane_top", lambda r: M.pane_top(GLASS))
reg("tinted_glass", lambda r: M.glass(r, TINTED, 190, 245))
reg("candle", lambda r: M.candle(r, CANDLE, False))
reg("candle_lit", lambda r: M.candle(r, CANDLE, True))
reg("shulker_box", lambda r: M.shell(r, SHULKER))
