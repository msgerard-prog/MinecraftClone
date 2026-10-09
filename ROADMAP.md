# Roadmap

Only what remains is logged here; finished work lives in git history and the release tags
(v0.13.0 ... v1.5.0; `git log --oneline` per step). Claude rewrites
**Status** and **Next** every session.

## Status (2026-10-09)
Feature-complete at v1.5.0: every milestone M0-M34 is done - vanilla Java 1.21.11 in full,
play parity (M32), the 26.1-26.3 additions (M33) and 26.3's save format (M34). The user
(2026-10-09): "We may be finished. We do not need a 100% match. I will test myself and relay
any issues." Bench: steady CPU p99 0.25 ms, GPU 0.16 ms.

## Next
No milestone is scheduled. Work comes from the user's play-testing reports: each issue gets
a fix, a regression test and a commit (patch versions v1.5.x). Candidates if the user asks
for more, by how much they bring the clone toward vanilla:
- Structures from vanilla's multi-piece layouts (villages, outposts, mansions, trial
  chambers...) - today our own one-piece designs.
- All of vanilla's ~125 advancements with real triggers (today 67); the full recipe book.
- The mob-behaviour gaps recorded in docs/game-design.md.
- Explorer and treasure maps with markers (see Backlog).
- Sounds: a pass on our synthesized sounds, or loading vanilla's from `resourcepacks/`.
- Worldgen closer to vanilla's numbers (our generator is vanilla's Perlin pipeline with our
  own noise settings/seeding/biome table - the wiki documents the formats, not the values);
  would be a new pinned generator (overworld9). Not needed: no 100% match required.
All recorded differences from vanilla: docs/game-design.md › Known deviations from vanilla.

## Backlog (unscheduled)
- F2 screenshot key (vanilla: PNG to `screenshots/` named by date-time, chat message; F1 hides
  the HUD). Added by Claude at M1.3, not a user request.
- Explorer and buried treasure maps: locate structures from their grids and draw map markers
  (26.3's camp maps, shipwreck treasure maps; chests hold empty maps until then).
- 26.4 content once it is released (the wiki lists released versions only).

## Waiting on the user
- **M34 note:** (user, 2026-10-09: yes to 26.3's save format) worlds now save as 26.3
  (DataVersion 5023, 26.1 layout); an older world is moved the first time it opens, after
  which v1.4.0 and earlier (and vanilla 1.21.11) can't read it. In-game check, if you have
  26.3: copy one of our worlds into `.minecraft/saves` - does it open with its time of day and
  game rules (those files' insides are our best guess)?
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

## Deferred performance work
Not needed at current numbers; revisit if CPU work p99 > 4 ms or GPU > 8 ms on the target
hardware. Notes kept from the perf reviews:
- Dense ring-indexed section grid (vanilla ViewArea) instead of hash maps; column culling.
- Persistent-mapped command ring.
- Arena pages + size classes, per-frame upload budget.
- Faster snapshots / immutable sections readable by workers.
- Per-face-direction draw commands, cave culling.
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

