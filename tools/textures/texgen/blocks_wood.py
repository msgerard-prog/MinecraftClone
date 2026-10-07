"""Batch 2: woods. (Oak only so far; the other woods are added in batch 2.)"""
from . import materials as M
from .core import hexc, ramp
from .registry import add

PLANKS = [(98, 70, 38, 255), (132, 100, 56, 255), (158, 124, 72, 255), (180, 145, 88, 255),
          (204, 170, 108, 255)]
BARK = [(40, 29, 18, 255), (62, 46, 28, 255), (86, 64, 38, 255), (108, 82, 50, 255),
        (128, 100, 62, 255)]
LOG_END = [(132, 100, 56, 255), (158, 124, 72, 255), (184, 150, 92, 255), (206, 174, 112, 255),
           (220, 190, 130, 255)]


def reg(name, fn, mcmeta=None):
    add(name, fn, "wood", mcmeta)


reg("oak_planks", lambda r: M.planks(r, PLANKS))
reg("oak_log", lambda r: M.bark(r, BARK))
reg("oak_log_top", lambda r: M.log_end(r, LOG_END, BARK))
