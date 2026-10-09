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
    bool soulFire = false; // (M29.4c) the fire is soul fire: 2 a hit
    bool fire = false;    // touching a fire block (same scan: one lookup per cell)
    int bubble = 0;       // (M29.5) in a bubble column: 1 up (soul sand), -1 down (magma)
    bool bubbleSurface = false; // its top cell, with air above, is the one touched
};

// A bubble column's push (M29.5; wiki: Bubble Column): up 0.06 a tick to 0.7 inside it,
// 0.1 to 1.8 at its surface; down 0.03 a tick to 0.3 inside, 0.9 at the surface.
void applyBubbleColumn(const FluidContact& c, glm::dvec3& velocity);

// A fluid cell counts when the box reaches below its surface (amount / 9 high).
FluidContact fluidContact(const world::World& world, const Aabb& box);

// True if the point is under a fluid surface of `block` (eyes in water...).
bool pointInFluid(const world::World& world, const glm::dvec3& p, world::BlockId block);

} // namespace mc
