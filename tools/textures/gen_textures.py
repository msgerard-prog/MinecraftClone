#!/usr/bin/env python3
"""Generate every block texture: original Minecraft-style art (docs/art-style.md).

Writes assets/minecraft/textures/block/<name>.png (+ .mcmeta for animations) for
every entry in the registry and removes stale files there. Deterministic: each
texture's RNG is seeded by its name.

Usage:
  tools/textures/gen_textures.py                     regenerate everything
  tools/textures/gen_textures.py --preview DIR       also write one preview sheet per
                                                     family (4x, transparency on a
                                                     checker, tinted textures tinted)
  tools/textures/gen_textures.py --only stone,dirt   regenerate just these
"""
import argparse
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen import blocks_stone, blocks_wood  # noqa: F401,E402  (registration)
from texgen.core import Img, encode_png, tint  # noqa: E402
from texgen.registry import TEXTURES  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "assets/minecraft/textures/block"

# Textures the game multiplies by a biome/fixed colour; previews show them tinted.
PREVIEW_TINT = {
    "grass_block_top": (0x91, 0xBD, 0x59),
    "grass_block_side_overlay": (0x91, 0xBD, 0x59),
}


def render(name):
    fn, family, mcmeta = TEXTURES[name]
    img = fn(random.Random(name))
    assert img.w == 16 and img.h % 16 == 0, f"{name}: bad size {img.w}x{img.h}"
    return img, family, mcmeta


def preview(images, path):
    scale, gap, cols = 4, 4, 16
    cell = 16 * scale + gap
    rows = (len(images) + cols - 1) // cols
    sheet = Img(cols * cell + gap, rows * cell + gap, (255, 255, 255, 255))
    for i, (name, img) in enumerate(images):
        ox, oy = gap + (i % cols) * cell, gap + (i // cols) * cell
        t = PREVIEW_TINT.get(name)
        for y in range(16 * scale):
            for x in range(16 * scale):
                c = img.get(x // scale, y // scale)  # first frame of animations
                if t:
                    c = tint(c, t)
                checker = (200, 200, 200, 255) if ((x // 8 + y // 8) % 2) else (150, 150, 150, 255)
                a = c[3] / 255
                c = tuple(int(c[k] * a + checker[k] * (1 - a)) for k in range(3)) + (255,)
                sheet.set(ox + x, oy + y, c)
    Path(path).write_bytes(encode_png(sheet))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="directory for preview sheets")
    ap.add_argument("--only", help="comma-separated texture names")
    args = ap.parse_args()
    names = sorted(TEXTURES) if not args.only else args.only.split(",")
    OUT.mkdir(parents=True, exist_ok=True)

    families = {}
    for name in names:
        img, family, mcmeta = render(name)
        (OUT / f"{name}.png").write_bytes(encode_png(img))
        meta = OUT / f"{name}.png.mcmeta"
        if mcmeta:
            meta.write_text(mcmeta)
        elif meta.exists():
            meta.unlink()
        families.setdefault(family, []).append((name, img))

    if not args.only:  # remove files that are no longer registered
        for f in OUT.iterdir():
            stem = f.name.split(".png")[0]
            if f.suffix in (".png", ".mcmeta") and stem not in TEXTURES:
                f.unlink()
                print(f"removed stale {f.name}")

    print(f"wrote {len(names)} textures to {OUT}")
    for family, items in sorted(families.items()):
        print(f"  {family}: {len(items)}")
    if args.preview:
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)
        for family, items in families.items():
            path = out / f"tex-{family.replace(' ', '_').replace('&', 'and')}.png"
            preview(items, path)
            print(f"preview: {path}  (order: {', '.join(n for n, _ in items)})")


if __name__ == "__main__":
    main()
