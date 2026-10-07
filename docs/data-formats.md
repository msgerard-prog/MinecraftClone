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
Support starts with `cube_all`, `cube_column`, `cross`; full element models later.

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
