#pragma once

#include "world/BlockRegistry.h"

// The vanilla blocks implemented so far. Add new ones with the add-block skill.
namespace mc::world {

// Shared properties (wiki: Block states). Reuse these; don't redefine.
namespace properties {
extern const Property axis;  // x | y | z  (logs, pillars)
extern const Property snowy; // true | false (grass block, podzol, mycelium)
extern const Property level; // 0..15 (fluids: 0 = source, 1..7 flowing, 8..15 falling)
extern const Property lit;        // true | false (redstone ore, furnace...)
extern const Property distance;   // 1..7 (leaves: steps to the nearest log)
extern const Property persistent; // true | false (leaves placed by a player)
extern const Property layers;     // 1..8 (snow)
extern const Property facing;     // north | south | west | east (horizontal facing)
// Redstone (M11, wiki: each block's "Block states").
extern const Property power;      // 0..15
extern const Property north, east, south, west; // redstone wire: up | side | none
extern const Property delay;      // 1..4 (repeater)
extern const Property locked;     // true | false
extern const Property powered;    // true | false
extern const Property face;       // floor | wall | ceiling (lever, buttons)
extern const Property facing6;    // "facing": down | up | north | south | west | east
extern const Property extended;   // true | false (pistons)
extern const Property shortArm;   // "short": true | false (piston head)
extern const Property pistonType; // "type": normal | sticky (piston head)
// Dimensions (M12).
extern const Property haxis; // "axis": x | z (nether portal)
extern const Property eye;   // true | false (end portal frame)
// Random ticks and fire (M15).
extern const Property stage; // 0..1 (saplings)
extern const Property age;   // 0..15 (fire)
extern const Property fireUp, fireNorth, fireEast, fireSouth, fireWest; // "up"...: true | false
extern const Property faceDown; // "down": true | false (with the fire ones: huge mushroom faces)
// Farming (M17.1).
extern const Property moisture; // 0..7 (farmland)
extern const Property age7;     // "age": 0..7 (wheat, carrots, potatoes)
extern const Property age3;     // "age": 0..3 (beetroots)
extern const Property age5;     // "age": 0..5 (chorus flowers)
extern const Property age25;    // "age": 0..25 (weeping and twisting vine tips)
extern const Property chestType; // "type": single | left | right
extern const Property bedPart;   // "part": head | foot
extern const Property occupied;  // true | false
extern const Property composterLevel; // "level": 0..8 (M23.5)
extern const Property cauldronLevel;  // "level": 1..3 (water and powder snow cauldrons)
extern const Property noteInstrument; // "instrument": harp | basedrum | ... (16, M23.6)
extern const Property note;           // 0..24
extern const Property hasRecord;      // "has_record": true | false (jukebox)
extern const Property hasBook;        // "has_book": true | false (lectern)
extern const Property waterlogged;    // true | false (M25.1: corals, sea pickles)
extern const Property pickles;        // 1..4 (sea pickles)
extern const Property eggs;           // 1..4 (turtle eggs)
extern const Property hatch;          // 0..2 (turtle eggs)
extern const Property hydration;      // "hydration": 0..3 (M26.5b: dried ghasts)
extern const Property honeyLevel;     // "honey_level": 0..5 (M26.3b: bee nests and beehives)
extern const Property mossTip;        // "tip": true | false (M27.1: pale hanging moss)
extern const Property creakingState;  // "creaking_heart_state": uprooted | dormant | awake (M27.1c)
extern const Property natural;        // true | false (M27.1c: generated creaking hearts)
extern const Property berries;        // true | false (M27.2: cave vines)
extern const Property tilt;           // none | unstable | partial | full (big dripleaf)
extern const Property thickness;      // tip_merge | tip | frustum | middle | base (pointed dripstone)
extern const Property verticalDirection; // "vertical_direction": up | down
extern const Property sculkPhase;     // "sculk_sensor_phase": inactive | active | cooldown (M27.3)
extern const Property bloom;          // true | false (sculk catalyst)
extern const Property shrieking;      // true | false (sculk shrieker)
extern const Property canSummon;      // "can_summon": true | false
extern const Property trialState;     // "trial_spawner_state": inactive | waiting_for_players | active |
                                      // waiting_for_reward_ejection | ejecting_reward | cooldown (M27.4d)
extern const Property vaultState;     // "vault_state": inactive | active | unlocking | ejecting
extern const Property ominous;        // true | false
extern const Property dusted;         // 0..3 (M27.5: suspicious sand and gravel being brushed)
extern const Property age1;           // "age": 0..1 (M27.5c: torchflower crops)
extern const Property age4;           // "age": 0..4 (pitcher crops)
extern const Property age2;           // "age": 0..2 (M29.4b: cocoa)
extern const Property disarmed;       // true | false (M29.5: tripwire cut with shears)
extern const Property inverted;       // true | false (M29.5: daylight detectors)
extern const Property charges;        // 0..4 (M29.5: respawn anchors)
extern const Property bookSlots[6];   // slot_0_occupied..slot_5_occupied: true | false (M29.5)
extern const Property scaffoldDistance; // "distance" 0..7 (M29.5: scaffolding)
extern const Property bottom;           // true | false (M29.5: scaffolding)
extern const Property crafting;         // true | false (M29.5: crafters)
extern const Property orientation;      // the crafter's front and top: north_up, down_east... (vanilla order)
extern const Property drag;             // true | false (M29.5: bubble columns pulling down)
extern const Property sideChain;        // unconnected | right | center | left (M29.6: powered shelves in a row)
extern const Property golemPose;        // standing | sitting | running | star (M29.6: copper golem statues)
extern const Property conditional;      // true | false (M29.7: command blocks)
extern const Property structureMode;    // save | load | corner | data (M29.7)
extern const Property testMode;         // start | log | fail | accept (M29.7)
extern const Property candles;        // 1..4 (M28.5a: candles)
extern const Property bites;          // 0..6 (cake)
extern const Property flowerAmount;   // "flower_amount": 1..4 (pink petals, wildflowers)
extern const Property segmentAmount;  // "segment_amount": 1..4 (leaf litter)
// Redstone 2 (M21).
extern const Property open;      // true | false (doors, trapdoors, fence gates)
extern const Property doorHalf;  // "half": upper | lower
extern const Property hinge;     // left | right
extern const Property slabHalf;  // "half": top | bottom (trapdoors, stairs)
extern const Property slabType;  // "type": top | bottom | double (slabs, M23.1)
extern const Property stairShape; // "shape": straight | inner_left | inner_right | outer_left | outer_right
extern const Property wallNorth, wallEast, wallSouth, wallWest; // none | low | tall
extern const Property hanging; // true | false (lanterns)
extern const Property bambooLeaves; // "leaves": none | small | large
extern const Property rotation16;   // "rotation": 0..15 (standing signs; 22.5 degrees each, 0 = facing south)
extern const Property attached;     // true | false (hanging signs on chains to one block)
extern const Property signalFire;   // "signal_fire": true | false (campfires over hay)
extern const Property inWall;    // "in_wall": true | false (fence gates)
extern const Property comparatorMode; // "mode": compare | subtract
extern const Property hopperFacing;   // "facing": down | north | south | west | east
extern const Property enabled;        // true | false (hoppers)
extern const Property triggered;      // true | false (dispensers, droppers)
// "shape": north_south | east_west | ascending_east | ascending_west | ascending_north |
// ascending_south (| south_east | south_west | north_west | north_east: plain rails)
extern const Property railShape, straightRailShape;
} // namespace properties

// Block ids in registration order; Blocks.cpp asserts this matches.
namespace blocks {
enum : BlockId {
    Air,
    Stone,
    GrassBlock,
    Dirt,
    Cobblestone,
    OakPlanks,
    Bedrock,
    Sand,
    OakLog,
    Water,
    Deepslate,
    Gravel,
    Glowstone,
    Torch,
    Glass,
    // World generation (M8). Appended: existing state ids never move.
    CoalOre,
    DeepslateCoalOre,
    IronOre,
    DeepslateIronOre,
    CopperOre,
    DeepslateCopperOre,
    GoldOre,
    DeepslateGoldOre,
    RedstoneOre,
    DeepslateRedstoneOre,
    LapisOre,
    DeepslateLapisOre,
    DiamondOre,
    DeepslateDiamondOre,
    EmeraldOre,
    DeepslateEmeraldOre,
    Granite,
    Diorite,
    Andesite,
    Tuff,
    Calcite,
    Sandstone,
    RedSand,
    RedSandstone,
    Terracotta,
    SnowBlock,
    Ice,
    PackedIce,
    Clay,
    CoarseDirt,
    MossyCobblestone,
    Lava,
    OakLeaves,
    BirchLog,
    BirchPlanks,
    BirchLeaves,
    SpruceLog,
    SprucePlanks,
    SpruceLeaves,
    AcaciaLog,
    AcaciaPlanks,
    AcaciaLeaves,
    ShortGrass,
    Fern,
    Dandelion,
    Poppy,
    Cornflower,
    AzureBluet,
    OxeyeDaisy,
    DeadBush,
    Snow, // snow layers (1..8)
    CraftingTable,
    Furnace, // facing, lit
    // Redstone (M11).
    RedstoneWire,      // north/east/south/west, power
    RedstoneTorch,     // lit
    RedstoneWallTorch, // facing, lit
    Repeater,          // facing (toward the input), delay, locked, powered
    Lever,             // face, facing, powered
    StoneButton,       // face, facing, powered
    OakButton,
    RedstoneBlock,
    RedstoneLamp, // lit
    Piston,       // facing (6), extended
    StickyPiston,
    PistonHead, // facing (6), short, type
    // Nether and End (M12).
    Netherrack,
    SoulSand,
    NetherQuartzOre,
    NetherGoldOre,
    MagmaBlock,
    Obsidian,
    NetherPortal, // axis (x | z)
    EndStone,
    EndPortalFrame, // eye, facing
    EndPortal,
    // Random ticks and fire (M15).
    OakSapling, // stage
    BirchSapling,
    SpruceSapling,
    AcaciaSapling,
    Fire, // age, east, north, south, up, west (faces it clings to)
    // Wool (M16.3), in vanilla's dye order.
    WhiteWool,
    OrangeWool,
    MagentaWool,
    LightBlueWool,
    YellowWool,
    LimeWool,
    PinkWool,
    GrayWool,
    LightGrayWool,
    CyanWool,
    PurpleWool,
    BlueWool,
    BrownWool,
    GreenWool,
    RedWool,
    BlackWool,
    // Farming (M17.1).
    Farmland, // moisture
    Wheat,    // age 0..7 (the crop; the wheat item is separate)
    Carrots,
    Potatoes,
    Beetroots, // age 0..3
    Chest,     // facing, type (single | left | right)
    RedBed,    // facing (foot -> head), occupied, part
    // Enchanting and anvils (M17.5).
    Bookshelf,
    EnchantingTable,
    Anvil, // facing
    ChippedAnvil,
    DamagedAnvil,
    IronBlock,
    SugarCane, // age 0..15
    // Overworld 2 vegetation (M18.1).
    Cactus, // age 0..15
    Pumpkin,
    BrownMushroom,
    RedMushroom,
    // M18.2 woods and biome blocks.
    JungleLog, // axis
    JunglePlanks,
    JungleLeaves, // distance, persistent
    JungleSapling, // stage
    DarkOakLog,
    DarkOakPlanks,
    DarkOakLeaves,
    DarkOakSapling,
    CherryLog,
    CherryPlanks,
    CherryLeaves,
    CherrySapling,
    Podzol,   // snowy
    Mycelium, // snowy
    BrownMushroomBlock, // down, east, north, south, up, west (cap faces)
    RedMushroomBlock,
    MushroomStem,
    // Structures (M18.3).
    Spawner,
    // Structure blocks (M18.4).
    ChiseledSandstone,
    CutSandstone,
    SmoothSandstone,
    OrangeTerracotta,
    BlueTerracotta,
    StoneBricks,
    MossyStoneBricks,
    CrackedStoneBricks,
    ChiseledStoneBricks,
    Tnt, // (explodes from M21; a plain block until then)
    DirtPath, // village roads (M18.5)
    // Nether 2 (M19.1).
    CrimsonNylium,
    WarpedNylium,
    CrimsonStem, // axis
    WarpedStem,
    CrimsonPlanks,
    WarpedPlanks,
    NetherWartBlock,
    WarpedWartBlock,
    Shroomlight,
    CrimsonFungus,
    WarpedFungus,
    CrimsonRoots,
    WarpedRoots,
    NetherSprouts,
    WeepingVines,       // the bottom tip
    WeepingVinesPlant,  // the rest of the strand
    TwistingVines,      // the top tip
    TwistingVinesPlant,
    SoulSoil,
    Basalt,    // axis
    Blackstone,
    BoneBlock, // axis
    // Fortresses and bastions (M19.3).
    NetherBricks,
    NetherBrickFence,
    NetherWart, // age 0..3
    PolishedBlackstoneBricks,
    CrackedPolishedBlackstoneBricks,
    ChiseledPolishedBlackstone,
    GildedBlackstone,
    GoldBlock,
    PolishedBasalt, // axis
    BrewingStand,   // (M19.4)
    // The End 2 (M20.1).
    EndStoneBricks,
    PurpurBlock,
    PurpurPillar, // axis
    ChorusPlant,  // down, east, north, south, up, west: connections (true | false)
    ChorusFlower, // age 0..5 (5: dead, grows no more)
    IronBars,     // east, north, south, west: connections
    DragonEgg,    // (M20.2)
    EndGateway,   // (M20.3)
    EndRod,       // facing (down | up | north | south | west | east) (M20.4)
    // Redstone 2 (M21.1).
    OakDoor,  // facing, half, hinge, open, powered
    IronDoor,
    OakTrapdoor, // facing, half (top | bottom), open, powered
    IronTrapdoor,
    OakFence,     // east, north, south, west: connections
    OakFenceGate, // facing, in_wall, open, powered
    OakPressurePlate,   // powered
    StonePressurePlate, // powered
    LightWeightedPressurePlate, // power (gold)
    HeavyWeightedPressurePlate, // power (iron)
    Comparator, // facing, mode (compare | subtract), powered (M21.2)
    Observer,   // facing (6: where its face looks), powered
    Hopper,     // facing (down | north | south | west | east), enabled (M21.3)
    Dispenser,  // facing (6), triggered
    Dropper,    // facing (6), triggered
    Rail,         // shape (10) (M21.4)
    PoweredRail,  // powered, shape (6)
    DetectorRail, // powered, shape (6)
    ActivatorRail, // powered, shape (6)
    SlimeBlock,    // (M21.5)
    MovingPiston,  // facing (6), type: a block being pushed (2 ticks; invisible)
    // Thin and small blocks (M23.2).
    WallTorch,     // facing (the way it points out from the wall)
    SoulTorch,
    SoulWallTorch, // facing
    Lantern,       // hanging
    SoulLantern,   // hanging
    Chain,         // axis
    Ladder,        // facing (out from the block it hangs on)
    GlassPane,     // east, north, south, west: connections
    // New woods (M23.3b).
    MangroveLog,       // axis
    MangrovePlanks,
    MangroveLeaves,    // distance, persistent
    MangrovePropagule, // stage
    MangroveRoots,
    PaleOakLog,        // axis
    PaleOakPlanks,
    PaleOakLeaves,     // distance, persistent
    PaleOakSapling,    // stage
    BambooBlock,       // axis
    StrippedBambooBlock, // axis
    BambooPlanks,
    BambooMosaic,
    Bamboo,            // leaves (none | small | large), stage (0 | 1: ready to grow)
    Campfire,          // facing, lit, signal_fire (M23.4c)
    SoulCampfire,
    HayBlock,          // axis (M23.4c; signal fires, softer landings)
    Smoker,            // facing, lit (M23.5: a furnace for food, twice as fast)
    BlastFurnace,      // facing, lit (ores and metal, twice as fast)
    Barrel,            // facing (6), open (27 slots like a chest)
    Composter,         // level 0..8 (8 = bone meal ready)
    Cauldron,          // empty
    WaterCauldron,     // level 1..3
    LavaCauldron,      // full
    PowderSnowCauldron, // level 1..3
    Stonecutter,       // facing
    Grindstone,        // face, facing (like a lever)
    EnderChest,        // facing (the player's own 27 slots)
    ShulkerBox,        // facing (6); the 16 dyed boxes are added after the enum, like it
    AncientDebris,
    NetheriteBlock,
    SmithingTable,
    Loom,              // facing
    CartographyTable,
    Beacon,
    Conduit,
    DiamondBlock,
    EmeraldBlock,
    LapisBlock,
    CoalBlock,
    SeaLantern,
    NoteBlock,         // instrument, note 0..24, powered
    Jukebox,           // has_record
    Lectern,           // facing, has_book (M24.1: the librarian's job site)
    FletchingTable,
    Bell,              // facing (M24.1: the village meeting point)
    CarvedPumpkin,     // facing (M24.3: shears on a pumpkin; tops iron golems)
    Kelp,              // age 0..25 (M25.1: the growing tip; always water)
    KelpPlant,         // the stem below a kelp tip (always water)
    Seagrass,          // always water
    TallSeagrass,      // half (always water)
    SeaPickle,         // pickles 1..4, waterlogged (glows only in water)
    DriedKelpBlock,
    BlueIce,
    TurtleEgg, // eggs 1..4, hatch 0..2 (M25.3b)
    Sponge,    // (M25.5) soaks up water around it, turning wet
    WetSponge, // dries in the Nether (or a furnace)
    SweetBerryBush, // age 0..3 (M26.3: berries at 2 and 3; foxes eat them)
    BeeNest,        // facing, honey_level 0..5 (M26.3b; a block entity holds the bees inside)
    Beehive,        // facing, honey_level 0..5
    HoneyBlock,     // slows and sticks (M26.3b)
    HoneycombBlock,
    OchreFroglight,       // axis (M26.3c: from magma cubes eaten by temperate frogs)
    VerdantFroglight,     // axis (cold frogs)
    PearlescentFroglight, // axis (warm frogs)
    Frogspawn,            // on water; hatches into tadpoles (M26.3c)
    Cobweb,               // (M26.4a) slows whatever is in it; swords and shears cut it
    InfestedStone,        // (M26.4a) a silverfish hides inside: it comes out when broken
    InfestedCobblestone,
    InfestedStoneBricks,
    InfestedMossyStoneBricks,
    InfestedCrackedStoneBricks,
    InfestedChiseledStoneBricks,
    InfestedDeepslate,    // axis
    // Mob heads (M26.4b): standing (rotation 0..15) and wall (facing) kinds.
    SkeletonSkull,
    SkeletonWallSkull,
    WitherSkeletonSkull,
    WitherSkeletonWallSkull,
    ZombieHead,
    ZombieWallHead,
    CreeperHead,
    CreeperWallHead,
    PiglinHead,
    PiglinWallHead,
    DragonHead,
    DragonWallHead,
    DriedGhast, // facing, hydration 0..3, waterlogged (M26.5b: soaked, it becomes a ghastling)
    // Two-block plants (M27.1; wiki: Sunflower, Lilac, Rose Bush, Peony, Tall Grass, Large
    // Fern): half (upper | lower), the two halves stay together.
    Sunflower,
    Lilac,
    RoseBush,
    Peony,
    TallGrass,
    LargeFern,
    // Swamp and garden ground (M27.1): mangrove swamps, pale gardens, lush caves (M27.2).
    Mud,
    PackedMud,
    MuddyMangroveRoots, // axis
    MossBlock,
    MossCarpet,
    PaleMossBlock,
    PaleMossCarpet,
    PaleHangingMoss, // tip (true: the lowest of a strand)
    // The pale garden's heart (M27.1c; wiki: Creaking Heart, Eyeblossom, Resin Clump).
    CreakingHeart,   // axis, creaking_heart_state (uprooted | dormant | awake), natural
    OpenEyeblossom,  // (open at night)
    ClosedEyeblossom,
    ResinClump,      // facing (the side it sits on - ours: one face, vanilla: any of six)
    ResinBlock,
    // Lush caves (M27.2; wiki: Cave Vines, Spore Blossom, Azalea, Rooted Dirt, Hanging
    // Roots, Small Dripleaf, Big Dripleaf).
    CaveVines,        // age 0..25, berries (the lowest piece)
    CaveVinesPlant,   // berries
    SporeBlossom,
    Azalea,
    FloweringAzalea,
    AzaleaLeaves,     // distance, persistent
    FloweringAzaleaLeaves,
    RootedDirt,
    HangingRoots,
    SmallDripleaf,    // half, facing
    BigDripleaf,      // facing, tilt (none | unstable | partial | full)
    BigDripleafStem,  // facing
    // Dripstone caves (M27.2b; wiki: Pointed Dripstone, Dripstone Block).
    PointedDripstone, // thickness, vertical_direction, waterlogged
    DripstoneBlock,
    // The deep dark (M27.3; wiki: Sculk, Sculk Vein, Sculk Catalyst, Sculk Sensor, Sculk
    // Shrieker, Reinforced Deepslate).
    Sculk,
    SculkVein,           // facing (the side it covers - ours: one face)
    SculkCatalyst,       // bloom
    SculkSensor,         // sculk_sensor_phase, power, waterlogged
    SculkShrieker,       // shrieking, can_summon, waterlogged
    ReinforcedDeepslate,
    // Geodes (M27.4a; wiki: Amethyst Geode, Budding Amethyst, Amethyst Bud, Amethyst Cluster).
    AmethystBlock,
    BuddingAmethyst,
    SmallAmethystBud,  // facing, waterlogged (the side it grows toward)
    MediumAmethystBud,
    LargeAmethystBud,
    AmethystCluster,
    SmoothBasalt,
    TintedGlass,
    CryingObsidian, // (M27.4b; wiki: Crying Obsidian - glows 10)
    // Trial chambers (M27.4d; wiki: Trial Spawner, Vault).
    TrialSpawner, // trial_spawner_state, ominous
    Vault,        // facing, vault_state, ominous
    // Archaeology (M27.5; wiki: Suspicious Sand, Suspicious Gravel, Decorated Pot).
    SuspiciousSand,   // dusted 0..3 (block entity: its loot)
    SuspiciousGravel,
    DecoratedPot,     // facing
    // Sniffer finds (M27.5c; wiki: Sniffer Egg, Torchflower, Pitcher Plant).
    SnifferEgg,       // hatch 0..2
    TorchflowerCrop,  // age 0..1 (then a torchflower)
    Torchflower,
    PitcherCrop,      // age 0..4 (then a pitcher plant; ours one block tall)
    PitcherPlant,     // half
    Lodestone,        // (M28.2a; wiki: Lodestone)
    HeavyCore,        // (M28.4d; wiki: Heavy Core) the mace's head, from ominous vaults
    // The remaining blocks (M28.5a; wiki: Cake, Candle, Candle Cake, Pink Petals, Wildflowers,
    // Leaf Litter, Firefly Bush, Bush, Short/Tall Dry Grass, Cactus Flower, Vines). Dyed
    // candles and candle cakes are added after the enum (like = Candle / CandleCake).
    Cake,         // bites 0..6
    CandleCake,   // lit
    Candle,       // candles 1..4, lit, waterlogged
    PinkPetals,   // flower_amount, facing
    Wildflowers,
    LeafLitter,   // segment_amount, facing
    FireflyBush,  // glows 2
    Bush,
    ShortDryGrass,
    TallDryGrass,
    CactusFlower,
    Vine,         // up, north, east, south, west
    FrostedIce,   // (M29.2b) age 0..3: Frost Walker's ice, melting back to water
    // M29.4a (wiki pages of each)
    Allium,
    BlueOrchid,
    RedTulip,
    OrangeTulip,
    WhiteTulip,
    PinkTulip,
    LilyOfTheValley,
    WitherRose,   // withers what walks into it; grows on netherrack and soul sand too
    LilyPad,      // on water: a thin pad to stand on
    JackOLantern, // a carved pumpkin with a light (facing)
    RawIronBlock,
    RawCopperBlock,
    RawGoldBlock,
    ChiseledNetherBricks,
    CrackedNetherBricks,
    // M29.4b (wiki: Melon, Melon Seeds, Pumpkin Seeds, Cocoa Beans)
    Melon,
    PumpkinStem,         // age 0..7, then it grows a pumpkin beside it
    MelonStem,
    AttachedPumpkinStem, // bent toward its fruit (facing)
    AttachedMelonStem,
    Cocoa,               // on a jungle log's side (facing: toward the log), age 0..2
    // (M29.4b; wiki: Bed) the other 15 colours, in dye order without red; like the red bed
    WhiteBed,
    OrangeBed,
    MagentaBed,
    LightBlueBed,
    YellowBed,
    LimeBed,
    PinkBed,
    GrayBed,
    LightGrayBed,
    CyanBed,
    PurpleBed,
    BlueBed,
    BrownBed,
    GreenBed,
    BlackBed,
    // (M29.4b; wiki: Flower Pot) the pot and a potted block per plant (kPottedPlants order),
    // all like the empty pot
    FlowerPot,
    PottedFirst,
    PottedLast = PottedFirst + 36,
    // (M29.4c; wiki pages of each)
    SoulFire,      // fire on soul sand or soil: blue, light 10, never spreads
    GlowLichen,    // facing: the side it covers (ours: one face, like sculk veins); light 7
    PowderSnow,    // sink in and freeze
    BambooSapling, // what a planted bamboo shoot is until it grows
    CoralWallFanFirst, // the 10 wall fans: kCoralKinds alive then dead each (facing: out of the wall)
    CoralWallFanLast = CoralWallFanFirst + 9,
    // M29.5 (wiki pages of each)
    Target, // power 0..15 while hit by a projectile
    Tripwire,     // string between two hooks (attached, disarmed, east..west, powered)
    TripwireHook, // on a wall, facing out (attached, powered)
    DaylightDetector, // inverted, power 0..15 from the sky
    TrappedChest,     // like the chest; powers while open
    LightningRod,     // then its 3 aged and 4 waxed kinds, like it (facing, powered, waterlogged)
    LightningRodLast = LightningRod + 7,
    CalibratedSculkSensor, // like the sculk sensor, hears twice as far (facing)
    RespawnAnchor,         // charges 0..4 of glowstone: a respawn point in the Nether
    ChiseledBookshelf,     // facing, slot_0..5_occupied: holds 6 books
    Scaffolding,           // bottom, distance 0..7 (from what holds it up), waterlogged
    Crafter,               // crafting, orientation (front_top), triggered: crafts its 3x3 on a pulse
    BubbleColumn,          // water over soul sand (up) or magma (drag: down)
    // M29.6 - the Copper Age (1.21.9): each metal piece in 4 stages, then waxed, like the iron one
    CopperBars,
    CopperBarsLast = CopperBars + 7,
    CopperChain,
    CopperChainLast = CopperChain + 7,
    CopperLantern,
    CopperLanternLast = CopperLantern + 7,
    CopperTorch,     // like the torch (light 14), green flame
    CopperWallTorch, // like the wall torch
    Shelf,           // oak, then the other 11 woods (kShelfWoods) like it: facing, powered, side_chain
    ShelfLast = Shelf + 11,
    CopperGolemStatue, // 4 stages, then waxed (copper_golem_pose, facing, waterlogged)
    CopperGolemStatueLast = CopperGolemStatue + 7,
    // M29.7 - technical blocks (wiki pages of each)
    CommandBlock,          // impulse (conditional, facing)
    ChainCommandBlock,     // runs after the one pointing into it
    RepeatingCommandBlock, // runs every tick while active
    StructureBlock,        // mode (save, load, corner, data) - ours keeps only its mode
    StructureVoid,         // an invisible, see-through marker
    Jigsaw,                // orientation - for structure pieces (no function of ours)
    Barrier,               // invisible and solid
    Light,                 // invisible light, level 0..15
    PlayerHead,            // as the mob heads (rotation)
    PlayerWallHead,        // (facing)
    PetrifiedOakSlab,      // an oak slab that is stone to tools
    TestBlock,             // mode (start, log, fail, accept) - 1.21.5 game tests
    TestInstanceBlock,
    // M33.1 - 26.1 "Tiny Takeover" (wiki: Golden Dandelion)
    GoldenDandelion,
    // M33.3a - 26.3 "Wilderness Bound" (wiki: Poplar): the poplar's own blocks (its wood set
    // follows the other woods' lists); leaves in three autumn colours.
    PoplarLog,
    PoplarPlanks,
    RedPoplarLeaves,
    OrangePoplarLeaves,
    YellowPoplarLeaves,
    PoplarSapling,
    Count
};
} // namespace blocks
inline bool isSuspicious(BlockId b) { return b == blocks::SuspiciousSand || b == blocks::SuspiciousGravel; }
// Amethyst buds and clusters, smallest to grown (M27.4a), in enum order.
inline bool isAmethystBud(BlockId b) { return b >= blocks::SmallAmethystBud && b <= blocks::AmethystCluster; }
// Two-block plants (M27.1), in enum order.
inline bool isTallPlant(BlockId b) { // (M27.5c: and the pitcher plant)
    return (b >= blocks::Sunflower && b <= blocks::LargeFern) || b == blocks::PitcherPlant;
}
// Two-block plants with halves kept together: those and the small dripleaf (M27.2).
inline bool isTwoBlockPlant(BlockId b) { return isTallPlant(b) || b == blocks::SmallDripleaf; }
// Mob heads (M26.4b): the standing kinds sit at even ids, each wall kind right after.
// (M29.7: player heads too, registered later)
inline bool isMobHead(BlockId b) {
    return (b >= blocks::SkeletonSkull && b <= blocks::DragonWallHead) || b == blocks::PlayerHead || b == blocks::PlayerWallHead;
}
inline bool isWallHead(BlockId b) {
    return b == blocks::PlayerWallHead || (isMobHead(b) && b != blocks::PlayerHead && (b - blocks::SkeletonSkull) % 2 == 1);
}

// Dye colours in vanilla's order (wiki: Dye › Data values): wool, carpets, stained
// glass, dyes...
inline constexpr const char* kDyeColours[16] = {"white", "orange", "magenta", "light_blue", "yellow", "lime",
                                                "pink",  "gray",   "light_gray", "cyan",     "purple", "blue",
                                                "brown", "green",  "red",        "black"};

// Coral kinds (M25.1; wiki: Coral): blocks, plants and fans of each, alive and dead.
inline constexpr const char* kCoralKinds[5] = {"tube", "brain", "bubble", "fire", "horn"};

// What goes in a flower pot (M29.4b; wiki: Flower Pot), in the potted blocks' order: the
// block is "potted_<name>" (the azaleas are "potted_<name>_bush").
inline constexpr const char* kPottedPlants[37] = {
    "torchflower",    "oak_sapling",     "spruce_sapling",  "birch_sapling",      "jungle_sapling",
    "acacia_sapling", "cherry_sapling",  "dark_oak_sapling", "pale_oak_sapling",  "mangrove_propagule",
    "fern",           "dandelion",       "poppy",           "blue_orchid",        "allium",
    "azure_bluet",    "red_tulip",       "orange_tulip",    "white_tulip",        "pink_tulip",
    "oxeye_daisy",    "cornflower",      "lily_of_the_valley", "wither_rose",     "red_mushroom",
    "brown_mushroom", "dead_bush",       "cactus",          "bamboo",             "crimson_fungus",
    "warped_fungus",  "crimson_roots",   "warped_roots",    "azalea",             "flowering_azalea",
    "open_eyeblossom", "closed_eyeblossom"};
static_assert(blocks::PottedLast - blocks::PottedFirst + 1 == 37);
// Fire and soul fire (M29.4c): both burn what is in them (soul fire 2 a hit, fire 1).
inline bool isFire(BlockId b) { return b == blocks::Fire || b == blocks::SoulFire; }
inline bool isPotted(BlockId b) { return b >= blocks::PottedFirst && b <= blocks::PottedLast; }
// The plant in a potted block / the potted block for a plant (none: 0).
BlockId plantInPot(BlockId potted);
BlockId pottedFor(BlockId plant);

// The woods of shelves (M29.6), in their blocks' order.
inline constexpr const char* kShelfWoods[12] = {"oak",     "spruce",   "birch",  "jungle", "acacia",  "dark_oak",
                                                "mangrove", "cherry",  "pale_oak", "bamboo", "crimson", "warped"};

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

// What stays when a block is broken or blown up: its water if it was waterlogged
// (M25.1, wiki: Waterlogging), else air.
inline BlockStateId leftAfterBreaking(BlockStateId s) {
    // (M29.2b; wiki: Ice) broken ice and frosted ice turn back into water.
    const BlockId b = blockRegistry().blockOf(s);
    return blockRegistry().waterlogged(s) || b == blocks::Ice || b == blocks::FrostedIce
               ? blockRegistry().defaultState(blocks::Water)
               : BlockStateId{0};
}

} // namespace mc::world
