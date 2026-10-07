---
name: add-asset
description: Create a new placeholder texture or other asset (our own, never Mojang's) and wire it into the resource pack layout. Use when a block, item, entity or UI element needs a texture.
---
# Add an asset

Textures are **original art in Minecraft's style, sharper than vanilla**
(docs/art-style.md, ADR 0006). Read docs/art-style.md before making one.

1. **Never copy** from a vanilla jar, the internet, or `resourcepacks/` — no tracing,
   colour sampling or pixel-by-pixel redraws (ADR 0004/0006). Work from a written
   description of the material.
2. Path follows vanilla's layout so a real pack overrides it file for file:
   `assets/minecraft/textures/{block,item,entity,gui}/<vanilla_name>.png`.
3. Register it in the matching `tools/textures/texgen/blocks_*.py` table: a palette
   from `ramp(hexc("base"))` and a painter from `texgen/materials.py` (add a new
   painter there if no existing one fits). Tinted materials (leaves, grass, water)
   are greyscale; add them to `PREVIEW_TINT` in gen_textures.py.
4. Animated textures: vertical strip of N 16x16 frames + `<name>.png.mcmeta`
   (`{"animation":{"frametime":2}}`); the atlas animates it per tick.
5. Run `tools/textures/gen_textures.py --preview out/screenshots/tex` and Read the
   family's sheet: crisp shapes, readable material, no colour cast.
   Only the new texture's PNG should change (`git status`).
6. No registration needed: `TextureAtlas` stitches every PNG in `textures/block/`.
   Reference it by file stem. Then `visual-check` in game (close-up and far).
   Commit the PNG and the generator change together.
