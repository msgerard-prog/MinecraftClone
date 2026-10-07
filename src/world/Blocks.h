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
// Redstone 2 (M21).
extern const Property open;      // true | false (doors, trapdoors, fence gates)
extern const Property doorHalf;  // "half": upper | lower
extern const Property hinge;     // left | right
extern const Property slabHalf;  // "half": top | bottom (trapdoors)
extern const Property inWall;    // "in_wall": true | false (fence gates)
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
    Count
};
} // namespace blocks

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

} // namespace mc::world
