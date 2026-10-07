# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-06)
M3 done (pending milestone reviews): seeded terrain generator (Xoroshiro128++, Perlin
octaves, vanilla-like surface rules, deepslate, bedrock floor), water (translucent
pass, 8/9 surface, see-through), cylindrical fog, chunk streaming on worker threads
with neighbour-complete meshing, --render-distance. All 1,028 block textures exist.
Measured (release, RTX 5080, --auto-fly at 4x sprint, 240 fps cap):
| Render distance | Sections / quads | Mesh at start | CPU work/frame p99 / max | GPU max |
|---|---|---|---|---|
| 12 | ~1,000 / 260k | 126 ms | 0.56 / 0.81 ms | 0.04 ms |
| 32 | ~6,900 / 1.8M | 825 ms | 0.89 / 1.36 ms | 0.21 ms |
Movement is still free flight (physics is M4). 75 test cases.

## Next
1. M3 milestone reviews (code / perf / parity agents), fix findings, push.
2. M4.1 — Player physics: vanilla AABB 0.6x1.8, gravity, jumping, walking/sprinting/
   sneaking speeds and friction per tick (wiki: Player, Entity), axis-by-axis
   collision against block shapes, step-up, creative flight toggle (double space).
3. M4.2 — Block raycast (DDA) from the eye, outline of the targeted block.
4. M4.3 — Break (left click) / place (right click) with the chosen block; renderer
   re-meshes the edited section and its neighbours across section/chunk borders.
5. M4.4 — Hotbar selection (1-9 / wheel) of placeable blocks (UI proper is M6).

Deferred performance work (from the M2 perf review) — not needed at current numbers;
revisit if CPU work p99 > 4 ms or GPU > 8 ms on the target hardware:
- Dense ring-indexed section grid (vanilla ViewArea) instead of hash maps; column culling.
- Static section origins + camera block/fraction uniforms; persistent-mapped command ring.
- Arena pages + size classes, per-frame upload budget.
- Faster snapshots / immutable sections readable by workers.
- Per-face-direction draw commands, cave culling.

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
| M3 ✅ 2026-10-06 | Resource-pack loader; simple noise terrain (placeholder for M8), grass/dirt/stone/water layers, chunk loading around the player | Walkable rolling terrain at render distance 12 |
| M4 | Player: vanilla movement & AABB collision, gravity, jumping, sprint/sneak; block raycast, break/place | Movement constants match the wiki; can build a house |
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
- 2026-10-06 M3: terrain generator, water, fog, chunk streaming, GPU timing, auto-fly bench.
- 2026-10-06 All block textures (6 batches, 1,028 textures, texgen library).
- 2026-10-06 Texture pass: original vanilla-style textures, art style guide, ADR 0006.
- 2026-10-06 M3.0: resource packs, HD + animated sprites.
- 2026-10-06 M2: block states, paletted chunks, flat world, chunk renderer, worker meshing.
- 2026-10-06 M1: camera, controls, textures, atlas, textured cubes.
- 2026-10-06 M0: project setup (build, window, tests, screenshots, docs, tooling).
