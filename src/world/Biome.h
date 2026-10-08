#pragma once

#include "world/Coords.h"

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
    // Other dimensions (M12).
    NetherWastes,
    TheEnd,
    // Overworld 2 (M18.2; only the "overworld2" generator places these).
    Jungle,
    SparseJungle,
    DarkForest,
    FlowerForest,
    OldGrowthSpruceTaiga,
    CherryGrove,
    IceSpikes,
    MushroomFields,
    WoodedBadlands,
    ErodedBadlands,
    // Nether 2 (M19.1; the "nether2" generator).
    CrimsonForest,
    WarpedForest,
    SoulSandValley,
    BasaltDeltas,
    // The End 2 (M20.1; the "end2" generator).
    EndHighlands,
    EndMidlands,
    SmallEndIslands,
    EndBarrens,
    // Overworld 4 (M25.1; the "overworld4" generator): the deep ocean variants.
    DeepLukewarmOcean,
    DeepColdOcean,
    DeepFrozenOcean,
    Count
};

struct BiomeInfo {
    std::string_view id; // "minecraft:plains"
    float temperature;   // wiki: each biome's page (affects snow, colours)
    uint32_t grass;      // 0xRRGGBB, wiki biome pages
    uint32_t foliage;
    uint32_t water;
    uint32_t fog = 0; // Nether biomes: their fog colour (0: the dimension's default)
};

const BiomeInfo& biomeInfo(Biome b);
// Under-water fog colour (wiki: biome pages, water fog colour): warm oceans #041F33,
// lukewarm #041633, swamps #232317, everywhere else #050533. 0xRRGGBB.
inline uint32_t waterFogColor(Biome b) {
    switch (b) {
    case Biome::WarmOcean: return 0x041F33;
    case Biome::LukewarmOcean:
    case Biome::DeepLukewarmOcean: return 0x041633;
    case Biome::Swamp: return 0x232317;
    default: return 0x050533;
    }
}

// The sky colour of a biome from its temperature (wiki: Sky; vanilla's formula, which
// gives plains #78A7FF): hue 0.62222 - t x 0.05, saturation 0.5 + t x 0.1, value 1,
// t = temperature / 3 clamped to -1..1 - warm biomes get a paler, cyan sky. 0xRRGGBB.
uint32_t skyColorFor(float temperature);
// The Overworld biomes' fog colour (wiki: Fog; #C0D8FF in every Overworld biome).
inline constexpr uint32_t kOverworldFog = 0xC0D8FF;
std::optional<Biome> findBiome(std::string_view id); // with or without "minecraft:"

// Palette slots used for fixed-colour foliage (not biomes): birch and spruce leaves
// ignore the biome (wiki: Leaves - #80A755 and #619961).
inline constexpr uint8_t kBirchFoliageSlot = 254;
inline constexpr uint8_t kSpruceFoliageSlot = 255;
// Redstone dust colour per power level 0..15 (slots 238..253, "foliage" channel).
inline constexpr uint8_t kRedstoneSlot0 = 238;

// Dust colour at a power level, 0xRRGGBB. The wiki only says it goes from dark red
// at 0 to bright red at 15 (Redstone Dust › Appearance); this curve is our estimate
// of the in-game look (red from 0.3 up, a little green/blue near full power).
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
    std::array<Biome, kMaxSections * kPerSection> cells{}; // by section index (bottom = 0)

    static int index(int section, int qx, int qy, int qz) {
        return section * kPerSection + (qy * 4 + qz) * 4 + qx;
    }
    // Local block x/z (0..15), world y.
    Biome at(int x, int y, int z, const HeightRange& height = kOverworldHeight) const; // world y
};

} // namespace mc::world
