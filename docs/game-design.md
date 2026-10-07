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
| Terrain is simple noise until M8 | Need ground to test M3–M7 | M8 |
| Free flight uses constant speeds (10.92 / 21.6 / 7.49 b/s), no acceleration or drag; sprint needs Ctrl held (vanilla keeps sprinting until you stop moving forward) | Camera needed before player physics | M4 |
| No dynamic FOV: vanilla widens FOV ~10% while flying and more when sprinting (FOV Effects scale) | Needs player abilities state | M4 |
| Sky is a flat #78A7FF clear colour: no gradient into fog colour near the horizon, no time-of-day change | No fog or day cycle yet | M5 (day–night) |
| Animated textures show frame 0; non-16px sprites (HD resource packs) are skipped | No animated sprites or pack loading yet | M3.0 (resource packs) |
| Random model variants are picked with our own position hash, so a given position may show a different variant than vanilla | Vanilla's per-position seed isn't documented on the wiki | When documented / observed |
| `grass_block[snowy=true]` renders like snowy=false (vanilla: snowy side, untinted top) | No snow yet | When snow is added |
| Superflat presets accept block-state layers (`oak_log[axis=x]`), ignore the biome, no villages | Extension used by tests; no biomes/structures yet | M8 |
| Chunks next to unloaded chunks show walls at the edge of the loaded area (vanilla waits for neighbours before drawing) | Fixed test world | M3 (chunk loading) |
| Grass side has a baked green fringe instead of dirt + biome-tinted overlay | Overlay needs a second quad and biome colours | M8 (biomes) |
| Grass tint is always plains #91BD59 | No biomes yet | M8 |
