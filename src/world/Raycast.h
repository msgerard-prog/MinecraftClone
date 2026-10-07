#pragma once

#include "world/Chunk.h"
#include "world/Direction.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <optional>

namespace mc::world {

struct RayHit {
    BlockPos block;  // the block hit
    Direction face;  // its face the ray entered through (place against this face)
    double distance; // from the ray origin, blocks
};

// Vanilla reach (wiki: Attribute, block_interaction_range since 1.20.5).
inline constexpr double kSurvivalReach = 4.5;
inline constexpr double kCreativeReach = 5.0;

// What a ray does with fluids (vanilla ClipContext.Fluid): passes through them, or
// stops at fluid sources (buckets).
enum class RayFluids { Skip, Sources };

// First targetable block along a ray (voxel walk, Amanatides & Woo 1987). Like
// vanilla's default block raycast it skips air and fluids. The block the origin is
// inside is ignored. `direction` need not be normalised.
std::optional<RayHit> raycastBlocks(const World& world, const glm::dvec3& origin,
                                    const glm::dvec3& direction, double maxDistance,
                                    RayFluids fluids = RayFluids::Skip);

// The block position next to `pos` across `face` (where a placed block goes).
constexpr BlockPos neighbour(const BlockPos& pos, Direction face) {
    const glm::ivec3 n = normal(face);
    return {pos.x + n.x, pos.y + n.y, pos.z + n.z};
}

} // namespace mc::world
