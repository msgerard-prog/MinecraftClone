#include "gameplay/DragonFight.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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

BlockPos DragonFight::gatewayPos(int i) {
    const double a =
        2.0 * (-std::numbers::pi + std::numbers::pi / 20.0 * i); // (matches the wiki's table)
    return {static_cast<int>(std::floor(96.0 * std::cos(a))), 75,
            static_cast<int>(std::floor(96.0 * std::sin(a)))};
}

bool DragonFight::buildGateway(World& world, const BlockPos& at, std::vector<BlockPos>& edits) {
    const auto& r = blockRegistry();
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!world.chunk(BlockPos{at.x + dx, at.y, at.z + dz}.chunk())) return false;
    auto put = [&](const BlockPos& p, BlockStateId s) {
        if (!world.isInHeight(p.y) || !world.chunk(p.chunk()) || world.getBlock(p) == s) return;
        world.updateBlock(p, s);
        edits.push_back(p);
    };
    const BlockStateId bedrock = r.defaultState(blocks::Bedrock);
    put({at.x, at.y + 2, at.z}, bedrock);
    put({at.x, at.y - 2, at.z}, bedrock);
    for (const int dy : {-1, 1}) {
        put({at.x, at.y + dy, at.z}, bedrock);
        put({at.x + 1, at.y + dy, at.z}, bedrock);
        put({at.x - 1, at.y + dy, at.z}, bedrock);
        put({at.x, at.y + dy, at.z + 1}, bedrock);
        put({at.x, at.y + dy, at.z - 1}, bedrock);
    }
    put(at, r.defaultState(blocks::EndGateway));
    return true;
}

std::optional<glm::dvec3> DragonFight::gatewayTarget(const EndGenerator& gen, const BlockPos& g) {
    const glm::dvec2 out(g.x + 0.5, g.z + 0.5);
    const double dist = glm::length(out);
    if (dist < 1.0) return std::nullopt;
    const glm::dvec2 dir = out / dist;
    if (dist < 512.0) {
        // Out: along its direction to the first island past 1024 blocks (vanilla also
        // makes a small island when it finds none; ours lands on the first one within
        // 4096 blocks or not at all).
        for (double r = 1024.0; r < 4096.0; r += 16.0) {
            const int x = int(std::floor(dir.x * r)), z = int(std::floor(dir.y * r));
            const int top = gen.outerTop(x, z);
            if (top < 0) continue;
            // The exit gateway floats 10 above this column; the player lands on its
            // ground (below it, out of its reach).
            if (m_pendingGateways.size() < 32) m_pendingGateways.push_back({x, top + 10, z});
            return glm::dvec3(x + 0.5, top + 1.0, z + 0.5);
        }
        return std::nullopt;
    }
    // Back: to the main island, beside the nearest ring gateway, on solid ground.
    for (double r = 96.0; r > 0.0; r -= 1.0) {
        const int x = int(std::floor(dir.x * r)), z = int(std::floor(dir.y * r));
        const int top = gen.islandTop(x, z);
        if (top >= 0) return glm::dvec3(x + 0.5, top + 1.0, z + 0.5);
    }
    return glm::dvec3(100.5, 49.0, 0.5); // (the arrival platform)
}

void DragonFight::respawnStep(World& world, const EndGenerator& gen, Xoroshiro& rng,
                              std::vector<BlockPos>& edits) {
    ++m_respawnTicks;
    const auto& r = blockRegistry();
    // Breaking a summoning crystal calls it off (wiki: Ender Dragon › Re-summoning).
    int summoning = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* c = world.chunk({dx, dz}))
                for (const MobData& m : c->mobs())
                    summoning += m.type == MobType::EndCrystal && !m.showBottom &&
                                 m.health > 0.0f && std::abs(m.pos.x) < 4.0 &&
                                 std::abs(m.pos.z) < 4.0;
    if (summoning < 4) {
        m_respawnTicks = -1;
        return;
    }
    if (m_respawnTicks == 100) {
        // The pillars come back as they were made, crystals and cages included.
        const BlockStateId obsidian = r.defaultState(blocks::Obsidian),
                           bedrock = r.defaultState(blocks::Bedrock);
        for (int i = 0; i < EndGenerator::kPillars; ++i) {
            const auto& p = gen.pillar(i);
            for (int dz = -p.radius - 1; dz <= p.radius + 1; ++dz)
                for (int dx = -p.radius - 1; dx <= p.radius + 1; ++dx) {
                    if (dx * dx + dz * dz > p.radius * p.radius + 1) continue;
                    for (int y = 0; y <= p.height + 4; ++y) {
                        const BlockPos b{p.x + dx, y, p.z + dz};
                        if (!world.chunk(b.chunk())) continue;
                        const BlockStateId want = y <= p.height ? obsidian : 0;
                        if (y > p.height && (y > p.height + 1 || dx != 0 || dz != 0)) {
                            if (world.getBlock(b) != 0 &&
                                r.blockOf(world.getBlock(b)) != blocks::IronBars) {
                                world.updateBlock(b, 0);
                                edits.push_back(b);
                            }
                            continue;
                        }
                        const BlockStateId s = y == p.height + 1 ? bedrock : want;
                        if (world.getBlock(b) != s) {
                            world.updateBlock(b, s);
                            edits.push_back(b);
                        }
                    }
                }
            // Its cage, if it had one.
            for (int y = p.height + 1; y <= p.height + 4; ++y)
                for (int z = p.z - 2; z <= p.z + 2; ++z)
                    for (int x = p.x - 2; x <= p.x + 2; ++x) {
                        const BlockStateId cage = gen.cageBlock(x, y, z);
                        const BlockPos b{x, y, z};
                        if (!cage || !world.chunk(b.chunk()) || world.getBlock(b) == cage) continue;
                        world.updateBlock(b, cage);
                        edits.push_back(b);
                    }
            // Its crystal, if it lost it.
            bool has = false;
            if (Chunk* c = world.chunk({blockToChunk(p.x), blockToChunk(p.z)}))
                for (const MobData& m : c->mobs())
                    has = has ||
                          (m.type == MobType::EndCrystal && std::abs(m.pos.x - (p.x + 0.5)) < 1.0 &&
                           std::abs(m.pos.z - (p.z + 0.5)) < 1.0);
            if (!has)
                Mobs::add(world, Mobs::make(MobType::EndCrystal,
                                            {p.x + 0.5, p.height + 2.0, p.z + 0.5}, rng));
        }
    }
    if (m_respawnTicks < 200) return;
    // Done: the summoning crystals go, the portal shuts, the dragon comes.
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (Chunk* c = world.chunk({dx, dz}))
                std::erase_if(c->mobs(), [](const MobData& m) {
                    return m.type == MobType::EndCrystal && !m.showBottom &&
                           std::abs(m.pos.x) < 4.0 && std::abs(m.pos.z) < 4.0;
                });
    for (int z = -3; z <= 3; ++z)
        for (int x = -3; x <= 3; ++x)
            for (int y = 40; y < 90; ++y)
                if (r.blockOf(world.getBlock({x, y, z})) == blocks::EndPortal) {
                    world.updateBlock({x, y, z}, 0);
                    edits.push_back({x, y, z});
                }
    // The egg, if still on the podium, goes (wiki).
    if (const int top = columnTop(world);
        top >= 0 && r.blockOf(world.getBlock({0, top + 1, 0})) == blocks::DragonEgg) {
        world.updateBlock({0, top + 1, 0}, 0);
        edits.push_back({0, top + 1, 0});
    }
    killed = false;
    uuidHi = uuidLo = 0;
    m_scanClock = 99; // (spawns the dragon on the next tick)
    m_respawnTicks = -1;
}

void DragonFight::tick(World& world, const EndGenerator& gen, const Mobs& mobs,
                       const glm::dvec3& playerPos, ExperienceOrbs& orbs, Xoroshiro& rng,
                       std::vector<BlockPos>& edits) {
    if (!gatewaysReady) { // the 20 gateways in a random order
        gateways.clear();
        for (int i = 0; i < 20; ++i)
            gateways.push_back(i);
        for (int i = 19; i > 0; --i)
            std::swap(gateways[size_t(i)], gateways[size_t(rng.nextInt(uint32_t(i + 1)))]);
        gatewaysReady = true;
    }
    for (const glm::dvec3& at : mobs.dragonDeaths()) {
        orbs.drop(at, previouslyKilled ? 500 : 12000, rng);
        if (!openExitPortal(world, !previouslyKilled, edits))
            m_pendingPortal = previouslyKilled ? 1 : 2;
        if (!gateways.empty()) { // the next gateway opens (once its chunks are there)
            if (m_pendingGateways.size() < 32)
                m_pendingGateways.push_back(gatewayPos(gateways.front()));
            gateways.erase(gateways.begin());
        }
        killed = true;
        previouslyKilled = true;
        uuidHi = uuidLo = 0;
        missingScans = 0;
    }
    if (m_pendingPortal && openExitPortal(world, m_pendingPortal == 2, edits)) m_pendingPortal = 0;
    std::erase_if(m_pendingGateways,
                  [&](const BlockPos& g) { return buildGateway(world, g, edits); });
    if (killed) {
        if (m_respawnTicks >= 0) {
            respawnStep(world, gen, rng, edits);
            return;
        }
        // A crystal on each side of the open portal starts a respawn.
        bool side[4] = {};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (const Chunk* c = world.chunk({dx, dz}))
                    for (const MobData& m : c->mobs()) {
                        if (m.type != MobType::EndCrystal || m.health <= 0.0f) continue;
                        const double x = m.pos.x - 0.5, z = m.pos.z - 0.5, d2 = x * x + z * z;
                        if (d2 < 6.5 || d2 > 12.5) continue; // on the rim
                        side[std::abs(x) > std::abs(z) ? (x > 0 ? 0 : 1) : (z > 0 ? 2 : 3)] = true;
                    }
        if (side[0] && side[1] && side[2] && side[3]) m_respawnTicks = 0;
        return;
    }
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
    if (uuidHi != 0) {
        // A known dragon may just be in an unloaded chunk: it counts as missing only
        // when its whole flying range (80 blocks round the middle) was scanned.
        for (int dz = -5; dz <= 5; ++dz)
            for (int dx = -5; dx <= 5; ++dx)
                if (!world.chunk({dx, dz})) return;
        if (++missingScans < 3) return;
    }
    MobData d = Mobs::make(MobType::EnderDragon, {0.5, 128.0, 0.5}, rng);
    d.persistent = true;
    d.lastHealth = d.health;
    if (Mobs::add(world, d)) {
        uuidHi = d.uuidHi;
        uuidLo = d.uuidLo;
        missingScans = 0;
    }
}

bool DragonFight::openExitPortal(World& world, bool egg, std::vector<BlockPos>& edits) {
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!world.chunk({dx, dz})) return false;
    const int top = columnTop(world);
    if (top < 0) return false;
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
    return true;
}

bool DragonFight::teleportEgg(World& world, const BlockPos& egg, Xoroshiro& rng,
                              std::vector<BlockPos>& edits) {
    const BlockStateId s = world.getBlock(egg);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const BlockPos to{egg.x + static_cast<int>(rng.nextInt(31)) - 15,
                          egg.y + static_cast<int>(rng.nextInt(15)) - 7,
                          egg.z + static_cast<int>(rng.nextInt(31)) - 15};
        if (!world.isInHeight(to.y) || !world.chunk(to.chunk()) || world.getBlock(to) != 0)
            continue;
        world.updateBlock(egg, 0);
        world.updateBlock(to, s); // (falls from there if nothing holds it)
        edits.push_back(egg);
        edits.push_back(to);
        return true;
    }
    return false;
}

} // namespace mc
