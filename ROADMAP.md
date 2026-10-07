# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-06)
M4 done (pending milestone reviews): player with vanilla movement physics (walk/
sprint/sneak/jump/fly match the wiki's speeds in tests), AABB collision with step-up
and sneak edge protection, block raycast with outline and crosshair, creative
break/place with vanilla repeat delays and log axes, hotbar 1-9/wheel. 91 test cases.
M3 done and reviewed (code, perf, parity findings fixed or recorded): seeded terrain
generator (Xoroshiro128++, Perlin octaves, vanilla-like surface rules, deepslate,
bedrock floor), water (translucent pass, 8/9 surface, two-sided top/sides, visible
under solid blocks), linear cylindrical fog from 92% of the render distance, chunk
streaming (render circle + all neighbours, chunk recycling), distance culling,
--render-distance. All 1,028 block textures exist. 87 test cases.
Measured (release, RTX 5080, `--auto-fly` = 4x sprint flight, 240 fps cap). "CPU work"
is game-side main-thread time before the swap (excludes the driver thread and vsync);
GPU is drawFrame's two passes after loading:
| Render distance | Loaded sections / quads | CPU work/frame p99 / max | GPU avg / max |
|---|---|---|---|
| 12 | ~1,250 / 305k | 0.53 / 0.83 ms | 0.02 / 0.14 ms |
| 32 | ~7,800 / 2.0M | 0.78 / 0.93 ms | 0.14 / 0.45 ms |
Static load (no flight): RD12 ~0.3 s, RD32 ~0.8 s. Movement is still free flight (M4).

## Next
1. M4 milestone reviews (code / perf / parity agents), fix findings, push.
2. M5.1 — Light engine: sky light (15 from the sky, straight down without loss,
   -1 per block sideways) and block light (emitters, -1 per step), stored per section
   (nibble arrays like vanilla), BFS propagation and removal on edits, on workers.
3. M5.2 — Lit meshes: per-vertex sky/block light in the packed vertex, smooth lighting
   and ambient occlusion (vanilla's 4-sample average per corner).
4. M5.3 — Day/night: time of day (24000 ticks), sky light multiplier, sky colour,
   sun/moon; torch + glowstone blocks to test block light.

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
| M5 | Lighting: sky light + block light propagation, smooth lighting / AO, day–night cycle | Caves dark, torches light correctly, matches vanilla light levels |
| M6 | UI: crosshair, hotbar, inventory screen, F3 debug screen, chat/commands (`/tp`, `/time`, `/give`) | Usable creative-mode inventory |
| M7 | Save/load: region files (format chosen by ADR) | Worlds survive restart; round-trip tests |
| M8 | Faithful 1.21 worldgen: noise router/density functions, multi-noise biomes, aquifers, caves, features | Terrain shapes recognisably vanilla for the same kinds of seeds |
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
- 2026-10-06 M4: player physics, raycast/outline/crosshair, break/place, hotbar.
- 2026-10-06 M3: terrain generator, water, fog, chunk streaming, GPU timing, auto-fly bench.
- 2026-10-06 All block textures (6 batches, 1,028 textures, texgen library).
- 2026-10-06 Texture pass: original vanilla-style textures, art style guide, ADR 0006.
- 2026-10-06 M3.0: resource packs, HD + animated sprites.
- 2026-10-06 M2: block states, paletted chunks, flat world, chunk renderer, worker meshing.
- 2026-10-06 M1: camera, controls, textures, atlas, textured cubes.
- 2026-10-06 M0: project setup (build, window, tests, screenshots, docs, tooling).
