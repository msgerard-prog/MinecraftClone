#pragma once

#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// The wandering trader's arrival (M24.4; wiki: Wandering Trader › Spawning): every
// 24000 ticks a roll at `chance` percent (25, then 50 and 75 after misses; back to 25
// once one comes) puts a trader near the village bell within 48 blocks of the player,
// or else near the player. Saved in level.dat (WanderingTraderSpawnDelay/Chance).
struct WanderingTraderSpawner {
    int delay = 24000;
    int chance = 25;
    // One game tick; returns true when a trader arrived.
    bool tick(world::World& world, const glm::dvec3& player, world::Xoroshiro& rng);
};

} // namespace mc
