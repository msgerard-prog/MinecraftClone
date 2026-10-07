#pragma once

#include "world/Items.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace mc {

// Buckets (M14; wiki: Bucket): an empty bucket picks up a water or lava source (or
// milks a cow - the caller handles mobs); a full one places its source where the
// player looks. Water evaporates in an ultrawarm dimension (the Nether).
struct BucketResult {
    world::ItemId filled = 0; // the item the used bucket becomes
};

// `held` is the bucket item used. Returns what it turns into (survival: the caller
// swaps or splits the stack), or nullopt if nothing happened. `changed` gets edits.
std::optional<BucketResult> useBucket(world::World& world, world::ItemId held, const glm::dvec3& eye,
                                      const glm::dvec3& look, double reach, std::vector<world::BlockPos>& changed);

} // namespace mc
