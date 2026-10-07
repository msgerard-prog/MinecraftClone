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

## Save format (M7, ADR 0007 — proposed, awaiting the user's OK)
Vanilla Java **Anvil** layout under `saves/<world>/` (git-ignored; `--world NAME`,
default "New World" for interactive runs; `--no-save`):
```
level.dat                  gzip NBT: Data { DataVersion 3955, version 19133, LevelName,
                           DayTime, Time, LastPlayed, GameType 1, SpawnX/Y/Z,
                           SpawnAngle, Version { Id, Name, Series, Snapshot },
                           WorldGenSettings { seed, generate_features, bonus_chest },
                           Player { Pos, Rotation, Dimension, abilities { flying,
                           mayfly, instabuild, invulnerable, mayBuild, flySpeed,
                           walkSpeed }, playerGameType, Health, foodLevel,
                           foodSaturationLevel, foodExhaustionLevel,
                           SelectedItemSlot, Inventory [ { Slot 0..35, id, count,
                           components { "minecraft:block_state", "minecraft:damage" } } ] },
                           MinecraftClone { generator: "overworld" | "terrain" | "flat" } }
level.dat_old              backup copy of the previous level.dat (load falls back to it,
                           then to level.dat_new); level.dat_new is written first and
                           renamed over level.dat in one step
session.lock               held exclusively while the world is open (one instance)
entities/r.<x>.<z>.mca     same region layout; per chunk { DataVersion, Position [I; x, z],
                           Entities [ mobs: { id, Pos, Motion, Rotation, Health,
                           OnGround, FallDistance, Fire, HurtTime, DeathTime,
                           PersistenceRequired, UUID [I; 4 ints] } ] } (M10, 1.17+ layout)
region/r.<x>.<z>.mca       32x32 chunks: 4 KiB location table + timestamps, payloads in
                           4 KiB sectors (BE length, type 2 = zlib, NBT)
```
Chunk NBT (Java 1.21): `DataVersion`, `xPos`, `zPos`, `yPos` -4, `Status`
`minecraft:full`, `isLightOn`, `sections` [24 × { `Y`, `block_states` { `palette` [
{ `Name`, `Properties` } ], `data` (longs; bits = max(4, ceil(log2 n)), 64/bits entries
per long, none if 1 entry) }, `biomes` { `palette` [biome ids], `data` (longs, ceil(log2 n) bits, 64 entries; none if 1) }, `SkyLight`, `BlockLight`
(2048-byte nibble arrays, omitted when all 0) }], `block_entities` [ furnaces: { id
"minecraft:furnace", x, y, z, keepPacked, Items [ { Slot 0 input / 1 fuel / 2 output,
id, count, components } ], BurnTime, CookTime, CookTimeTotal (shorts, 1.21.1 names) } ]. Not written yet: heightmaps,
dropped items, ticks, structures, POI; `InhabitedTime` is 0; level.dat
omits GameRules, DataPacks, difficulty and WorldGenSettings.dimensions, so vanilla
may not open these worlds. Region compression types 4 (LZ4), 127 and external
`.mcc` chunks are not readable (such chunks regenerate). NBT strings are written as
plain UTF-8 (vanilla: modified UTF-8; differs only for NUL and 4-byte characters).
Rules: like vanilla, every generated chunk is saved (so terrain never changes after
generation, whatever the generator does later); chunks save when they unload, every
6000 ticks of play (autosave), when the game pauses (Esc) and on exit — each only
if changed since its last save (`Chunk::dirty`). A world with region files but no
readable level.dat is refused (no silent re-creation with another seed). Saving runs on an IO thread from shared section
snapshots. On load, unknown block ids become air (logged); light is recomputed.
The seed, generator kind, time, player and hotbar come from level.dat.

## Screenshots
`--screenshot <path>` writes an 8-bit RGB PNG, top row first, of the final back
buffer at framebuffer size. `tools/screenshot.sh` puts them in `out/screenshots/`.
