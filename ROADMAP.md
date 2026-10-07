# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-06)
M2 done (pending milestone reviews): block registry with vanilla block states,
paletted sections/chunks, superflat generator, face-culling mesher with packed
vertices, arena + multi-draw renderer, worker-thread meshing. 8x8 flat world:
~2600 fps (0.38 ms avg) release, meshed in 2 ms. 54 test cases.

## Next
1. Handle M2 review findings (code / perf / parity agents), push M2.
2. M3.0 — Resource-pack loader (agreed 2026-10-06): read a pack folder or .zip from
   git-ignored `resourcepacks/` (the user's own copy of vanilla textures), override
   our placeholders file by file; animated textures (.mcmeta, frame 0 → animation)
   and non-16px (HD) sprites. Our placeholders stay the in-repo fallback.
   M3 design input from the M2 perf review (apply while building streaming):
   - Dense ring-indexed section grid around the camera (vanilla ViewArea) replacing
     the hash maps in ChunkRenderer/WorldRenderer; frustum-test columns first.
   - Static per-section integer origins + camera block/fraction uniforms (no per-frame
     offset upload); persistent-mapped, fenced ring for the command buffer.
   - Arena in fixed pages with size classes; per-frame upload budget.
   - Faster snapshots (27 section pointers, face/edge/corner loops, uniform-section
     fast fill), or immutable sections readable by workers.
   - Worldgen writes a flat 4096 buffer then `Section::assign` (one palette build).
   - Mesh a column only when all 8 neighbours exist; re-mesh only facing borders.
   - Later: per-face-direction draw commands (back-face groups), cave culling.
3. M3.1+ — terrain: simple noise heightmap, stone/dirt/grass/water/sand layers, chunk
   loading/unloading around the player at render distance 12, worldgen on workers.

## Texture plan (agreed 2026-10-06)
Textures arrive with their blocks (add-block skill makes the placeholder), by
milestone: M3 stone types, ores, gravel, water, sand, sandstone, clay · M5 light
sources · M8 biome blocks, all woods, leaves · M9 items, crafting table, furnace ·
M10–M12 mobs, redstone, Nether/End. Cutout (glass, leaves), translucent (water, ice)
and animated textures get renderer support with the first block that needs them.
Optional art pass on placeholders later (basic graphics first).

## Waiting on the user
- Try the controls by hand: `tools/run.sh`, click the window, WASD + mouse, Esc.
  Tell me if the mouse feel or speeds are off.
- Decide which exact 1.21 patch ADR 0002 targets (e.g. 1.21.10 vs 1.21.11): some
  defaults changed in 1.21.11 (mipmap levels 4 → 2 with graphics presets).
- Optional: confirm the mouse-sensitivity curve in your game (degrees per mouse
  movement at 0%, 100%, 200% sensitivity) — it isn't documented on the wiki.

## Milestones
| # | Milestone | Done when |
|---|---|---|
| M0 | Setup: build, window, tests, screenshot, docs, Claude tooling | ✅ 2026-10-06 |
| M1 | Camera + texture atlas + one textured cube | ✅ 2026-10-06 |
| M2 | Block registry + block states, chunk sections (paletted), face-culled meshing on worker threads | ✅ 2026-10-06 |
| M3 | Resource-pack loader; simple noise terrain (placeholder for M8), grass/dirt/stone/water layers, chunk loading around the player | Walkable rolling terrain at render distance 12 |
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
- Cosmetic: cobblestone placeholder mortar is too thick/dark.
- F2 screenshot key (vanilla) for interactive play.
- NVIDIA debug output: "vertex shader recompiled based on GL state" (id 131218) on the
  block program in debug runs — find which state triggers it.

## Done (latest 10)
- 2026-10-06 M2: block states, paletted chunks, flat world, chunk renderer, worker meshing.
- 2026-10-06 M1: camera, controls, textures, atlas, textured cubes.
- 2026-10-06 M0: project setup (build, window, tests, screenshots, docs, tooling).
