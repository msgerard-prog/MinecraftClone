#pragma once

#include "world/Biome.h"
#include "world/ChunkGenerator.h"
#include "world/Noise.h"

#include <array>
#include <cstdint>

namespace mc::world {

// The Nether (M12, wiki: The Nether › Generation, Nether Wastes): netherrack caverns
// between a bedrock floor (Y 0-4) and roof (Y 123-127), a lava sea up to Y 31, soul
// sand and gravel near the lava shore, glowstone clusters under the ceiling,
// nether quartz / nether gold ore and magma veins. Our own noise constants (vanilla's
// noise settings are game data, ADR 0004); one biome, nether_wastes.
class NetherGenerator final : public ChunkGenerator {
public:
    static constexpr int kLavaLevel = 31; // lava fills air at Y <= 31
    static constexpr int kFloor = 0, kRoof = 127;

    // 1 = "nether" (M12: nether wastes only), 2 = "nether2" (M19: five biomes).
    static constexpr int kNewest = 2;
    explicit NetherGenerator(uint64_t seed, int version = kNewest);

    void generate(Chunk& chunk) const override;
    // A free spot near the origin on solid ground above the lava sea.
    glm::dvec3 findSpawn() const override;
    std::string_view kind() const override { return m_version >= 2 ? "nether2" : "nether"; }
    int version() const { return m_version; }
    uint64_t seed() const override { return m_seed; }
    // Solid (netherrack) at a position before features (tests, portal placement).
    bool solidAt(int32_t x, int32_t y, int32_t z) const;

private:
    uint64_t m_seed;
    int m_version;
    OctaveNoise m_main;    // cavern shape
    OctaveNoise m_detail;  // small bumps
    OctaveNoise m_shore;   // soul sand vs gravel patches
    OctaveNoise m_temperature, m_humidity; // nether2 biomes

    void netherFeatures(BlockStateId* blocks, ChunkPos pos, const std::array<Biome, 16>& biomes) const;

public:
    // Nether structures (M19.3, NetherStructures.cpp): chests and spawners placed into
    // the block array, made block entities once the chunk is written; bastion mobs.
    struct Entity {
        int8_t x, z;
        int16_t y;
        bool chest; // else a blaze spawner
        uint8_t loot; // LootTable
    };
    struct Entities {
        int count = 0;
        std::array<Entity, 64> list{};
    };
    // The nether complex (fortress or bastion) of a region's candidate chunk, if any.
    enum class Complex : uint8_t { None, Fortress, Bastion };
    Complex complexAt(ChunkPos start) const;

private:
    void placeNetherStructures(BlockStateId* blocks, Chunk& out, ChunkPos pos, Entities& ents) const;

public:
    // nether2: the biome at a column (nether_wastes for "nether").
    Biome biomeAt(int32_t x, int32_t z) const;
};

// The End (M12, wiki: The End › Generation): the central end stone island, ten
// obsidian pillars around it, and the exit portal (active from the start: there is
// no ender dragon). No outer islands.
class EndGenerator final : public ChunkGenerator {
public:
    explicit EndGenerator(uint64_t seed);

    void generate(Chunk& chunk) const override;
    // Players arrive on the obsidian platform at (100, 49, 0) (wiki: The End).
    glm::dvec3 findSpawn() const override { return {100.5, 49.0, 0.5}; }
    std::string_view kind() const override { return "end"; }
    uint64_t seed() const override { return m_seed; }
    // Top of the island at a column (below 0: no island there).
    int islandTop(int32_t x, int32_t z) const;
    int islandBottom(int32_t x, int32_t z) const;

    struct Pillar {
        int x, z, radius, height;
    };
    static constexpr int kPillars = 10;
    const Pillar& pillar(int i) const { return m_pillars[i]; }

private:
    uint64_t m_seed;
    OctaveNoise m_edge;
    Pillar m_pillars[kPillars];
};

} // namespace mc::world
