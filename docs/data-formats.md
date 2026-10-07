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

## Save format (M7, ADR 0007 — accepted; Java Edition 1.21.11 since M13)
Vanilla Java **Anvil** layout under `saves/<world>/` (git-ignored; `--world NAME`,
default "New World" for interactive runs; `--no-save`):
```
level.dat                  gzip NBT: Data { DataVersion 4671, version 19133, LevelName,
                           DayTime, Time, LastPlayed, GameType, allowCommands, initialized,
                           spawn { dimension, pos [I; x,y,z], yaw, pitch } (1.21.9+; older
                           SpawnX/Y/Z still read), Difficulty 2, DifficultyLocked, hardcore,
                           weather fields 0, WasModded 1, ServerBrands, GameRules { 1.21.11
                           ids "minecraft:keep_inventory"... : string values }, DataPacks
                           { Enabled ["vanilla"], Disabled [] }, Version { Id 4671, Name
                           "1.21.11", Series, Snapshot }, WorldGenSettings { seed,
                           generate_features, bonus_chest, dimensions { overworld, the_nether,
                           the_end: vanilla noise generators } },
                           Player { DataVersion, Pos, Motion, Rotation, Dimension, OnGround,
                           fall_distance, Air, Fire, XpLevel, XpP (bar 0..1), XpTotal,
                           XpSeed (enchanting table seed), Score, abilities { flying,
                           mayfly, instabuild, invulnerable, mayBuild, flySpeed,
                           walkSpeed }, playerGameType, Health, foodLevel,
                           foodSaturationLevel, foodExhaustionLevel,
                           SelectedItemSlot, Inventory [ { Slot 0..35, id, count,
                           components { "minecraft:block_state", "minecraft:damage",
                           "minecraft:enchantments" | "minecraft:stored_enchantments"
                           { "minecraft:<id>": level } (1.21.5+ map; the older
                           { levels: {...} } is read), "minecraft:repair_cost" } } ],
                           equipment { head, chest, legs, feet, offhand: item } (1.21.5+;
                           older saves' Inventory slots 100..103 / -106 are read),
                           respawn { pos [I; x, y, z], dimension, yaw, pitch, forced }
                           (a bed's respawn point) },
                           MinecraftClone { generator: "overworld" | "terrain" | "flat",
                           portals [ { dimension, x, y, z } ] (known nether portals, M12) } }
                           (Player.Dimension: "minecraft:overworld" | "the_nether" | "the_end")
level.dat_old              backup copy of the previous level.dat (load falls back to it,
                           then to level.dat_new); level.dat_new is written first and
                           renamed over level.dat in one step
session.lock               held exclusively while the world is open (one instance)
DIM-1/region, DIM-1/entities   the Nether (M12), same layouts; DIM1/... the End
entities/r.<x>.<z>.mca     same region layout; per chunk { DataVersion, Position [I; x, z],
                           Entities [ mobs: { id, Pos, Motion, Rotation, Health,
                           OnGround, fall_distance (double, 1.21.5+; FallDistance still read),
                           Fire (-20 when not burning), Air, PortalCooldown, Invulnerable, AbsorptionAmount,
                           equipment (omitted when empty), HurtTime, DeathTime, PersistenceRequired, UUID
                           [I; 4 ints], cow/pig/chicken variant "minecraft:temperate", zombie IsBaby...;
                           animals Age, ForcedAge, InLove; sheep Color (byte), Sheared;
                           chicken EggLayTime, IsChickenJockey; creeper Fuse,
                           ExplosionRadius, ignited, powered; enderman carriedBlockState
                           {Name, Properties} } ] }
region/r.<x>.<z>.mca       32x32 chunks: 4 KiB location table + timestamps, payloads in
                           4 KiB sectors (BE length, type 2 = zlib, NBT)
```
Chunk NBT (Java 1.21): `DataVersion`, `xPos`, `zPos`, `yPos` (the lowest section:
-4 in the Overworld, 0 in the Nether and End; their chunks have 16 sections, Y 0..15), `Status`
`minecraft:full` (1.21.11's name; `status` from 26.4; not read back: every saved chunk is full), `Heightmaps` { MOTION_BLOCKING,
MOTION_BLOCKING_NO_LEAVES, OCEAN_FLOOR, WORLD_SURFACE: 256 x 9-bit heights above the bottom,
7 per long, 37 longs }, empty `PostProcessing`, `fluid_ticks`, `structures` { references, starts }, `isLightOn`, `sections` [one per section of the dimension (24 Overworld, 16 Nether/End) × { `Y`, `block_states` { `palette` [
{ `Name`, `Properties` } ], `data` (longs; bits = max(4, ceil(log2 n)), 64/bits entries
per long, none if 1 entry) }, `biomes` { `palette` [biome ids], `data` (longs, ceil(log2 n) bits, 64 entries; none if 1) }, `SkyLight`, `BlockLight`
(2048-byte nibble arrays, omitted when all 0) }], `block_ticks` [ { `i` block id, `p`
priority, `t` delay in ticks, `x`, `y`, `z` } ] (M11, in scheduling order), `fluid_ticks` (same
fields; `i` "minecraft:water"/"lava" for sources, "minecraft:flowing_water"/"flowing_lava" else), `block_entities` [ furnaces: { id
"minecraft:furnace", x, y, z, keepPacked, Items [ { Slot 0 input / 1 fuel / 2 output,
id, count, components } ], lit_time_remaining, lit_total_time, cooking_time_spent,
cooking_total_time (shorts, 1.21.4+ names; BurnTime/CookTime still read), clone_experience
(our float: experience stored by smelting; vanilla uses RecipesUsed) }, chests: { id
"minecraft:chest", x, y, z, keepPacked, Items [ { Slot 0..26, id, count, components } ] } ]
(a double chest is two chests). Not written:
dropped items, structure starts, POI; `InhabitedTime` is 0. Vanilla 1.21.11 is expected to open
these worlds (unverified - in-game check in ROADMAP); chunks we never saved are generated by vanilla's
own generator there. Region compression types 4 (LZ4), 127 and external
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
