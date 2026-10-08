#pragma once

#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// Pillager patrols (M24.4; wiki: Patrol): from the fifth in-game day, every 10-11
// minutes a 1 in 5 roll sends 1-5 pillagers, led by a captain, onto open ground 24-48
// blocks from the player. (Not saved: the timer restarts with the world.)
struct PatrolSpawner {
    int delay = 12000;
    // One game tick; returns the number of pillagers spawned.
    int tick(world::World& world, const glm::dvec3& player, int64_t dayTime, world::Xoroshiro& rng);
};

} // namespace mc
