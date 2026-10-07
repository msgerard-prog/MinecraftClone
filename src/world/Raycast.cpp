#include "world/Raycast.h"

#include "world/Blocks.h"

#include <cmath>
#include <limits>

namespace mc::world {

std::optional<RayHit> raycastBlocks(const World& world, const glm::dvec3& origin,
                                    const glm::dvec3& direction, double maxDistance, RayFluids fluids) {
    const double len = glm::length(direction);
    if (len == 0.0) return std::nullopt;
    const glm::dvec3 d = direction / len;
    const auto& reg = blockRegistry();

    glm::ivec3 cell(static_cast<int>(std::floor(origin.x)), static_cast<int>(std::floor(origin.y)),
                    static_cast<int>(std::floor(origin.z)));
    glm::ivec3 step;
    glm::dvec3 tMax, tDelta;
    constexpr double inf = std::numeric_limits<double>::infinity();
    for (int a = 0; a < 3; ++a) {
        step[a] = d[a] > 0 ? 1 : d[a] < 0 ? -1 : 0;
        tDelta[a] = step[a] != 0 ? 1.0 / std::abs(d[a]) : inf;
        const double boundary = step[a] > 0 ? cell[a] + 1.0 : cell[a];
        tMax[a] = step[a] != 0 ? (boundary - origin[a]) / d[a] : inf;
    }
    // Entering face per axis and step direction.
    const Direction enter[3][2] = {{Direction::East, Direction::West},
                                   {Direction::Up, Direction::Down},
                                   {Direction::South, Direction::North}};
    while (true) {
        const int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
        const double t = tMax[axis];
        if (t > maxDistance) return std::nullopt;
        cell[axis] += step[axis];
        tMax[axis] += tDelta[axis];
        const BlockPos p{cell.x, cell.y, cell.z};
        const HeightRange& h = world.height();
        if (!h.contains(p.y)) {
            // Outside the world vertically: nothing more to hit in that direction.
            if ((p.y < h.minY && step.y <= 0) || (p.y > h.maxY() && step.y >= 0)) return std::nullopt;
            continue;
        }
        const BlockStateId state = world.getBlock(p);
        if (state == 0) continue;
        const BlockId b = reg.blockOf(state);
        if (b == blocks::Water || b == blocks::Lava) {
            const bool source = reg.get(state, properties::level) == 0;
            if (fluids == RayFluids::Skip || !source) continue;
        }
        // Moving +axis enters through the block's negative face, and vice versa.
        return RayHit{p, enter[axis][step[axis] > 0 ? 1 : 0], t};
    }
}

} // namespace mc::world
