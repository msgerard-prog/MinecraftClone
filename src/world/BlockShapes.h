#pragma once

#include "world/BlockRegistry.h"

#include <array>
#include <cstdint>

namespace mc::world {

// Collision shapes (M21.1; wiki: each block's "collision box"): up to 5 boxes per
// state in 1/16 block, `to` may reach 24 (fences and gates are 1.5 tall). States that
// collide and aren't listed are full cubes; `count` 0 means no collision.
struct ShapeBox {
    uint8_t from[3], to[3];
};
struct BlockShape {
    uint8_t count = 0;
    std::array<ShapeBox, 5> boxes{};
};
const BlockShape& collisionShape(BlockStateId state);
// The stair step boxes for a state (M23.1; also its model): the half slab, then the
// raised part on its facing side (a quarter for outer corners, three for inner).
BlockShape stairShapeOf(BlockStateId state);

} // namespace mc::world
