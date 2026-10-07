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
  sneak edge protection, creative flight), block interaction (`BlockInteraction`:
  raycast target, break/place with vanilla repeat delays; edits go to
  `WorldRenderer::onBlocksChanged`, which re-meshes the section and border
  neighbours, and to `LightManager`, which relights around them), the day time
  (+1 per tick, `world/DayTime.h`), later entities, block updates, random and
  scheduled ticks. Deterministic given inputs. Texture animations advance here too.
- **Frame** (vsync): mouse look applied (per frame, as vanilla); camera position
  interpolated between previous and current tick with `alpha`; upload finished chunk
  meshes; draw; UI.
- Screenshot mode: render `--frames` frames, read the back buffer, write PNG, exit.

## Input and screens (`main.cpp`, M6)
One screen owns the keyboard at a time: **chat** (T, or / pre-filled) takes the typed
text stream (characters and backspaces in order), Enter, Esc, Up/Down; **creative
inventory** (E) takes clicks, wheel, 1-9 edges, Esc/E; otherwise the **game** reads
movement keys and clicks while the mouse is captured. Presses for the other modes
are drained each frame, and game presses (jump, use) are dropped while the mouse is
free, so nothing typed acts later. Chat lines are queued and run as commands
(`gameplay/Commands`) at the start of the next tick.

## Threading (M2.4)
- Main thread: GLFW, GL, tick, all `World` reads/writes, section snapshots, uploads.
- `MeshWorkers` (hardware threads − 1): run `meshSection` on 18³ snapshots. Jobs and
  their buffers are recycled through queues (`core/WorkQueue.h`), so steady-state
  meshing doesn't allocate. Each section has a version number; a result older than
  the latest submission is dropped (the section changed meanwhile).
- Workers only read the immutable `blockRegistry()` and baked `BlockModels`.
- `world::ChunkLoader` (cores/4 threads): generates missing chunks within render
  distance + 1, nearest first, bounded in flight; finished chunks are inserted by the
  main thread; chunks beyond render distance + 3 unload. The `ChunkGenerator` is
  immutable and pure, so workers share it.
- `world::ChunkStorage` (1 IO thread, M7): the chunk loader's workers load saved
  chunks from region files before generating; dirty chunks are snapshotted (shared
  sections, no copy) and written by the IO thread on unload, autosave (6000 ticks)
  , pause (Esc) and exit. level.dat is written by the main thread: an accepted
  exception to "no blocking IO on the main thread" (a few KB, every 5 minutes or on
  pause). See data-formats.md.
- `world::LightManager` (cores/4 threads, M5): lights a chunk once its 3x3
  neighbourhood is loaded (the loader keeps render distance + 2 rings for this).
  Jobs hold `shared_ptr`s to the neighbourhood's sections; sections are
  copy-on-write (`Chunk::mutableSection` copies a section a worker still holds), so
  the main thread keeps editing while jobs run. Each chunk has a version; stale
  results are dropped. An edit relights the 3x3 chunks around it; sections whose
  light changed are reported for re-meshing.
- The renderer meshes a chunk only when it and its 8 neighbours are lit
  (`WorldRenderer::onChunksLit`), so loaded-area edges never show walls. Mesh jobs
  also capture shared section/light pointers (`captureSection`, 27 sections) and
  build the 18³ padded arrays on the worker (`buildPadded`).

## World model (M2.1–M2.2, ADR 0005)
- `ChunkPos {x,z}` (`key()` packs like vanilla's `ChunkPos.toLong`), `BlockPos {x,y,z}`
  (int32), in `world/Chunk.h`. Conversions in `world/Coords.h`.
- `BlockRegistry` / `Blocks.h`: dense `uint16` `BlockStateId`s, air = 0; per-state
  flag arrays (`opaqueCube`) for hot loops. Global instance: `blockRegistry()`.
- `Section`: 4096 states as vanilla's PalettedContainer (single value → 4–8 bit local
  palette → direct ids), index `(y*16+z)*16+x`; `copyTo()` decodes all at once for
  meshing; tracks `nonAirCount` so empty sections are skipped.
- Heights (`world/Coords.h` `HeightRange`, vanilla LevelHeightAccessor): the
  Overworld is Y −64..319 (24 sections), the Nether and End Y 0..255 (16). `World`
  holds the current dimension's (`height()`, `isInHeight`); every chunk carries its
  own (`Chunk::height()`, `sectionCount()`), and section arrays have capacity
  `kMaxSections` (24). Section index = (y − minY) >> 4; "section Y" = y >> 4. Light,
  meshing (`SectionRefs::minSection/maxSection/openSky`), saving (`yPos` = the lowest
  section) and range checks all read it; nothing assumes −64..319.
- `Chunk` = its dimension's sections, each a `shared_ptr<Section>` (copy-on-write)
  plus per-section light (`SectionLight`: sky + block `LightLayer`, a uniform value
  or 2048-byte nibble array, like vanilla's DataLayer).
- Light (`world/LightEngine`, wiki: Light): computed per chunk over a 46×46 column
  region (the chunk + 15-block margin, enough for any light to reach it): sky light
  15 straight down through opacity-0 blocks, then BFS with loss max(1, opacity);
  block light BFS from emitters. Opacity per state (`lightOpacity`: opaque 15,
  water 1, else 0); emission per state (`lightEmission`). Full recompute per chunk,
  not incremental. `World` = map of `ChunkPos` → `Chunk`;
  unloaded chunks read as air.
- Generators (`world/ChunkGenerator`): `OverworldGenerator` (M8, kind "overworld",
  default for new worlds) - climate columns -> spline-shaped density on 4×8×4 cells,
  trilinear interpolation, cheese/spaghetti/noodle caves, biome per 4×4 column,
  surface rules, ores, trees (planned per chunk and cached per worker, so canopies
  from the 8 neighbours are placed exactly), plants, snow/ice top layer; the whole
  chunk is built in one flat array and each section encoded once. `TerrainGenerator`
  (M3 placeholder, kind "terrain") stays for worlds created with it. Both pin a hash.
- Biomes (`world/Biome`): 28 vanilla biomes with wiki colours; each chunk holds a
  shared immutable `ChunkBiomes` (one per 4×4×4 cell); mesh workers get the centre
  chunk's, vertices carry an 8-bit tint slot (w2 bits 12-19) read from the tint
  palette SSBO.
- Items (`world/Items`): item registry (block items + tools/materials/food),
  `ItemStack`; the player's 36-slot `gameplay/Inventory`.
- Survival (M9, gameplay): `Vitals` (health/hunger/falls), `Mining` (break ticks,
  harvest levels, drops), `BlockInteraction::tickSurvival` (break progress, wear,
  placing uses items, eating), `ItemEntities` (dropped stacks: physics, pickup,
  despawn; pooled), `Recipes` (crafting/smelting/fuel tables authored from the
  wiki), `Furnace` rules over `world::FurnaceData` block entities stored in their
  chunk (saved as block_entities, ticked each game tick by main).
- Mobs (M10): `world::MobData` values (zombie, cow; `world/Mob`) live in their
  chunk's `mobs()` and are saved with it (`entities/` region files). `World` keeps a
  ticking list (`markTicking`, `forEachTickingChunk`) of chunks with furnaces or mobs.
  `gameplay/Mobs` ticks them (physics with step-up and floating, wander / panic /
  chase goals, melee, daylight burning, death + loot, despawning), moves mobs across
  chunk borders, spawns zombies in the dark and ray-casts player attacks. Cows come
  with new grassy chunks (`OverworldGenerator` step 10; mobs only, so block hashes
  are unchanged). `rendering/MobModels` holds the cuboid models (vanilla box-UV
  layout on our own 64×64 skins, `tools/textures/gen_entities.py`); `EntityRenderer`
  draws them with world light, limb swing, head look, death tilt and the hurt tint.
- Block updates and redstone (M11, `world/Redstone`): gameplay edits go through
  `World::updateBlock`, which tells the listener (`Redstone`); it notifies the six
  neighbours in vanilla order (W, E, down, up, N, S) and lets components reach
  further (dust and torches: neighbours' neighbours). Scheduled block ticks live in
  their chunk (`Chunk::blockTicks`, saved as `block_ticks`, one pending per block)
  and run each game tick by time, priority and scheduling order; piston moves are
  block events after them. Power: `weak`/`strong` per component, conductors pass
  strong power on, dust reads other dust only directly (`m_wiresMuted`). Main sets
  the time at the start of each tick, runs `tick()` after player actions and before
  mobs, and drains `changed()` (relight/re-mesh) and `drops()`. Models are in
  `rendering/BlockModelsRedstone.cpp` (rotated boxes; dust tinted by power through
  palette slots 238-253).
- Dimensions (M12, `world/Dimension.h`): one dimension is loaded at a time in the
  single `World`. Travelling (main.cpp) saves, unloads every chunk, swaps the
  generator (`OverworldGenerator`/`TerrainGenerator`, `NetherGenerator`,
  `EndGenerator` in `world/NetherGenerator.*`) and the `ChunkStorage` folder
  (vanilla `DIM-1/`, `DIM1/`), sets `World::setHasSkyLight` (the light engine then
  keeps sky light 0) and `WorldRenderer::setDimension` (fog colour/range, no
  sun/moon/stars, ambient light uniform 8, End bright lightmap uniform 9), then waits
  for the destination's chunks and finds or builds the way in. `gameplay/Portals`
  holds the rules: lighting frames, 8:1 coordinates, the known-portal list (saved in
  level.dat, our tag; vanilla uses poi/ files), portal building, end portal frames
  and the End platform. Portal blocks check their frame on block updates
  (`Redstone::neighbourChanged`), so portals are placed all at once, then updated.
- Screens (ui): `ContainerScreen` (survival inventory 2x2, crafting table 3x3,
  furnace) next to `CreativeInventory`; `EntityRenderer` (rendering) draws dropped
  items and the breaking crack from per-frame data main builds.
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
  2 mip levels (the 1.21.11 default). Sprites are addressed by grid index in vertices.
- `BlockModels`: per-state baked models (sprite, rotation, tint per face), built once
  at startup. Block → model mapping is C++ (`BlockModels.cpp`) until the JSON loader.
- `ChunkMesher` (GL-free, thread-safe): emits a face when the neighbour is not an
  `opaqueCube` (or the same block for `cullSame` models: water, glass); 4
  `PackedVertex` (12 bytes) per quad: position in 1/16 block, face, texel UV, sprite,
  tint, fluidTop, AO (0–3 occluders) and smooth sky/block light (sum of the 4
  non-opaque samples around the corner). Box models (torch) are flat-lit from their
  own cell. Quads flip their diagonal to follow the brighter corners. Directional
  shade, the vanilla brightness curve (`f/(4−3f)`, gamma lift) and night sky
  darkening are applied in the shader.
- `SkyRenderer` (shader `sky`): additive sun and moon (8 phases) quads and a fixed
  1500-star field, rotated by the celestial angle; drawn after the clear with depth
  off. Clear and fog colour = plains sky × daylight.
- `ChunkRenderer`: one vertex arena buffer sub-allocated in quads (`RangeAllocator`,
  grows by copying), one shared quad index buffer (baseVertex per section), per-frame
  CPU frustum culling, then one `glMultiDrawElementsIndirect`. Each draw's
  `sectionOrigin − cameraPos` (double → float) goes to an SSBO read with
  `gl_BaseInstance`. Command/offset arrays are reused (no per-frame allocation).
- `OverlayRenderer` (shader `overlay`): targeted-block outline (12 thin edge boxes,
  black 40%) and the crosshair (vanilla 15x15 GUI px, inverting blend, auto GUI
  scale = largest that fits 320x240). Targeting: `world::raycastBlocks` (voxel DDA,
  skips air and fluids, vanilla reach 4.5 survival / 5.0 creative).
- `WorldRenderer`: owns the above; `markChunkDirty` (chunk + 4 neighbours),
  `update()` re-meshes dirty sections, `drawFrame()` does all of a frame's GL work.
  `main.cpp` makes no GL calls (hard rule 7).

- GUI (M6): `GuiBatch` (GL-free) collects quads in GUI pixels (screen / GUI scale,
  vanilla auto scale): sprites, text (glyph advances from the font sheet, vanilla
  shadow), isometric block icons from the atlas; capped, reserved once.
  `GuiRenderer` uploads it into one dynamic buffer and draws once per frame, after
  the overlay. `ui/` builds it: `drawHotbar`, `DebugScreen` (F3), `Chat`,
  `CreativeInventory` — all GL-free and unit-tested.

Fixed bindings (add new ones here):
| Kind | Slot | Use |
|---|---|---|
| uniform location | 0 | `uViewProj` (camera at origin) |
| uniform location | 1 | `uAtlasColumns` |
| SSBO binding | 1 | tint palette (256 slots × grass/foliage/water vec4; 238-253 redstone dust power 0-15 (foliage channel), 254/255 birch/spruce foliage) |
| uniform location | 4 | `uFog` (start, end in blocks) |
| uniform location | 5 | `uFogColor` (sky) |
| uniform location | 6 | `uAlphaCutoff` (0.5 opaque/cutout pass, 0 translucent) |
| uniform location | 7 | `uSkyDarken` (sky light levels lost at night, 0..11) |
| texture unit | 0 | block atlas |
| SSBO binding | 0 | section offsets (block pass) |
| uniform location (overlay) | 0, 1 | `uTransform`, `uColor` (outline, crosshair) |
| uniform location (sky) | 0, 1, 2 | `uTransform`, `uColor`, `uUvRect` |
| uniform location (gui) | 0 | `uGuiSize` (framebuffer / GUI scale) |
| texture units (gui) | 0–5 | white, font, hotbar, selection, block atlas, HUD icons strip (= `GuiTexture`) |
| uniform location (entity) | 0, 1 | `uViewProj`, `uAlphaCutoff` (dropped items, crack overlay) |

Passes (M3.2): **opaque** (with alpha-test cutout for torches and glass), then **translucent** (`BakedModel::translucent`: water...)
with alpha blending, no depth writes, no back-face culling (water seen from below),
sections sorted far→near each frame. Fluids hide faces against the same fluid and
lower their top vertices by 1/9 (`fluidTop` vertex flag; vanilla source height 8/9).
Linear cylindrical distance fog to the sky colour from 92% of the render distance (`Fog.h`).

Known simplifications: uploads use `glNamedBufferSubData` (persistent-mapped staging
when uploads get heavy); translucent sorting is per section, not per quad; Meshing threads: see Threading.

## Files outside src/
- `assets/shaders/<name>.vert|.frag` — loaded at runtime from the source tree
  (`MC_ASSETS_DIR`), so shader edits need no rebuild, only a restart.
- `assets/minecraft/` — our own placeholder resource pack in vanilla's layout
  (`blockstates/`, `models/`, `textures/`); `assets/data/minecraft/` — data-pack JSON
  (recipes, loot tables, tags). Both described in data-formats.md.
- `third_party/glad` — generated GL loader (do not edit).
- `cmake/` — `Dependencies.cmake` (pinned FetchContent), `Subsystem.cmake`
  (`mc_add_subsystem`, warnings), `GenBuildInfo.cmake` + `BuildInfo.h.in` (version and
  `git describe`, regenerated each build into `<build>/generated/BuildInfo.h`);
  `src/MinecraftClone.rc` is the exe's Windows version resource.
- `tools/` — WSL scripts; `tools/win/*.cmd` load the MSVC environment.
