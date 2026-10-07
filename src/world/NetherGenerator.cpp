#include "world/NetherGenerator.h"

#include "world/Biome.h"
#include "world/Blocks.h"
#include "world/Direction.h"
#include "world/Random.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace mc::world {

namespace {

// Per-position random in [0,1): same position + purpose + seed => same value.
double positional(uint64_t seed, int32_t x, int32_t y, int32_t z, uint64_t purpose) {
    uint64_t h = mixSeed(seed, purpose);
    h = mixSeed(h, static_cast<uint32_t>(x));
    h = mixSeed(h, static_cast<uint32_t>(y));
    h = mixSeed(h, static_cast<uint32_t>(z));
    return static_cast<double>(h >> 11) * 0x1.0p-53;
}

uint64_t chunkSeed(uint64_t seed, int32_t cx, int32_t cz, uint64_t feature) {
    return mixSeed(mixSeed(mixSeed(seed, feature), static_cast<uint32_t>(cx)), static_cast<uint32_t>(cz));
}

std::shared_ptr<const ChunkBiomes> uniformBiomes(Biome b) {
    auto biomes = std::make_shared<ChunkBiomes>();
    biomes->cells.fill(b);
    return biomes;
}

// Nether terrain is generated for Y 0..127 (the rest of our 384-block column is air).
constexpr int kNetherTop = 128;
constexpr int kCellW = 4, kCellH = 8;
constexpr int kCellsX = 16 / kCellW, kCellsY = kNetherTop / kCellH;

} // namespace

// ---------------------------------------------------------------------------------
// The Nether

NetherGenerator::NetherGenerator(uint64_t seed, int version)
    : m_seed(seed), m_version(version), m_main(mixSeed(seed, 0x4E31), -6, 4), m_detail(mixSeed(seed, 0x4E32), -3, 2),
      m_shore(mixSeed(seed, 0x4E33), -4, 2), m_temperature(mixSeed(seed, 0x4E34), -7, 3),
      m_humidity(mixSeed(seed, 0x4E35), -7, 3) {}

Biome NetherGenerator::biomeAt(int32_t x, int32_t z) const {
    if (m_version < 2) return Biome::NetherWastes;
    // Vanilla picks Nether biomes by the nearest climate point (wiki: Nether biomes -
    // a multi-noise of temperature and humidity). Ours: two noises and five points of
    // our own around the wastes; basalt deltas a little rarer.
    const double t = m_temperature.noise2d(x, z) * 2.2, h = m_humidity.noise2d(x, z) * 2.2;
    struct Point {
        Biome biome;
        double t, h, offset;
    };
    static constexpr Point kPoints[] = {{Biome::NetherWastes, 0.0, 0.0, 0.0},
                                        {Biome::SoulSandValley, 0.0, -0.5, 0.0},
                                        {Biome::CrimsonForest, 0.4, 0.0, 0.0},
                                        {Biome::WarpedForest, 0.0, 0.5, 0.0},
                                        {Biome::BasaltDeltas, -0.5, 0.0, 0.03}};
    Biome best = Biome::NetherWastes;
    double bestD = 1e9;
    for (const Point& p : kPoints) {
        const double d = (t - p.t) * (t - p.t) + (h - p.h) * (h - p.h) + p.offset;
        if (d < bestD) {
            bestD = d;
            best = p.biome;
        }
    }
    return best;
}

namespace {

// Cavern density: positive is netherrack. Thick near the floor and the roof, open
// caverns between (wiki: The Nether - "large, open caverns"); our own constants.
double netherDensity(const OctaveNoise& main, const OctaveNoise& detail, double x, double y, double z) {
    double d = main.noise(x, y * 1.4, z) * 1.3 + detail.noise(x, y, z) * 0.25;
    if (y < 40.0) d += (40.0 - y) / 40.0 * 1.4;
    if (y > 88.0) d += (y - 88.0) / 36.0 * 2.0;
    return d - 0.12;
}

} // namespace

bool NetherGenerator::solidAt(int32_t x, int32_t y, int32_t z) const {
    if (y <= kFloor || y >= kRoof) return true;
    // The same cell interpolation as generate() (raw noise would disagree with it).
    const int32_t x0 = static_cast<int32_t>(std::floor(double(x) / kCellW)) * kCellW;
    const int32_t z0 = static_cast<int32_t>(std::floor(double(z) / kCellW)) * kCellW;
    const int32_t y0 = y / kCellH * kCellH;
    auto c = [&](int dx, int dy, int dz) {
        return netherDensity(m_main, m_detail, x0 + dx * kCellW, y0 + dy * kCellH, z0 + dz * kCellW);
    };
    const double fx = double(x - x0) / kCellW, fy = double(y - y0) / kCellH, fz = double(z - z0) / kCellW;
    const double d00 = std::lerp(c(0, 0, 0), c(1, 0, 0), fx), d10 = std::lerp(c(0, 1, 0), c(1, 1, 0), fx);
    const double d01 = std::lerp(c(0, 0, 1), c(1, 0, 1), fx), d11 = std::lerp(c(0, 1, 1), c(1, 1, 1), fx);
    return std::lerp(std::lerp(d00, d10, fy), std::lerp(d01, d11, fy), fz) > 0.0;
}

void NetherGenerator::generate(Chunk& out) const {
    const auto& r = blockRegistry();
    const BlockStateId netherrack = r.defaultState(blocks::Netherrack), lava = r.defaultState(blocks::Lava),
                       bedrock = r.defaultState(blocks::Bedrock), soulSand = r.defaultState(blocks::SoulSand),
                       gravel = r.defaultState(blocks::Gravel), glowstone = r.defaultState(blocks::Glowstone),
                       quartz = r.defaultState(blocks::NetherQuartzOre), gold = r.defaultState(blocks::NetherGoldOre),
                       magma = r.defaultState(blocks::MagmaBlock);
    const ChunkPos pos = out.pos();
    const int32_t baseX = pos.x * 16, baseZ = pos.z * 16;

    // 1. Density on 4x8x4 cells, interpolated per block (vanilla's cell layout).
    static thread_local std::array<double, (kCellsX + 1) * (kCellsY + 1) * (kCellsX + 1)> corners;
    auto ci = [](int cx, int cy, int cz) { return (cy * (kCellsX + 1) + cz) * (kCellsX + 1) + cx; };
    for (int cy = 0; cy <= kCellsY; ++cy)
        for (int cz = 0; cz <= kCellsX; ++cz)
            for (int cx = 0; cx <= kCellsX; ++cx)
                corners[size_t(ci(cx, cy, cz))] =
                    netherDensity(m_main, m_detail, baseX + cx * kCellW, cy * kCellH, baseZ + cz * kCellW);
    static thread_local std::array<BlockStateId, 16 * 16 * kNetherTop> blocks;
    auto at = [](int x, int y, int z) -> size_t { return size_t((y * 16 + z) * 16 + x); };
    for (int y = 0; y < kNetherTop; ++y) {
        const int cy = y / kCellH;
        const double fy = double(y % kCellH) / kCellH;
        for (int z = 0; z < 16; ++z) {
            const int cz = z / kCellW;
            const double fz = double(z % kCellW) / kCellW;
            for (int x = 0; x < 16; ++x) {
                const int cx = x / kCellW;
                const double fx = double(x % kCellW) / kCellW;
                auto c = [&](int dx, int dy, int dz) { return corners[size_t(ci(cx + dx, cy + dy, cz + dz))]; };
                const double d00 = std::lerp(c(0, 0, 0), c(1, 0, 0), fx), d10 = std::lerp(c(0, 1, 0), c(1, 1, 0), fx);
                const double d01 = std::lerp(c(0, 0, 1), c(1, 0, 1), fx), d11 = std::lerp(c(0, 1, 1), c(1, 1, 1), fx);
                const double d = std::lerp(std::lerp(d00, d10, fy), std::lerp(d01, d11, fy), fz);
                BlockStateId b = d > 0.0 ? netherrack : y <= kLavaLevel ? lava : 0;
                // Bedrock: solid at Y 0 and 127, thinning over 4 layers (wiki: Bedrock).
                if (y == kFloor || y == kRoof) b = bedrock;
                else if (y < 5 && positional(m_seed, baseX + x, y, baseZ + z, 0xBED0) < (5 - y) / 5.0) b = bedrock;
                else if (y > 122 && positional(m_seed, baseX + x, y, baseZ + z, 0xBED1) < (y - 122) / 5.0) b = bedrock;
                blocks[at(x, y, z)] = b;
            }
        }
    }

    // 2. Shores: soul sand and gravel on ground around the lava sea's level.
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const double n = m_shore.noise2d(baseX + x, baseZ + z);
            const BlockStateId cover = n > 0.35 ? soulSand : n < -0.4 ? gravel : 0;
            if (!cover) continue;
            for (int y = kLavaLevel - 2; y <= kLavaLevel + 4; ++y)
                if (blocks[at(x, y, z)] == netherrack && blocks[at(x, y + 1, z)] != netherrack) {
                    blocks[at(x, y, z)] = cover;
                    if (blocks[at(x, y - 1, z)] == netherrack) blocks[at(x, y - 1, z)] = cover;
                }
        }

    // 2b. nether2 (M19.1): biomes per 4x4 column and their ground.
    std::array<Biome, 16> columnBiome;
    columnBiome.fill(Biome::NetherWastes);
    if (m_version >= 2) {
        for (int q = 0; q < 16; ++q)
            columnBiome[size_t(q)] = biomeAt(baseX + (q & 3) * 4 + 2, baseZ + (q >> 2) * 4 + 2);
        const BlockStateId crimsonNylium = r.defaultState(blocks::CrimsonNylium),
                           warpedNylium = r.defaultState(blocks::WarpedNylium), soulSoil = r.defaultState(blocks::SoulSoil),
                           basalt = r.defaultState(blocks::Basalt), blackstone = r.defaultState(blocks::Blackstone);
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                const Biome b = columnBiome[size_t((z >> 2) * 4 + (x >> 2))];
                if (b == Biome::NetherWastes) continue;
                const double n = m_shore.noise2d((baseX + x) * 1.7, (baseZ + z) * 1.7);
                for (int y = 1; y < kNetherTop - 1; ++y) {
                    const BlockStateId cur = blocks[at(x, y, z)];
                    if (cur != netherrack && cur != soulSand && cur != gravel) continue;
                    const BlockStateId above = blocks[at(x, y + 1, z)];
                    const bool floor = above == 0, ceiling = blocks[at(x, y - 1, z)] == 0;
                    switch (b) {
                    case Biome::CrimsonForest: // nylium on floors (wiki: Crimson Forest)
                        if (floor) blocks[at(x, y, z)] = crimsonNylium;
                        break;
                    case Biome::WarpedForest:
                        if (floor) blocks[at(x, y, z)] = warpedNylium;
                        break;
                    case Biome::SoulSandValley: // soul sand and soul soil, a few deep, on every open face
                        if (floor || ceiling || positional(m_seed, baseX + x, y, baseZ + z, 0x5501) < 0.15) {
                            const BlockStateId s = n > 0.0 ? soulSand : soulSoil;
                            if (floor || ceiling) {
                                blocks[at(x, y, z)] = s;
                                if (floor && y > 1 && blocks[at(x, y - 1, z)] == netherrack) blocks[at(x, y - 1, z)] = s;
                            }
                        }
                        break;
                    case Biome::BasaltDeltas: // basalt with blackstone patches over everything open
                        if (floor || ceiling) blocks[at(x, y, z)] = n > 0.25 ? blackstone : basalt;
                        break;
                    default: break;
                    }
                }
            }
    }

    // 3. Ores and magma: blobs of netherrack replaced (wiki: Nether Quartz Ore - 16
    //    veins of up to 14 at Y 10-117; Nether Gold Ore - 10 of up to 10; Magma
    //    Block - 4 of up to 33 around Y 27-36). Clipped at the chunk border.
    Xoroshiro rng(chunkSeed(m_seed, pos.x, pos.z, 0x4E40));
    auto vein = [&](BlockStateId ore, int count, int size, int yLo, int yHi) {
        for (int v = 0; v < count; ++v) {
            const int vx = static_cast<int>(rng.nextInt(16)), vz = static_cast<int>(rng.nextInt(16));
            const int vy = yLo + static_cast<int>(rng.nextInt(uint32_t(yHi - yLo + 1)));
            const double radius = std::cbrt(double(size)) * 0.75;
            for (int i = 0; i < size; ++i) {
                const int x = vx + static_cast<int>(std::lround((rng.nextDouble() * 2 - 1) * radius));
                const int y = vy + static_cast<int>(std::lround((rng.nextDouble() * 2 - 1) * radius));
                const int z = vz + static_cast<int>(std::lround((rng.nextDouble() * 2 - 1) * radius));
                if (x < 0 || x > 15 || z < 0 || z > 15 || y < 1 || y >= kNetherTop - 1) continue;
                if (blocks[at(x, y, z)] == netherrack) blocks[at(x, y, z)] = ore;
            }
        }
    };
    vein(quartz, 16, 14, 10, 117);
    vein(gold, 10, 10, 10, 117);
    vein(magma, 4, 33, 27, 36);

    // 4. Glowstone clusters hanging from the ceiling (wiki: Glowstone › Generation:
    //    ~10 attempts per chunk, a blob grown down from netherrack above).
    for (int g = 0; g < 10; ++g) {
        const int gx = 2 + static_cast<int>(rng.nextInt(12)), gz = 2 + static_cast<int>(rng.nextInt(12));
        const int gy = 4 + static_cast<int>(rng.nextInt(118));
        if (blocks[at(gx, gy, gz)] != 0 || blocks[at(gx, gy + 1, gz)] != netherrack) continue;
        blocks[at(gx, gy, gz)] = glowstone;
        for (int i = 0; i < 300; ++i) {
            const int x = gx + static_cast<int>(rng.nextInt(7)) - 3, z = gz + static_cast<int>(rng.nextInt(7)) - 3;
            const int y = gy - static_cast<int>(rng.nextInt(8));
            if (x < 0 || x > 15 || z < 0 || z > 15 || y < 1 || blocks[at(x, y, z)] != 0) continue;
            int touching = 0;
            for (const auto& d : kDirectionNormals) {
                const int nx = x + d.x, ny = y + d.y, nz = z + d.z;
                if (nx >= 0 && nx < 16 && nz >= 0 && nz < 16 && ny >= 0 && ny < kNetherTop &&
                    blocks[at(nx, ny, nz)] == glowstone)
                    ++touching;
            }
            if (touching == 1) blocks[at(x, y, z)] = glowstone;
        }
    }

    // 4b. nether2 features (M19.1).
    if (m_version >= 2) netherFeatures(blocks.data(), pos, columnBiome);

    // 5. Write the sections (Y 0..127 = sections 0..7 of the Nether's 0..255; the rest
    //    stays air): the array
    //    is in section index order, so each section is assigned in one call.
    for (int s = 0; s < out.sectionCount(); ++s) {
        Section& section = out.mutableSection(s);
        const int y0 = out.height().minY + s * 16;
        if (y0 < 0 || y0 >= kNetherTop) section.fill(0);
        else section.assign(blocks.data() + at(0, y0, 0));
    }
    if (m_version < 2) {
        static const auto nether = uniformBiomes(Biome::NetherWastes);
        out.setBiomes(nether);
    } else {
        auto biomes = std::make_shared<ChunkBiomes>();
        for (int s = 0; s < out.sectionCount(); ++s)
            for (int qy = 0; qy < 4; ++qy)
                for (int q = 0; q < 16; ++q)
                    biomes->cells[size_t(ChunkBiomes::index(s, q & 3, qy, q >> 2))] = columnBiome[size_t(q)];
        out.setBiomes(biomes);
    }
}

void NetherGenerator::netherFeatures(BlockStateId* blocks, ChunkPos pos, const std::array<Biome, 16>& biomes) const {
    // Biome features (wiki: Crimson Forest, Warped Forest, Soul Sand Valley, Basalt
    // Deltas): huge fungi with wart caps, shroomlights and vines; roots, fungi and
    // sprouts on nylium; basalt pillars and bone fossils in soul sand valleys; lava
    // deltas with magma rims and basalt columns. Counts are ours; everything stays
    // inside the chunk (huge fungi have their stems 3+ blocks from its edges).
    const auto& r = blockRegistry();
    auto S = [&](BlockId b) { return r.defaultState(b); };
    auto at = [](int x, int y, int z) -> size_t { return size_t((y * 16 + z) * 16 + x); };
    auto get = [&](int x, int y, int z) -> BlockStateId {
        return x < 0 || x > 15 || z < 0 || z > 15 || y < 0 || y >= kNetherTop ? BlockStateId{1} : blocks[at(x, y, z)];
    };
    auto set = [&](int x, int y, int z, BlockStateId s) {
        if (x >= 0 && x <= 15 && z >= 0 && z <= 15 && y > 0 && y < kNetherTop - 1) blocks[at(x, y, z)] = s;
    };
    const BlockStateId crimsonNylium = S(blocks::CrimsonNylium), warpedNylium = S(blocks::WarpedNylium),
                       lava = S(blocks::Lava), magma = S(blocks::MagmaBlock), basalt = S(blocks::Basalt),
                       bone = S(blocks::BoneBlock), soulSand = S(blocks::SoulSand), soulSoil = S(blocks::SoulSoil);
    Xoroshiro rng(chunkSeed(m_seed, pos.x, pos.z, 0x4E60));
    // Floors: the first open cell above a solid block, scanning down from y.
    auto floorBelow = [&](int x, int y, int z) {
        for (; y > 1; --y)
            if (get(x, y, z) == 0 && get(x, y - 1, z) != 0 && get(x, y - 1, z) != lava) return y;
        return -1;
    };
    const Biome centre = biomes[5];
    // Huge fungi: a stem 4-13 tall, a wart cap hanging 2-4 down around its top with
    // shroomlights inside, weeping vines under crimson caps.
    if (centre == Biome::CrimsonForest || centre == Biome::WarpedForest) {
        const bool crimson = centre == Biome::CrimsonForest;
        const BlockStateId stem = S(crimson ? blocks::CrimsonStem : blocks::WarpedStem),
                           wart = S(crimson ? blocks::NetherWartBlock : blocks::WarpedWartBlock),
                           light = S(blocks::Shroomlight), nylium = crimson ? crimsonNylium : warpedNylium;
        for (int f = 0; f < 8; ++f) {
            const int x = 3 + static_cast<int>(rng.nextInt(10)), z = 3 + static_cast<int>(rng.nextInt(10));
            const int y = floorBelow(x, 32 + static_cast<int>(rng.nextInt(90)), z);
            const int height = 4 + static_cast<int>(rng.nextInt(10));
            if (y < 0 || get(x, y - 1, z) != nylium) continue;
            bool room = true;
            for (int k = 0; k < height + 2 && room; ++k)
                room = get(x, y + k, z) == 0;
            if (!room) continue;
            for (int k = 0; k < height; ++k)
                set(x, y + k, z, stem);
            const int top = y + height;
            for (int dy = -3; dy <= 0; ++dy) {
                const int rad = dy == 0 ? 1 : 2 + (dy <= -2 ? 1 : 0);
                for (int dx = -rad; dx <= rad; ++dx)
                    for (int dz = -rad; dz <= rad; ++dz) {
                        const bool edge = std::abs(dx) == rad || std::abs(dz) == rad || dy == 0;
                        if (!edge || get(x + dx, top + dy, z + dz) != 0) continue;
                        if (std::abs(dx) == rad && std::abs(dz) == rad && rng.nextInt(3) == 0) continue;
                        set(x + dx, top + dy, z + dz, rng.nextInt(12) == 0 ? light : wart);
                        if (crimson && dy == -3 && rng.nextInt(4) == 0) { // weeping vines down from the rim
                            const int len = 1 + static_cast<int>(rng.nextInt(6));
                            for (int v = 1; v <= len && get(x + dx, top + dy - v, z + dz) == 0; ++v)
                                set(x + dx, top + dy - v, z + dz,
                                    S(v == len || get(x + dx, top + dy - v - 1, z + dz) != 0 ? blocks::WeepingVines
                                                                                              : blocks::WeepingVinesPlant));
                        }
                    }
            }
            set(x, top, z, wart);
        }
    }
    // Ground plants on nylium; twisting vines up from warped floors.
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const Biome b = biomes[size_t((z >> 2) * 4 + (x >> 2))];
            if (b != Biome::CrimsonForest && b != Biome::WarpedForest) continue;
            for (int y = 2; y < kNetherTop - 2; ++y) {
                const BlockStateId g = get(x, y - 1, z);
                if (get(x, y, z) != 0 || (g != crimsonNylium && g != warpedNylium)) continue;
                const double roll = positional(m_seed, pos.x * 16 + x, y, pos.z * 16 + z, 0x4E61);
                if (g == crimsonNylium) {
                    if (roll < 0.20) set(x, y, z, S(blocks::CrimsonRoots));
                    else if (roll < 0.24) set(x, y, z, S(blocks::CrimsonFungus));
                    else if (roll < 0.25) set(x, y, z, S(blocks::WarpedFungus));
                } else {
                    if (roll < 0.12) set(x, y, z, S(blocks::WarpedRoots));
                    else if (roll < 0.22) set(x, y, z, S(blocks::NetherSprouts));
                    else if (roll < 0.25) set(x, y, z, S(blocks::WarpedFungus));
                    else if (roll < 0.27) { // twisting vines, 1-8 up
                        const int len = 1 + static_cast<int>(roll * 1000) % 8;
                        int k = 0;
                        while (k < len && get(x, y + k, z) == 0)
                            ++k;
                        for (int v = 0; v < k; ++v)
                            set(x, y + v, z, S(v == k - 1 ? blocks::TwistingVines : blocks::TwistingVinesPlant));
                    }
                }
            }
        }
    // Soul sand valleys: basalt pillars from floor to ceiling; bone fossils.
    if (centre == Biome::SoulSandValley) {
        for (int p = 0; p < 2; ++p) {
            const int x = 1 + static_cast<int>(rng.nextInt(14)), z = 1 + static_cast<int>(rng.nextInt(14));
            const int y = floorBelow(x, 40 + static_cast<int>(rng.nextInt(80)), z);
            if (y < 0) continue;
            int top = y;
            while (top < kNetherTop - 2 && get(x, top, z) == 0 && top - y < 40)
                ++top;
            if (get(x, top, z) == 0) continue; // no ceiling in reach
            for (int k = y; k < top; ++k) {
                set(x, k, z, basalt);
                if (rng.nextInt(3) == 0) set(x + 1, k, z, basalt);
                if (rng.nextInt(3) == 0) set(x, k, z + 1, basalt);
            }
        }
        if (rng.nextInt(3) == 0) { // a fossil: a bone spine with ribs, half buried
            const int x = 4 + static_cast<int>(rng.nextInt(8)), z = 4 + static_cast<int>(rng.nextInt(8));
            const int y = floorBelow(x, 30 + static_cast<int>(rng.nextInt(60)), z);
            if (y > 0) {
                const bool alongX = rng.nextInt(2) == 0;
                for (int k = -3; k <= 3; ++k) {
                    const int sx = alongX ? x + k : x, sz = alongX ? z : z + k;
                    set(sx, y, sz, bone);
                    if (k % 2 == 0)
                        for (int h = 1; h <= 3; ++h) {
                            set(alongX ? sx : sx - 1 - (h == 3 ? 1 : 0), y + h - (h == 3 ? 1 : 0),
                                alongX ? sz - 1 - (h == 3 ? 1 : 0) : sz, bone);
                            set(alongX ? sx : sx + 1 + (h == 3 ? 1 : 0), y + h - (h == 3 ? 1 : 0),
                                alongX ? sz + 1 + (h == 3 ? 1 : 0) : sz, bone);
                        }
                }
            }
        }
        (void)soulSand;
        (void)soulSoil;
    }
    // Basalt deltas: shallow lava pools rimmed with magma, basalt columns.
    if (centre == Biome::BasaltDeltas) {
        for (int d = 0; d < 6; ++d) {
            const int x = 2 + static_cast<int>(rng.nextInt(12)), z = 2 + static_cast<int>(rng.nextInt(12));
            const int y = floorBelow(x, 32 + static_cast<int>(rng.nextInt(80)), z);
            if (y < 0) continue;
            const int rad = 1 + static_cast<int>(rng.nextInt(2));
            for (int dx = -rad - 1; dx <= rad + 1; ++dx)
                for (int dz = -rad - 1; dz <= rad + 1; ++dz) {
                    const int g = y - 1;
                    if (get(x + dx, g, z + dz) == 0 || get(x + dx, g + 1, z + dz) != 0) continue;
                    const bool rim = std::abs(dx) > rad || std::abs(dz) > rad;
                    set(x + dx, g, z + dz, rim ? magma : lava);
                }
        }
        for (int c = 0; c < 8; ++c) {
            const int x = static_cast<int>(rng.nextInt(16)), z = static_cast<int>(rng.nextInt(16));
            const int y = floorBelow(x, 32 + static_cast<int>(rng.nextInt(80)), z);
            if (y < 0) continue;
            const int h = 1 + static_cast<int>(rng.nextInt(5));
            for (int k = 0; k < h && get(x, y + k, z) == 0; ++k)
                set(x, y + k, z, basalt);
        }
    }
}

glm::dvec3 NetherGenerator::findSpawn() const {
    // A spot on the ground above the lava sea, nearest the origin first.
    for (int radius = 0; radius <= 256; radius += 4)
        for (int i = -radius; i <= radius; i += 4)
            for (const auto& [x, z] : {std::pair{i, -radius}, std::pair{i, radius}, std::pair{-radius, i}, std::pair{radius, i}})
                for (int y = kLavaLevel + 2; y < 110; ++y)
                    if (solidAt(x, y - 1, z) && !solidAt(x, y, z) && !solidAt(x, y + 1, z) && y - 1 > kLavaLevel)
                        return {x + 0.5, double(y), z + 0.5};
    return {0.5, 64.0, 0.5};
}

// ---------------------------------------------------------------------------------
// The End

EndGenerator::EndGenerator(uint64_t seed) : m_seed(seed), m_edge(mixSeed(seed, 0xE1D), -5, 3) {
    // Ten obsidian pillars on a circle of radius 42 (wiki: Obsidian Pillar): radii
    // 2..5 and heights 76..103 in steps of 3, shuffled per seed.
    int order[kPillars];
    for (int i = 0; i < kPillars; ++i)
        order[i] = i;
    Xoroshiro rng(mixSeed(seed, 0xE1E));
    for (int i = kPillars - 1; i > 0; --i)
        std::swap(order[i], order[rng.nextInt(uint32_t(i + 1))]);
    for (int i = 0; i < kPillars; ++i) {
        const double a = 2.0 * (-std::numbers::pi + std::numbers::pi / 10.0 * i); // (-42,-1) is one spot
        m_pillars[i] = {static_cast<int>(std::floor(42.0 * std::cos(a))), static_cast<int>(std::floor(42.0 * std::sin(a))),
                        2 + order[i] / 3, 76 + order[i] * 3};
    }
}

int EndGenerator::islandTop(int32_t x, int32_t z) const {
    // The main island: about 170 blocks across, its top near Y 56-66 (wiki: The End).
    const double n = m_edge.noise2d(x, z);
    const double radius = 85.0 + 12.0 * n;
    const double r = std::sqrt(double(x) * x + double(z) * z);
    if (r >= radius) return -1;
    const double t = 1.0 - r / radius;
    return 58 + static_cast<int>(std::lround(5.0 * t + 2.0 * n));
}

int EndGenerator::islandBottom(int32_t x, int32_t z) const {
    const double r = std::sqrt(double(x) * x + double(z) * z);
    const double t = std::max(0.0, 1.0 - r / (85.0 + 12.0 * m_edge.noise2d(x, z)));
    return islandTop(x, z) - 2 - static_cast<int>(std::lround(46.0 * std::pow(t, 1.5)));
}

void EndGenerator::generate(Chunk& out) const {
    const auto& r = blockRegistry();
    const BlockStateId endStone = r.defaultState(blocks::EndStone), obsidian = r.defaultState(blocks::Obsidian),
                       bedrock = r.defaultState(blocks::Bedrock), portal = r.defaultState(blocks::EndPortal);
    const ChunkPos pos = out.pos();
    const int32_t baseX = pos.x * 16, baseZ = pos.z * 16;
    // Built in a flat array (section index order), then each section assigned once.
    static thread_local std::array<BlockStateId, 16 * 16 * kMaxHeight> blocks;
    blocks.fill(0);
    const HeightRange h = out.height();
    auto set = [h](int x, int y, int z, BlockStateId b) {
        if (h.contains(y)) blocks[size_t(((y - h.minY) * 16 + z) * 16 + x)] = b;
    };
    const int centreTop = islandTop(0, 0);
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const int32_t wx = baseX + x, wz = baseZ + z;
            const int top = islandTop(wx, wz);
            if (top >= 0)
                for (int y = std::max(1, islandBottom(wx, wz)); y <= top; ++y)
                    set(x, y, z, endStone);
            // Pillars.
            for (const Pillar& p : m_pillars) {
                const int64_t dx = int64_t(wx) - p.x, dz = int64_t(wz) - p.z; // 64-bit: far chunks overflow int
                if (dx * dx + dz * dz > int64_t(p.radius) * p.radius + 1) continue;
                for (int y = 0; y <= p.height; ++y) // down to Y 0, below the island too
                    set(x, y, z, obsidian);
                if (dx == 0 && dz == 0) set(x, p.height + 1, z, bedrock); // where the crystal sits
            }
            // The exit portal at the origin (wiki: Exit Portal): a bedrock bowl, the
            // portal inside, a bedrock column in the middle. Active (no dragon).
            const int64_t d2 = int64_t(wx) * wx + int64_t(wz) * wz;
            if (d2 <= 12) set(x, centreTop, z, bedrock);
            if (d2 <= 12) set(x, centreTop + 1, z, d2 <= 6 && d2 > 0 ? portal : bedrock);
            if (d2 == 0)
                for (int y = centreTop + 1; y <= centreTop + 4; ++y)
                    set(x, y, z, bedrock);
        }
    for (int s = 0; s < out.sectionCount(); ++s) {
        const BlockStateId* src = blocks.data() + size_t(s) * Section::kVolume;
        Section& section = out.mutableSection(s);
        if (std::all_of(src, src + Section::kVolume, [](BlockStateId b) { return b == 0; })) section.fill(0);
        else section.assign(src);
    }
    static const auto end = uniformBiomes(Biome::TheEnd);
    out.setBiomes(end);
}

} // namespace mc::world
