#pragma once

#include "world/Chunk.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// A world generator: fills a chunk from (seed, position) alone. Implementations
// must be deterministic and thread-safe (hard rule 3): chunk-loader workers call
// generate() concurrently, in any order.
class ChunkGenerator {
public:
    virtual ~ChunkGenerator() = default;
    virtual void generate(Chunk& chunk) const = 0;
    // Where a new player appears (feet position).
    virtual glm::dvec3 findSpawn() const = 0;
    // Saved in level.dat ("MinecraftClone.generator") so a world keeps its generator.
    virtual std::string_view kind() const = 0;
    virtual uint64_t seed() const = 0;
    // Where eyes of ender lead (M18.5): the nearest stronghold's (x, z), if this
    // generator places strongholds.
    virtual std::optional<glm::ivec2> nearestStronghold(double, double) const { return std::nullopt; }
};

} // namespace mc::world
