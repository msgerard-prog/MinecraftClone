#pragma once

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/ItemEntities.h"
#include "world/Random.h"
#include "world/World.h"

#include <vector>

namespace mc {

// Dropped items and orbs go with their chunk (M30.4): while a chunk is loaded they live in
// the pools; unloading parks them in the chunk (saved with its entities), loading takes
// them back. Saving parks every drop into its chunk in one pass (beforeSave), the dirty
// chunks are written, then afterSave puts them back. A chunk whose last save held drops is
// saved again once they are gone, or they would come back on loading.
class DropKeeper final : public world::ChunkLifecycleListener {
public:
    DropKeeper(ItemEntities& items, ExperienceOrbs& orbs, uint64_t seed) : m_items(items), m_orbs(orbs), m_rng(seed) {
        m_touched.reserve(256);
    }
    void chunkLoaded(world::Chunk& c) override;
    void chunkUnloading(world::Chunk& c) override;
    void beforeSave(world::World& world);
    void afterSave(world::World& world);

private:
    ItemEntities& m_items;
    ExperienceOrbs& m_orbs;
    world::Xoroshiro m_rng; // (its own stream: saving doesn't shift gameplay's)
    std::vector<world::ChunkPos> m_touched;
};

} // namespace mc
