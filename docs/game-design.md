# Game design (GDD)

## The game in one sentence
A faithful, from-scratch replica of **Minecraft Java Edition 1.21.x**, built so its
author understands how every system in vanilla Minecraft actually works.

## Purpose and constraints
- **Learning project, non-commercial.** Understanding beats speed: each system is
  implemented the way vanilla does it, and the code explains the mechanic.
- **Replica, not reinterpretation.** When in doubt, do what vanilla does. Deviations are
  allowed only when deliberate, small, and recorded in an ADR (e.g. "simplified
  worldgen until M8").
- **Own assets.** Placeholder textures and sounds are ours. A player may point the game
  at their own resource pack in `resourcepacks/` (never committed).
- Single-player first. Multiplayer is out of scope (but the tick/simulation split keeps
  a server split possible later).

## Reference sources (in priority order)
1. The Minecraft Wiki (minecraft.wiki) — mechanics, constants, formulas, data formats.
2. Observing the real game (the user's own copy) — side-by-side screenshots.
3. Public technical write-ups (e.g. worldgen explanations). Never decompiled source.
Record the source of non-obvious constants in a comment ("wiki: Player#Movement").

## Core loop (survival, as in vanilla)
Spawn → punch trees → crafting table → wooden then stone tools → shelter before the
first night (mobs spawn in darkness) → mine for coal/iron → furnace and iron gear →
explore caves, biomes, villages → diamonds → Nether → End and the dragon.
Creative mode (fly, infinite blocks) comes first because it needs the fewest systems
and is the fastest way to test the world.

## Parity rules — what "done" means for a feature
A feature is done when:
1. Its behaviour matches the wiki's description, including the documented edge cases.
2. Its constants match vanilla (speeds, ranges, timings in ticks, light levels).
3. It has unit tests for the rules (outside rendering) and, if visible, a screenshot
   checked against expectations.
4. Any known difference from vanilla is listed in **Known deviations** below.

## Feature scope by milestone
See ROADMAP.md for order. In scope eventually: terrain & biomes (1.21 generator),
blocks & block states, lighting, fluids, player movement, survival/creative, items,
tools, crafting/smelting, inventory UI, mobs & AI, redstone, Nether, End, saving,
day/night, weather.
Out of scope: multiplayer/networking, Realms, marketplace, Bedrock-only features,
mod loaders, telemetry, accounts.

## Key vanilla facts (fixed design inputs)
| Fact | Value |
|---|---|
| Tick rate | 20 ticks/s (50 ms) |
| World height | Y −64 to 319 (384 blocks), 24 sections |
| Chunk | 16×16 columns; sections 16×16×16 |
| Light levels | 0–15 sky light and block light |
| Day length | 24000 ticks (20 minutes) |
| Default FOV | 70° vertical |
| Player | 0.6×1.8 blocks, eye height 1.62; walk 4.317 m/s, sprint 5.612 m/s |

## Art direction
16×16 pixel textures, nearest-neighbour filtering, vanilla's lighting look (light
levels + smooth lighting/AO). Placeholders first: flat colours with simple noise are
fine until a system works.

## Known deviations from vanilla
| Deviation | Why | Remove by |
|---|---|---|
| Placeholder terrain: 2D heightmap; no caves, aquifers, lava level, ores, features, trees or biomes; fixed 3-block dirt; no sandstone under sand; sand vs gravel by noise, not by biome; sand only in a narrow band at the waterline; bedrock/deepslate gradients use our positional hash (same probabilities as described, different pattern) | Need ground to test M3–M7 | M8 |
| Fog fades to the sky colour #78A7FF; vanilla fades to the biome fog colour (Overworld #C0D8FF) blended with a sky gradient | No sky/fog colour model yet | M5 |
| Water tint is always #3F76E4 (no biome water colours) | No biomes | M8 |
| Gravel and sand don't fall | Falling blocks need entities | M10 |
| All packs in `resourcepacks/` are enabled automatically (jars at the bottom, others by name); packs need no `pack.mcmeta`; a client `.jar` is treated as the Default pack | No Resource Packs screen yet; lets you use your own jar unpacked | M6 (UI) |
| No swimming: water has no physics, the player sinks through it | Fluid physics come with the fluids work | Fluids milestone |
| Every block has the default slipperiness 0.6 (ice, slime, honey, soul sand speeds not modelled); no fall damage or hunger | Those blocks/systems don't exist yet | With their blocks / M9 |
| Sprinting starts with Ctrl only (no double-tap W); sneak box is 1.5 tall but crawling/swimming poses don't exist | Simpler input | M6 (controls) |
| No dynamic FOV: vanilla widens FOV ~10% while flying and more when sprinting (FOV Effects scale) | Cosmetic; the player state exists now | M4 follow-up |
| Sky is a flat #78A7FF clear colour: no gradient into fog colour near the horizon, no time-of-day change | No fog or day cycle yet | M5 (day–night) |
| Water uses the still texture only and every level renders at source height 8/9; no flow, no underwater fog/tint | Fluid flow and camera-in-fluid effects come later | Fluids milestone |
| Translucent faces are sorted per section, not per quad (rare blending errors inside one section) | Simpler; vanilla sorts quads | When visible |
| Flowing water/lava textures are 16px frames (vanilla: 32px) | One 32px sprite would force 32px atlas cells for every texture | When the atlas packs mixed sizes |
| Animated textures ignore `.mcmeta` `frames`, `interpolate`, `width`/`height`; only vertical strips of width×width frames animate (vanilla: squares of the smaller dimension, any strip direction); a non-square image without `.mcmeta` shows its top square (vanilla stretches it); non-power-of-two sprites are skipped; one cell size for all sprites | Minimal .mcmeta reader, grid atlas | When a pack needs it |
| Random model variants are picked with our own position hash, so a given position may show a different variant than vanilla | Vanilla's per-position seed isn't documented on the wiki | When documented / observed |
| `grass_block[snowy=true]` renders like snowy=false (vanilla: snowy side, untinted top) | No snow yet | When snow is added |
| Superflat presets accept block-state layers (`oak_log[axis=x]`), ignore the biome, no villages | Extension used by tests; no biomes/structures yet | M8 |
| Grass side has a baked green fringe instead of dirt + biome-tinted overlay | Overlay needs a second quad and biome colours | M8 (biomes) |
| Grass tint is always plains #91BD59 | No biomes yet | M8 |
