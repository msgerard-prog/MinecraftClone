# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-07)
M15 done (reviews applied; v0.15.0): random ticks (3 per section, chunks within the
simulation distance with loaded neighbours), grass spread/death, leaf distance and
decay with loot, saplings growing worldgen trees, snow/ice melting, fire (aging,
burning, spreading by the wiki's odds, 128-block radius, infiniburn), lava starting
fires, flint and steel. M14 fluids (v0.14.0); M1-M13 done.

## Next
Agreed plan (2026-10-07): M13 the 1.21.11 migration, then the missing gameplay
systems M14-M22, then tag the codebase **v1.0** before polish (deviations, perf).

M16 — Falling blocks and mobs 2 (wiki: Falling Block, Mob AI, each mob's page):
1. ✅ M16.1 — Falling blocks: sand, red sand, gravel fall 2 ticks after losing support
   (falling_block entity with gravity, lands as the block or drops as an item on
   non-full blocks, crushes nothing), saved with the chunk's entities.
2. ✅ M16.2 — Pathfinding: A* over walkable nodes (vanilla's node types subset: open,
   walkable, blocked, danger: lava/fire/cactus), follow range, used by zombies and
   herd animals; mobs avoid drops > 3 and fluids where vanilla does.
3. ✅ M16.3 — Passive mobs: sheep (wool colours, eating grass, regrowing wool), pig,
   chicken (eggs, slow falling, flapping); breeding (love mode, babies growing up,
   tempt goals) with wheat / wheat seeds / carrots; their drops and items.
4. ✅ M16.4 — Projectiles and explosions: arrows (gravity, drag, damage by speed,
   sticking), explosions (vanilla ray algorithm, block resistance, entity damage).
5. ✅ M16.5 — Hostile mobs: skeleton (bow, strafing, burns in daylight), creeper
   (swelling, explosion 3), spider (climbing walls, neutral in light), enderman
   (teleport, stare aggro, carrying blocks, water damage). Original models/textures.

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
- Target patch: **1.21.11** (ADR 0002 update). Migration to do: DataVersion 4671 and
  1.21.5+ NBT names in saves, 1.21.11 defaults (render/graphics presets, mipmaps,
  Nether fog 10-96, sneak-sprint), then 1.21.9-1.21.11 content as milestones.
- The newest generator of each dimension is the default for new worlds, always;
  older generators stay only for worlds created with them (their pinned hashes).

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
- **M15 question (save compatibility):** leaves you placed in worlds saved before
  v0.15.0 were stored as `distance=7, persistent=false` (placement didn't set
  persistent yet), so they now decay. Shall I add a one-time load upgrade that marks
  such leaves persistent in old saves (needs a marker tag in level.dat: a save-format
  change), or accept the loss? Also in-game: does grass under one block of still water
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
| M16 | Falling blocks; mobs 2: pathfinding, sheep/pig/chicken, skeleton/creeper/spider/enderman, projectiles, breeding | Classic mobs behave like vanilla |
| M17 | Items & survival 2: armor, bows, shields, chests/containers, beds, enchanting, anvils, brewing, farming | Core survival loop complete |
| M18 | Overworld 2: remaining biomes, aquifers, lakes, ravines; structures (villages, dungeons, mineshafts, temples, strongholds) | Seeds look like vanilla's kind of world |
| M19 | Nether 2: biomes (crimson/warped, soul sand valley, basalt deltas), fortresses, bastions; ghasts, piglins, blazes, magma cubes | Nether as in 1.21 |
| M20 | The End 2: ender dragon fight, crystals, gateways, outer islands, end cities | Dragon can be beaten |
| M21 | Redstone 2: comparators, observers, pressure plates, hoppers, droppers/dispensers, doors, TNT, rails, slime, piston animation | Common farms/contraptions work |
| M22 | World & presentation: weather, clouds, sky gradient/sunsets, sounds, particles, pause/options/world-creation menus | Feels like the real game |
| v1.0 | Tag the codebase (git tag v1.0) | Then polish: deviations, performance |

## Backlog (unscheduled)
- Sound (miniaudio) — `src/audio` is a stub until needed.
- Weather, fog, clouds.
- F2 screenshot key (vanilla) for interactive play.
- NVIDIA debug output: "vertex shader recompiled based on GL state" (id 131218) on the
  block program in debug runs — find which state triggers it.

## Done (latest 10)
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
