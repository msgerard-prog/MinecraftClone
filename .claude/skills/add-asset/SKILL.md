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
3. Design it in `tools/textures/gen_placeholders.py`: add a palette ramp (4–5 shades,
   cool shadows, warm highlights) at the top, a function using the helpers (`fbm`,
   `voronoi`, `bevel`, `ramp`), and an entry in `TEXTURES`. Tinted materials
   (leaves, grass, water) are greyscale.
4. Animated textures: vertical strip of N 16x16 frames + `<name>.png.mcmeta`
   (`{"animation":{"frametime":2}}`); the atlas animates it per tick.
5. Run `tools/textures/gen_placeholders.py --preview out/screenshots/tex.png` and
   Read the preview: crisp shapes, readable material, no seams in the 3x3 tiling.
   Only the new texture's PNG should change (`git status`).
6. No registration needed: `TextureAtlas` stitches every PNG in `textures/block/`.
   Reference it by file stem. Then `visual-check` in game (close-up and far).
   Commit the PNG and the generator change together.
