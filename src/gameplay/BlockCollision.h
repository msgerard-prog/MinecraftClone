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

// Suffocation (M30.3; wiki: Suffocation, vanilla isInWall): whether a flat square
// `width` x 0.8 wide at the eye's height lies in an opaque full block (stone, dirt...;
// not glass, leaves or other see-through blocks).
bool headInWall(const world::World& world, const glm::dvec3& eye, double width);

} // namespace mc
