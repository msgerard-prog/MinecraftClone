# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-07)
M22 done (reviews applied; v0.22.0): weather (rain/snow/thunder, lightning, snow and
ice forming), sky (biome colours, sunrise/sunset, fog, Fancy clouds, End sky),
particles, sound (XAudio2, our own synthesized sounds), menus (title, world list and
creation, options.txt, Game Menu that pauses). M1-M21 done. v1.0 waits for M23-M28.

## Next
M23 - Building blocks & workstations (wiki pages of each block):
1. ✅ M23.1 - Block kinds and shapes: a `kind` per block (slab, stairs, wall, pane,
   carpet, ...) with its base texture, so placement, shapes, models and connections
   are written once; slabs (bottom/top/double merging), stairs (facing, half, corner
   shapes), walls (posts, low/tall sides) for vanilla's stone, brick, sandstone,
   deepslate, nether and end families and every wood; recipes, stonecutter-free.
2. ✅ M23.2 - Thin and small blocks: glass panes, the 16 stained glass blocks and panes,
   16 carpets, ladders (climbing), lanterns and soul lanterns (standing/hanging),
   chains, wall torches.
3. ✅ M23.3 - Woods: mangrove, bamboo and pale oak wood sets; every wood's doors,
   trapdoors, fences, gates, buttons and plates (the M21 oak-only code generalised by
   kind); signs and hanging signs with text editing and rendering.
4. M23.4 - Decorative and ageing: concrete and concrete powder (hardens in water), 16
   terracottas and glazed terracottas, the copper family (oxidation on random ticks,
   honeycomb waxing, axe scraping, copper bulbs), campfires (cooking, smoke, damage).
5. M23.5 - Workstations 1: stonecutter, smoker, blast furnace, barrel, composter,
   cauldrons (water/lava/powder snow, buckets and bottles, rain), grindstone.
6. M23.6 - Workstations 2: smithing table (netherite upgrades, armor trims), loom
   and cartography table (their screens; banners and maps come in M28), ender chest,
   shulker boxes, beacon (pyramid, beam, effects), conduit, note blocks, jukebox.

Deferred performance work (from the M2 perf review) — not needed at current numbers;
revisit if CPU work p99 > 4 ms or GPU > 8 ms on the target hardware:
- Dense ring-indexed section grid (vanilla ViewArea) instead of hash maps; column culling.
- Static section origins + camera block/fraction uniforms; persistent-mapped command ring.
- Arena pages + size classes, per-frame upload budget.
- Faster snapshots / immutable sections readable by workers.
- Per-face-direction draw commands, cave culling.
- Hard-rule-1 exceptions while streaming: node-based maps (`World`, `WorldRenderer`
  section states, `ChunkRenderer` sections) and `WorkQueue`'s deque allocate when
  chunks load — fixed by the dense grid and a ring-buffer queue.
- Incremental `rebuildQueue` (only the newly exposed strip when the centre moves).
- Terrain: whole-section fast paths (all air / all stone) using column min/max height.
- Translucent sort: keep last order, insertion-sort.
- Animated textures with HD packs: upload frames once to the GPU, copy per tick.
- From the M19 perf review: path searches for angry zombified-piglin herds (30
  chasers ~1-3 ms a tick, the first routine case near the 4 ms trigger: do the M16
  pathfinder items plus a cap of ~4 chase searches a tick first); blazes/ghasts check
  sight every tick (cache it for 5-10 ticks; a last-chunk cache in `raycastBlocks`);
  piglins scan all dropped items for gold every tick (a gold-stack count, or every
  10 ticks); ghast fire relights 3x3 chunks per fire (the M15 block-light item);
  striders and blazes compute fluid contact twice a tick.
- From the M22 perf review: sound plays poll `GetState` over 32 voices (TNT chains:
  ~1-3 ms in one tick - keep voice end times, merge identical events per tick, cap
  plays); cloud mesh rebuilt on every 12-block cell crossing (hysteresis of a few
  cells, indexed quads); `rainingNear` per fire tick in rain (a 3x3 chunk fetch);
  `runWeatherTicks` is another pass over the simulation square (merge it into the
  random tick loop - changes the gameplay RNG order); per-instance weather columns
  drawn by the vertex shader (today one quad per column, ~30 KB a frame).
- From the M21 perf review: comparators reading containers recompute their target every
  tick (cache the last container signal; vanilla: containers notify comparators);
  ticking-chunk passes per tick are now 6 (store Chunk* or merge them) and hoppers,
  comparators and plates tick beyond the simulation distance; railPowered walks up to
  700 lookups per call (walk the line once); PrimedTnt::push is O(T) per blast (O(T^2)
  over a chain); explosions look chunks up twice per destroyed container.
- From the M20 perf review: a gateway's outward search recomputes the island window
  per step (walk the cells along the ray); end city candidates evaluate outerValue up
  to 3 times; the dragon's break box looks up the chunk per cell (per column, skip
  empty sections); the respawn's pillar rebuild looks chunks up per cell; MobData
  carries the dragon's fields for every mob (a side struct).
- From the M16 perf review: classify each probed cell once per path search (cache
  standable/danger in the hash slot) and a 3x3 chunk-pointer cache in the pathfinder;
  explosion de-dup with a bitset over the blast cube (no sort) and per-ray chunk
  caching; a side pool for mob paths (MobData is ~700 bytes, copied on chunk moves);
  findMob is O(N) per breeding/baby animal (O(N^2) in dense farms).
- From the M15 perf review: fire and leaf-decay edits each recompute 9 chunks of
  light - a large fire kept all light workers busy ~75 s at 60 fps (block-light-only
  jobs for emission-only edits, rate-limit relights per chunk, then incremental light);
  copy-on-write section copies every tick while jobs hold them (section pool, release
  early); per-chunk ticking-section bit mask and cached "8 neighbours loaded" count
  for random ticks (today ~15k section reads + up to 5.6k lookups a tick at sim 12);
  fire spread scan from a 5x8x5 stack array, per-state ignite/burn tables; frozen
  far fires mark their chunk dirty on each reschedule.
- From the M14 perf review: fluid edits keep sections shared with light/mesh jobs,
  so copy-on-write copies sections every tick during floods (rate-limit fluid relights
  per chunk, or pool spare sections); per-chunk pending-edit lists in LightManager
  instead of one global erase; cache the slope search's cells in a 9x9 stack array;
  gather fluidContact's cells once (one chunk lookup) with an early out; a per-state
  fluid amount table; a 17x17 corner-height grid per fluid layer in the mesher;
  reserve LightManager/edit lists from the render distance; scratch buffer for
  `Section::resize`.
- From the M12 perf review: in the Nether only mesh sections within the fog end
  (+1 chunk) - today 4x more is meshed than its 96-block fog shows; light jobs in
  sky-less dimensions could start one section below the lowest non-empty one;
  free unloaded chunks and clear the renderer in bulk on a dimension switch (~38 ms).
- From the M11 perf review: incremental block light for torches/lamps toggling (today:
  a 3x3-chunk relight each); per-state lookup tables for redstone properties and an
  open-addressing set for `hasTick`/torch toggles; an explicit update queue instead
  of recursion (vanilla CollectingNeighborUpdater); dedupe re-mesh positions per
  section and keep WorldRenderer section states alive (no map node per toggle); cull
  box faces against opaque neighbours. Measured ~15 us per dust change.
- From the M10 perf review: instanced mob drawing (static model VBO + per-mob data;
  removes the 32k-quad entity buffer cap), a pooled mob store instead of per-chunk
  vectors (moves into a chunk past its 4 reserved slots allocate), an entities-only
  dirty flag so moving mobs don't rewrite the chunk's block NBT, cached section access
  in mob physics, sleeping idle mobs.
- From the M8 perf review: cave culling (per-section visibility graph) and arena
  pages (RD32 is now ~6.6M quads, ~400 MB of vertices; growth copies the buffer);
  per-cell interpolation stepping; all-solid section fast path.
- From the M5 perf review: re-mesh only neighbours on the sides whose border light
  changed (an edit marks ~20–100 sections today); incremental light updates instead of
  the 3x3 full relight; pre-size the vertex arena from the render distance (growth
  copies the whole buffer: a ~100 ms frame once at RD32); `cornerLight` offset table;
  Section copy-on-write allocates when a worker still holds the section (rare).

## Decisions (2026-10-07, user)
- **v1.0 is tagged only when the first revision is fully complete and resembles
  vanilla Minecraft** - after M22 come content milestones M23-M28 (building blocks,
  villages, oceans, the remaining mobs, biomes/structures, progression), then v1.0.
  Work continues autonomously, a commit per step and a push per milestone, stopping
  only for decisions or problems that need the user.
- Entity textures stay as they are (procedural skins on vanilla-shaped models); the
  user decides whether to change them.
- Target patch: **1.21.11** (ADR 0002 update). Migration to do: DataVersion 4671 and
  1.21.5+ NBT names in saves, 1.21.11 defaults (render/graphics presets, mipmaps,
  Nether fog 10-96, sneak-sprint), then 1.21.9-1.21.11 content as milestones.
- The newest generator of each dimension is the default for new worlds, always;
  older generators stay only for worlds created with them (their pinned hashes).
- Recipe ids (furnace RecipesUsed, later recipe books): follow vanilla's naming
  pattern with our own rules where the exact id can't be looked up (no in-game check).

## Texture plan (agreed 2026-10-06)
Textures arrive with their blocks (add-block skill makes the placeholder), by
milestone: M3 stone types, ores, gravel, water, sand, sandstone, clay · M5 light
sources · M8 biome blocks, all woods, leaves · M9 items, crafting table, furnace ·
M10–M12 mobs, redstone, Nether/End. Cutout (glass, leaves), translucent (water, ice)
and animated textures get renderer support with the first block that needs them.
Every block texture already exists as original art (docs/art-style.md, ADR 0006);
adding a block now means registering it and its model, not drawing. Items, entities
and GUI textures are made with their systems.

## Waiting on the user
- **M22 checks:** listen to the sounds (`tools/run.sh`): ours are synthesized - say if
  they need a pass. Cloud colour in rain and in a thunderstorm (the wiki says rgb 191 /
  30; ours ~158 / ~38). On a fresh 1.21.11 install: the default simulation distance and
  cloud type (the wiki says 6 and Fast; we use 12 and Fancy, as M13 assumed).
- **M20 decision (dragon head damage):** in 1.21.4-1.21.11 vanilla also reduces hits
  on the dragon's head (a bug, MC-308469, fixed in 26.3). Ours deals full damage to
  the head, as intended and as from 26.3. Keep that, or copy 1.21.11's bug?
- **M20 checks:** try `tools/run.sh --world "M20 test" --dimension end`: the fight,
  crystals, a gateway (throw a pearl into it), an end city (shulkers, elytra).
- **M19 checks:** in creative, does drinking a potion keep it (ours: yes, and no
  bottle)? Swap a brewing stand's ingredient mid-brew for another valid one: does the
  brew stop (ours: yes)? Try it: `tools/run.sh --world "M19 test" --dimension nether`.
- **M18 note:** "overworld2" is frozen as of v0.18.0 (pinned hash). Worlds you
  created with an M18 development build (before this tag) may show seams where later
  steps changed generation; re-create them. Try it: `tools/run.sh --world "M18 test"`,
  `/give @s ender_eye 16` and right-click to follow eyes to a stronghold.
- **M15 in-game checks:** does grass under one block of still water
  in sunlight turn to dirt; how long does a sapling take to grow at light 15?
- **M14 checks:** empty the air bar under water, surface and time the refill (ours
  3.75 s; the wiki text suggests 2 s). Time a 10-block sink and rise in deep still
  water. Does a torch in front of flowing lava drop as an item (ours: no, per the Lava
  page)? Does a water bucket clicked on short grass or a torch drop it (ours: yes),
  and into a lava source cell: obsidian or water (ours: water replaces it)? Can you
  jump normally from a 1-deep flowing puddle (ours: yes, up to 0.4 deep)? How far does
  falling lava spread where it lands in the Overworld (ours: 3)?
- **M13 checks:** open one of our worlds in vanilla 1.21.11 (copy `saves/<world>` into
  your `.minecraft/saves`): does it load, with our terrain and your inventory? In an
  NBT viewer on a fresh vanilla 1.21.11 level.dat: the game-rule compound's name and
  value types, and the types of `spawn.yaw`/`pitch`. On a fresh install (Fancy): are
  simulation distance 12 and mipmap levels 4? Can you start sprinting while already
  sneaking?
- **M10-M12 in-game check:** stay in the Nether portal you arrived through: do you go
  back after 4 s, or must you step out (ours: step out)?
- **Machine note:** while M5-M9 were built, a stuck build of another project
  (CubeCraft) held MSVC's shared mspdbsrv; I ended cl.exe/mspdbsrv processes globally
  once, which may have interrupted that build. Since then this project embeds debug
  info (/Z7) and only touches its own processes.
- M8 in-game checks: sky light under a surface lava pool (lava opacity), the
  default Biome Blend radius, snow line heights in windswept hills/taiga.
- M7 in-game check: load one of our worlds' chunks in vanilla? (not a goal; vanilla
  probably refuses without WorldGenSettings.dimensions).
- M6 in-game checks: exact feedback of `/tp 100 64 -20`, `/give @s oak_log 64` and an
  unknown command; does Return open the chat; carrying a creative item and clicking
  another - is the cursor empty afterwards; compare hotbar / creative panel offsets
  with a screenshot at the same GUI scale; how many sent lines Up recalls.
- M5 in-game checks: does a torch stand on glowstone, and can it be placed in water
  (ours: yes / no)? Compare night darkness and sun/moon size side by side
  (`tools/run.sh --time 18000`). Which side of the waning moon is lit (ours: right)?
  Do glass and glowstone darken neighbouring corners (AO)?
- Build note: another project's (CubeCraft) build was hung on this machine and
  blocked the shared mspdbsrv; we now embed debug info (/Z7) to stay independent.
- M4 in-game checks (we can't verify these from the wiki):
  - Sprint diagonally (mostly along Z) into a block corner from both sides: which
    side catches? (pins the collision axis order; the public source contradicts itself)
  - Does the camera ease down when you sneak, or snap? (ours snaps)
  - Time a 10 s creative climb with F3: ours rises 7.5 b/s; sprint-fly: ours 21.78 vs
    the wiki's 21.6 b/s.
- In your game (spectator + F3): how much of Y -60 is bedrock compared with Y -63?
  (Ours thins 4/5, 3/5, 2/5, 1/5 over -63..-60; the wiki only says "rare gaps".)
- Try your own textures: copy your 1.21.x client jar into `resourcepacks/` (README ›
  Using your own Minecraft textures) and run `tools/run.sh`.
- Try the controls by hand: `tools/run.sh`, click the window, WASD + mouse, Esc.
  Tell me if the mouse feel or speeds are off.
- In your game, check horizontal oak logs (axis x and z) against ours
  (`tools/screenshot.sh logs --pos -1.2,-58.5,-2.8 --look 45,15`): the face rotations
  in `BlockModels.cpp` were derived from vanilla model files a review agent should not
  have downloaded (deleted; rule added to design-keeper). Also: is the underside of the
  Y -64 bedrock layer drawn when seen from below?
- Optional: confirm the mouse-sensitivity curve in your game (degrees per mouse
  movement at 0%, 100%, 200% sensitivity) — it isn't documented on the wiki.

## Milestones
| # | Milestone | Done when |
|---|---|---|
| M0 | Setup: build, window, tests, screenshot, docs, Claude tooling | ✅ 2026-10-06 |
| M1 | Camera + texture atlas + one textured cube | ✅ 2026-10-06 |
| M2 | Block registry + block states, chunk sections (paletted), face-culled meshing on worker threads | ✅ 2026-10-06 |
| M3 | Resource-pack loader; simple noise terrain (placeholder for M8), grass/dirt/stone/water layers, chunk loading around the player | ✅ 2026-10-06 |
| M4 | Player: vanilla movement & AABB collision, gravity, jumping, sprint/sneak; block raycast, break/place | ✅ 2026-10-06 |
| M5 | Lighting: sky light + block light propagation, smooth lighting / AO, day–night cycle | ✅ 2026-10-07 |
| M6 | UI: crosshair, hotbar, inventory screen, F3 debug screen, chat/commands (`/tp`, `/time`, `/give`) | ✅ 2026-10-07 |
| M7 | Save/load: region files (format chosen by ADR) | ✅ 2026-10-07 |
| M8 | Faithful 1.21 worldgen: noise router/density functions, multi-noise biomes, aquifers, caves, features | ✅ 2026-10-07 (aquifers, biome subset: see deviations) |
| M9 | Survival basics: items, tools, mining speed/drops, crafting table, furnace, recipes (vanilla JSON) | ✅ 2026-10-07 (recipes authored from the wiki, not vanilla JSON) |
| M10 | Entities & mobs: entity system, physics, AI goals, spawning, health/damage | ✅ 2026-10-07 (zombie + cow, no pathfinding: see deviations) |
| M11 | Redstone: power, dust, torches, repeaters, pistons, update order | ✅ 2026-10-07 (no comparators/observers; instant piston moves: see deviations) |
| M12 | Dimensions: Nether and End, portals | ✅ 2026-10-07 (one dimension loaded at a time; no dragon or strongholds: see deviations) |
| M13 | 1.21.11 migration: saves at DataVersion 4671, vanilla-openable worlds, 1.21.11 defaults | ✅ 2026-10-07 v0.13.0 (vanilla opening: in-game check) |
| M14 | Fluids: water/lava flow, swimming, drowning, lava damage, buckets | ✅ 2026-10-07 v0.14.0 |
| M15 | Random ticks & fire: saplings, leaf decay, grass spread, fire, flint and steel (crops moved to M17 farming) | ✅ 2026-10-07 v0.15.0 |
| M16 | Falling blocks; mobs 2: pathfinding, sheep/pig/chicken, skeleton/creeper/spider/enderman, projectiles, breeding | ✅ 2026-10-07 v0.16.0 |
| M17 | Items & survival 2: armor, bows, shields, chests/containers, beds, enchanting, anvils, farming (brewing moved to M19) | ✅ 2026-10-07 v0.17.0 |
| M18 | Overworld 2: remaining biomes, aquifers, lakes, ravines; structures (villages, dungeons, mineshafts, temples, strongholds) | ✅ 2026-10-07 v0.18.0 (basic structures, no aquifers: see deviations) |
| M19 | Nether 2: biomes (crimson/warped, soul sand valley, basalt deltas), fortresses, bastions; ghasts, piglins, blazes, magma cubes; brewing | ✅ 2026-10-07 v0.19.0 (basic structures, player-only effects: see deviations) |
| M20 | The End 2: ender dragon fight, crystals, gateways, outer islands, end cities | ✅ 2026-10-07 v0.20.0 (basic cities, simplified dragon AI: see deviations) |
| M21 | Redstone 2: comparators, observers, pressure plates, hoppers, droppers/dispensers, doors, TNT, rails, slime, piston animation | ✅ 2026-10-07 v0.21.0 (plain minecarts; see deviations) |
| M22 | World & presentation: weather, clouds, sky gradient/sunsets, sounds, particles, pause/options/world-creation menus | ✅ 2026-10-07 v0.22.0 (synthesized sounds, one-page options: see deviations) |
| M23 | Building blocks & workstations: slabs, stairs, walls, panes, carpets, ladders, signs, lanterns, campfires, all wood types' doors/trapdoors/fences, mangrove/bamboo/pale oak, copper ageing, concrete, stained glass; stonecutter, smithing (netherite, trims), grindstone, loom, cartography, composter, cauldron, barrel, smoker, blast furnace, ender chest, shulker boxes, beacon, conduit, note block, jukebox | Vanilla's building and crafting palette |
| M24 | Villages 2: villagers (professions, trading, breeding), iron golems, wandering traders, pillagers, outposts and raids, witches | Villages live |
| M25 | Oceans: water aquifers, ocean biomes and features, drowned, guardians and ocean monuments, shipwrecks, ocean ruins, boats, fishing, fish, squid, dolphins, turtles, tridents | Oceans as in 1.21 |
| M26 | Mobs 3: wolves, cats, horses, llamas, foxes, bees, goats, frogs, axolotls, pandas, parrots, polar bears, allays, phantoms, silverfish, cave spiders, wither skeletons and the Wither, the warden, the breeze, 1.21.6-1.21.11 mobs (happy ghast, copper golem, nautilus...) | Vanilla's mob roster |
| M27 | World 3: the remaining biomes, lush and dripstone caves, the deep dark and ancient cities, woodland mansions, ruined portals, trial chambers, trail ruins, geodes, archaeology | Vanilla's world |
| M28 | Progression & game: difficulty settings, adventure/spectator modes, advancements, statistics, game rules, maps/compass/clock, books, item frames, paintings, armor stands, banners, fireworks, crossbows, mace, spears, lingering potions, tipped arrows | Complete first revision |
| v1.0 | Tag the codebase (git tag v1.0) - only when the first revision is complete | Then polish: deviations, performance |

## Backlog (unscheduled)
- F2 screenshot key (vanilla) for interactive play.
- NVIDIA debug output: "vertex shader recompiled based on GL state" (id 131218) on the
  block program in debug runs — find which state triggers it.

## Done (latest 10)
- 2026-10-07 M22 (v0.22.0): weather, sky and clouds, particles, sound, menus and options.
- 2026-10-07 M21 (v0.21.0): doors, plates, TNT, comparators, observers, hoppers, dispensers, rails, minecarts, slimes, piston animation, collision shapes.
- 2026-10-07 M20 (v0.20.0): end2 - outer islands, chorus, end cities, shulkers, elytra, crystals, the ender dragon fight, gateways, pearls, respawning.
- 2026-10-07 M19 (v0.19.0): nether2 - 4 Nether biomes, fortresses, bastions, 7 Nether mobs, status effects, potions, brewing, splash potions.
- 2026-10-07 M18 (v0.18.0): overworld2 - ravines, lakes, springs, 10 biomes, 3 woods, dungeons, temples, mineshafts, strongholds, villages.
- 2026-10-07 M17 (v0.17.0): farming, chests, armor/shields, beds, experience, enchanting, anvils.
- 2026-10-07 M16 (v0.16.0): falling blocks, pathfinding, farm animals, bows/arrows/eggs, explosions, skeletons, creepers, spiders, endermen.
- 2026-10-07 M15 (v0.15.0): random ticks, grass, leaves, saplings, snow/ice melt, fire.
- 2026-10-07 M14 (v0.14.0): fluids - flow, lava/water reactions, swimming, drowning, burning, buckets.
- 2026-10-07 M13 (v0.13.0): Java Edition 1.21.11 saves and defaults, copper tools, versioning.
- 2026-10-07 Vanilla world heights per dimension (Nether/End 0..255).
- 2026-10-07 M12: Nether, End, portals, dimension travel and saves.
- 2026-10-07 M11: block updates, scheduled ticks, redstone components, pistons.
- 2026-10-07 M10: zombies and cows (AI, spawning, saving, models, attacks).
- 2026-10-07 M9: survival, items, crafting, furnace.
- 2026-10-07 M8: overworld generator, biomes, snow, ores, trees.
- 2026-10-07 M7: saves (Anvil regions, level.dat, autosave, session lock).
- 2026-10-07 M6: HUD, F3, chat + commands, creative inventory.
- 2026-10-07 M5: light engine, smooth lighting/AO, daylight cycle, sun/moon/stars.
- 2026-10-06 M4: player physics, raycast/outline/crosshair, break/place, hotbar.
- 2026-10-06 M3: terrain generator, water, fog, chunk streaming, GPU timing, auto-fly bench.
- 2026-10-06 All block textures (6 batches, 1,028 textures, texgen library).
- 2026-10-06 Texture pass: original vanilla-style textures, art style guide, ADR 0006.
- 2026-10-06 M3.0: resource packs, HD + animated sprites.
- 2026-10-06 M2: block states, paletted chunks, flat world, chunk renderer, worker meshing.
- 2026-10-06 M1: camera, controls, textures, atlas, textured cubes.
- 2026-10-06 M0: project setup (build, window, tests, screenshots, docs, tooling).
