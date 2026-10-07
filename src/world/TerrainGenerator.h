#pragma once

#include "world/ChunkGenerator.h"
#include "world/Noise.h"

#include <cstdint>

namespace mc::world {

// Placeholder overworld generator (M3) until vanilla's noise router in M8: a 2D
// heightmap from octave noise, then vanilla-like surface rules. Deterministic: the
// result depends only on (seed, chunk position), never on thread or call order
// (hard rule 3), so chunks can be generated on any worker in any order.
class TerrainGenerator final : public ChunkGenerator {
public:
    static constexpr int kSeaLevel = 63;  // vanilla: water fills up to y = 62
    static constexpr int kDeepslateY = 0; // vanilla: deepslate below ~y 0, ragged to y 8

    explicit TerrainGenerator(uint64_t seed);

    // Y of the highest solid block at a world column.
    int surfaceHeight(int32_t x, int32_t z) const;
    void generate(Chunk& chunk) const override;
    // Nearest land column to the origin (spiral search), feet on its surface.
    glm::dvec3 findSpawn() const override;
    std::string_view kind() const override { return "terrain"; } // the M3 placeholder
    uint64_t seed() const override { return m_seed; }

private:
    uint64_t m_seed;
    OctaveNoise m_continents; // ocean vs land, very low frequency
    OctaveNoise m_hills;      // rolling hills
    OctaveNoise m_detail;     // small bumps
    OctaveNoise m_beach;      // where shores get gravel instead of sand
};

} // namespace mc::world
