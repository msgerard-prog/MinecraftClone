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
    Count
};
} // namespace blocks

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

} // namespace mc::world
