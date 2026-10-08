#include "gameplay/ExperienceOrbs.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

void ExperienceOrbs::drop(const glm::dvec3& pos, int points, Xoroshiro& rng) {
    // Vanilla splits experience into orbs of these sizes (wiki: Experience orb).
    static constexpr int kSizes[] = {2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3, 1};
    while (points > 0) {
        int v = 1;
        for (int s : kSizes)
            if (points >= s) {
                v = s;
                break;
            }
        points -= v;
        if (m_orbs.size() >= size_t(kMax)) { // full: merge into the youngest orb
            if (!m_orbs.empty()) m_orbs.back().value += v;
            continue;
        }
        ExperienceOrb o;
        o.pos = o.prevPos = pos;
        o.vel = {(rng.nextDouble() * 0.2 - 0.1) * 2.0, rng.nextDouble() * 0.2 * 2.0,
                 (rng.nextDouble() * 0.2 - 0.1) * 2.0};
        o.value = v;
        m_orbs.push_back(o);
    }
}

int ExperienceOrbs::tick(const World& world, const Aabb& player, bool canCollect) {
    int collected = 0;
    if (m_pickupDelay > 0) --m_pickupDelay;
    const glm::dvec3 target = (player.min + player.max) * 0.5;
    const auto& reg = blockRegistry();
    for (size_t i = 0; i < m_orbs.size();) {
        ExperienceOrb& o = m_orbs[i];
        o.prevPos = o.pos;
        ++o.age;
        o.vel.y -= 0.03; // gravity
        // Drawn to the player within 8 blocks, harder the closer it is.
        const glm::dvec3 d = target - o.pos;
        const double dist = glm::length(d) / 8.0;
        if (canCollect && dist < 1.0 && dist > 1e-6) {
            const double pull = (1.0 - dist) * (1.0 - dist);
            o.vel += d / glm::length(d) * pull * 0.1;
        }
        // Simple ground collision: stop falling into solid blocks.
        glm::dvec3 next = o.pos + o.vel;
        if (reg.collides(world.getBlock(
                {int(std::floor(next.x)), int(std::floor(next.y)), int(std::floor(next.z))}))) {
            if (o.vel.y < 0.0) o.vel.y = 0.0;
            o.vel.x *= 0.6;
            o.vel.z *= 0.6;
            next = o.pos + o.vel;
            if (reg.collides(world.getBlock(
                    {int(std::floor(next.x)), int(std::floor(next.y)), int(std::floor(next.z))})))
                next = o.pos;
        }
        o.pos = next;
        o.vel *= 0.98;
        bool remove = o.age >= 6000 || o.pos.y < world.height().minY - 64;
        if (!remove && canCollect && m_pickupDelay == 0 &&
            Aabb{player.min - glm::dvec3(0.25), player.max + glm::dvec3(0.25)}.intersects(
                Aabb::fromFeet(o.pos, 0.5, 0.5))) {
            collected += o.value;
            m_pickupDelay = 2; // one orb every 2 ticks (wiki)
            remove = true;
        }
        if (remove) {
            m_orbs[i] = m_orbs.back();
            m_orbs.pop_back();
        } else {
            ++i;
        }
    }
    return collected;
}

} // namespace mc
