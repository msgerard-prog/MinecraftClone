#pragma once

#include "world/BlockRegistry.h"
#include "world/Direction.h"
#include "world/World.h"

namespace mc::world {

// Rails (M21.4; wiki: Rail): shapes 0..9 in vanilla's "shape" order - north_south,
// east_west, ascending_east/west/north/south (rising toward that side), then the
// curves south_east, south_west, north_west, north_east (plain rails only).
bool isRail(BlockId b);
int railShapeOf(BlockStateId s);                       // -1: not a rail
BlockStateId withRailShape(BlockStateId s, int shape); // (curves only on plain rails)
struct RailExits {
    Direction a, b;
    bool aUp = false; // a's side is one block higher (ascending)
};
RailExits railExits(int shape);
// The shape a rail at `p` takes from the rails around it (wiki: Rail › Placement): it
// keeps both of its ends while they still lead to rails; otherwise it turns to the
// rails beside it - straight along a pair, rising toward one a block up, curving
// between a north/south and an east/west neighbour (plain rails), else unchanged.
int chooseRailShape(const World& world, const BlockPos& p, BlockStateId current);

} // namespace mc::world
