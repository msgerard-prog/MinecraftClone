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
  light changed are reported for re-meshing. Three queues: edits, streaming, then
  settling - fluid flow (`BlockUpdates::settling`) is re-meshed at once and relit
  only when nothing streams, so springs in new chunks don't starve their light.
  Scheduled block and fluid ticks run only within the simulation distance (vanilla
  ticking chunks), like random ticks.
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
  chunk is built in one flat array and each section encoded once. Version 2, kind
  "overworld2" (M18, default for new worlds), adds ravines (`ravine`/`inRavine`:
  pure per start chunk, carved by every chunk within 9), lava lakes, springs (a
  source plus a pending fluid tick, `Chunk::ticksRelative`) and more vegetation;
  "overworld" (version 1) stays for worlds created with it. `TerrainGenerator`
  (M3 placeholder, kind "terrain") stays too. Each pins a hash.
  `NetherGenerator` is versioned the same way: "nether2" (M19, new worlds; level.dat
  `nether_generator`, unknown kinds refused like the Overworld's) adds per-column
  Nether biomes (`biomeAt`), their surfaces and features (`netherFeatures`) and
  fortresses/bastions; "nether" (M12) stays for older worlds. `EndGenerator` too:
  "end2" (M20, level.dat `end_generator`) adds outer islands beyond 1024 blocks
  (`islandCells`/`islandValue`: centres on a 16-block grid, the strongest island per
  column), the four outer End biomes (`biomeAt`), small end islands, chorus trees
  (inside one chunk), the iron bar cages and an end crystal on each pillar (a
  `MobType::EndCrystal` entity in the pillar's chunk: no AI, any damage explodes it
  through `Mobs::die`; items place them with `Mobs::placeEndCrystal`); "end" (M12) is
  the main island only. End cities (M20.4, `placeEndCities`): `endCityAt` checks
  vanilla's grid on high highlands; each chunk builds the parts of the cities
  starting within 2 chunks west/2 north-south (tower, top room, bridge, ship) and
  fills their chests (loot or the elytra) after encoding.
- Biomes (`world/Biome`): 38 vanilla biomes with wiki colours (10 only from
  overworld2: `OverworldGenerator::biomeAt` refines `baseBiome`, the M8 choice); each chunk holds a
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
  chunk (saved as block_entities, ticked each game tick by main). Furnaces count
  recipe uses (vanilla RecipesUsed) by interned recipe id (`world/RecipeIds`).
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
- Block updates and redstone (M11, `world/BlockUpdates`): gameplay edits go through
  `World::updateBlock`, which tells the listener (`BlockUpdates`); it notifies the six
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
  (`BlockUpdates::neighbourChanged`), so portals are placed all at once, then updated.
- Fluids (M14, `world/Fluids.cpp`, part of `BlockUpdates`): water/lava blocks react to
  block updates by scheduling a fluid tick (water 5, lava 30 / Nether 10); the tick
  recomputes the level from the neighbours (new water sources between two), then
  flows down or sideways toward the nearest drop (slope search 4 / Overworld lava 2).
  Fluid ticks run after block ticks and save as `fluid_ticks`. `gameplay/FluidContact`
  tells entities which fluid they touch and the current; the mesher sets surface
  corners from vanilla's averaged heights. Buckets: `gameplay/Buckets`.
- Random ticks (M15, `world/RandomTicks.cpp`, part of `BlockUpdates`): after the
  scheduled ticks, 3 random positions per section in the chunks within the simulation
  distance (`setRandomTicks`); `Section::randomTickingCount` (per-state
  `BlockRegistry::randomTicks` flags) skips sections with nothing to tick. Grass,
  leaves (distance kept by 1-tick scheduled updates), saplings (grow the shapes in
  `world/TreeFeature.h`, shared with worldgen), snow/ice melting, lava starting fires.
  Pending ticks are found in O(1) through each chunk's `TickSet`.
- Weather (M22.1, `world/Weather`): `Weather` (rain/thunder states, countdowns, 0..1
  strengths easing 0.01 a tick) is one per world, owned by main, ticked after the day
  time and saved in level.dat. `precipitationAt` (biome temperature, height),
  `rainHeight` and `rainingAt` answer where rain/snow fall. `skyDarken(angle, rain,
  thunder)` dims the light (tick and renderer); the renderer greys the sky and hides
  the sun, moon and stars. `BlockUpdates::runWeatherTicks` (RandomTicks.cpp, ticking
  chunks): lightning in storms (`strikeLightning`: fire, `lightning()` for main), ice
  and snow forming; rain puts out fires (Fire.cpp) and waters farmland (Farming.cpp).
  Main applies bolts to mobs (`Mobs::strikeLightning`) and the player and draws rain/
  snow columns and bolts (`EntityRenderer::addPrecipitation/addLightning`, a blended
  pass after entities; textures `block/weather_*.png`).
- Fire (M15, `world/Fire.cpp`): scheduled every 30-40 ticks; ages, burns neighbours
  by their burn odds, spreads by ignite odds; flint and steel (`gameplay/Portals`)
  places it (or a portal inside a frame); `Vitals::touchFire`, mobs and items burn in
  it. Decaying leaves report `Drop::loot`, rolled by `blockDrops` in main.
- Enchanting (M17.5): `world/Enchantments` (registry: wiki weights, cost ranges,
  targets, groups; items carry up to 4 as id<<8|level, saved as
  minecraft:enchantments); effects in Mining (Efficiency, Silk Touch, Fortune,
  `wearItem` Unbreaking), Vitals (protection EPF), main (weapon enchantments),
  Projectiles (bow enchantments). `gameplay/Enchanting` (bookshelves, offers,
  picking from the player's XpSeed) and `gameplay/Anvil` (repair, combine, books,
  costs, prior work) back `ContainerScreen::Type::Enchanting/Anvil`; level spending
  and anvil wear happen in the tick (main) from the screen's requests.
- Effects and potions (M19.4, `world/Potions`): `Effect` and `Potion` tables (vanilla
  ids, durations, colours); `ItemStack::potion` is saved as `minecraft:potion_contents`.
  `Vitals` holds up to 16 active effects (`addEffect`, `tickEffects`, saved as
  active_effects); movement effects reach `Player::setEffects`, night vision
  `WorldRenderer::setNightVision`, strength/weakness the melee damage in main.
  Drinking is `BlockInteraction::tickDrinking` (both game modes). Brewing:
  `gameplay/Brewing` (`brewResult`, `tickBrewing`) over `world::BrewingData` block
  entities (`Chunk::brewingStands()`, ticked by main, `ContainerScreen::Type::Brewing`).
  Splash potions are `ProjectileKind::SplashPotion` (`throwSplashPotion`; the area
  effect on impact in `Projectiles::tick`).
- Experience (M17.5): levels in `Vitals` (vanilla's points per level), `gameplay/
  ExperienceOrbs` (pooled orbs drawn to the player), sources: player kills
  (`Mobs::Context::orbs`), breeding, ores (`blockExperience`), furnaces (stored per
  smelt, paid when the output is taken), death drops; the HUD bar; /xp.
- Beds (M17.4, `gameplay/Beds`): two-block red bed (the foot brings its head in
  `BlockUpdates::onBlockChanged`; halves break together), `useBed` rules (night,
  monsters, occupied, explodes outside the Overworld), `bedStandSpot`; main handles
  sleeping (100 ticks, then the next morning), the respawn point (level.dat
  `respawn`) and respawning there.
- Armor and shields (M17.3): armor items carry `armorSlot`/`armor`/`toughness`;
  `Inventory` holds 4 worn pieces and the offhand (saved as level.dat `equipment`);
  `Vitals::attacked` (mob hits, arrows, explosions, lava, fire blocks) applies the
  shield (front, raised 5 ticks) and the armor formula and collects wear that main
  applies to the items; the HUD shows the armor bar.
- Structures (M18.3): `world/StructurePlacement` (random-spread grids, mineshaft
  odds), `world/Loot` (chest loot tables from the wiki, rolled at generation),
  `SpawnerData` block entities (`Chunk::spawners()`, ticked by `Mobs::tickSpawners`
  within the simulation distance). Dungeons are an overworld2 feature; their chests
  and spawners become block entities after the chunk is encoded. Surface structures
  (`OverworldGenerator::placeStructures`, M18.4): each chunk checks the grid candidates
  within 2 chunks, tests the biome and builds the clipped parts through a rotating
  `StructureBuilder` (local coordinates, foundations, chests with a loot table).
  Mineshafts and strongholds (M18.5) are piece trees planned from their start
  (`planMineshaft`, `planStronghold`: boxes that must not overlap, within 80 / 112
  blocks) and built per chunk; stronghold positions (8 rings) are computed when the
  generator is made, and `ChunkGenerator::nearestStronghold` guides eyes of ender
  (`Projectiles`, kind EyeOfEnder).
- Chests (M17.2): `world::ChestData` block entities (27 slots) in `Chunk::chests()`,
  created/removed by `World::setBlock`, saved as block_entities `Items`; double
  chests are two chests whose `type` points at each other (`BlockUpdates::
  chestPartner`, kept paired by neighbour updates); `ContainerScreen::Type::Chest`
  shows 3 or 6 rows (main re-points it at the chests each frame).
- Farming (M17.1, `world/Farming.cpp`, part of `BlockUpdates`): farmland moisture and
  crop growth on random ticks (vanilla's speed points), `till` (hoes), `boneMeal`,
  `trample` (main rolls the fall chance); crops pop without farmland. Seeds,
  carrots and potatoes are items whose `block` is the crop.
- Nether structures (M19.3, `world/NetherStructures.cpp`, part of `NetherGenerator`):
  `complexAt` picks fortress/bastion per grid candidate; fortresses are cached piece
  trees built per chunk, bastions one keep; chests/blaze spawners become block
  entities after the chunk is written, bastion mobs are added to their start chunk.
- The ender dragon (M20.2, `gameplay/EnderDragon.cpp`, part of `Mobs`): `dragonAi`
  runs vanilla's DragonPhase numbers (holding ring, strafe with `ProjectileKind::
  DragonFireball`, landing on the exit portal column, perched flames, takeoff,
  charge), heals from the nearest crystal (`hasBeam`/`beam`, drawn by
  `EntityRenderer::addBeam`), breaks non-End blocks in its box and bites/knocks the
  player. `dragonDamage` applies the head rule in melee (main), arrows and
  explosions. Dying takes 200 ticks in `Mobs::tick`, then `dragonDeaths()` reports it;
  `bossHealth()` feeds `ui::drawBossBar`. Breath clouds are pooled in `Projectiles`.
- The dragon fight (M20.2, `gameplay/DragonFight`, end2 worlds): main ticks it in the
  End; every 5 s near the middle it looks for the dragon and spawns one over the
  (shut) exit portal when there is none; on `Mobs::dragonDeaths` it drops the
  experience, opens the exit portal and puts the egg on its column (first kill).
  Saved as level.dat DragonFight. The egg (`DragonFight::teleportEgg`) flees clicks.
  Gateways (M20.3): each kill builds the next ring gateway (`gatewayPos`,
  `buildGateway`); `gatewayTarget` finds the way out (the first outer island along
  its direction, `EndGenerator::outerTop`, the exit gateway built when that chunk
  loads) or back (the main island). Ender pearls (`ProjectileKind::EnderPearl`,
  `Projectiles::pearls`) and touching a gateway move the player (main). Four rim
  crystals start `respawnStep`: pillars rebuilt from the generator's pillar data,
  the portal shut, a new dragon.
- Elytra (M20.4c): main tells `Player::setCanGlide` whether a working elytra is worn;
  `Player::tick` starts gliding on a jump press in the air and runs the glide motion;
  `takeImpact` reports wall hits; main wears the elytra 1 a second.
- Collision shapes (M21.1, `world/BlockShapes`): per-state boxes in 1/16 (up to 1.5
  tall), built once; `gameplay/BlockCollision::gatherBlockBoxes` feeds them to the
  player, mobs and dropped items. Doors (two halves kept together in `BlockUpdates`),
  trapdoors, fence gates open by hand (wood) or redstone; pressure plates are pressed by
  main each tick (`BlockUpdates::pressPlate`/`settlePlates`) and spring up on their
  scheduled tick when nothing is left on them.
- Comparators and observers (M21.2): `ComparatorData` block entities keep the output
  strength (`weakAt`/`strongAt` read it); `comparatorTarget` (rear signal or container
  fullness via `containerSignal`, side inputs, compare/subtract), 2-tick updates, and
  `watchComparators` each tick for comparators reading containers. Observers are
  scheduled by `afterChange` when the block they face changes; their pulse is a full
  update, so observer chains work.
- Hoppers (M21.3, `gameplay/Hoppers`): `HopperData`/`DispenserData` block entities;
  `tickHoppers` (each tick, ticking chunks) pushes into and pulls from containers with
  vanilla's per-side slot rules (`insertOne`/`extractOne`, reused by droppers) and picks
  up dropped items; power disables them (`enabled`). Screens: `ContainerScreen::Type::
  Hopper/Dispenser` over the entity's slots (`openStore`).
- Dispensers and droppers (M21.3b): `BlockUpdates` fires them 4 ticks after a rising
  edge of power (`dispensed()`); main calls `gameplay/Dispensers::dispense`, which picks
  a random slot and drops/inserts (droppers) or uses the item (dispensers).
- Rails (M21.4, `world/Rails`): shapes 0..9 with their exits, `chooseRailShape` on
  placement and neighbour updates; powered/activator rails `railPowered` (8 along the
  line); detector rails reuse the pressure plate mechanism (pressed by minecarts).
- Minecarts (M21.4b, `gameplay/Minecarts.cpp`, part of `Mobs`): `MobType::Minecart`
  follows its rail's line (`world/Rails` exits), slopes, powered rails and drag;
  off rails it falls and slides. main keeps the ridden cart's UUID, moves the player
  with it, dismounts on shift or a powered activator rail; carts press detector rails
  and plates (`pressPlate(..., minecart)`).
- Pistons 2 (M21.5): `gatherPush` collects the moved blocks (slime blocks pull their
  neighbours, 12 at most); blocks leave at once, their targets hold `moving_piston`
  for 2 ticks (`BlockUpdates::moving()`, drawn sliding by main), then `finishMoves`
  lands them. Slimes share the magma cube's code; slime blocks bounce the player.
- TNT (M21.1b): `BlockUpdates::primeTnt` (redstone, fire, flint and steel) lists lit
  blocks; main turns them into `gameplay/PrimedTnt` entities (pooled), explodes them
  with power 4 (`ExplosionTargets::dropAll`), and explosions light TNT blocks and push
  primed TNT through `ExplosionTargets::tnt`.
- Shulkers (M20.4b, in `Mobs::ai`): fixed in place, `peek` opens the lid (a `Lift`
  model part), `ProjectileKind::ShulkerBullet` homes in and gives Levitation, which
  `Player::setEffects` turns into a rise.
- Nether mobs (M19.2, `gameplay/NetherMobs.cpp`, part of `Mobs`): `netherAi` (ghast,
  blaze, magma cube; zombified piglin anger), `spawnNether` (biome weights); flying
  mobs (`MobInfo::flies`) ease their velocity to a 3D wish in `physics`; fireballs are
  `ProjectileKind::GhastFireball/BlazeFireball` (main explodes ghast ones through its
  own `Explosion`); `MobInfo::modelScale` and the magma cube's `size` scale models.
- Monsters 2 (M16.5, `gameplay/Monsters.cpp`, part of `Mobs`): `mayTarget` (spiders
  in the dark, angry endermen), `monsterTick` (creeper fuse -> `Explosion`, skeleton
  bow into `Context::projectiles`, spider leap/climb, enderman stare, water,
  teleport, carried blocks). Spawn mix by vanilla's weights.
- Projectiles and explosions (M16.4): `gameplay/Projectiles` (arrows, eggs: pooled,
  swept against blocks with `raycastBlocks`, against mobs with `Mobs::raycast` and the
  player's box; stuck arrows; chicks from eggs), bows drawn in main (`bowPower`);
  `gameplay/Explosion` (vanilla's 1352 rays, block resistance, 1/power drops, exposure
  sampling, damage and push). Arrows are drawn as crossed quads from the mob strip's
  projectile row (`EntityRenderer::addArrow`).
- Farm animals (M16.3, `gameplay/Animals.cpp`, part of `Mobs`): sheep/pig/chicken
  next to cows; upkeep (growing up, love timers, eggs, grazing) and goals (partner,
  tempting food via `Context::heldItem`, parent); babies are queued in `m_births`;
  `Mobs::interact` feeds and shears. Sheep wool is a model layer (`MobPart::layer`,
  inflated, tinted by dye colour) drawn from an extra texture row (`kSheepWoolRow`).
- Pathfinding (M16.2, `gameplay/Pathfinder`): A* over standable cells (body fits,
  solid below or water), 4 directions, step up 1 / drop 3, lava and fire blocked,
  danger and water costs; preallocated nodes, heap and stamped hash; partial paths.
  `Mobs` keeps one and stores each mob's path (32 cells) in `MobData`.
- Falling blocks (M16): `BlockUpdates` schedules a 2-tick check when sand/gravel has
  a free block below and lists it in `fallingStarts()`; main hands it to
  `gameplay/FallingBlocks` (pooled entities: gravity, drag, landing through
  `World::updateBlock` or dropping as an item), drawn by `EntityRenderer::addBlock`.
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
  4 mip levels (the 1.21.11 Fancy default). Sprites are addressed by grid index in vertices.
- `BlockModels`: per-state baked models (sprite, rotation, tint per face), built once
  at startup. Block → model mapping is C++ (`BlockModels.cpp`) until the JSON loader.
- `ChunkMesher` (GL-free, thread-safe): emits a face when the neighbour is not an
  `opaqueCube` (or the same block for `cullSame` models: water, glass); 4
  `PackedVertex` (12 bytes) per quad: position in 1/16 block, face, texel UV, sprite,
  tint, AO (0–3 occluders) and smooth sky/block light (sum of the 4
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
sections sorted far→near each frame. Fluids hide faces against the same fluid; their
surface corners sit at vanilla's averaged heights (`fluidCornerHeight`: amount/9 per
cell, sources weigh 10x, open cells count 0, solid cells not at all; full under the
same fluid), rounded to 1/16 block.
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
