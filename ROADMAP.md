# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-07)
M8 done and reviewed (code, perf, parity findings fixed or recorded): 1.21-style
overworld - climate noises, spline-shaped 3D terrain (oceans, coasts, rivers, hills,
mountain ranges), cheese/spaghetti/noodle caves with surface entrances, lava below
-54, 28 biomes with wiki colours (per-vertex tints, saved palettes), surface rules,
snow layers and ice by altitude temperature, ores and stone blobs per the wiki,
oak/birch/spruce/acacia trees across chunk borders, plants. ~1 ms per chunk; static
load RD12 0.34 s, RD32 1.6 s (release, /Ob2).
M9 started: item textures (40, original), item registry, inventory of item stacks
(saved), item icons, mining speed/harvest levels/drops (not yet used in game).
M7 done: saves. M6 done: UI. M5 done: lighting/day-night.

## Next
M9 — Survival basics:
1. M9.2 — Game modes (/gamemode), survival mining with break progress and times,
   drops as item entities with pickup, health/hunger HUD, fall damage, death/respawn.
2. M9.3 — Crafting: survival inventory screen with 2x2 grid, crafting table (3x3),
   recipes authored from the wiki's recipe pages.
3. M9.4 — Furnace: block entity, smelting, fuel, saved block entities.

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
- From the M8 perf review: cave culling (per-section visibility graph) and arena
  pages (RD32 is now ~6.6M quads, ~400 MB of vertices; growth copies the buffer);
  per-cell interpolation stepping; all-solid section fast path.
- From the M5 perf review: re-mesh only neighbours on the sides whose border light
  changed (an edit marks ~20–100 sections today); incremental light updates instead of
  the 3x3 full relight; pre-size the vertex arena from the render distance (growth
  copies the whole buffer: a ~100 ms frame once at RD32); `cornerLight` offset table;
  Section copy-on-write allocates when a worker still holds the section (rare).

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
- **Save format (ADR 0007, proposed):** vanilla's own Anvil format (region .mca,
  Java 1.21 chunk NBT, gzip level.dat, DataVersion 3955 = 1.21.1). Built because no
  saves existed yet; say if you want something else before worlds matter. Every
  generated chunk is saved (vanilla), so worlds are larger on disk than "edits only".
- M8 in-game checks: sky light under a surface lava pool (lava opacity), the
  default Biome Blend radius, snow line heights in windswept hills/taiga.
- **M8 generator (needs your OK to become the default):** new worlds use a new
  generator kind "overworld"; the M3 placeholder ("terrain", pinned hash) is kept
  for worlds that already use it. `--generator terrain` will select the old one.
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
- The 1.21 patch decision (below) also sets the render distance default (12 up to
  1.21.10; graphics presets from 1.21.11) and mipmap levels (4 vs 2).
- Try your own textures: copy your 1.21.x client jar into `resourcepacks/` (README ›
  Using your own Minecraft textures) and run `tools/run.sh`.
- Try the controls by hand: `tools/run.sh`, click the window, WASD + mouse, Esc.
  Tell me if the mouse feel or speeds are off.
- Decide which exact 1.21 patch ADR 0002 targets (e.g. 1.21.10 vs 1.21.11): some
  defaults changed in 1.21.11 (mipmap levels 4 → 2 with graphics presets).
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
| M9 | Survival basics: items, tools, mining speed/drops, crafting table, furnace, recipes (vanilla JSON) | Wood → stone → iron progression works |
| M10 | Entities & mobs: entity system, physics, AI goals, spawning, health/damage | Zombies/cows behave like vanilla |
| M11 | Redstone: power, dust, torches, repeaters, pistons, update order | Classic circuits behave like vanilla |
| M12 | Dimensions: Nether and End, portals | Can travel to both |

## Backlog (unscheduled)
- Sound (miniaudio) — `src/audio` is a stub until needed.
- Fluids (water/lava flow) — likely between M5 and M9.
- Weather, fog, clouds.
- F2 screenshot key (vanilla) for interactive play.
- NVIDIA debug output: "vertex shader recompiled based on GL state" (id 131218) on the
  block program in debug runs — find which state triggers it.

## Done (latest 10)
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
