#include "world/Biome.h"

#include "world/Coords.h"

#include <algorithm>

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
    {"minecraft:nether_wastes", 2.0f, 0xBFB755, 0xAEA42A, 0x3F76E4},
    {"minecraft:the_end", 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4},
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

} // namespace mc::world
