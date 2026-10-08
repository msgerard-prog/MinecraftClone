#pragma once

#include "world/Biome.h"
#include "world/ChunkGenerator.h"
#include "world/Loot.h"
#include "world/Noise.h"

#include <array>
#include <cstdint>

namespace mc::world {

// Vanilla-style (1.18+) overworld (M8). The pipeline follows what the wiki documents
// (World generation, Noise router, Biome, Ore, Tree): climate noises -> a terrain
// density shaped by continentalness / erosion / peaks-and-valleys, evaluated on 4x8x4
// cells and interpolated; cheese / spaghetti / noodle caves; sea level 63, lava below
// -54; multi-noise style biome choice; per-biome surface rules; ores, trees and
// plants. The constants (spline points, noise scales, thresholds, biome table) are our
// own tuning: vanilla's noise settings and biome parameter tables are game data we
// don't read (ADR 0004), so terrain looks vanilla-like, not identical for a seed.
//
// Deterministic and thread-safe (hard rule 3). Trees from the 8 neighbouring chunks
// are placed too (their ground height comes from the same interpolated density), so
// trees cross chunk borders seamlessly.
class OverworldGenerator final : public ChunkGenerator {
public:
    static constexpr int kSeaLevel = 63; // water fills y <= 62
    static constexpr int kLavaLevel = -54; // caves below this hold lava (wiki: Lava)

    // Generator generations (data-formats: generator kinds). Each world keeps the one
    // it was created with, so its chunks never change; new worlds get the newest.
    // 1 = "overworld" (M8), 2 = "overworld2" (M18: + lava lakes, springs, ravines,
    // sugar cane, pumpkins, cacti, mushrooms), 3 = "overworld3" (M24: beds, job sites,
    // a bell and villagers in villages).
    static constexpr int kNewest = 3;
    explicit OverworldGenerator(uint64_t seed, int version = kNewest);

    void generate(Chunk& chunk) const override;
    glm::dvec3 findSpawn() const override;
    // The nearest stronghold's staircase chunk corner (x, z), overworld2 only.
    std::optional<glm::ivec2> nearestStronghold(double x, double z) const override;
    std::string_view kind() const override {
        return m_version >= 3 ? "overworld3" : m_version == 2 ? "overworld2" : "overworld";
    }
    int version() const { return m_version; }
    uint64_t seed() const override { return m_seed; }

    // Climate and shape at a column (public for tests and spawn search).
    struct Column {
        double continentalness, erosion, weirdness, peaksValleys, temperature, humidity;
        double height;    // target surface y
        double scale;     // blocks per density unit (steeper terrain: larger)
        double roughness; // 3D noise weight (overhangs in mountains)
        double mountain;  // 0..1
    };
    Column column(int32_t x, int32_t z) const;
    Biome biomeAt(const Column& c) const;
    Biome baseBiome(const Column& c) const; // the M8 choice (overworld2 refines it)
    // Highest solid y of the interpolated terrain (caves ignored) at a column.
    int surfaceY(int32_t x, int32_t z) const;

    // Ravines (vanilla's canyon carver): a worm of tall ellipsoids starting in 1 of 100
    // chunks, up to ~112 blocks long, so each chunk carves the ravines of the chunks
    // within kRavineReach of it. Steps are a pure function of the start chunk.
    struct RavineStep {
        float x, y, z; // centre (world)
        float h, v;    // horizontal and vertical radius
    };
    static constexpr int kRavineSteps = 112, kRavineReach = 9; // (start offset 15 + 112 steps + radius < 9 chunks)
    struct Ravine {
        int count = 0;
        std::array<RavineStep, kRavineSteps> steps{};
        std::array<float, kOverworldHeight.height> rough{}; // wall roughness per y
    };
    // Chests and spawners placed by features and structures; they become block
    // entities once the chunk is encoded.
    struct GeneratedEntity {
        int8_t x, z;
        int16_t y;
        bool chest;
        MobType mob;
        LootTable loot = LootTable::SimpleDungeon;
        bool furnace = false; // (instead of a chest or spawner: an empty furnace)
        bool villager = false; // (M24.1: a villager standing here, of `villagerType`)
        uint8_t villagerType = 0;
        bool nitwit = false;
    };
    struct GeneratedEntities {
        int count = 0;
        std::array<GeneratedEntity, 128> list{};
        bool full() const { return count >= int(list.size()); }
    };
    // The ravine starting in a chunk (false if none); whether a block lies in one.
    bool ravine(int32_t cx, int32_t cz, Ravine& out) const;
    bool inRavine(int32_t x, int32_t y, int32_t z) const;

private:
    double terrainDensity(int32_t x, int32_t y, int32_t z, const Column& c) const;
    double caveDensity(int32_t x, int32_t y, int32_t z, const Column& c) const;
    struct TreePlan {
        struct Tree {
            int32_t wx, wz;
            int ground;
            uint8_t kind, height;
        };
        int count = 0;
        std::array<Tree, 10> trees{};
    };
    const TreePlan& treePlan(int32_t cx, int32_t cz) const;
    bool groundCarved(int32_t x, int32_t y, int32_t z) const;
    void placeTrees(BlockStateId* blocks, ChunkPos pos, int32_t cx, int32_t cz) const;

    // --- Overworld 2 features (M18.1) ---
    void carveRavines(BlockStateId* blocks, int32_t cx, int32_t cz, std::array<int, 256>& topY) const;
    void placeLavaLakes(BlockStateId* blocks, int32_t cx, int32_t cz, const std::array<int, 256>& topY) const;
    void placeSprings(BlockStateId* blocks, Chunk& out, int32_t cx, int32_t cz, int maxTop) const;
    void placeDungeons(BlockStateId* blocks, int32_t cx, int32_t cz, int maxTop, GeneratedEntities& out) const;
    // Surface structures (M18.4): desert pyramids, jungle temples, igloos, swamp huts
    // from the random-spread grids; each chunk builds the parts of the structures
    // whose start lies within 2 chunks of it.
    void placeStructures(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const;
    // Mineshafts (M18.5; wiki: Mineshaft): a start room and a tree of corridors,
    // crossings and stairs reaching up to 80 blocks out; each chunk builds the pieces
    // that cross it.
    void placeMineshafts(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const;
    // Strongholds (M18.5; wiki: Stronghold): 128 in 8 rings around the origin, each a
    // tree of stone brick rooms from a spiral staircase, with one end portal room.
    void placeStrongholds(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const;
    // Villages (M18.5; wiki: Village): a well, houses and farms around it joined by
    // dirt paths, in the biome's materials (no villagers yet).
    void placeVillages(BlockStateId* blocks, int32_t cx, int32_t cz, const std::array<int, 256>& topY,
                       GeneratedEntities& out) const;
    void placeVegetation(BlockStateId* blocks, int32_t cx, int32_t cz, const std::array<int, 256>& topY,
                         const std::array<Biome, 16>& biomes) const;

    uint64_t m_seed;
    int m_version;
    std::array<ChunkPos, 128> m_strongholds{}; // staircase chunks (overworld2)
    int m_strongholdCount = 0;
    OctaveNoise m_continentalness[2], m_erosion[2], m_weirdness[2], m_temperature[2],
        m_humidity[2];
    OctaveNoise m_terrain3d;  // overhangs / roughness
    OctaveNoise m_jagged;     // peak jaggedness
    OctaveNoise m_cheese[2];  // large caves
    OctaveNoise m_spaghettiA, m_spaghettiB, m_spaghettiWidth;
    OctaveNoise m_noodleA, m_noodleB;
    OctaveNoise m_surface;    // surface depth / patches
};

} // namespace mc::world
