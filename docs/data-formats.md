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
                           SpawnX/Y/Z still read), Difficulty (0-3, M28.1), DifficultyLocked, hardcore,
                           raining, rainTime, thundering, thunderTime, clearWeatherTime
                           (M22.1), WasModded 1, ServerBrands, GameRules { 1.21.11
                           ids "minecraft:keep_inventory"... : string values; the 20 rules of
                           world/GameRules are read back, M28.1 }, DataPacks
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
                           { levels: {...} } is read), "minecraft:repair_cost",
                           "minecraft:potion_contents" { potion } (M19.4) } } ],
                           active_effects [ { id, amplifier (byte), duration, ambient,
                           show_particles, show_icon } ] (M19.4),
                           equipment { head, chest, legs, feet, offhand: item } (1.21.5+;
                           older saves' Inventory slots 100..103 / -106 are read),
                           respawn { pos [I; x, y, z], dimension, yaw, pitch, forced }
                           (a bed's respawn point) },
                           DragonFight { DragonKilled, PreviouslyKilled, NeedsStateScanning 0
                           (bytes), Dragon [I; uuid] (while one lives), Gateways [I; ...] (the
                           ring gateways still to open, shuffled when the fight starts; written
                           once it has) } (M20, vanilla's tag; written for every world, used by
                           end2),
                           MinecraftClone { generator: "overworld2" (M18, new worlds) |
                           "overworld" (M8) | "terrain" (M3) | "flat",
                           portals [ { dimension, x, y, z } ] (known nether portals, M12),
                           nether_generator: "nether2" (M19, new worlds) | "nether" (M12),
                           end_generator: "end2" (M20, new worlds) | "end" (M12; also when missing),
                           format: Int (kCloneFormat the world was created with; missing = 0,
                           before v0.17.1) } }
                           (Player.Dimension: "minecraft:overworld" | "the_nether" | "the_end")
level.dat_old              backup copy of the previous level.dat (load falls back to it,
                           then to level.dat_new); level.dat_new is written first and
                           renamed over level.dat in one step
session.lock               held exclusively while the world is open (one instance)
                           (Items may carry minecraft:lodestone_tracker {target: {pos,
                           dimension}, tracked} - M28.2a; Player.LastDeathLocation
                           {dimension, pos} is kept too.)
data/map_<id>.dat          (M28.2b) gzip NBT { data { scale, dimension, trackingPosition,
                           unlimitedTracking, locked, xCenter, zCenter, banners [], frames [],
                           colors byte[16384] (base x 4 + shade) }, DataVersion }; written
                           when changed. data/idcounts.dat { data { map: last id } }. Filled
                           map items carry minecraft:map_id (and map_post_processing 0 lock /
                           1 scale until the next tick).
stats/<uuid>.json          (M28.1d) the player's statistics, vanilla's JSON: { "stats": {
                           "minecraft:custom": { "minecraft:play_time": ticks, ...distances
                           in cm, damage in tenths }, "minecraft:mined" (blocks),
                           "minecraft:crafted" | "used" | "broken" | "picked_up" | "dropped"
                           (items), "minecraft:killed" | "killed_by" (mobs) }, "DataVersion"
                           4671 }; zero counters and empty groups are left out
advancements/<uuid>.json   (M28.5c) the player's advancements, vanilla's JSON: {
                           "minecraft:story/mine_stone": { "criteria": { "requirement":
                           "2026-10-08 12:00:00 +0000" }, "done": true }, ..., "DataVersion":
                           4671 }; only made ones are written (one criterion each, ours)
DIM-1/region, DIM-1/entities   the Nether (M12), same layouts; DIM1/... the End
entities/r.<x>.<z>.mca     same region layout; per chunk { DataVersion, Position [I; x, z],
                           Entities [ mobs: { id, Pos, Motion, Rotation, Health,
                           OnGround, fall_distance (double, 1.21.5+; FallDistance still read),
                           Fire (-20 when not burning), Air, PortalCooldown, Invulnerable, AbsorptionAmount,
                           equipment (omitted when empty), HurtTime, DeathTime, PersistenceRequired, UUID
                           [I; 4 ints], cow/pig/chicken variant "minecraft:temperate", zombie IsBaby...;
                           animals Age, ForcedAge, InLove; sheep Color (byte), Sheared;
                           chicken EggLayTime, IsChickenJockey; creeper Fuse,
                           ExplosionRadius, ignited, powered (struck by lightning, M22.1); enderman carriedBlockState
                           {Name, Properties}; end_crystal ShowBottom (byte; M20, with the mob
                           fields vanilla ignores); ender_dragon DragonPhase (Int, vanilla's
                           phase numbers); shulker AttachFace (0), Peek, Color 16 } ] }
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
cooking_total_time (shorts, 1.21.4+ names; BurnTime/CookTime still read), RecipesUsed
{ recipe id: Int uses since the output was last taken } (vanilla; experience = uses x the
recipe's experience, paid when the output is taken or the furnace broken; ids we lack are
kept; v0.17.0's float `clone_experience` is read as uses of minecraft:stone, 0.1 each) }, chests: { id
"minecraft:chest", x, y, z, keepPacked, Items [ { Slot 0..26, id, count, components } ] } ]
(a double chest is two chests); brewing stands: { id "minecraft:brewing_stand", x, y, z, Items [ Slot
0-2 bottles, 3 ingredient, 4 fuel ], BrewTime (short), Fuel (byte) } (M19.4); hoppers: { id "minecraft:hopper", x, y, z, Items [ Slot 0-4 ],
TransferCooldown (Int) }, dispensers/droppers: { id "minecraft:dispenser" | "minecraft:dropper", x, y, z,
Items [ Slot 0-8 ] } (M21.3); comparators: { id "minecraft:comparator",
x, y, z, OutputSignal (Int 0-15) } (M21.2); spawners: { id "minecraft:mob_spawner", x, y, z, Delay (short),
SpawnData { entity { id } }, MinSpawnDelay 200, MaxSpawnDelay 800, SpawnCount 4,
MaxNearbyEntities 6, RequiredPlayerRange 16, SpawnRange 4 } (M18.3; the fixed values are
written for vanilla, ours are constant). Not written:
structure starts, POI; `InhabitedTime` is counted since M32.2 (ticks with the player within 8 chunks; regional difficulty). Zombies save `IsBaby`, `CanBreakDoors` and the `minecraft:spawn_reinforcements` (and a leader's `max_health`) attribute. Our root tag `clone_format`
(Int, kCloneFormat = 1) marks chunks we wrote since v0.17.1: in a world whose level.dat
`format` is 0, a chunk without it gets leaves stored as distance=7, persistent=false (placed
before v0.15.0, when placing didn't set persistent) loaded as persistent; generated and grown
leaves always have distance 1..6. Vanilla 1.21.11 is expected to open
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

## Sounds (M22.4)
`assets/minecraft/sounds/<folder>/<name>.wav`: 16-bit PCM, mono, 22,050 Hz (other
formats are refused with a warning). Folders follow vanilla's layout (`dig/stone1`,
`step/grass2`, `mob/cow/say1`, `random/explode1`, `ambient/weather/rain1`...); the
event -> files table is `world/Sounds.cpp`. Generated by `tools/sounds/gen_sounds.py`;
a resource pack may replace any file with a WAV of the same path.

## options.txt (M22.5)
Next to `saves/` (git-ignored), vanilla's `key:value` lines and encodings (wiki:
Options.txt): `fov` ((degrees - 70) / 40), `renderDistance`, `simulationDistance`,
`mouseSensitivity` (0..1, 0.5 = 100%), `guiScale` (0 = auto), `soundCategory_master`
(0..1), `renderClouds` ("true"/"false"), `enableVsync`. Unknown keys are ignored;
only interactive runs read or write it. `--render-distance` and `--no-vsync` override
it for one run without being saved.

## Block families (M23.1)
Slabs (`type` top|bottom|double), stairs (`facing`, `half` top|bottom, `shape`
straight|inner_left|inner_right|outer_left|outer_right) and walls (`up`, `north`/
`east`/`south`/`west` none|low|tall) use vanilla's property names and values, so saved
states read back by name. Vanilla's `waterlogged` property isn't modelled (states with
it load with it ignored). New materials: brick, nether_brick, clay_ball items.

## Campfires (M23.4c)
block_entities `minecraft:campfire`: `Items` (Slot 0-3), `CookingTimes` and
`CookingTotalTimes` (int arrays, 600).

## Workstations 1 (M23.5)
block_entities `minecraft:smoker` and `minecraft:blast_furnace` save like furnaces
(`FurnaceData::kind` 1/2 comes from the id); `minecraft:barrel` saves `Items` like a
chest (`ChestData::barrel`). Composters (`level` 0..8) and the four cauldron blocks
(`cauldron`, `water_cauldron` and `powder_snow_cauldron` with `level` 1..3,
`lava_cauldron`) are block states only, as in vanilla. Stonecutters (`facing`) and
grindstones (`face`, `facing`) have no block entity.

## Workstations 2 (M23.6)
- Items carry vanilla's `minecraft:container` component (`[{slot: int, item: {...}}]`,
  shulker boxes; at runtime `ItemStack::contents` indexes `world/ItemContainers`) and
  `minecraft:trim` (`{pattern: "minecraft:coast", material: "minecraft:iron"}`,
  `ItemStack::trim` = pattern << 8 | material, tables in `world/ArmorTrims.h`).
- level.dat `Player.EnderItems` (Slot 0..26) holds the ender chest; in `LevelData`
  these are inventory slots 200..226.
- block_entities `minecraft:shulker_box` save `Items` (`ChestData::shulker`).
- `nether_generator` "nether3" (new worlds): nether2 plus ancient debris.

## Villagers (M24.1)
Entities `minecraft:villager`: `VillagerData` {type, profession, level}, `Xp`,
`LastRestock`, `RestocksToday`, `Brain.memories` with `minecraft:home`, `job_site`,
`meeting_point` as {value: {pos: [I; x, y, z], dimension}}. New blocks: lectern
(`facing`, `has_book`), fletching table, bell (`facing`; ours stands on the floor).
Level.dat `generator` "overworld3" (new worlds). `Offers.Recipes`: {buy, buyB, sell
(items), uses, maxUses, rewardExp, xp, priceMultiplier, specialPrice, demand} (M24.2).
Villager food as `Inventory` (stacks of bread, carrots, potatoes, beetroots, wheat,
seeds); zombie villagers save `ConversionTime` (-1 when not curing) and the villager
fields; `minecraft:iron_golem`, block `carved_pumpkin` (`facing`) (M24.3).
M24.4: `minecraft:witch`, `minecraft:wandering_trader` (`DespawnDelay`, `Offers`),
`minecraft:pillager` (`PatrolLeader`, `Patrolling`, `CanJoinRaid`); level.dat
`WanderingTraderSpawnDelay`/`WanderingTraderSpawnChance`; items `crossbow`,
`ominous_bottle`; loot table `PillagerOutpost`.
M24.5: `minecraft:vindicator`, `evoker`, `vex`, `ravager`; raiders in a raid carry
`Wave` and `RaidId`; iron golems `PlayerCreated`; effects `bad_omen`, `raid_omen`,
`hero_of_the_village`; item `totem_of_undying`. level.dat `Raid` (our tag; vanilla keeps
raids in data/raids.dat): `Active`, `Id`, `NextAvailableID`, `Center` [x,y,z], `Wave`,
`NumGroups`, `BadOmenLevel`, `TicksActive`, `PreRaidTicks`, `TotalHealth`, and the
pending raid `OmenTicks`, `OmenLevel`, `OmenCenter`.

## Oceans (M25.1)
Blocks `kelp` (age 0..25), `kelp_plant`, `seagrass`, `tall_seagrass` (half) - always
waterlogged, no property; `sea_pickle` (pickles 1..4, waterlogged), `dried_kelp_block`,
`blue_ice`; for each of tube, brain, bubble, fire, horn, alive and `dead_`:
`<kind>_coral_block`, `<kind>_coral` and `<kind>_coral_fan` (waterlogged). Properties
`waterlogged` (true|false), `pickles`. `BlockRegistry::waterlogged(state)`: the block
holds a water source (fluids, swimming, light opacity 1, meshing, `leftAfterBreaking`).
Item `dried_kelp` (food); recipes dried kelp block <-> 9 dried kelp, kelp smelts to
dried kelp. Biomes `deep_lukewarm_ocean`, `deep_cold_ocean`, `deep_frozen_ocean`.
Level.dat `generator` "overworld4" (new worlds).
M25.2: entities `minecraft:cod`, `salmon`, `tropical_fish` (`Variant`: shape | pattern
<< 8 | base colour << 16 | pattern colour << 24), `pufferfish` (`PuffState`), `squid`,
`glow_squid`; fish save `FromBucket`, water mobs `Air`. Items `cod`, `cooked_cod`,
`salmon`, `cooked_salmon`, `tropical_fish`, `pufferfish`, `ink_sac`, `glow_ink_sac`,
`<fish>_bucket`, `fishing_rod`; enchantments `luck_of_the_sea`, `lure`; effect `hunger`.
M25.2b: entities and items `<wood>_boat` (oak, spruce, birch, jungle, acacia, dark_oak,
mangrove, cherry, pale_oak) and `bamboo_raft`.
M25.3: entity `minecraft:drowned` (a held trident as `equipment.mainhand`); items
`trident`, `nautilus_shell`, `heart_of_the_sea`; enchantments `loyalty`, `riptide`,
`impaling`, `channeling`.
M25.3b: entities `minecraft:dolphin`, `minecraft:turtle` (`HasEgg`, `home_pos`); block
`turtle_egg` (eggs 1..4, hatch 0..2); items `turtle_scute`, `turtle_helmet`; effect
`dolphins_grace`.
M25.4: loot tables ShipwreckSupply/Map/Treasure, UnderwaterRuinSmall/Big, BuriedTreasure
(authored from the wiki; missing items such as maps are skipped).
M25.5: entities `minecraft:guardian`, `minecraft:elder_guardian`; blocks `sponge`,
`wet_sponge`; effect `mining_fatigue`.
M26.1: entities `minecraft:wolf`, `cat` (`Owner` int-array UUID, `Sitting`, `CollarColor`,
`variant` "minecraft:pale"...), `ocelot` (`Trusting`), `parrot` (`Variant` 0-4, `Owner`,
`Sitting`); level.dat `Player.UUID`.
M26.2: entities `minecraft:horse` (`Variant` = colour | markings << 8), `donkey`, `mule`,
`llama` and `trader_llama` (`Variant` 0-3, `Strength`; trader llamas `DespawnDelay`),
`camel` (`LastPoseTick` < 0: sitting) with `Tame`, `Temper`, `Owner`, `ChestedHorse`,
`Items` (chest slots numbered from 2), `attributes` [{id: max_health / movement_speed /
jump_strength, base}] and 1.21.5+ `equipment` {saddle, body (horse armor or carpet)};
`minecraft:<wood>_chest_boat` / `bamboo_chest_raft` with `Items` (from 0).
M26.3a: entities `minecraft:rabbit` (`RabbitType` 0-5), `fox` (`Type` red/snow, `Sleeping`,
`Trusted` [UUID], equipment.mainhand), `polar_bear`, `panda` (`MainGene`, `HiddenGene`),
`goat` (`HasLeftHorn`, `HasRightHorn`, `IsScreamingGoat`), `armadillo` (`state`,
`scute_time`); wolves' equipment.body `wolf_armor` (its damage). Block
`sweet_berry_bush[age]`; items `sweet_berries`, `rabbit`, `cooked_rabbit`, `rabbit_hide`,
`rabbit_foot`, `goat_horn` (component `minecraft:instrument` "minecraft:<name>_goat_horn"),
`armadillo_scute`, `wolf_armor`. /summon shorthands `FoxType`, `MainGene`, `HiddenGene`.
M26.3b: entity `minecraft:bee` (`HasNectar`, `HasStung`, `AngerTime`, `hive_pos`); blocks
`bee_nest`/`beehive[facing,honey_level]` with block entity `minecraft:beehive` {bees:
[{entity_data {id, Health, Age, HasNectar, UUID}, ticks_in_hive, min_ticks_in_hive}]},
`honey_block`, `honeycomb_block`; item `honey_bottle`. Generator kind "overworld5" (new
worlds): bee nests, sweet berry patches.
M26.3c: entities `minecraft:frog` (`variant` minecraft:temperate|warm|cold), `tadpole`
(`Age`, `FromBucket`), `axolotl` (`Variant` 0-4, `FromBucket`); blocks `ochre_froglight`,
`verdant_froglight`, `pearlescent_froglight` [axis], `frogspawn`; items `axolotl_bucket`,
`tadpole_bucket`.
M26.4a: entities `minecraft:cave_spider`, `silverfish`, `wither_skeleton`, `phantom`;
blocks `cobweb`, `infested_stone|cobblestone|stone_bricks|mossy_stone_bricks|
cracked_stone_bricks|chiseled_stone_bricks`, `infested_deepslate[axis]`; item
`phantom_membrane`; effect `minecraft:wither`; level.dat Player `TimeSinceRest` (our tag).
M26.4b: entity `minecraft:wither`; blocks `skeleton_skull`, `wither_skeleton_skull`,
`zombie_head`, `creeper_head`, `piglin_head`, `dragon_head` [rotation 0-15] and their
`*_wall_skull`/`*_wall_head` [facing]; items: the six standing kinds (wall kinds drop
them). Block textures `clone_head_<kind>_{front,back,side,top}` are ours (not vanilla
names).
M26.4c: entity `minecraft:breeze`; items `breeze_rod`, `wind_charge`.
M26.5a: entities `minecraft:allay` (equipment.mainhand: its item, `Inventory`,
`DuplicationCooldown`), `nautilus` (Tame, Owner, equipment.saddle); item `amethyst_shard`;
effect `minecraft:breath_of_the_nautilus`.
M26.5b: entities `minecraft:happy_ghast` (equipment.body `<colour>_harness`), `copper_golem`
(`weather_state`, `Waxed` - our tag); blocks `dried_ghast[facing,hydration,waterlogged]`,
`[waxed_][exposed_|weathered_|oxidized_]copper_chest[facing,type]` (block entity
minecraft:chest); items `snowball`, `<colour>_harness`. Block textures
`clone_dried_ghast_{front,side,top}` are ours. Items `saddle`,
`leather/iron/golden/diamond_horse_armor`, the chest boats. /summon also takes our
shorthands `Saddle:1b`, `Armor:1-4`, `Decor:1-16`.
M27.1: blocks `sunflower`, `lilac`, `rose_bush`, `peony`, `tall_grass`, `large_fern`
[half upper|lower], `mud`, `packed_mud`, `muddy_mangrove_roots[axis]`, `moss_block`,
`moss_carpet`, `pale_moss_block`, `pale_moss_carpet`, `pale_hanging_moss[tip]` (new property
`tip` true|false); biomes `sunflower_plains`, `old_growth_birch_forest`,
`old_growth_pine_taiga`, `savanna_plateau`, `windswept_savanna`, `windswept_forest`,
`windswept_gravelly_hills`, `bamboo_jungle`, `mangrove_swamp`, `pale_garden`. Generator kind
"overworld6" (new worlds): those biomes, giant spruces, mangrove roots, two-block plants,
bamboo, pale moss.
M27.1c: entity `minecraft:creaking` (`home_pos`); blocks `creaking_heart[axis,
creaking_heart_state uprooted|dormant|awake, natural]`, `open_eyeblossom`,
`closed_eyeblossom`, `resin_clump[facing]` (ours: one face; vanilla's multiface
north/south/... booleans), `resin_block`, `resin_bricks` (+ slab, stairs, wall),
`chiseled_resin_bricks`; item `resin_brick`.
M27.2: blocks `cave_vines[age,berries]`, `cave_vines_plant[berries]`, `spore_blossom`,
`azalea`, `flowering_azalea`, `azalea_leaves`/`flowering_azalea_leaves[distance,persistent]`,
`rooted_dirt`, `hanging_roots`, `small_dripleaf[half,facing]`, `big_dripleaf[facing,tilt]`,
`big_dripleaf_stem[facing]`, `pointed_dripstone[thickness,vertical_direction,waterlogged]`,
`dripstone_block` (new properties `berries`, `tilt`, `thickness`, `vertical_direction`);
item `glow_berries`. Biomes `lush_caves`, `dripstone_caves` (overworld6, M27.2c).
M27.3: blocks `sculk`, `sculk_vein[facing]` (ours: one face), `sculk_catalyst[bloom]`,
`sculk_sensor[sculk_sensor_phase,power,waterlogged]`,
`sculk_shrieker[shrieking,can_summon,waterlogged]`, `reinforced_deepslate`; effect
`minecraft:darkness`; level.dat Player `warden_spawn_tracker` {warning_level,
ticks_since_last_warning, cooldown_ticks}. Entity `minecraft:warden` (anger and phase
not saved: a reloaded warden is calm). M27.3b: biome `deep_dark`; item `echo_shard`;
enchantment `minecraft:swift_sneak`; loot entries can carry a given enchantment.
M27.4a: blocks `amethyst_block`, `budding_amethyst`, `small_amethyst_bud`/`medium_`/`large_`,
`amethyst_cluster` [facing,waterlogged], `smooth_basalt`, `tinted_glass`. M27.4b: block
`crying_obsidian`; loot table ruined_portal. M27.4c: loot table woodland_mansion. M27.4d:
blocks `trial_spawner[trial_spawner_state,ominous]` (block entity minecraft:trial_spawner:
spawn_data like a mob spawner's, our tags `spawned`, `total`, `cooldown`),
`vault[facing,vault_state,ominous]`; item `trial_key`. M27.5: blocks `suspicious_sand`,
`suspicious_gravel` [dusted] with block entity minecraft:brushable_block {item | LootTable
"minecraft:archaeology/..."}, `decorated_pot[facing]`; items `brush`, 23
`<name>_pottery_sherd`. Loot tables are saved by vanilla's ids (`lootTableName`). M27.5c:
entity `minecraft:sniffer`; blocks `sniffer_egg[hatch]`, `torchflower_crop[age 0..1]`,
`torchflower`, `pitcher_crop[age 0..4]` (ours: no `half`), `pitcher_plant[half]`; items
`torchflower_seeds`, `pitcher_pod`.

## Signs (M23.3c)
block_entities `minecraft:sign` / `minecraft:hanging_sign`: `front_text` and
`back_text` { `messages`: 4 strings (plain text; older JSON-quoted strings are
unquoted on load), `color` (dye name), `has_glowing_text` }, `is_waxed`.

## Completeness (M29)
Every 1.21.11 block, item, entity, effect and enchantment id is registered
(`tests/data/ids_1_21_11.txt`, checked by tests/world_completeness.cpp).
- Renamed: `minecraft:chain` -> `minecraft:iron_chain` (1.21.9); saves and item ids naming
  `chain` still load (`BlockRegistry::findBlock`, `ItemRegistry::find` aliases). `cave_air`
  and `void_air` in palettes load as air.
- New properties: `age` 0..2 (cocoa), `disarmed`, `inverted`, `charges` 0..4,
  `slot_N_occupied`, `distance` 0..7 + `bottom` (scaffolding), `crafting`, `orientation`
  (crafter, jigsaw), `drag`, `side_chain`, `copper_golem_pose`, `conditional`, `mode`
  (structure block: save/load/corner/data; test block: start/log/fail/accept).
- New block entities: `minecraft:trapped_chest`, `minecraft:chiseled_bookshelf` (Items
  0-5, `last_interacted_slot`), `minecraft:shelf` (Items 0-2), `minecraft:crafter` (Items
  0-8; no `disabled_slots`), `minecraft:command_block` (`Command`, `auto`, `powered`,
  `SuccessCount`, `LastOutput`, `conditionMet`).
- Entities: command block minecarts save `Command`; `block_display` `block_state:{Name}`,
  `item_display`/`ominous_item_spawner` `item:{id,count}`, `text_display` `text` (plain);
  `spawner_minecart`, `giant`, `mannequin`, `marker`, `interaction`.
- level.dat Player `respawn.dimension` is now written as the respawn point's dimension
  (a respawn anchor's: `minecraft:the_nether`); older worlds read as the Overworld.
- Generator kinds: "overworld7" (M29.8, new worlds: flowers, melons, cocoa, lily pads,
  powder snow, deep-ocean magma with pending ticks for bubble columns, glow lichen, ore
  veins with raw blocks, fossils); "nether4" (piglin brutes in bastions). overworld6 and
  nether3 are frozen for their worlds.

## Dropped items and orbs (M30.4)
Saved in `entities/` with the chunk's mobs, as vanilla: `minecraft:item` (Pos, Motion, Item,
Age, PickupDelay, Health 5) and `minecraft:experience_orb` (Pos, Motion, Value, Count, Age,
Health 5). Unknown fields are ignored on loading.
M32.3 adds `minecraft:arrow`/`spectral_arrow`/`trident` (Pos, Motion, inGround, life, pickup,
crit, PierceLevel, damage, item, DealtDamage; our `clone_facing`, `clone_power`, `clone_punch`,
`clone_potion`, `clone_flame`, `clone_fromPlayer`, `clone_shooter`), `minecraft:tnt` (fuse,
explosion_power, block_state) and `minecraft:falling_block` (BlockState, Time, DropItem; our
`clone_startY`) - parked in `Chunk::parkedEntities` by gameplay/DropKeeper like the drops.
Monsters (M32.2) save `equipment` head..feet/mainhand with `drop_chances` (2.0: picked up),
`CanPickUpLoot`, zombies `IsBaby`, `CanBreakDoors` and the `spawn_reinforcements` attribute. Villagers save `Gossips` ({Type, Value, Target}: ours are all about the one player); creepers `ignited`.
