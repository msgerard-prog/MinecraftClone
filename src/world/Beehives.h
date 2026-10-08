#pragma once

#include "world/BlockEntity.h"
#include "world/Chunk.h"
#include "world/World.h"

namespace mc::world {

// Bee nests and beehives (M26.3b; wiki: Beehive, Bee). The bees inside a hive are kept
// in its block entity; these turn them back into mobs.

// A mob for a bee leaving its hive at `at` (its home is the hive at `hive`).
MobData beeFromHive(const HiveBee& b, const glm::dvec3& at, const BlockPos& hive);
// Every bee inside comes out (above the hive), angry or not; the hive is emptied.
void releaseBees(Chunk& chunk, const BlockPos& hive, BeehiveData& data, bool angry);
// Whether a lit campfire within 5 blocks below calms the hive's bees (wiki: Campfire ›
// Smoke): then harvesting doesn't anger them.
bool hiveSmoked(const World& world, const BlockPos& hive);

} // namespace mc::world
