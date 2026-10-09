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
        // (M30.4; vanilla tryMergeToExisting) an orb of the same value within half a block
        // takes it as one more in its count.
        bool merged = false;
        for (ExperienceOrb& o : m_orbs)
            if (!merged && o.value == v && o.count > 0 && std::abs(o.pos.x - pos.x) <= 0.5 &&
                std::abs(o.pos.y - pos.y) <= 0.5 && std::abs(o.pos.z - pos.z) <= 0.5) {
                ++o.count;
                merged = true;
            }
        if (merged) continue;
        if (m_orbs.size() >= size_t(kMax)) { // full: add the points to a single orb (not one
            // counting several: that would multiply them - M30 review)
            for (auto it = m_orbs.rbegin(); it != m_orbs.rend(); ++it)
                if (it->count == 1) {
                    it->value += v;
                    break;
                }
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
        // Merging (M30.4; vanilla 1.17+ ExperienceOrb.scanForMerges, once a second): orbs of
        // the same value within half a block become one with a count.
        if ((o.age + int(i)) % 20 == 0)
            for (size_t j = 0; j < m_orbs.size(); ++j) {
                ExperienceOrb& n = m_orbs[j];
                if (j == i || n.count == 0 || n.value != o.value) continue;
                const glm::dvec3 gap = n.pos - o.pos;
                if (std::abs(gap.x) > 0.5 || std::abs(gap.y) > 0.5 || std::abs(gap.z) > 0.5) continue;
                o.count += n.count;
                o.age = std::min(o.age, n.age);
                n.count = 0; // (removed in its own turn)
            }
        bool remove = o.age >= 6000 || o.pos.y < world.height().minY - 64 || o.count <= 0;
        if (!remove && canCollect && m_pickupDelay == 0 &&
            Aabb{player.min - glm::dvec3(0.25), player.max + glm::dvec3(0.25)}.intersects(
                Aabb::fromFeet(o.pos, 0.5, 0.5))) {
            collected += o.value;
            m_pickupDelay = 2; // one orb every 2 ticks (wiki)
            remove = --o.count <= 0;
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

int ExperienceOrbs::park(Chunk& chunk) {
    auto& out = chunk.droppedOrbs();
    const size_t before = out.size(); // (appends: drops still parked there stay)
    for (size_t i = 0; i < m_orbs.size();) {
        const ExperienceOrb& o = m_orbs[i];
        const ChunkPos at{blockToChunk(int(std::floor(o.pos.x))), blockToChunk(int(std::floor(o.pos.z)))};
        if (at == chunk.pos() && o.count > 0) {
            out.push_back({o.pos, o.vel, o.value, o.count, int16_t(std::min(o.age, 32767))});
            m_orbs[i] = m_orbs.back();
            m_orbs.pop_back();
        } else {
            ++i;
        }
    }
    return int(out.size() - before);
}

int ExperienceOrbs::unpark(Chunk& chunk) {
    auto& parked = chunk.droppedOrbs();
    size_t n = 0;
    for (; n < parked.size() && m_orbs.size() < size_t(kMax); ++n) {
        const Chunk::DroppedOrb& d = parked[n];
        ExperienceOrb o;
        o.pos = o.prevPos = d.pos;
        o.vel = d.vel;
        o.value = d.value;
        o.count = d.count;
        o.age = d.age;
        m_orbs.push_back(o);
    }
    parked.erase(parked.begin(), parked.begin() + std::ptrdiff_t(n));
    return int(n);
}

void ExperienceOrbs::parkAll(World& world, std::vector<ChunkPos>& touched) {
    for (size_t i = 0; i < m_orbs.size();) {
        const ExperienceOrb& o = m_orbs[i];
        const ChunkPos at{blockToChunk(int(std::floor(o.pos.x))), blockToChunk(int(std::floor(o.pos.z)))};
        Chunk* c = o.count > 0 ? world.chunk(at) : nullptr;
        if (!c) {
            ++i;
            continue;
        }
        if (!c->holdsParked()) touched.push_back(at);
        c->droppedOrbs().push_back({o.pos, o.vel, o.value, o.count, int16_t(std::min(o.age, 32767))});
        m_orbs[i] = m_orbs.back();
        m_orbs.pop_back();
    }
}

} // namespace mc
