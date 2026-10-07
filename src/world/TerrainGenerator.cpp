#include "world/TerrainGenerator.h"

#include "world/Blocks.h"
#include "world/Random.h"

#include <algorithm>
#include <array>
#include <cmath>

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

} // namespace

TerrainGenerator::TerrainGenerator(uint64_t seed)
    : m_seed(seed), m_continents(mixSeed(seed, 1), -9, 4), m_hills(mixSeed(seed, 2), -7, 4),
      m_detail(mixSeed(seed, 3), -5, 3), m_beach(mixSeed(seed, 4), -6, 2) {}

int TerrainGenerator::surfaceHeight(int32_t x, int32_t z) const {
    const double c = m_continents.noise2d(x, z) * 1.6;   // ~[-1, 1]
    const double land = std::clamp(c + 0.15, -1.0, 1.0); // a bit more land than sea
    const double hills = m_hills.noise2d(x, z);
    const double detail = m_detail.noise2d(x, z);
    // Oceans sink to ~y 40, plains sit just above sea level, hills rise inland.
    double h = kSeaLevel + land * 22.0;
    h += hills * 14.0 * std::clamp(land + 0.4, 0.0, 1.4);
    h += detail * 3.0;
    return static_cast<int>(std::floor(h));
}

void TerrainGenerator::generate(Chunk& chunk) const {
    const auto& r = blockRegistry();
    const BlockStateId air = 0;
    const BlockStateId stone = r.defaultState(blocks::Stone);
    const BlockStateId deepslate = r.defaultState(blocks::Deepslate);
    const BlockStateId dirt = r.defaultState(blocks::Dirt);
    const BlockStateId grass = r.defaultState(blocks::GrassBlock);
    const BlockStateId sand = r.defaultState(blocks::Sand);
    const BlockStateId gravel = r.defaultState(blocks::Gravel);
    const BlockStateId water = r.defaultState(blocks::Water);
    const BlockStateId bedrock = r.defaultState(blocks::Bedrock);

    const int32_t baseX = chunk.pos().x * 16;
    const int32_t baseZ = chunk.pos().z * 16;

    // Column data first (height + surface materials), then fill section by section.
    std::array<int, 256> height{};
    std::array<BlockStateId, 256> top{};
    std::array<BlockStateId, 256> under{};
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            const int32_t wx = baseX + x, wz = baseZ + z;
            const int h = surfaceHeight(wx, wz);
            const int i = z * 16 + x;
            height[i] = h;
            if (h < kSeaLevel - 1) { // sea floor
                const bool gravelly = m_beach.noise2d(wx, wz) > 0.15 || h < kSeaLevel - 12;
                top[i] = under[i] = gravelly ? gravel : sand;
            } else if (h <= kSeaLevel) { // shore: a narrow band at the waterline
                const bool gravelly = m_beach.noise2d(wx, wz) > 0.35;
                top[i] = under[i] = gravelly ? gravel : sand;
            } else {
                top[i] = grass;
                under[i] = dirt;
            }
        }
    }

    static thread_local std::array<BlockStateId, Section::kVolume> buffer;
    for (int s = 0; s < kSectionsPerChunk; ++s) {
        const int baseY = kMinY + s * 16;
        bool any = false;
        for (int ly = 0; ly < 16; ++ly) {
            const int y = baseY + ly;
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    const int i = z * 16 + x;
                    const int h = height[i];
                    BlockStateId b = air;
                    if (y == kMinY) {
                        b = bedrock;
                    } else if (y <= kMinY + 4 && positional(m_seed, baseX + x, y, baseZ + z, 10) <
                                                     (kMinY + 5 - y) / 5.0) {
                        b = bedrock; // vanilla: bedrock thins out over y -63..-60
                    } else if (y <= h - 4) {
                        // Deepslate below ~0, with a ragged transition up to y 8.
                        const bool deep =
                            y < kDeepslateY ||
                            (y < kDeepslateY + 8 && positional(m_seed, baseX + x, y, baseZ + z,
                                                               11) < (kDeepslateY + 8 - y) / 8.0);
                        b = deep ? deepslate : stone;
                    } else if (y < h) {
                        b = under[i];
                    } else if (y == h) {
                        b = top[i];
                    } else if (y < kSeaLevel) {
                        b = water;
                    }
                    buffer[Section::index(x, ly, z)] = b;
                    any |= b != air;
                }
            }
        }
        if (any) {
            chunk.section(s).assign(buffer.data());
        } else {
            chunk.section(s).fill(air);
        }
    }
}

} // namespace mc::world
