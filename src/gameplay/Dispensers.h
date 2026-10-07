#pragma once

#include "gameplay/ItemEntities.h"
#include "gameplay/PrimedTnt.h"
#include "gameplay/Projectiles.h"
#include "world/BlockUpdates.h"
#include "world/Random.h"
#include "world/World.h"

#include <vector>

namespace mc {

// A dispenser or dropper firing (M21.3b; wiki: Dispenser, Dropper): a random non-empty
// slot gives up one item. A dropper puts it into the container in front, or drops it.
// A dispenser uses it: arrows, eggs, splash potions and fire charges fly; water and
// lava buckets pour (the bucket stays), empty buckets scoop a source; flint and steel
// lights fire or TNT; TNT is primed in front; bone meal fertilises; anything else drops.
struct DispenseContext {
    world::World& world;
    world::BlockUpdates& updates;
    ItemEntities& items;
    Projectiles& projectiles;
    PrimedTnt& tnt;
    world::Xoroshiro& rng;
    std::vector<world::BlockPos>& edits;
};
void dispense(DispenseContext& ctx, const world::BlockPos& p);

} // namespace mc
