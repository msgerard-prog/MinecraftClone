# Data formats

Formats, registries and on-disk layouts. **Changing a shipped format needs the user's
OK** (see CLAUDE.md). Mark sections *planned* until implemented.

## Identifiers
Vanilla-style namespaced ids: `minecraft:stone`, `minecraft:oak_log`. Our content uses
the `minecraft` namespace when it replicates a vanilla thing. Ids are lowercase
`[a-z0-9_./-]`.

## Block registry and block states (M2.1, ADR 0005)
- Blocks are registered in C++ (`world/Blocks.cpp`, `blocks::` enum in `Blocks.h`):
  id, properties, hardness, blast resistance, light emission, `opaqueCube`, render
  layer. Sound group etc. come with the systems that use them.
- Every combination of property values is a **block state**. All states get a dense
  global `uint16` id at startup: each block owns a contiguous range, properties sorted
  by name, last property varying fastest. Air is state 0. The ids are runtime-only;
  saves store palettes of `name + properties` (see below).
- Property values keep vanilla's order (booleans `true, false`; axis `x, y, z`).
- String form (used in tests, commands, saves): `minecraft:oak_log[axis=y]`, properties
  sorted by name. `parse` fills missing properties from the default state and accepts
  ids without `minecraft:`.

## Resource formats (planned, M1–M2)
We read the **vanilla resource-pack layout** so that the user's own resource pack can be
dropped into `resourcepacks/` for side-by-side comparison. Our own placeholder pack in
`assets/` uses the same layout and only our own files:
```
assets/minecraft/blockstates/<block>.json   variants / multipart → model
assets/minecraft/models/block/<model>.json  parent, textures, elements (cuboids)
assets/minecraft/textures/block/<name>.png  16×16 (animated: N×16 strip + .mcmeta)
```
**Resource packs (M3.0):** `rendering/ResourcePack` reads a pack from a folder, a
`.zip`, or a client `.jar` (zip; stored + deflate entries, no zip64). `PackStack`:
our `assets/` at the bottom, then every pack in `resourcepacks/` (or
`--resourcepacks DIR`) sorted by name, later overriding earlier file by file. The
block atlas takes every `textures/block/*.png` in the stack: cell size = largest
sprite (smaller ones scaled up nearest-neighbour), strips with an `.mcmeta`
`animation` section animate with their `frametime` (custom `frames` lists and
`interpolate` not yet supported). Test fixtures: `tools/make_test_pack.py` →
`tests/data/testpack{,.zip}` (our own content).

**Status:** textures are loaded from this layout. Blockstate/model JSON is not parsed
yet (needs a JSON library — dependency change, ask the user); until then the block →
model mapping is C++ in `rendering/BlockModels.cpp`, mirroring `cube_all`,
`cube_column` (+ horizontal-log face rotations) and `grass_block`, and the random
variants of stone/bedrock (mirrored × y0/y180) and dirt/sand/grass (y0/90/180/270).
Not modelled yet: `grass_block_snow` (snowy=true). Variant choice per position uses
our own hash (see game-design.md › Known deviations).

## Data packs (planned, M9)
Recipes, loot tables, tags follow vanilla's data-pack JSON:
```
assets/data/minecraft/recipe/<name>.json        crafting_shaped / shapeless / smelting
assets/data/minecraft/loot_table/blocks/<b>.json
assets/data/minecraft/tags/block/<tag>.json      e.g. mineable/pickaxe
```

## Save format (planned, M7 — decided by ADR at the time)
Candidate: vanilla's **Anvil** layout — `region/r.<x>.<z>.mca` (32×32 chunks, 4 KiB
sector table, zlib-compressed NBT per chunk) with paletted sections. Choosing it
means learning NBT and lets us open worlds in external viewers.

## Screenshots
`--screenshot <path>` writes an 8-bit RGB PNG, top row first, of the final back
buffer at framebuffer size. `tools/screenshot.sh` puts them in `out/screenshots/`.
