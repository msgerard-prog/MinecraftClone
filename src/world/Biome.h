#pragma once

#include <algorithm>
#include <cmath>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace mc::world {

// Biomes the overworld generator places (vanilla ids; wiki: Biome). Ids are dense
// uint8 values, runtime-only: saves store names.
enum class Biome : uint8_t {
    Plains,
    Forest,
    BirchForest,
    Taiga,
    SnowyTaiga,
    SnowyPlains,
    Desert,
    Savanna,
    Badlands,
    Swamp,
    Meadow,
    Grove,
    SnowySlopes,
    FrozenPeaks,
    JaggedPeaks,
    StonyPeaks,
    WindsweptHills,
    Beach,
    SnowyBeach,
    StonyShore,
    River,
    FrozenRiver,
    Ocean,
    DeepOcean,
    WarmOcean,
    LukewarmOcean,
    ColdOcean,
    FrozenOcean,
    Count
};

struct BiomeInfo {
    std::string_view id; // "minecraft:plains"
    float temperature;   // wiki: each biome's page (affects snow, colours)
    uint32_t grass;      // 0xRRGGBB, wiki biome pages
    uint32_t foliage;
    uint32_t water;
};

const BiomeInfo& biomeInfo(Biome b);
std::optional<Biome> findBiome(std::string_view id); // with or without "minecraft:"

// Palette slots used for fixed-colour foliage (not biomes): birch and spruce leaves
// ignore the biome (wiki: Leaves - #80A755 and #619961).
inline constexpr uint8_t kBirchFoliageSlot = 254;
inline constexpr uint8_t kSpruceFoliageSlot = 255;
// Redstone dust colour per power level 0..15 (slots 238..253, "foliage" channel).
inline constexpr uint8_t kRedstoneSlot0 = 238;

// Dust colour at a power level, 0xRRGGBB (wiki: Redstone Dust › Appearance: red
// rises with power from 0.3; green and blue only near full power).
inline uint32_t redstoneColor(int power) {
    const float f = static_cast<float>(power) / 15.0f;
    const float r = f * 0.6f + (power > 0 ? 0.4f : 0.3f);
    const float g = std::clamp(f * f * 0.7f - 0.5f, 0.0f, 1.0f);
    const float b = std::clamp(f * f * 0.6f - 0.7f, 0.0f, 1.0f);
    auto c = [](float v) { return static_cast<uint32_t>(std::lround(v * 255.0f)); };
    return c(r) << 16 | c(g) << 8 | c(b);
}

// Biomes of one chunk at vanilla's resolution: one per 4x4x4 cell, 64 per section,
// index section * 64 + (qy * 4 + qz) * 4 + qx. Immutable once generated, shared
// read-only with mesh workers.
struct ChunkBiomes {
    static constexpr int kPerSection = 64;
    std::array<Biome, 24 * kPerSection> cells{};

    static int index(int section, int qx, int qy, int qz) {
        return section * kPerSection + (qy * 4 + qz) * 4 + qx;
    }
    // Local block x/z (0..15), world y.
    Biome at(int x, int y, int z) const;
};

} // namespace mc::world
