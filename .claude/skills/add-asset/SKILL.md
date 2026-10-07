---
name: add-asset
description: Create a new placeholder texture or other asset (our own, never Mojang's) and wire it into the resource pack layout. Use when a block, item, entity or UI element needs a texture.
---
# Add an asset

1. **Never copy** from a vanilla jar, the internet, or `resourcepacks/` (ADR 0004).
   We make our own.
2. Path follows vanilla's layout so a real pack overrides it file for file:
   `assets/minecraft/textures/{block,item,entity,gui}/<vanilla_name>.png`.
3. Make it: 16×16 RGBA PNG (entities/GUI: vanilla's sheet size). Placeholder style:
   the material's base colour + light per-pixel noise + a 1px darker edge where it
   helps read block boundaries. Generate it with a small Python script using
   only the standard library: add a function + entry in `TEXTURES` in
   `tools/textures/gen_placeholders.py` (colour constants at the top), then run it.
   Each texture has its own seeded RNG, so other textures don't change.
4. Animated textures: vertical strip of N 16×16 frames + `<name>.png.mcmeta`
   (`{"animation":{"frametime":2}}`).
5. No registration needed: `TextureAtlas` stitches every PNG in `textures/block/`.
   Reference it by file stem (`atlas.sprite("stone")`).
6. Look at the generated PNG with Read. Then `visual-check` it in-game. Commit
   the PNG and the generator change together.
