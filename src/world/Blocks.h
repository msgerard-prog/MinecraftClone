#pragma once

#include "world/BlockRegistry.h"

// The vanilla blocks implemented so far. Add new ones with the add-block skill.
namespace mc::world {

// Shared properties (wiki: Block states). Reuse these; don't redefine.
namespace properties {
extern const Property axis;  // x | y | z  (logs, pillars)
extern const Property snowy; // true | false (grass block, podzol, mycelium)
extern const Property level; // 0..15 (fluids: 0 = source, 1..7 flowing, 8..15 falling)
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
    Count
};
} // namespace blocks

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

} // namespace mc::world
