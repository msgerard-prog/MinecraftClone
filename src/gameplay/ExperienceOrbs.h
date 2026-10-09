#pragma once

#include "gameplay/Aabb.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Experience orbs (M17.5; wiki: Experience orb): dropped by mobs, ores, breeding,
// smelting; fall with gravity 0.03, are drawn toward a player within 8 blocks and
// picked up on touch (one orb every 2 ticks); despawn after 6000 ticks. Pooled.
struct ExperienceOrb {
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    int value = 0;
    int count = 1; // (M30.4; vanilla 1.17+) orbs of the same value merged into one
    int age = 0;
};

class ExperienceOrbs {
public:
    static constexpr int kMax = 512;
    ExperienceOrbs() { m_orbs.reserve(kMax); }

    // Splits `points` into vanilla's orb sizes (2477, 1237 ... 3, 1) at `pos`.
    void drop(const glm::dvec3& pos, int points, world::Xoroshiro& rng);
    // One tick; returns the points the player collected (0 if `canCollect` is false).
    int tick(const world::World& world, const Aabb& player, bool canCollect);

    const std::vector<ExperienceOrb>& orbs() const { return m_orbs; }
    void clear() { m_orbs.clear(); }
    // Saving (M30.4): orbs inside `chunk` into its droppedOrbs() and back. Return how many.
    int park(world::Chunk& chunk);
    int unpark(world::Chunk& chunk); // (what doesn't fit stays parked)
    void parkAll(world::World& world, std::vector<world::ChunkPos>& touched);

private:
    std::vector<ExperienceOrb> m_orbs;
    int m_pickupDelay = 0;
};

} // namespace mc
