# Art style (textures)

The user's direction (2026-10-06): **recreate Minecraft's look as our own original
art, and make it a little sharper and more vivid than vanilla.** Every texture,
current and future, follows this page.

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
`tools/textures/gen_placeholders.py`: one function per texture, palettes at the top,
shared helpers (`fbm`, `voronoi`, `bevel`, `ramp`). Each texture uses its own seeded
RNG, so editing one never changes another.
```
tools/textures/gen_placeholders.py --preview out/screenshots/tex.png
```
The preview shows each texture at 8x and as a 3x3 tiling. Then check in game
(`visual-check` skill): a close-up, the default view, and from far away.
