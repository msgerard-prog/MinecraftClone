#pragma once

#include "gameplay/Aabb.h"
#include "world/World.h"

#include <vector>

namespace mc {

// Collects the collision boxes (world/BlockShapes) of the blocks overlapping `region`
// into `out` (cleared first) - from one layer lower too, since fences and fence gates
// reach 1.5 blocks up. `unloadedSolid`: chunks not loaded count as full cubes (the
// player never walks into terrain that isn't there yet).
void gatherBlockBoxes(const world::World& world, const Aabb& region, std::vector<Aabb>& out,
                      bool unloadedSolid = false);

} // namespace mc
