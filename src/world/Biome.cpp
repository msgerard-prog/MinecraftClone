#include "world/Biome.h"

#include "world/Coords.h"

#include <algorithm>
#include <cmath>

namespace mc::world {

namespace {

// Temperature and colours as listed on each biome's minecraft.wiki page (Java).
constexpr BiomeInfo kBiomes[] = {
    {"minecraft:plains", 0.8f, 0x91BD59, 0x77AB2F, 0x3F76E4},
    {"minecraft:forest", 0.7f, 0x79C05A, 0x59AE30, 0x3F76E4},
    {"minecraft:birch_forest", 0.6f, 0x88BB67, 0x6BA941, 0x3F76E4},
    {"minecraft:taiga", 0.25f, 0x86B783, 0x68A464, 0x3F76E4},
    {"minecraft:snowy_taiga", -0.5f, 0x80B497, 0x60A17B, 0x3D57D6},
    {"minecraft:snowy_plains", 0.0f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:desert", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4},
    {"minecraft:savanna", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4},
    {"minecraft:badlands", 2.0f, 0x90814D, 0x9E814D, 0x3F76E4},
    {"minecraft:swamp", 0.8f, 0x6A7039, 0x6A7039, 0x617B64},
    {"minecraft:meadow", 0.5f, 0x83BB6D, 0x63A948, 0x0E4ECF},
    {"minecraft:grove", -0.2f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:snowy_slopes", -0.3f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:frozen_peaks", -0.7f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:jagged_peaks", -0.7f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:stony_peaks", 1.0f, 0x9ABE4B, 0x82AC1E, 0x3F76E4},
    {"minecraft:windswept_hills", 0.2f, 0x8AB689, 0x6DA36B, 0x3F76E4},
    {"minecraft:beach", 0.8f, 0x91BD59, 0x77AB2F, 0x3F76E4},
    {"minecraft:snowy_beach", 0.05f, 0x83B593, 0x64A278, 0x3D57D6},
    {"minecraft:stony_shore", 0.2f, 0x8AB689, 0x6DA36B, 0x3F76E4},
    {"minecraft:river", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:frozen_river", 0.0f, 0x80B497, 0x60A17B, 0x3938C9},
    {"minecraft:ocean", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:deep_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:warm_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x43D5EE},
    {"minecraft:lukewarm_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x45ADF2},
    {"minecraft:cold_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x3D57D6},
    {"minecraft:frozen_ocean", 0.0f, 0x80B497, 0x60A17B, 0x3938C9},
    {"minecraft:nether_wastes", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x330808},
    {"minecraft:the_end", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:jungle", 0.95f, 0x59C93C, 0x30BB0B, 0x3F76E4},
    {"minecraft:sparse_jungle", 0.95f, 0x64C73F, 0x3EB80F, 0x3F76E4},
    {"minecraft:dark_forest", 0.7f, 0x507A32, 0x59AE30, 0x3F76E4},
    {"minecraft:flower_forest", 0.7f, 0x79C05A, 0x59AE30, 0x3F76E4},
    {"minecraft:old_growth_spruce_taiga", 0.25f, 0x86B783, 0x68A464, 0x3F76E4},
    {"minecraft:cherry_grove", 0.5f, 0xB6DB61, 0xB6DB61, 0x5DB7EF},
    {"minecraft:ice_spikes", 0.0f, 0x80B497, 0x60A17B, 0x3F76E4},
    {"minecraft:mushroom_fields", 0.9f, 0x55C93F, 0x2BBB0F, 0x3F76E4},
    {"minecraft:wooded_badlands", 2.0f, 0x90814D, 0x9E814D, 0x3F76E4},
    {"minecraft:eroded_badlands", 2.0f, 0x90814D, 0x9E814D, 0x3F76E4},
    // Nether biomes: fog colours from their wiki pages.
    {"minecraft:crimson_forest", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x330303},
    {"minecraft:warped_forest", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x1A051A},
    {"minecraft:soul_sand_valley", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x1B4745},
    {"minecraft:basalt_deltas", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x685F70},
    {"minecraft:end_highlands", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:end_midlands", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:small_end_islands", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:end_barrens", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:deep_lukewarm_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x45ADF2},
    {"minecraft:deep_cold_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x3D57D6},
    {"minecraft:deep_frozen_ocean", 0.5f, 0x8EB971, 0x71A74D, 0x3938C9}, // (wiki: 0.5, frozen by its surface rule)
    // Overworld 6 (M27.1).
    {"minecraft:sunflower_plains", 0.8f, 0x91BD59, 0x77AB2F, 0x3F76E4},
    {"minecraft:old_growth_birch_forest", 0.6f, 0x88BB67, 0x6BA941, 0x3F76E4},
    {"minecraft:old_growth_pine_taiga", 0.3f, 0x86B87F, 0x68A55F, 0x3F76E4},
    {"minecraft:savanna_plateau", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4},
    {"minecraft:windswept_savanna", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4},
    {"minecraft:windswept_forest", 0.2f, 0x8AB689, 0x6DA36B, 0x3F76E4},
    {"minecraft:windswept_gravelly_hills", 0.2f, 0x8AB689, 0x6DA36B, 0x3F76E4},
    {"minecraft:bamboo_jungle", 0.95f, 0x59C93C, 0x30BB0B, 0x3F76E4},
    {"minecraft:mangrove_swamp", 0.8f, 0x6A7039, 0x8DB127, 0x3A7A6A},
    {"minecraft:pale_garden", 0.7f, 0x778272, 0x878D76, 0x76889D, 0x817770, 0xB9B9B9}, // (grey sky and fog)
    {"minecraft:lush_caves", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
    {"minecraft:dripstone_caves", 0.8f, 0x91BD59, 0x77AB2F, 0x3F76E4},
    {"minecraft:deep_dark", 0.8f, 0x91BD59, 0x77AB2F, 0x3F76E4},
};
static_assert(std::size(kBiomes) == static_cast<size_t>(Biome::Count));

} // namespace

const BiomeInfo& biomeInfo(Biome b) { return kBiomes[static_cast<size_t>(b)]; }

std::optional<Biome> findBiome(std::string_view id) {
    for (size_t i = 0; i < std::size(kBiomes); ++i) {
        const std::string_view name = kBiomes[i].id;
        if (name == id || name.substr(10) == id) return static_cast<Biome>(i);
    }
    return std::nullopt;
}

Biome ChunkBiomes::at(int x, int y, int z, const HeightRange& h) const {
    const int s = std::clamp(h.sectionIndex(std::clamp(y, h.minY, h.maxY())), 0, h.sections() - 1);
    return cells[size_t(index(s, x >> 2, blockToLocal(y) >> 2, z >> 2))];
}

uint32_t skyColorFor(float temperature) {
    const float t = std::clamp(temperature / 3.0f, -1.0f, 1.0f);
    float h = 0.62222f - t * 0.05f;
    h -= std::floor(h);
    const float s = 0.5f + t * 0.1f, v = 1.0f;
    // HSV to RGB.
    const float hh = h * 6.0f;
    const int sector = int(hh) % 6;
    const float f = hh - std::floor(hh);
    const float p = v * (1 - s), q = v * (1 - s * f), u = v * (1 - s * (1 - f));
    float r = 0, g = 0, b = 0;
    switch (sector) {
    case 0: r = v, g = u, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = u; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = u, g = p, b = v; break;
    default: r = v, g = p, b = q; break;
    }
    auto c = [](float x) { return uint32_t(std::clamp(int(x * 255.0f), 0, 255)); };
    return c(r) << 16 | c(g) << 8 | c(b);
}

int farmVariant(Biome b) {
    switch (b) {
    case Biome::Desert: case Biome::Savanna: case Biome::SavannaPlateau: case Biome::WindsweptSavanna:
    case Biome::Badlands: case Biome::WoodedBadlands: case Biome::ErodedBadlands: case Biome::Jungle:
    case Biome::SparseJungle: case Biome::BambooJungle: case Biome::MangroveSwamp: case Biome::WarmOcean:
    case Biome::NetherWastes: case Biome::CrimsonForest: case Biome::WarpedForest: case Biome::SoulSandValley:
    case Biome::BasaltDeltas:
        return 1;
    default:
        return biomeInfo(b).temperature <= 0.25f ? 2 : 0;
    }
}

} // namespace mc::world
