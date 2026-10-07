#pragma once

#include "world/Biome.h"
#include "world/ChunkGenerator.h"
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

    explicit OverworldGenerator(uint64_t seed);

    void generate(Chunk& chunk) const override;
    glm::dvec3 findSpawn() const override;
    std::string_view kind() const override { return "overworld"; }
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
    // Highest solid y of the interpolated terrain (caves ignored) at a column.
    int surfaceY(int32_t x, int32_t z) const;

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

    uint64_t m_seed;
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
