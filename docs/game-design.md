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
| Legacy "terrain" generator (M3 placeholder: 2D heightmap, no caves/ores/trees/biomes) is kept for worlds created with it | Saved worlds keep their generator | — (legacy worlds) |
| Overworld generator: vanilla's pipeline with our own climate/spline/cave constants (vanilla's noise settings and biome tables are game data we don't read, ADR 0004), so a seed gives different terrain than vanilla; bedrock/deepslate gradients use our positional hash | ADR 0004 | — |
| 28 of ~56 overworld biomes (no cave biomes, no deep lukewarm/cold/frozen oceans, no jungles, dark forests, mushroom fields...); biomes don't vary with height; biome tints per 4×4×4 cell without blending; swamp grass one colour (vanilla: noise #4C763C/#6A7039) | Subset first | Biome pass |
| No aquifers (caves below sea level are dry, no lava pockets above -55, no flooded caves under the sea) and no carver caves or ravines; cheese/noodle caves stay under the surface (spaghetti tunnels make the entrances) | Simpler first cave model | Aquifer step |
| Ores: no air-exposure skipping (coal/gold/lapis/diamond rules), no large ore veins (copper/iron), emerald biome taken at the chunk centre; veins clipped at chunk borders; tree canopies cross borders but ore blobs don't | Simpler placement | Feature pass |
| Trees: oak, birch, spruce, acacia (straight 2-block lean) only - no fancy oak, pine, savanna oak, branching acacia, bee nests; plains tree density our own; plants: 5 flowers, short grass, fern, dead bush (no tall grass, sugar cane, cactus, pumpkins, berries, tulips); no lakes, springs, disks, geodes, dungeons, fossils, structures | Feature subset | Feature pass |
| Surfaces: badlands are plain terracotta (no coloured bands), frozen oceans fully iced with gravel floors (vanilla: patches, icebergs, stone), ocean floors several blocks of sand/gravel, stony peaks calcite speckles (vanilla strips), no powder snow, steepness symmetric (vanilla: north/east faces) | Blocks/rules not added yet | Biome pass |
| Survival: difficulty fixed at Normal; no drowning, lava/fire damage, suffocation or burning; fall damage rounds up (the wiki says floor; in-game check); no XP; a hit during invulnerability is ignored by the player and mobs (vanilla: the stronger hit deals the difference) | Damage sources come with fluids | Fluids |
| Drops that need missing items: clay (4 clay balls), glowstone (2-4 dust), grass/fern (seeds), leaves (saplings), deepslate (cobblestone instead of cobbled deepslate), snow (snowballs); no silk touch, shears, fortune; ice leaves air (vanilla: water over a block) | Items not added yet | Item additions / enchanting |
| Dropped items don't merge into nearby stacks; death drops all land at the feet (vanilla scatters them) | Simpler | Entity pass |
| Containers: no drag-split, double-click, 1-9 swap, Ctrl+Q, offhand/F, armor slots, player model, recipe book or tool repair; shift-click order is ours; screen clicks act at frame time (dropped items spawn on the next tick) | Minimal screens | UI pass |
| HUD/death: no XP bar or heart/hunger animations, no damage flash; death screen text, the "Player died" message and Enter-to-respawn are ours | Minimal UI | UI pass |
| New worlds start in Creative (vanilla: Survival) | No world-creation screen yet | World creation screen |
| Copper tools (1.21.9) and later additions are missing | Pinned patch pending (ADR 0002) | Patch decision |
| Snow layers have no collision; snow isn't placed by the player yet; lava filters sky light like water (opacity 1; wiki suggests transparent) | Simplified | In-game check / block shapes |
| Fog fades to the sky colour (#78A7FF × daylight), so night fog is black; vanilla fades to the biome fog colour (Overworld #C0D8FF; dark blue at night) blended toward the sky and the sunrise colour | Simple sky model | Biome pass (fog colours) |
| Mobs: only zombie and cow; no pathfinding (zombies steer straight at the player and jump over 1-block steps; mobs don't avoid drops), no door breaking, baby zombies, reinforcements, armour/held items, drowned conversion or villager hunting; zombie natural armour 2 and 0-5% knockback resistance not applied; no rare drops (iron ingot, carrot, potato); cows don't follow wheat, breed or give milk; walking speeds, the wander rate, look-at range, melee reach widening (0.8) and cow panic length (100 ticks) are our estimates; burning has no fire visuals; zombies keep the player as target until out of range (vanilla also forgets after losing sight); despawn timer has no 10 s bright-light rule; no mob sounds, death smoke or XP; loot drops after the 20-tick death animation (in-game check) | First mobs, simplified AI | Mob pass |
| Natural spawning: one hostile spawn attempt per tick around the player in a flat cap of 70 (vanilla: per-chunk spawn cycles, 3 packs of 4 spread +-5, cap scaled by loaded chunks); no 400-tick passive spawn cycle. Chunk-generation animals: cows only, in 1 of 10 cow-biome chunks, groups of 4 (vanilla: a weighted creature loop at p = 0.1 per biome, so cows are rarer and other animals appear) | Only two mob types | Mob pass |
| Player attacks: no attack cooldown/charge, critical hits, sweep or sprint knockback; damage is the held item's attack value without enchantments | Combat pass | Combat pass |
| Redstone: no comparators, observers, daylight sensors, pressure plates, tripwires, note blocks, droppers/dispensers, hoppers, rails, doors/trapdoors, TNT, targets or slime; sticky pistons can't be crafted (no slime balls) | Core components first | Redstone pass |
| Pistons move blocks instantly when their block event runs (vanilla: 2-tick animation through moving_piston blocks), so there is no short-pulse block dropping, no pushing of entities and no piston-head "short" arm; push order of the block-event phase within a tick is ours | Simpler | Piston animation pass |
| Dust update order: our deterministic depth-first order, which can differ from vanilla's in contraptions that depend on it (vanilla's legacy order is position-hash based); updates nested more than 2048 deep are dropped (vanilla: an update queue capped at 1,000,000) | Order not documented in detail | When a build needs it |
| Redstone models: dust floats 1/16 above the ground (vanilla 1/64; our vertices are 1/16 units) and has no overlay texture; wall torches stand upright against the wall (vanilla tilts them); lever handles shift instead of tilting; repeater torches are our layout; components have no collision boxes (vanilla: thin boxes you can stand on) | Box-only models in 1/16 | Model pass |
| Gravel and sand don't fall | Falling blocks need entities | Falling blocks |
| All packs in `resourcepacks/` are enabled automatically (jars at the bottom, others by name); packs need no `pack.mcmeta`; a client `.jar` is treated as the Default pack | No Resource Packs screen yet; lets you use your own jar unpacked | Options screens |
| No swimming: water has no physics, the player sinks through it | Fluid physics come with the fluids work | Fluids milestone |
| Every block has the default slipperiness 0.6 (ice, slime, honey, soul sand speeds not modelled) | Those blocks don't exist yet | With their blocks |
| Block updates only reach redstone components, pistons and their supports (M11): no falling sand, no water flow into holes, and plain torches stay without support | Those blocks' reactions come later | Fluids / falling blocks |
| Sprinting starts with Ctrl only (no double-tap W); sneak box is 1.5 tall but crawling/swimming poses don't exist | Simpler input | Controls options |
| Sneaking cancels sprinting (1.21.4 behaviour; 1.21.5+ keeps sprinting while sneaking, faster than plain sneaking) | Depends on the pinned 1.21 patch (ADR 0002) | Patch decision |
| Spawn: 8-block ring search to 1024 for a dry, non-ocean/river column, then the nearest non-tree ground within 8 blocks; no climate fitness search, no 21×21 random respawn area | Simpler search | Spawn pass |
| Block outline thickness is in world space (vanilla: constant on-screen line width); no High Contrast outline option; hotbar ignores horizontal scrolling | Simpler geometry / input | Options screens |
| Unloaded chunks collide as solid (vanilla keeps entities out of unloaded chunks differently) | Never move into ungenerated terrain | — |
| No dynamic FOV: vanilla widens FOV ~10% while flying and more when sprinting (FOV Effects scale) | Cosmetic; the player state exists now | Options screens |
| Sky is one colour (#78A7FF × daylight): no gradient toward the fog colour at the horizon, no sunrise/sunset glow, no dark void plane below the horizon, no clouds. Sun/moon apparent sizes and the star count are estimated from observation; star positions differ | Simple sky model | Weather/sky pass |
| The Brightness option's lift of dark light levels is our estimate (`block.vert`); the l/(60−3l) curve and ~5% at full darkness follow public sources; no block-light flicker | Exact lightmap not documented on the wiki | When observed side by side |
| Non-cube blocks (torch) are targeted and outlined as full cubes | Shape-aware raycast comes with block shapes | Block shapes (slabs...) |
| Torches stand on top of any full-collision block (glass included) but not on walls (no wall torches), and stay when their support is removed; no torch particles | Wall torches are a separate block; no block updates yet | Block updates |
| Light is recomputed for the whole 3×3 chunks around an edit (vanilla updates incrementally); the edited block's section is re-meshed when that light arrives, a frame or two after the click. Redstone changes that don't change light (dust power, repeaters, levers) skip the relight; torches and lamps toggling relight | Simpler and exact; runs on workers | Incremental light |
| Water uses the still texture only and every level renders at source height 8/9; no flow, no underwater fog/tint | Fluid flow and camera-in-fluid effects come later | Fluids milestone |
| Translucent faces are sorted per section, not per quad (rare blending errors inside one section) | Simpler; vanilla sorts quads | When visible |
| Flowing water/lava textures are 16px frames (vanilla: 32px) | One 32px sprite would force 32px atlas cells for every texture | When the atlas packs mixed sizes |
| Animated textures ignore `.mcmeta` `frames`, `interpolate`, `width`/`height`; only vertical strips of width×width frames animate (vanilla: squares of the smaller dimension, any strip direction); a non-square image without `.mcmeta` shows its top square (vanilla stretches it); non-power-of-two sprites are skipped; one cell size for all sprites | Minimal .mcmeta reader, grid atlas | When a pack needs it |
| Random model variants are picked with our own position hash, so a given position may show a different variant than vanilla | Vanilla's per-position seed isn't documented on the wiki | When documented / observed |
| `grass_block[snowy=true]` renders like snowy=false (vanilla: snowy side, untinted top) | No snow yet | When snow is added |
| Superflat presets accept block-state layers (`oak_log[axis=x]`); the preset's biome field is ignored (always plains); no villages | Extension used by tests; no structures yet | Structures |
| Grass side has a baked green fringe instead of dirt + biome-tinted overlay | Overlay needs a second quad | Biome pass |
| Saves: no heightmaps, fluid ticks or POI in chunks; dropped items are not saved (lost on quit); mob AI state (goals, panic, cooldowns) isn't saved; entity NBT is the 1.21.1 layout (1.21.5+ renames FallDistance to fall_distance and adds equipment and cow variants); furnace NBT uses the 1.21.0-1.21.3 names (BurnTime/CookTime) and no burn total; level.dat without GameRules/DataPacks/difficulty/dimensions; no pause-menu save (we save when Esc releases the mouse) | Systems/patch decisions pending | With each system / ADR 0002 |
| Esc releases the mouse instead of opening the pause menu; no F1 (hide HUD); no item name shown above the hotbar when switching | No menus or display names yet | Menus / display-name table |
| Chat: ASCII input only (vanilla: Unicode via its fallback font); no Tab completion, scrolling, cursor movement or paste; history counts wrapped lines, not messages; commands' feedback wording is ours | Minimal chat | Chat polish |
| Commands: /setblock has no destroy/keep/replace mode or block NBT; /summon takes no NBT and only zombie/cow; /tp accepts `x y z yaw pitch` without a target, no `^` local coordinates, `facing`, entity or name targets; /give accepts block states (`oak_log[axis=x]`) and drops nothing when the inventory is full; /gamemode only survival/creative for yourself; /help is one line | Subset | Commands pass |
| Creative inventory: one list of every item in registry order; no tabs, search, saved hotbars, survival tab or destroy slot; tooltips show ids, not display names; clicking an item while carrying one swaps to it; clicking outside deletes (vanilla drops an item entity); no right/shift/middle click or Q | Minimal screen | UI pass |
| F3 shows our own version, fps, C:, system lines and the time of day; the crosshair stays (vanilla: axis gizmo); line formats follow 1.21.x until ADR 0002 picks a patch | Debug data of this engine | Patch decision |
| GUI sprites (hotbar, panel) and the font are our own art and palette; panel offsets follow the commonly known layout, not verified | ADR 0004 | In-game comparison |
