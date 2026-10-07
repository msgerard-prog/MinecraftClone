# Architecture

Living map of how the code is put together. Update it when structure changes.

## Layers
Each subsystem is a static library `mc_<name>` (`src/<name>/`). A layer may only
depend on layers **above** it in this list (enforced by CMake target links):

| Layer | Owns | Depends on |
|---|---|---|
| `core` | Window/input (GLFW), fixed-tick clock, logging, command line, files, math helpers, allocators | glfw, glm |
| `world` | Coordinates, block registry & states, chunks/sections, worldgen, lighting, saves | core |
| `rendering` | All OpenGL: context, shaders, textures/atlas, chunk meshes, camera, screenshots | core, world, glad, stb |
| `audio` | Sound (stub) | core |
| `gameplay` | Player, physics/collision, entities, inventory, items, crafting | core, world |
| `ui` | HUD, hotbar, inventory screens, F3 debug, menus | core, rendering, gameplay |
| `main.cpp` | Wires everything together; owns the main loop | all |

`world` and `gameplay` contain **no OpenGL** and are fully unit-testable;
`tests/` links them directly. Keep simulation logic there, not in `rendering`.

## Main loop (`src/main.cpp`)
```
poll input → clock.advance(frameTime) → tick() × ticksDue (20 TPS) → render(alpha) → swap
```
- **Tick** (50 ms, fixed): all simulation — player physics, entities, block updates,
  random ticks, scheduled ticks. Deterministic given inputs.
- **Frame** (vsync): camera interpolated between previous and current tick state with
  `alpha`; upload finished chunk meshes; draw; UI.
- Screenshot mode: render `--frames` frames, read the back buffer, write PNG, exit.

## Threading (planned, M2+)
- Main thread: GLFW, GL, tick.
- Worker pool: chunk generation and chunk meshing. Workers read immutable snapshots
  (a chunk plus its 8 neighbours' border data) and return results through a lock-free
  or mutex-guarded queue; only the main thread mutates the world.

## World model (planned, M2)
- `ChunkPos {x,z}`, `BlockPos {x,y,z}` (int32). See `world/Coords.h` for conversions.
- `Chunk` = 24 `Section`s (Y −64..319). `Section` = 4096 block states stored as a
  **palette + packed index array** (as vanilla does) so most sections cost a few bytes.
- `BlockState` = compact `uint16` id into a global state table generated from the
  block registry (each block × its property combinations, like vanilla's
  `Block.STATE_REGISTRY`).

## Rendering (planned, M1–M5)
- One texture atlas (later array texture) of 16×16 tiles; nearest filtering.
- Chunk meshes per section, face-culled against neighbours; opaque, cutout and
  translucent passes (vanilla's `solid`, `cutout_mipped`, `translucent` render types).
- Vertex: packed position, UV, normal/face, light (sky, block), AO.

## Files outside src/
- `assets/shaders/<name>.vert|.frag` — loaded at runtime from the source tree
  (`MC_ASSETS_DIR`), so shader edits need no rebuild, only a restart.
- `assets/minecraft/` — our own placeholder resource pack in vanilla's layout
  (`blockstates/`, `models/`, `textures/`); `assets/data/minecraft/` — data-pack JSON
  (recipes, loot tables, tags). Both described in data-formats.md.
- `third_party/glad` — generated GL loader (do not edit).
- `cmake/` — `Dependencies.cmake` (pinned FetchContent), `Subsystem.cmake`
  (`mc_add_subsystem`, warnings).
- `tools/` — WSL scripts; `tools/win/*.cmd` load the MSVC environment.
