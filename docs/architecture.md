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
`tests/` also links `rendering` for its GL-free parts (e.g. `ChunkMesher`); tests
never create a GL context.

## Main loop (`src/main.cpp`)
```
poll input → clock.advance(frameTime) → tick() × ticksDue (20 TPS) → render(alpha) → swap
```
- **Tick** (50 ms, fixed): all simulation — player physics (`gameplay/Player`:
  vanilla acceleration/friction/gravity, axis-by-axis AABB collision, step-up,
  sneak edge protection, creative flight), later entities, block updates, random and
  scheduled ticks. Deterministic given inputs. Texture animations advance here too.
- **Frame** (vsync): mouse look applied (per frame, as vanilla); camera position
  interpolated between previous and current tick with `alpha`; upload finished chunk
  meshes; draw; UI.
- Screenshot mode: render `--frames` frames, read the back buffer, write PNG, exit.

## Threading (M2.4)
- Main thread: GLFW, GL, tick, all `World` reads/writes, section snapshots, uploads.
- `MeshWorkers` (hardware threads − 1): run `meshSection` on 18³ snapshots. Jobs and
  their buffers are recycled through queues (`core/WorkQueue.h`), so steady-state
  meshing doesn't allocate. Each section has a version number; a result older than
  the latest submission is dropped (the section changed meanwhile).
- Workers only read the immutable `blockRegistry()` and baked `BlockModels`.
- `world::ChunkLoader` (cores/4 threads): generates missing chunks within render
  distance + 1, nearest first, bounded in flight; finished chunks are inserted by the
  main thread; chunks beyond render distance + 3 unload. `TerrainGenerator` is
  immutable and pure, so workers share it.
- The renderer meshes a chunk only when it and its 8 neighbours are loaded
  (`WorldRenderer::onChunksLoaded`), so loaded-area edges never show walls.

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
Pipeline (M2.3): `World` → `snapshotSection` (section + 1-block border, 18³ states)
→ `meshSection` (face culling, packed quads) → `ChunkRenderer` (arena + one
multi-draw) → screen.
- `Camera`: vanilla FOV 70, near 0.05; rotation from `world/Rotation.h`.
  `viewProjectionAtOrigin()` is used for drawing: geometry is camera-relative.
- `TextureAtlas`: stitches every PNG in `assets/minecraft/textures/block/` (sorted by
  name) on a power-of-two grid; `missingno` is always sprite 0; nearest mag filter,
  4 mip levels. Sprites are addressed by grid index in vertices.
- `BlockModels`: per-state baked models (sprite, rotation, tint per face), built once
  at startup. Block → model mapping is C++ (`BlockModels.cpp`) until the JSON loader.
- `ChunkMesher` (GL-free, thread-safe): emits a face when the neighbour is not an
  `opaqueCube`; 4 `PackedVertex` (8 bytes) per quad: section-local xyz, face, UV
  corner, sprite, tint. Directional shade comes from the face in the shader.
- `ChunkRenderer`: one vertex arena buffer sub-allocated in quads (`RangeAllocator`,
  grows by copying), one shared quad index buffer (baseVertex per section), per-frame
  CPU frustum culling, then one `glMultiDrawElementsIndirect`. Each draw's
  `sectionOrigin − cameraPos` (double → float) goes to an SSBO read with
  `gl_BaseInstance`. Command/offset arrays are reused (no per-frame allocation).
- `WorldRenderer`: owns the above; `markChunkDirty` (chunk + 4 neighbours),
  `update()` re-meshes dirty sections, `drawFrame()` does all of a frame's GL work.
  `main.cpp` makes no GL calls (hard rule 7).

Fixed bindings (add new ones here):
| Kind | Slot | Use |
|---|---|---|
| uniform location | 0 | `uViewProj` (camera at origin) |
| uniform location | 1 | `uAtlasColumns` |
| uniform location | 2 | `uGrassColor` |
| uniform location | 3 | `uWaterColor` |
| uniform location | 4 | `uFog` (start, end in blocks) |
| uniform location | 5 | `uFogColor` (sky) |
| texture unit | 0 | block atlas |
| SSBO binding | 0 | section offsets (block pass) |

Passes (M3.2): **opaque**, then **translucent** (`BakedModel::translucent`: water...)
with alpha blending, no depth writes, no back-face culling (water seen from below),
sections sorted far→near each frame. Fluids hide faces against the same fluid and
lower their top vertices by 1/9 (`fluidTop` vertex flag; vanilla source height 8/9).
Linear-smoothstep distance fog to the sky colour from 75% of the render distance.

Known simplifications: uploads use `glNamedBufferSubData` (persistent-mapped staging
when uploads get heavy); translucent sorting is per section, not per quad; no cutout
pass yet (leaves/glass panes come with their blocks). Meshing threads: see Threading.

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
