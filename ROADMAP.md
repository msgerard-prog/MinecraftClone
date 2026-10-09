# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-09)
M33 done (reviews applied; v1.4.0): the 26.x additions - 26.1 golden dandelion and name tags,
26.2 sulfur caves (sulfur/cinnabar, potent sulfur and geysers, spikes, sulfur cubes, "Bounce"),
26.3 poplars, the dappled forest, shelf mushrooms, wool/concrete stairs and slabs, straw beds,
cushions, abandoned camps and the behaviour changes; overworld8 is the default generator
(pinned). Bench: steady CPU p99 0.25 ms, GPU 0.16 ms. M32 done (v1.3.0).

## Next
Nothing scheduled: the user's request (play parity, then the 26.x additions) is done. Open:
the DataVersion question (Waiting on the user); 26.4 when it releases; the backlog below.

Deferred performance work (from the M2 perf review) — not needed at current numbers;
revisit if CPU work p99 > 4 ms or GPU > 8 ms on the target hardware:
- Dense ring-indexed section grid (vanilla ViewArea) instead of hash maps; column culling.
- Static section origins (camera block/fraction uniforms done 2026-10-08); persistent-mapped command ring.
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
- From the M32 perf review (bench: night on Hard CPU p99 ~1.2 ms, streaming ~1.0 ms, GPU
  0.18-0.37 ms; fixed in the review: pickup staggered to every 4 ticks, one sight ray per
  skeleton, InhabitedTime saves only once moved): monster gear lives in 27-slot mob-store
  entries that are copied and may grow the destination vector when a geared mob changes chunk
  (a hard-rule-1 exception like the M26 mount chests - keep 5 stacks inline or in a pooled side
  table); `ItemEntities::takeOne` searches for a pointer it could index; the spawn cycle's
  289 `rainHeight` walks a tick (~0.05-0.1 ms).
- From the M30 perf review (bench: CPU p99 ~1.0 ms steady, ~1.5 ms streaming, GPU 0.16 ms -
  no regression): item merging scans the whole pool per moving item (a per-tick cell hash
  when drops pile up: ~3-5 ms at 2048 moving items); mob suffocation reads 4 cells a mob a
  tick (look each cell up once, or check every few ticks); 8-direction paths and wide
  footprints cost 2-8x per node (classify cells once per search, cap chase searches); a
  chunk holding drops is re-saved on every autosave (mark it only when its drops changed).
  Build note: the debug test PDB grows past MSVC's limit after many incremental links
  (LNK1140) - delete `out/build/debug/**/*.pdb` when it happens.
- From the M29 perf review (fixed in the review: Frost Walker relights, bubble columns
  generated in place, command chain loops, the jockey pass): lightning rods are found by
  scanning the sections of 17x17 chunks per strike (a per-chunk rod list, as vanilla's POI);
  shelves draw their items by looping over every chest of a chunk; hopper minecarts scan all
  dropped items every 4 ticks; glowing mobs add a second draw each; repeating command blocks
  and command minecarts allocate their result messages each run (`CommandResult::message`
  strings, `format`) - an accepted exception to hard rule 1 while one is powered.
- From the M28 perf review (bench: CPU p99 0.97 ms steady, ~1.7 ms streaming; GPU 0.16 ms):
  holding a filled map costs ~0.1-0.3 ms a tick (`Maps::update` looks the chunk up per
  pixel and walks each column from the top: cache the Chunk*, start from the last top Y);
  item frames are drawn with a chunk lookup + linear mob-store scan each (O(F^2) per chunk
  for map walls: keep the store index); decorations are full MobData mobs in every mob scan
  (a decoration store, with the M10 pool; ambient rolls and resting armor stands are skipped
  since the review); llama caravan followers look their head up over 5x5 chunks a tick;
  banner/firework extras de-duplicate by O(N) scan under a mutex (intern by hash); the
  held-map upload sends all 64 KB when any pixel changed.
- From the M27 perf review (bench: no change - CPU p99 ~1.2 ms, GPU ~0.35 ms on both
  overworld5/6): per-chunk sculk listener lists instead of scanning for sensors (done:
  each section decoded once); budding amethyst relights 3x3 chunks per bud (the M15
  incremental-light item); trial spawners recount their mobs over 7x7 chunks a tick
  (keep a count); ancient city/mansion/chamber boxes looped whole per chunk (clip first);
  under Darkness the full render distance is still drawn.
- From the M26 perf review (bench: CPU p99 1.08 -> 1.35 ms, GPU 0.25 ms): fox item scans
  every tick (stagger like allays could); mob-store push_back allocates on the first
  chest per chunk; `pointMount` marks the chunk dirty every frame while a mount screen is
  open; hive chunks stay in the ticking list; `findCart` runs several times a tick; the
  mineshaft cobweb loop could clamp to the chunk (output unchanged).
- From the M25 perf review (streaming bench: no CPU change, p99 ~1.4-1.6 ms on both
  overworld3/4; GPU avg 0.17 -> 0.27 ms): overworld4 has +8-29% quads and 2.5x the
  translucent sections (ocean-floor plants; flooded-cave water surfaces and new cave
  entrances in low columns) - find the water's source and pull the arena pre-size/pages
  item forward (RD32 ~ +90 MB of vertices in ocean seeds); use the coral table in
  blockDrops (string ends_with per drop, a "dead_" string per coral block); water mobs
  and boats compute fluidContact twice a tick (pass it to physics, with the M19 strider
  item); fish schools spawn into 4-slot chunk mob vectors and mark the chunk dirty per
  spawn (with the M10 mob pool); boatId builds 10 strings per placement click.
- From the M24 perf review: a per-chunk POI index (vanilla PoiManager) instead of
  section scans for beds/job sites/bells; trade offers in a side pool (MobData is
  1048 bytes since M24); `mobByUuid` own chunk first or a target-chunk hint (pillagers
  look their target up twice a tick); breeding partner search once a second; check
  remembered points every 20 ticks; villager item pickup only from nearby chunks;
  `Mobs::die`'s 64-entry drop-name cache now holds ~47 names (move to item statics
  before M26); generated villagers could get their bed as home at generation.
- From the M23 perf review: one playing jukebox holds 15-25 of the 32 sound voices
  (a music voice budget, or stop the previous lead note - with the M22 sound item);
  the item-contents table (`world/ItemContainers`) never frees entries - each chunk
  load of a stored shulker box adds ~1 KB (intern identical contents or ref-count);
  cache sign glyph geometry per sign instead of rebuilding it each frame; a dense
  per-state `likeOf` array; ItemStack grew to ~36 bytes (side table if pools grow);
  sign/sheep dyeing builds 16 strings per click.
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
  the 3x3 full relight; `cornerLight` offset table (the arena is pre-sized since
  2026-10-08);
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
- (2026-10-07) Small parity details that can't be checked from the wiki: make the
  best assumption and move on (kept: note-block bass drum guess, disc lengths, our
  composed disc tunes). The clone is a base to study and spin off, not a 100% match.
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
- **M33 decision (DataVersion):** matching 26.3 would raise the saves' DataVersion above
  1.21.11's 4671 - a save-format change. Until you say so, the 26.x content is added and
  saves stay at 4671 (vanilla 1.21.11 then can't know the new blocks).
- **M33 note:** new worlds use "overworld8" (pinned since v1.4.0). Try it: `tools/run.sh
  --world "M33 test" --seed 42 --pos 584,110,-600` (a dappled forest; a camp near 967,-318);
  find a sulfur cave deep under a low, eroded area; `/give @s golden_dandelion` on a baby.
  Kept as ours: the camp layout, the poplar canopy shape, sulfur cube archetype factors,
  "sneak to till" with a shield.
- **M32 checks (in-game):** how long a zombie takes to break a door on Hard (ours 12 s; the
  wiki "about 10 s"); trading's gossip per trade (`/data get entity @e[type=villager,limit=1,
  sort=nearest] Gossips` after one trade: ours 2, the wiki's table 4). Try it:
  `tools/run.sh --world "M32 test" --difficulty hard`, a night out; E then drag a stack.
- **M30 note:** try `tools/run.sh --world "M30 test"`: F5 (three views), swing a sword and
  watch the bar under the crosshair, sprint under water to swim, crawl under a trapdoor, die
  and reload (your drops wait where you fell), E in creative (tabs, Search), E in survival
  with the green book. In-game checks: does a wandering trader open doors in Java, and where
  exactly does the creative inventory put the Operator Utilities tab?
- **M29 note:** new worlds use "overworld7" and "nether4" (pinned since v0.29.0). Try it:
  `tools/run.sh --world "M29 test"`, find a jungle (melons, cocoa) or a swamp (lily pads);
  `/give @s command_block`, place it and right-click it; `/give @s light` to see light
  blocks; trap a tripwire. Unverifiable details kept as best guesses: the shelf's "rightmost
  hotbar slots" wording (from the wiki), powder-snow movement factors (ours).
- **M28 note:** (user, 2026-10-08) on Peaceful with natural_health_regeneration false,
  hunger doesn't refill - kept. Try it: `tools/run.sh --world "M28 test"`, Esc >
  Advancements, `/give @s mace`, `/give @s crossbow`, `/give @s firework_rocket 16` with
  an elytra.
- **M27 note:** new worlds use "overworld6" (all of vanilla's biomes, cave biomes and the
  M27 structures), frozen as of v0.27.0. Try it: `tools/run.sh --world "M27 test"`, look
  for a pale garden at night (creakings), a lush cave, an ancient city deep under the
  mountains (sneak!), a trial chamber, trail ruins with a brush (`/give @s brush`).
- **M26 note:** new worlds use "overworld5" (bee nests, berry bushes, infested blocks),
  frozen as of v0.26.0. Try it: `tools/run.sh --world "M26 test"`, tame a wolf
  (`/give @s bone 8`), ride a horse (`--mount`), build a Wither in creative
  (soul sand T + 3 wither skeleton skulls) - or a copper golem (carved pumpkin on copper).
- **M25 note:** new worlds use "overworld4" (oceans), frozen as of v0.25.0. Try it:
  `tools/run.sh --world "M25 test"`, swim in a warm ocean (reefs), fish with a rod
  (`/give @s fishing_rod`), sail a boat (`/give @s oak_boat`), find a monument in a deep
  ocean.
- **M24 note:** overworld3 (new worlds) is frozen as of v0.24.0; worlds made with M24
  development builds may show seams near pillager outposts (outposts became 5x rarer
  in the review). Try it: `tools/run.sh --world "M24 test"`, find a village, trade
  (right-click an employed villager), `/effect give @s bad_omen 6000` near the bell
  for a raid.
- **M23 note:** new worlds use the "nether3" Nether (ancient debris); nether2 is now
  pinned too and older worlds keep it.
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
| M23 | Building blocks & workstations: slabs, stairs, walls, panes, carpets, ladders, signs, lanterns, campfires, all wood types' doors/trapdoors/fences, mangrove/bamboo/pale oak, copper ageing, concrete, stained glass; stonecutter, smithing (netherite, trims), grindstone, loom, cartography, composter, cauldron, barrel, smoker, blast furnace, ender chest, shulker boxes, beacon, conduit, note block, jukebox | ✅ 2026-10-07 v0.23.0 (our own disc tunes, loom/cartography screens only: see deviations) |
| M24 | Villages 2: villagers (professions, trading, breeding), iron golems, wandering traders, pillagers, outposts and raids, witches | ✅ 2026-10-08 v0.24.0 (no gossip, bell-centred raids: see deviations) |
| M25 | Oceans: water aquifers, ocean biomes and features, drowned, guardians and ocean monuments, shipwrecks, ocean ruins, boats, fishing, fish, squid, dolphins, turtles, tridents | ✅ 2026-10-08 v0.25.0 (simple flooded caves, own monument design, no chest boats: see deviations) |
| M26 | Mobs 3: wolves, cats, horses, llamas, foxes, bees, goats, frogs, axolotls, pandas, parrots, polar bears, allays, phantoms, silverfish, cave spiders, wither skeletons and the Wither, the warden, the breeze, 1.21.6-1.21.11 mobs (happy ghast, copper golem, nautilus...) | ✅ 2026-10-08 v0.26.0 (no leads, shoulders or statues; the warden moves to M27.3: see deviations) |
| M27 | World 3: the remaining biomes, lush and dripstone caves, the deep dark and ancient cities, woodland mansions, ruined portals, trial chambers, trail ruins, geodes, archaeology | ✅ 2026-10-08 v0.27.0 (our own structure designs, cave biomes by column climate: see deviations) |
| M28 | Progression & game: difficulty settings, adventure/spectator modes, advancements, statistics, game rules, maps/compass/clock, books, leads (llama caravans), item frames, paintings, armor stands, banners, fireworks, crossbows, mace, spears, lingering potions, tipped arrows | ✅ 2026-10-08 v0.28.0 (67 advancements with simple triggers, our spear charge formula: see deviations) |
| M29 | Completeness: every 1.21.11 block, item, entity, effect and enchantment; overworld7/nether4 | ✅ 2026-10-09 v0.29.0 (simplified technical blocks, chunk-local ore veins: see deviations) |
| M30 | Play feel: player model and views, attack cooldown, poses, saved drops, pathfinding 2, creative tabs, recipe book | ✅ 2026-10-09 v1.1.0 (basic recipe book, flat held items: see deviations) |
| M31 | Performance: incremental light, dense chunk grid, ring work queue, cheap guards | ✅ 2026-10-09 v1.2.0 |
| M32 | Play parity: spawning, monsters, damage rules, inventory handling, villagers 2, mob rows | ✅ 2026-10-09 v1.3.0 (gossip about one player, no gossip sharing: see deviations) |
| M33 | The 26.x additions: 26.1, 26.2 (sulfur caves, sulfur cubes), 26.3 (poplars, dappled forest, camps), overworld8 | ✅ 2026-10-09 v1.4.0 (our camp design, no explorer maps or fallen poplars: see deviations) |
| v1.0 | Tag the codebase (git tag v1.0) - only when the first revision is complete | ✅ 2026-10-09 v1.0 (then polish: deviations, performance) |

## Backlog (unscheduled)
- F2 screenshot key (vanilla) for interactive play.
- Explorer and buried treasure maps: locate structures from their grids and draw map markers
  (26.3's camp maps, shipwreck treasure maps).

## Done (latest 10)
- 2026-10-09 M33 (v1.4.0): the 26.x additions - golden dandelion, sulfur caves and cubes, potent sulfur, poplars, dappled forest, straw beds, cushions, camps; overworld8.
- 2026-10-09 M32 (v1.3.0): play parity - spawn cycle, regional difficulty, zombies 2, monster gear, damage rules, saved projectiles, inventory shortcuts, gossip, mob rows.
- 2026-10-09 M31 (v1.2.0): incremental light, dense chunk grid, ring work queue, path search cap, drops saved only when changed.
- 2026-10-09 M30 (v1.1.0): play feel - views and player model, combat cooldown, poses, saved drops, pathfinding 2, creative tabs, recipe book.
- 2026-10-09 M29 (v0.29.0) and v1.0: the remaining mobs, effects, enchantments, items and blocks, the Copper Age, technical blocks, overworld7 and nether4; a pinned completeness test.
- 2026-10-08 M28 (v0.28.0): game rules, difficulty, game modes, statistics, navigation, maps, books, decorations, leads, banners, crossbows, arrows, lingering potions, fireworks, the mace, spears, the remaining blocks, advancements.
- 2026-10-08 M27 (v0.27.0): overworld6 - the remaining biomes, creakings, lush and dripstone caves, the deep dark, sculk, the warden, ancient cities, geodes, ruined portals, mansions, trial chambers, archaeology, sniffers.
- 2026-10-08 M26 (v0.26.0): pets, mounts, wildlife, bees, frogs, axolotls, cave spiders, silverfish, wither skeletons, phantoms, heads, the Wither, the breeze, allays, nautiluses, happy ghasts, copper golems.
- 2026-10-08 Fixed known issues: thin dark lines over far water (atlas filtering at sprite edges; UVs now clamped inside the sprite), exact section offsets (no cracks), the arena reserved exactly (no 200-400 MB growth copy, NVIDIA 131186); the old 131218 recompile warning no longer appears.
- 2026-10-08 M25 (v0.25.0): oceans - waterlogging, ocean plants and corals, deep oceans, flooded caves, icebergs, water mobs, fishing, boats, drowned, tridents, dolphins, turtles, shipwrecks, ruins, buried treasure, guardians and monuments, sponges.
- 2026-10-08 M24 (v0.24.0): villagers, trading, breeding, zombie villagers, iron golems, witches, wandering traders, pillagers, patrols, outposts, illagers, raids.
- 2026-10-07 M23 (v0.23.0): building blocks, woods, signs, copper, workstations, ender chests, shulker boxes, netherite, beacons, conduits, note blocks, jukeboxes.
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
