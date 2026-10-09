// Nether structures (M19.3; wiki: Nether Fortress, Bastion Remnant, Structure set).
// Part of NetherGenerator ("nether2" only).
#include "world/NetherGenerator.h"

#include "world/Blocks.h"
#include "world/Loot.h"
#include "world/Random.h"
#include "world/StructurePlacement.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>

namespace mc::world {

namespace {

constexpr int kTop = 128; // the generated part of the Nether (Y 0..127)
// Nether complexes: one start candidate per 27-chunk region (432 blocks), a fortress
// 2 times in 5 and a bastion 3 in 5 (wiki: Nether Fortress, Structure set). Spacing
// and separation are vanilla's; the salt and seed mixing are ours.
constexpr RandomSpread kNetherComplexes{27, 4, 30084232};

uint64_t mix3(uint64_t seed, uint64_t salt, int32_t a, int32_t b) {
    return mixSeed(mixSeed(mixSeed(seed, salt), static_cast<uint32_t>(a)), static_cast<uint32_t>(b));
}
double positional(uint64_t seed, int32_t x, int32_t y, int32_t z, uint64_t purpose) {
    const uint64_t h = mixSeed(mix3(seed, purpose, x, z), static_cast<uint32_t>(y));
    return static_cast<double>(h >> 11) * 0x1.0p-53;
}

constexpr int kFx[4] = {0, -1, 0, 1}, kFz[4] = {1, 0, -1, 0};

// A fortress piece: as stronghold pieces, `half` to each side of its axis, `len`
// long, `h` tall from its floor; placed at its entrance (x, y, z) facing `dir`.
struct FPiece {
    enum Kind : uint8_t { Crossing, Bridge, Corridor, ChestCorridor, WartRoom, SpawnerPlatform } kind;
    int32_t x, y, z;
    uint8_t dir;
    int half, len, h;
    int32_t x0, z0, x1, z1;
    bool intersects(const FPiece& o) const { return x0 <= o.x1 && x1 >= o.x0 && z0 <= o.z1 && z1 >= o.z0; }
};
void toWorld(const FPiece& p, int u, int v, int32_t& wx, int32_t& wz) {
    wx = p.x + kFx[p.dir] * v - kFz[p.dir] * u;
    wz = p.z + kFz[p.dir] * v + kFx[p.dir] * u;
}
struct FPlan {
    int count = 0;
    std::array<FPiece, 96> pieces{};
};

void planFortress(uint64_t seed, ChunkPos start, FPlan& plan) {
    Xoroshiro r(mix3(seed, 0x464f5254u, start.x, start.z));
    plan.count = 0;
    const int32_t cx = start.x * 16 + 11, cz = start.z * 16 + 11; // (wiki: centred at 11, 11)
    const int32_t y0 = 48 + static_cast<int32_t>(r.nextInt(23));  // our band
    int spawners = 0;
    auto make = [&](FPiece::Kind kind, int32_t x, int32_t z, int dir) {
        FPiece p{kind, x, y0, z, static_cast<uint8_t>(dir), 2, 5, 5, 0, 0, 0, 0};
        switch (kind) {
        case FPiece::Crossing: p.half = 3, p.len = 7, p.h = 5; break;
        case FPiece::Bridge: p.half = 2, p.len = 19, p.h = 5; break;
        case FPiece::Corridor: p.half = 2, p.len = 5 + static_cast<int>(r.nextInt(5)), p.h = 6; break;
        case FPiece::ChestCorridor: p.half = 2, p.len = 7, p.h = 6; break;
        case FPiece::WartRoom: p.half = 6, p.len = 13, p.h = 8; break;
        case FPiece::SpawnerPlatform: p.half = 3, p.len = 7, p.h = 6; break;
        }
        int32_t ax, az, bx, bz;
        toWorld(p, -p.half, 0, ax, az);
        toWorld(p, p.half, p.len - 1, bx, bz);
        p.x0 = std::min(ax, bx), p.x1 = std::max(ax, bx), p.z0 = std::min(az, bz), p.z1 = std::max(az, bz);
        return p;
    };
    auto fits = [&](const FPiece& p) {
        if (plan.count >= int(plan.pieces.size())) return false;
        if (std::abs(p.x0 - cx) > 112 || std::abs(p.x1 - cx) > 112 || std::abs(p.z0 - cz) > 112 || std::abs(p.z1 - cz) > 112)
            return false;
        for (int i = 0; i < plan.count; ++i)
            if (plan.pieces[size_t(i)].intersects(p)) return false;
        return true;
    };
    auto grow = [&](auto& self, const FPiece& from, int u, int v, int dir, int depth, bool inside) -> void {
        if (depth > 8) return;
        int32_t x, z;
        toWorld(from, u, v, x, z);
        const uint32_t roll = r.nextInt(100);
        FPiece::Kind kind;
        if (!inside) { // outside: bridges, crossings, blaze platforms, then the halls
            kind = roll < 35 ? FPiece::Bridge
                   : roll < 65 ? FPiece::Crossing
                   : roll < 80 && spawners < 2 ? FPiece::SpawnerPlatform
                               : FPiece::Corridor;
        } else {
            kind = roll < 45 ? FPiece::Corridor : roll < 65 ? FPiece::ChestCorridor : roll < 80 ? FPiece::WartRoom
                                                                                               : FPiece::Kind(255);
            if (kind == FPiece::Kind(255)) return;
        }
        const FPiece p = make(kind, x, z, dir);
        if (!fits(p)) return;
        plan.pieces[size_t(plan.count++)] = p;
        const FPiece me = p;
        switch (kind) {
        case FPiece::Bridge: self(self, me, 0, me.len, dir, depth + 1, false); break;
        case FPiece::Crossing:
            self(self, me, 0, me.len, dir, depth + 1, false);
            self(self, me, me.half + 1, me.len / 2, (dir + 1) & 3, depth + 1, false);
            self(self, me, -me.half - 1, me.len / 2, (dir + 3) & 3, depth + 1, false);
            break;
        case FPiece::Corridor:
        case FPiece::ChestCorridor: self(self, me, 0, me.len, dir, depth + 1, true); break;
        case FPiece::SpawnerPlatform: ++spawners; break;
        default: break; // wart rooms: dead ends
        }
    };
    const int dir = static_cast<int>(r.nextInt(4));
    FPiece centre = make(FPiece::Crossing, cx - kFx[dir] * 3, cz - kFz[dir] * 3, dir);
    plan.pieces[size_t(plan.count++)] = centre;
    grow(grow, centre, 0, centre.len, dir, 1, false);
    grow(grow, centre, centre.half + 1, centre.len / 2, (dir + 1) & 3, 1, false);
    grow(grow, centre, -centre.half - 1, centre.len / 2, (dir + 3) & 3, 1, false);
    grow(grow, centre, 0, -1, (dir + 2) & 3, 1, false);
}

} // namespace

NetherGenerator::Complex NetherGenerator::complexAt(ChunkPos start) const {
    if (m_version < 2 || !isSpreadCandidate(m_seed, kNetherComplexes, start)) return Complex::None;
    Xoroshiro r(mix3(m_seed, 0x434f4d50u, start.x, start.z));
    const bool fortress = r.nextInt(5) < 2;
    // Bastions avoid basalt deltas: a fortress goes there instead (wiki).
    if (!fortress && biomeAt(start.x * 16 + 8, start.z * 16 + 8) != Biome::BasaltDeltas) return Complex::Bastion;
    return Complex::Fortress;
}

void NetherGenerator::placeNetherStructures(BlockStateId* blocks, Chunk& out, ChunkPos pos, Entities& ents) const {
    const auto& reg = blockRegistry();
    auto S = [&](BlockId b) { return reg.defaultState(b); };
    const int32_t baseX = pos.x * 16, baseZ = pos.z * 16;
    auto at = [](int x, int y, int z) -> size_t { return size_t((y * 16 + z) * 16 + x); };
    auto put = [&](int32_t wx, int32_t y, int32_t wz, BlockStateId s) {
        const int lx = wx - baseX, lz = wz - baseZ;
        if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || y <= 0 || y >= kTop - 1) return;
        if (reg.blockOf(blocks[at(lx, y, lz)]) == blocks::Bedrock) return;
        blocks[at(lx, y, lz)] = s;
    };
    auto get = [&](int32_t wx, int32_t y, int32_t wz) -> BlockStateId {
        const int lx = wx - baseX, lz = wz - baseZ;
        if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || y < 0 || y >= kTop) return 0;
        return blocks[at(lx, y, lz)];
    };
    auto entity = [&](int32_t wx, int32_t y, int32_t wz, bool chest, LootTable loot) {
        const int lx = wx - baseX, lz = wz - baseZ;
        if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || ents.count >= int(ents.list.size())) return;
        blocks[at(lx, y, lz)] = chest ? S(blocks::Chest) : S(blocks::Spawner);
        ents.list[size_t(ents.count++)] = {static_cast<int8_t>(lx), static_cast<int8_t>(lz), static_cast<int16_t>(y),
                                           chest, static_cast<uint8_t>(loot)};
    };
    const BlockStateId bricks = S(blocks::NetherBricks), fence = S(blocks::NetherBrickFence),
                       soulSand = S(blocks::SoulSand), lava = S(blocks::Lava);
    // Pillars: a column down to the first solid block (or the lava sea).
    auto pillar = [&](int32_t wx, int32_t y, int32_t wz, BlockStateId s) {
        for (int k = y; k > 1; --k) {
            const BlockStateId c = get(wx, k, wz);
            if (c != 0 && c != lava) break;
            put(wx, k, wz, s);
        }
    };

    struct Cached {
        uint64_t seed = 0;
        int32_t sx = INT32_MIN, sz = 0;
        FPlan plan;
    };
    static thread_local std::array<Cached, 4> cache;
    for (int32_t dz = -8; dz <= 8; ++dz)
        for (int32_t dx = -8; dx <= 8; ++dx) {
            const ChunkPos start{pos.x + dx, pos.z + dz};
            const Complex kind = complexAt(start);
            if (kind == Complex::None) continue;
            if (kind == Complex::Bastion) {
                // ---- Bastion remnant (wiki): our basic keep - a 21x21, two-storey ring
                // of polished blackstone bricks (cracked here and there) with battlements,
                // gold blocks and gilded blackstone, basalt corner pillars down to the
                // ground or lava, an open middle; chests and its piglins and hoglins.
                if (std::abs(dx) > 1 || std::abs(dz) > 1) continue; // it spans < 2 chunks
                const int32_t ox = start.x * 16 - 2, oz = start.z * 16 - 2;
                // On the highest cavern floor below Y 96 (open above, solid below), or
                // over the lava sea on its pillars.
                int y0 = kLavaLevel + 1;
                for (int y = 96; y > kLavaLevel + 1; --y)
                    if (!solidAt(ox + 10, y, oz + 10) && !solidAt(ox + 10, y + 3, oz + 10) && solidAt(ox + 10, y - 1, oz + 10)) {
                        y0 = y;
                        break;
                    }
                for (int u = 0; u < 21; ++u)
                    for (int v = 0; v < 21; ++v)
                        for (int y = 0; y < 16; ++y) {
                            const int32_t wx = ox + u, wz = oz + v, wy = y0 + y;
                            const bool wall = u == 0 || u == 20 || v == 0 || v == 20 || u == 7 || u == 13 || v == 7 ||
                                              v == 13;
                            const bool middle = u > 7 && u < 13 && v > 7 && v < 13;
                            const bool floor = (y == 0 || y == 7) && !middle;
                            const bool battlement = y == 15 && (u == 0 || u == 20 || v == 0 || v == 20) && (u + v) % 2 == 0;
                            BlockStateId b = 0;
                            if (floor || (wall && y < 15) || battlement) {
                                const double roll = positional(m_seed, wx, wy, wz, 0x4E80);
                                b = roll < 0.15  ? S(blocks::CrackedPolishedBlackstoneBricks)
                                    : roll < 0.2 ? S(blocks::GildedBlackstone)
                                    : roll < 0.23 ? S(blocks::Blackstone)
                                                  : S(blocks::PolishedBlackstoneBricks);
                                if (wall && y == 4 && (u == 0 || u == 20) && v % 4 == 2) b = S(blocks::ChiseledPolishedBlackstone);
                            }
                            // doorways at the middle of each outer wall, on both floors
                            if ((u == 10 && (v == 0 || v == 20)) || (v == 10 && (u == 0 || u == 20)))
                                if ((y >= 1 && y <= 3) || (y >= 8 && y <= 10)) b = 0;
                            if (!b && y > 0 && y != 7 && !(y == 15 && !battlement)) b = 0;
                            put(wx, wy, wz, b);
                        }
                for (const auto& c : {std::array{0, 0}, std::array{20, 0}, std::array{0, 20}, std::array{20, 20}}) {
                    for (int y = 0; y < 15; ++y)
                        put(ox + c[0], y0 + y, oz + c[1], S(blocks::Basalt));
                    pillar(ox + c[0], y0 - 1, oz + c[1], S(blocks::Basalt));
                }
                for (int u = 2; u <= 18; u += 4) // more support under the walls
                    for (int v : {0, 20}) {
                        pillar(ox + u, y0 - 1, oz + v, S(blocks::PolishedBlackstoneBricks));
                        pillar(ox + v, y0 - 1, oz + u, S(blocks::PolishedBlackstoneBricks));
                    }
                put(ox + 2, y0 + 1, oz + 2, S(blocks::GoldBlock));
                put(ox + 18, y0 + 8, oz + 18, S(blocks::GoldBlock));
                entity(ox + 3, y0 + 1, oz + 17, true, LootTable::BastionOther);
                entity(ox + 17, y0 + 8, oz + 3, true, LootTable::BastionOther);
                entity(ox + 17, y0 + 1, oz + 17, true, LootTable::BastionOther);
                // Its piglins and hoglins live in the chunk the keep starts in (they never
                // despawn: wiki); added once, by that chunk.
                if (start.x == pos.x && start.z == pos.z) {
                    Xoroshiro mr(mix3(m_seed, 0x4D4F4253u, start.x, start.z));
                    const int piglins = 4 + static_cast<int>(mr.nextInt(3)), hoglins = 1 + static_cast<int>(mr.nextInt(2));
                    for (int i = 0; i < piglins + hoglins; ++i) {
                        MobData m;
                        m.type = i < piglins ? MobType::Piglin : MobType::Hoglin;
                        m.uuidHi = (mr.nextLong() & ~0xF000ull) | 0x4000ull;
                        m.uuidLo = (mr.nextLong() & ~(3ull << 62)) | (2ull << 62);
                        const int u = 2 + static_cast<int>(mr.nextInt(4)), v = 2 + static_cast<int>(mr.nextInt(12));
                        const double fy = (i % 2 == 0 ? y0 + 1 : y0 + 8);
                        m.pos = m.prevPos = m.goal = glm::dvec3(ox + u + 0.5, fy, oz + v + 0.5);
                        if (m.pos.x < baseX || m.pos.x >= baseX + 16 || m.pos.z < baseZ || m.pos.z >= baseZ + 16)
                            m.pos = m.prevPos = m.goal = glm::dvec3(baseX + 3.5 + i, fy, baseZ + 3.5);
                        m.health = mobInfo(m.type).maxHealth;
                        m.persistent = true;
                        out.mobs().push_back(m);
                    }
                    // nether4 (M29.8; wiki: Piglin Brute - bastion guards, never respawned):
                    // two brutes on the keep's floor (drawn after the others: their mobs stay).
                    for (int i = 0; i < 2 && m_version >= 4; ++i) {
                        MobData b;
                        b.type = MobType::PiglinBrute;
                        b.uuidHi = (mr.nextLong() & ~0xF000ull) | 0x4000ull;
                        b.uuidLo = (mr.nextLong() & ~(3ull << 62)) | (2ull << 62);
                        b.pos = b.prevPos = b.goal = glm::dvec3(baseX + 6.5 + i * 3, y0 + 1, baseZ + 8.5);
                        b.health = mobInfo(b.type).maxHealth;
                        b.persistent = true;
                        out.mobs().push_back(b);
                    }
                }
                continue;
            }
            // ---- Nether fortress (wiki): our piece tree of bridges, crossings, halls,
            // wart rooms and blaze spawner platforms, all of nether bricks.
            Cached& e = cache[size_t((uint32_t(start.x) * 7u + uint32_t(start.z) * 13u) & 3u)];
            if (e.seed != m_seed || e.sx != start.x || e.sz != start.z) {
                e.seed = m_seed, e.sx = start.x, e.sz = start.z;
                planFortress(m_seed, start, e.plan);
            }
            for (int i = 0; i < e.plan.count; ++i) {
                const FPiece& p = e.plan.pieces[size_t(i)];
                if (p.x1 < baseX || p.x0 > baseX + 15 || p.z1 < baseZ || p.z0 > baseZ + 15) continue;
                auto pp = [&](int u, int y, int v, BlockStateId s) {
                    int32_t wx, wz;
                    toWorld(p, u, v, wx, wz);
                    put(wx, p.y + y, wz, s);
                };
                const bool outside = p.kind == FPiece::Crossing || p.kind == FPiece::Bridge ||
                                     p.kind == FPiece::SpawnerPlatform;
                if (outside) {
                    // A deck with fence railings, open air above; pillars under the edges.
                    for (int v = 0; v < p.len; ++v)
                        for (int u = -p.half; u <= p.half; ++u) {
                            pp(u, 0, v, bricks);
                            pp(u, -1, v, bricks);
                            for (int y = 1; y < p.h; ++y)
                                pp(u, y, v, 0);
                            const bool edge = std::abs(u) == p.half;
                            const bool exit = p.kind == FPiece::Crossing && std::abs(v - p.len / 2) <= 1;
                            if (edge && !exit) pp(u, 1, v, fence);
                            if (edge && (v % 6 == 2)) {
                                int32_t wx, wz;
                                toWorld(p, u, v, wx, wz);
                                pillar(wx, p.y - 2, wz, bricks);
                            }
                        }
                    if (p.kind == FPiece::SpawnerPlatform) { // a raised step with a blaze spawner
                        for (int v = 2; v <= 4; ++v)
                            for (int u = -1; u <= 1; ++u)
                                pp(u, 1, v, bricks);
                        int32_t wx, wz;
                        toWorld(p, 0, 3, wx, wz);
                        entity(wx, p.y + 2, wz, false, LootTable::NetherFortress);
                    }
                    continue;
                }
                // Halls: a nether brick shell, fence windows, air inside.
                for (int v = 0; v < p.len; ++v)
                    for (int u = -p.half; u <= p.half; ++u)
                        for (int y = 0; y < p.h; ++y) {
                            const bool shell = std::abs(u) == p.half || y == 0 || y == p.h - 1 ||
                                               (p.kind == FPiece::WartRoom && v == p.len - 1);
                            BlockStateId b = shell ? bricks : BlockStateId{0};
                            if (shell && std::abs(u) == p.half && y >= 2 && y <= 3 && v % 2 == 1) b = fence;
                            pp(u, y, v, b);
                        }
                if (p.kind == FPiece::ChestCorridor) {
                    int32_t wx, wz;
                    toWorld(p, p.half - 1, p.len / 2, wx, wz);
                    entity(wx, p.y + 1, wz, true, LootTable::NetherFortress);
                } else if (p.kind == FPiece::WartRoom) { // soul sand beds of nether wart
                    for (int v = 3; v < p.len - 2; ++v)
                        for (int u : {-p.half + 1, -p.half + 2, p.half - 2, p.half - 1}) {
                            pp(u, 1, v, soulSand);
                            int32_t wx, wz;
                            toWorld(p, u, v, wx, wz);
                            const int age = static_cast<int>(positional(m_seed, wx, p.y, wz, 0x4E81) * 4.0);
                            pp(u, 2, v, reg.with(S(blocks::NetherWart), "age", std::to_string(age)).value_or(0));
                        }
                }
            }
        }
}

} // namespace mc::world
