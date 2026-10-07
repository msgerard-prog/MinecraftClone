"""Texture registry: vanilla texture name -> painter.

Each entry: fn(rng) -> Img, a family name (for previews/reports) and an optional
.mcmeta text (animated strips). Names are vanilla's file stems so a real resource
pack overrides our art file for file.
"""

TEXTURES = {}


def add(name, fn, family, mcmeta=None):
    if name in TEXTURES:
        raise ValueError(f"texture registered twice: {name}")
    TEXTURES[name] = (fn, family, mcmeta)


def animated(frametime):
    return '{\n  "animation": {\n    "frametime": %d\n  }\n}\n' % frametime
