# Roadmap

Claude rewrites **Status** and **Next** every session and ticks steps as they land.
Milestone details live here; design detail lives in `docs/`.

## Status (2026-10-06)
M0 done: CMake/MSVC build driven from WSL, hello-triangle window, doctest suite
(7 cases), `--screenshot`, hooks (format + Stop build), docs, skills, agents.
Next session starts M1.

## Next
1. M1.1 — `gfx::Camera` (perspective, yaw/pitch, vanilla FOV 70) + fly controls
   (WASD/space/shift, mouse look, Esc releases cursor). Add `--pos x,y,z --look yaw,pitch`.
2. M1.2 — Our own 16×16 placeholder textures + `TextureAtlas` (stb_image, nearest filter, mips).
3. M1.3 — Draw one textured cube; replace the hello triangle; screenshot check.

## Waiting on the user
- Nothing.

## Milestones
| # | Milestone | Done when |
|---|---|---|
| M0 | Setup: build, window, tests, screenshot, docs, Claude tooling | ✅ 2026-10-06 |
| M1 | Camera + texture atlas + one textured cube | A screenshot shows a textured cube from a scripted camera |
| M2 | Block registry + block states, chunk sections (paletted), face-culled meshing on worker threads | A flat 8×8-chunk world renders at 60 fps with correct culling |
| M3 | Simple noise terrain (placeholder for M8), grass/dirt/stone/water layers, chunk loading around the player | Walkable rolling terrain at render distance 12 |
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

## Done (latest 10)
- 2026-10-06 M0: project setup (see Status).
