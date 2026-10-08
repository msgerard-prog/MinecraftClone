#!/usr/bin/env python3
"""Mob head block textures (M26.4b): cut from our own mob skins (gen_entities.py).

Vanilla draws heads with each mob's entity texture; our block renderer uses the block
atlas, so each head gets 16x16 block textures of its own - its skin's head faces scaled
up: assets/minecraft/textures/block/clone_head_<kind>_{front,back,side,top}.png. The
"clone_" prefix keeps them apart from vanilla's names (no resource pack overrides them).
Usage: tools/textures/gen_heads.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import gen_entities as ge  # noqa: E402
from texgen.core import CLEAR, Img, encode_png  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"


def face16(img, region):
    """A face region of a skin, scaled (nearest) to 16x16."""
    x0, y0, w, h = region
    out = Img(16, 16, CLEAR)
    for y in range(16):
        for x in range(16):
            out.set(x, y, img.get(x0 + x * w // 16, y0 + y * h // 16))
    return out


def main():
    # kind -> (skin, head box (u, v, w, h, d))
    heads = {
        "skeleton": (ge.skeleton(), (0, 0, 8, 8, 8)),
        "wither_skeleton": (ge.skeleton("wither_skeleton", "#3A3A3C"), (0, 0, 8, 8, 8)),
        "zombie": (ge.zombie(), (0, 0, 8, 8, 8)),
        "creeper": (ge.creeper(), (0, 0, 8, 8, 8)),
        "piglin": (ge.piglin(), (0, 0, 10, 8, 8)),
        "dragon": (ge.ender_dragon(), (24, 22, 6, 5, 8)),
    }
    for kind, (skin, box) in heads.items():
        faces = ge.box_faces(*box)
        for name, region in (("front", faces["front"]), ("back", faces["back"]), ("side", faces["right"]),
                             ("top", faces["top"])):
            (OUT / f"clone_head_{kind}_{name}.png").write_bytes(encode_png(face16(skin, region)))
    print(f"wrote {len(heads) * 4} head textures to {OUT}")


if __name__ == "__main__":
    main()
