#pragma once

#include "gameplay/Aabb.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Primed TNT (M21.1b; wiki: TNT): a lit block that hops up (0.2) with a small random
// sideways push, falls (gravity 0.04, drag 0.98, ground friction 0.7) and explodes
// with power 4 when its fuse (80 ticks; 10-30 when lit by another explosion) runs out.
// Pooled (hard rule 1); not saved (one lit when the world is saved is lost).
struct PrimedTntEntity {
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    int fuse = 80;
};

class PrimedTnt {
public:
    static constexpr int kMax = 1024;
    PrimedTnt() {
        m_items.reserve(kMax);
        m_boxes.reserve(256);
        m_explode.reserve(kMax);
    }
    // A TNT block at `block` lit with `fuse` ticks to go.
    void prime(const world::BlockPos& block, int fuse, world::Xoroshiro& rng);
    // One tick; those whose fuse ran out are removed and listed in explosions().
    void tick(const world::World& world);
    // An explosion at `centre` pushes primed TNT within 2 x power, like other
    // entities (wiki: Explosion - what makes TNT cannons work); exposure not sampled.
    void push(const glm::dvec3& centre, float power);

    const std::vector<PrimedTntEntity>& items() const { return m_items; }
    std::vector<PrimedTntEntity>& mutableItems() { return m_items; } // (M32.3: DropKeeper)
    const std::vector<glm::dvec3>& explosions() const { return m_explode; }
    void clear() { m_items.clear(); }

private:
    std::vector<PrimedTntEntity> m_items;
    std::vector<Aabb> m_boxes;
    std::vector<glm::dvec3> m_explode;
};

} // namespace mc
