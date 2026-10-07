#include "gameplay/DragonFight.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

// The top of the exit portal's bedrock column at the origin (-1: not loaded).
int columnTop(const World& world) {
    if (!world.chunk({0, 0})) return -1;
    for (int y = world.height().maxY(); y > world.height().minY; --y)
        if (blockRegistry().blockOf(world.getBlock({0, y, 0})) == blocks::Bedrock) return y;
    return -1;
}

} // namespace

void DragonFight::tick(World& world, const Mobs& mobs, const glm::dvec3& playerPos, ExperienceOrbs& orbs,
                       Xoroshiro& rng, std::vector<BlockPos>& edits) {
    for (const glm::dvec3& at : mobs.dragonDeaths()) {
        orbs.drop(at, previouslyKilled ? 500 : 12000, rng);
        openExitPortal(world, !previouslyKilled, edits);
        killed = true;
        previouslyKilled = true;
        uuidHi = uuidLo = 0;
        missingScans = 0;
    }
    if (killed) return;
    // Every 5 s near the middle: is the dragon there? Spawn it the first time, or
    // again if it went missing for three scans in a row (vanilla rescans for it too).
    if (++m_scanClock < 100) return;
    m_scanClock = 0;
    if (playerPos.x * playerPos.x + playerPos.z * playerPos.z > 128.0 * 128.0) return;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (!world.chunk({dx, dz})) return; // (wait for the middle to load)
    bool found = false;
    world.forEachChunk([&](Chunk& c) {
        for (const MobData& m : c.mobs())
            if (m.type == MobType::EnderDragon) {
                found = true;
                uuidHi = m.uuidHi;
                uuidLo = m.uuidLo;
            }
    });
    if (found) {
        missingScans = 0;
        return;
    }
    if (uuidHi != 0 && ++missingScans < 3) return;
    MobData d = Mobs::make(MobType::EnderDragon, {0.5, 128.0, 0.5}, rng);
    d.persistent = true;
    d.lastHealth = d.health;
    if (Mobs::add(world, d)) {
        uuidHi = d.uuidHi;
        uuidLo = d.uuidLo;
        missingScans = 0;
    }
}

void DragonFight::openExitPortal(World& world, bool egg, std::vector<BlockPos>& edits) {
    const int top = columnTop(world);
    if (top < 0) return;
    // The column rises 4 above the portal's floor; the portal is the ring of cells
    // within 2.5 of it one above the bowl (wiki: Exit Portal).
    const int y = top - 3;
    const BlockStateId portal = blockRegistry().defaultState(blocks::EndPortal);
    for (int z = -3; z <= 3; ++z)
        for (int x = -3; x <= 3; ++x) {
            const int d2 = x * x + z * z;
            if (d2 == 0 || d2 > 6 || world.getBlock({x, y, z}) != 0) continue;
            world.updateBlock({x, y, z}, portal);
            edits.push_back({x, y, z});
        }
    if (egg && world.getBlock({0, top + 1, 0}) == 0) {
        world.updateBlock({0, top + 1, 0}, blockRegistry().defaultState(blocks::DragonEgg));
        edits.push_back({0, top + 1, 0});
    }
}

bool DragonFight::teleportEgg(World& world, const BlockPos& egg, Xoroshiro& rng, std::vector<BlockPos>& edits) {
    const BlockStateId s = world.getBlock(egg);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const BlockPos to{egg.x + static_cast<int>(rng.nextInt(31)) - 15, egg.y + static_cast<int>(rng.nextInt(15)) - 7,
                          egg.z + static_cast<int>(rng.nextInt(31)) - 15};
        if (!world.isInHeight(to.y) || !world.chunk(to.chunk()) || world.getBlock(to) != 0) continue;
        world.updateBlock(egg, 0);
        world.updateBlock(to, s); // (falls from there if nothing holds it)
        edits.push_back(egg);
        edits.push_back(to);
        return true;
    }
    return false;
}

} // namespace mc
