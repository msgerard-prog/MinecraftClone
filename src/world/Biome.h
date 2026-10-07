#pragma once

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
