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
`tests/` also links `rendering` for its GL-free parts (e.g. `CubeMesher`); tests
never create a GL context.

## Main loop (`src/main.cpp`)
```
poll input → clock.advance(frameTime) → tick() × ticksDue (20 TPS) → render(alpha) → swap
```
- **Tick** (50 ms, fixed): all simulation — player physics, entities, block updates,
  random ticks, scheduled ticks. Deterministic given inputs.
- **Frame** (vsync): mouse look applied (per frame, as vanilla); camera position
  interpolated between previous and current tick with `alpha`; upload finished chunk
  meshes; draw; UI.
- Screenshot mode: render `--frames` frames, read the back buffer, write PNG, exit.

## Threading (planned, M2+)
- Main thread: GLFW, GL, tick.
- Worker pool: chunk generation and chunk meshing. Workers read immutable snapshots
  (a chunk plus its 8 neighbours' border data) and return results through a lock-free
  or mutex-guarded queue; only the main thread mutates the world.

## World model (M2.1–M2.2, ADR 0005)
- `ChunkPos {x,z}` (`key()` packs like vanilla's `ChunkPos.toLong`), `BlockPos {x,y,z}`
  (int32), in `world/Chunk.h`. Conversions in `world/Coords.h`.
- `BlockRegistry` / `Blocks.h`: dense `uint16` `BlockStateId`s, air = 0; per-state
  flag arrays (`opaqueCube`) for hot loops. Global instance: `blockRegistry()`.
- `Section`: 4096 states as vanilla's PalettedContainer (single value → 4–8 bit local
  palette → direct ids), index `(y*16+z)*16+x`; `copyTo()` decodes all at once for
  meshing; tracks `nonAirCount` so empty sections are skipped.
- `Chunk` = 24 sections (Y −64..319). `World` = map of `ChunkPos` → `Chunk`;
  unloaded chunks read as air.
- `FlatGenerator`: superflat from vanilla's preset string (default Classic Flat:
  bedrock, 2×dirt, grass at Y −64..−61). Output hash pinned in `tests/world_flat.cpp`.

## Rendering
Current (M1):
- `Camera`: vanilla FOV 70, near plane 0.05; rotation from `world/Rotation.h`.
- `TextureAtlas`: stitches every PNG in `assets/minecraft/textures/block/` (sorted by
  name) on a power-of-two grid + generated `missingno` sprite; nearest mag filter,
  4 mip levels (as vanilla). Sprites are looked up by name at mesh-build time.
- `CubeMesher`: full-cube faces with vanilla directional shade (up 1.0, N/S 0.8,
  E/W 0.6, down 0.5) multiplied by a per-face tint (grass top = plains #91BD59).
  `CubeFaces` helpers mirror vanilla models `cube_all`, `cube_column`, `grass_block`.
- `Mesh`: static VBO of `BlockVertex` (pos, uv, RGBA8 colour), DSA vertex format.
- Fixed bindings: uniform location 0 = `uViewProj`; texture unit 0 = block atlas.
  Shaders: `block` (opaque pass). Add new fixed bindings to this list.
- `WorldRenderer`: owns the block shader, atlas and world mesh; `drawFrame(camera, w, h)`
  does all of a frame's GL work. `main.cpp` makes no GL calls (hard rule 7).
- Test scene in `main.cpp` (`buildTestScene`, GL-free) until chunks exist (M2).

Planned (M2–M5):
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
