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
    Count
};
} // namespace blocks

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

} // namespace mc::world
