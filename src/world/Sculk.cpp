// Sculk (M27.3; wiki: Sculk Sensor, Sculk Shrieker, Sculk Catalyst, Vibration). Part of
// BlockUpdates.
//
// Steps, jumps, landings, placed and broken blocks and explosions make vibrations. A sculk
// sensor within 8 blocks (sneaking players make none) wakes for 30 ticks, giving redstone
// power that falls off with distance, then rests for 10. A vibration a player made also
// sets off the shriekers within 8 blocks of the sensor that heard it; a shrieker shrieks
// for 90 ticks, and one that can summon tells main (warning levels, Darkness, the warden).
// A creature dying near a sculk catalyst blooms sculk around it instead of dropping its
// experience.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <array>
#include <cmath>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }

// Calls f(pos, state) for every block of `kind` within `radius` of `c` (cubes of
// sections that hold none are skipped through their palettes).
template <typename F> void forEachNear(const World& world, const glm::dvec3& c, int radius, BlockId kind, F&& f) {
    const BlockPos lo{int(std::floor(c.x)) - radius, int(std::floor(c.y)) - radius, int(std::floor(c.z)) - radius};
    const BlockPos hi{int(std::floor(c.x)) + radius, int(std::floor(c.y)) + radius, int(std::floor(c.z)) + radius};
    const HeightRange& h = world.height();
    for (int scz = blockToChunk(lo.z); scz <= blockToChunk(hi.z); ++scz)
        for (int scx = blockToChunk(lo.x); scx <= blockToChunk(hi.x); ++scx) {
            const Chunk* ch = world.chunk({scx, scz});
            if (!ch) continue;
            for (int sy = std::max(lo.y, h.minY) >> 4; sy <= (std::min(hi.y, h.maxY()) >> 4); ++sy) {
                const Section& sec = ch->section(h.sectionIndex(sy * 16));
                if (sec.allPaletteStates([&](BlockStateId s) { return R().blockOf(s) != kind; })) continue;
                // (decoded once, then read straight from the array - M27 perf review)
                static thread_local std::array<BlockStateId, Section::kVolume> cells;
                sec.copyTo(cells.data());
                for (int y = std::max(lo.y, sy * 16); y <= std::min(hi.y, sy * 16 + 15); ++y)
                    for (int z = std::max(lo.z, scz * 16); z <= std::min(hi.z, scz * 16 + 15); ++z)
                        for (int x = std::max(lo.x, scx * 16); x <= std::min(hi.x, scx * 16 + 15); ++x) {
                            const BlockStateId s = cells[size_t(Section::index(blockToLocal(x), y - sy * 16, blockToLocal(z)))];
                            if (R().blockOf(s) == kind) f(BlockPos{x, y, z}, s);
                        }
            }
        }
}

} // namespace

void BlockUpdates::vibrate(const glm::dvec3& at, bool byPlayer) {
    m_world.vibration(at, byPlayer); // (wardens hear it too)
    // The shriekers a sensor here could reach (within 8 of a sensor within 8), found once.
    std::array<BlockPos, 32> shriekers;
    int nShriekers = 0;
    if (byPlayer)
        forEachNear(m_world, at, 16, B::SculkShrieker, [&](const BlockPos& q, BlockStateId) {
            if (nShriekers < int(shriekers.size())) shriekers[size_t(nShriekers++)] = q;
        });
    // (M29.5; wiki: Calibrated Sculk Sensor) calibrated sensors hear within 16 (ours: every
    // vibration - we don't give vibrations frequencies to filter by).
    auto hear = [&](const BlockPos& p, BlockStateId s) {
        if (R().get(s, sculkPhase) != 0) return; // (active or resting)
        const double range = R().blockOf(s) == B::CalibratedSculkSensor ? 16.0 : 8.0;
        const glm::dvec3 c(p.x + 0.5, p.y + 0.5, p.z + 0.5);
        const double d = glm::length(at - c);
        if (d > range) return;
        // Power by distance (wiki: Sculk Sensor - 15 close, weaker to 1 at 8 blocks).
        const int strength = std::clamp(15 - int(std::floor(d * 14.0 / range)), 1, 15);
        set(p, R().set(R().set(s, sculkPhase, 1), power, strength));
        schedule(p, B::SculkSensor, 30, 0);
        for (int k = 0; k < nShriekers; ++k) {
            const BlockPos& q = shriekers[size_t(k)];
            if (glm::length(glm::dvec3(q.x + 0.5, q.y + 0.5, q.z + 0.5) - c) <= 8.5) shriek(q);
        }
    };
    forEachNear(m_world, at, 8, B::SculkSensor, hear);
    forEachNear(m_world, at, 16, B::CalibratedSculkSensor, hear);
}

void BlockUpdates::shriek(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (R().blockOf(s) != B::SculkShrieker || R().get(s, shrieking) == 0) return; // (already shrieking)
    set(p, R().set(s, shrieking, 0));
    schedule(p, B::SculkShrieker, 90, 0);
    if (m_shrieks.size() < m_shrieks.capacity()) m_shrieks.push_back({p, R().get(s, canSummon) == 0});
}

bool BlockUpdates::tickSculk(const BlockPos& p, BlockStateId s) {
    switch (R().blockOf(s)) {
    case B::CalibratedSculkSensor: // (M29.5)
    case B::SculkSensor: // active -> cooldown (10 ticks) -> inactive
        if (R().get(s, sculkPhase) == 1) {
            set(p, R().set(R().set(s, sculkPhase, 2), power, 0));
            schedule(p, B::SculkSensor, 10, 0);
        } else if (R().get(s, sculkPhase) == 2) {
            set(p, R().set(s, sculkPhase, 0));
        }
        return true;
    case B::SculkShrieker:
        if (R().get(s, shrieking) == 0) set(p, R().set(s, shrieking, 1));
        return true;
    default: return false;
    }
}

bool BlockUpdates::sculkBloom(World& world, const glm::dvec3& at, int charge, Xoroshiro& rng) {
    // A catalyst within 8 blocks takes the experience and spreads that much sculk over the
    // blocks around where the creature fell (wiki: Sculk Catalyst; ours: up to `charge`
    // blocks within 3, now and then a sensor or a shrieker that can't summon).
    bool near = false;
    forEachNear(world, at, 8, B::SculkCatalyst, [&](const BlockPos& p, BlockStateId) {
        near = near || glm::length(glm::dvec3(p.x + 0.5, p.y + 0.5, p.z + 0.5) - at) <= 8.5;
    });
    if (!near || charge <= 0) return false;
    const BlockStateId sculk = R().defaultState(B::Sculk);
    int left = std::min(charge, 30);
    for (int tries = 0; tries < 60 && left > 0; ++tries) {
        const BlockPos q{int(std::floor(at.x)) + int(rng.nextInt(7)) - 3, int(std::floor(at.y)) - 1 - int(rng.nextInt(2)),
                         int(std::floor(at.z)) + int(rng.nextInt(7)) - 3};
        const BlockStateId cur = world.getBlock(q);
        const BlockId cb = R().blockOf(cur);
        if (!R().opaqueCube(cur) || cb == B::Sculk || cb == B::SculkCatalyst || cb == B::Bedrock ||
            cb == B::ReinforcedDeepslate || world.getBlock({q.x, q.y + 1, q.z}) != 0)
            continue;
        world.updateBlock(q, sculk);
        --left;
        const uint32_t r = rng.nextInt(100);
        if (r < 8) world.updateBlock({q.x, q.y + 1, q.z}, R().defaultState(B::SculkSensor));
        else if (r < 10) world.updateBlock({q.x, q.y + 1, q.z}, R().defaultState(B::SculkShrieker));
    }
    return true;
}

} // namespace mc::world
