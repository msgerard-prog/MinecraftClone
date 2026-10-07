# Art style (textures)

The user's direction (2026-10-06): **recreate Minecraft's look as our own original
art, and make it a little sharper and more vivid than vanilla.** Every texture,
current and future, follows this page. Status: every vanilla 1.21.4 block texture
(~1,030) has an original version; items, entities and GUI come with their systems.
GUI sprites (hotbar, selection) and the 5x7 font are original too
(`tools/textures/gen_gui.py`, glyphs in `texgen/font5x7.py`), as are the sun and moon
(`tools/textures/gen_environment.py`).

## The line we don't cross (ADR 0004, ADR 0006)
Same *style and subject*, independently made — never the same *pixels*.
- OK: looking at the game for what a material is like ("cobblestone: rounded grey
  stones in dark mortar"), matching the general palette family, 16x16 pixel art,
  vanilla's file names.
- Not OK: copying, tracing, recolouring or upscaling Mojang PNGs; redrawing them
  pixel for pixel by eye; sampling exact colours or layouts from their files; using
  the user's `resourcepacks/` files as input in any way (Claude never opens them).
- Feedback from the user comparing in-game ("grass too blue") is fine and wanted.

## Look
- 16x16, one texel = one pixel, nearest filtering. (The atlas supports bigger
  sprites; we stay at vanilla's density.)
- **A small palette ramp per material**: 4–5 shades dark → light. Shadows lean
  cooler / more saturated, highlights warmer. Pick shades from the ramp — no blur,
  no gradients between ramp steps (this is what makes it crisp).
- **Light from the top-left.** Raised shapes: lit top/left rim, dark bottom/right rim
  (1px bevel). Recesses: the opposite.
- **Readable shapes over noise**: stones, boards, bark ridges, clods, blades. Noise
  only chooses between ramp shades; it never is the texture.
- **Contrast to "pop"**: clear darkest/lightest accents (gaps, crevices, highlights),
  a bit stronger than vanilla, but the material must stay recognisable from afar
  (check the 40-block screenshot: no sparkle, no muddy greys).
- **Seamless tiling**: noise and cells wrap around; check the 3x3 tile preview.
- Tinted textures (grass top, later leaves, water) are **greyscale**; the game
  multiplies by the biome colour. Baked colour only where the deviation table says so
  (grass side fringe), using ramp × plains tint so the colours match the top.

## How textures are made
`tools/textures/gen_textures.py` drives the `tools/textures/texgen/` library:
- `core.py`: image type, `ramp()` palettes (built from one base colour: shadows drift
  toward blue-violet, highlights toward warm yellow, pale colours shift less), tileable
  noise/cells, bevels, PNG output.
- `materials.py`: reusable painters (stone, cells, bricks, tiles, polished, chiseled,
  cracked, mossy, ores, metal/gem blocks, soils, grass, ice, planks, bark, log ends...).
- `blocks_*.py`: tables mapping every vanilla texture name to a painter + palette.
  Base colours are picked from a description of the material, never sampled.
  `blocks_stone` (stone, terrain, ores, minerals), `blocks_wood` (all woods),
  `blocks_colored` (16 dye colours), `blocks_plants` (+ `plants.py` painters),
  `blocks_nether_end`, `blocks_utility` (+ `utility.py`: workstations, redstone,
  copper, sculk, fluids, fire, cracks), `blocks_misc` (the rest).
- `registry.py`: name → painter, family, optional animation `.mcmeta`.
Each texture's RNG is seeded by its name, so editing one never changes another.
```
tools/textures/gen_textures.py --preview out/screenshots/tex     # all + sheets
tools/textures/gen_textures.py --only stone,dirt                 # just these
```
Preview sheets (one per family, 4x, transparency on a checker, tinted textures
tinted) print their cell order. Then check in game (`visual-check` skill).
