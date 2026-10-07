# Data formats

Formats, registries and on-disk layouts. **Changing a shipped format needs the user's
OK** (see CLAUDE.md). Mark sections *planned* until implemented.

## Identifiers
Vanilla-style namespaced ids: `minecraft:stone`, `minecraft:oak_log`. Our content uses
the `minecraft` namespace when it replicates a vanilla thing. Ids are lowercase
`[a-z0-9_./-]`.

## Block registry and block states (planned, M2)
- Blocks are registered in C++ (`world/Blocks.cpp`), like vanilla's `Blocks` class:
  id, properties (e.g. `axis`, `facing`, `waterlogged`), hardness, blast resistance,
  light emission, opacity, render type, sound group.
- Every combination of property values is a **block state**. All states get a dense
  global `uint16` id at startup (registration order, then property order) — the
  equivalent of vanilla's `Block.STATE_REGISTRY`. The ids are not saved to disk; saves
  store palettes of `name + properties` (see below).
- String form (used in tests, commands, saves): `minecraft:oak_log[axis=y]`.

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
