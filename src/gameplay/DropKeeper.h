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
    // (M32.3) arrows and tridents, primed TNT and falling blocks go with their chunk too.
    void track(class Projectiles* projectiles, class PrimedTnt* tnt, class FallingBlocks* falling) {
        m_projectiles = projectiles;
        m_tnt = tnt;
        m_falling = falling;
    }
    void chunkLoaded(world::Chunk& c) override;
    void chunkUnloading(world::Chunk& c) override;
    void beforeSave(world::World& world);
    void afterSave(world::World& world);

private:
    ItemEntities& m_items;
    ExperienceOrbs& m_orbs;
    class Projectiles* m_projectiles = nullptr;
    class PrimedTnt* m_tnt = nullptr;
    class FallingBlocks* m_falling = nullptr;
    // Parks the tracked entities standing in `c` (all = any loaded chunk: saving); unparks.
    void parkEntities(world::World* world, world::Chunk* only);
    void unparkEntities(world::Chunk& c);
    world::Xoroshiro m_rng; // (its own stream: saving doesn't shift gameplay's)
    std::vector<world::ChunkPos> m_touched;
};

} // namespace mc
