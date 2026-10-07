#pragma once

#include "world/BlockRegistry.h"

// The vanilla blocks implemented so far. Add new ones with the add-block skill.
namespace mc::world {

// Shared properties (wiki: Block states). Reuse these; don't redefine.
namespace properties {
extern const Property axis;  // x | y | z  (logs, pillars)
extern const Property snowy; // true | false (grass block, podzol, mycelium)
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
    Count
};
} // namespace blocks

// The global registry, built on first use (thread-safe) and immutable afterwards.
const BlockRegistry& blockRegistry();

} // namespace mc::world
