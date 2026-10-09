#pragma once

#include "gameplay/Aabb.h"
#include "gameplay/ItemEntities.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Falling blocks (M16.1; wiki: Falling Block): sand and gravel that lost their support
// become an entity (box 0.98 x 0.98) falling with gravity 0.04 and drag 0.98 a tick.
// Landing in a replaceable block with something solid below puts the block back;
// landing anywhere else (a torch, a flower, a slab...) drops it as an item. One that
// falls for 600 ticks (or out of the world) is dropped / removed. Pooled (hard rule 1).
struct FallingBlock {
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0}; // pos: bottom centre
    world::BlockStateId state = 0;
    int time = 0;
    uint8_t skyLight = 15, blockLight = 0; // for rendering
    double startY = 0.0;                   // (M27.2: a stalactite hurts by how far it fell)
};

class FallingBlocks {
public:
    static constexpr int kMax = 1024;
    static constexpr double kSize = 0.98;

    FallingBlocks() {
        m_blocks.reserve(kMax);
        m_impacts.reserve(64);
    }

    // The block that was at `p` starts falling (it has already been removed).
    void spawn(const world::BlockPos& p, world::BlockStateId state);
    // One tick: fall, land (placing the block through World::updateBlock, so its
    // neighbours react; position in `changed`) or drop as an item into `items`.
    void tick(world::World& world, ItemEntities& items, world::Xoroshiro& rng,
              std::vector<world::BlockPos>& changed);

    const std::vector<FallingBlock>& blocks() const { return m_blocks; }
    std::vector<FallingBlock>& mutableBlocks() { return m_blocks; } // (M32.3: DropKeeper)
    void clear() { m_blocks.clear(); }
    // Falling stalactites that landed this tick (M27.2; wiki: Pointed Dripstone): whoever
    // stands where one lands takes `damage`. Main applies them and clears the list.
    struct Impact {
        glm::dvec3 pos;
        float damage;
    };
    std::vector<Impact>& impacts() { return m_impacts; }

private:
    bool move(const world::World& world, FallingBlock& f); // true if it landed
    std::vector<FallingBlock> m_blocks;
    std::vector<Aabb> m_boxes; // reused
    std::vector<Impact> m_impacts;
};

} // namespace mc
