#pragma once

#include "gameplay/Aabb.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// Which fluids a box is in, and the current there (M14; wiki: Water, Lava, Fluid).
struct FluidContact {
    bool water = false;
    bool lava = false;
    glm::dvec3 flow{0.0}; // unit direction of the water current (0 in still water)
    double height = 0.0;  // how far the fluid reaches above the box's bottom
};

// A fluid cell counts when the box reaches below its surface (amount / 9 high).
FluidContact fluidContact(const world::World& world, const Aabb& box);

// True if the point is under a fluid surface of `block` (eyes in water...).
bool pointInFluid(const world::World& world, const glm::dvec3& p, world::BlockId block);

} // namespace mc
