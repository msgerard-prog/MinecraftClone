#include "world/Raycast.h"

#include "world/BlockShapes.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mc::world {

std::optional<RayHit> raycastBlocks(const World& world, const glm::dvec3& origin,
                                    const glm::dvec3& direction, double maxDistance, RayFluids fluids, bool targetLight) {
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
        if (b == blocks::BubbleColumn && fluids == RayFluids::Skip) continue; // (M29.5: a water source)
        if (b == blocks::Light && !targetLight) continue; // (M29 review: invisible unless held)
        // Shaped blocks (slabs, stairs, doors...: M23.1) are hit only on their boxes
        // (vanilla's outline shapes; boxes taller than the cell count up to its top).
        if (reg.collides(state)) {
            const BlockShape& shape = collisionShape(state);
            if (shape.count > 0) {
                double best = inf;
                int bestAxis = axis, bestSign = 0;
                for (int i = 0; i < shape.count; ++i) {
                    const ShapeBox& bx = shape.boxes[size_t(i)];
                    double tEnter = -inf, tExit = inf;
                    int enterAxis = 0, enterSign = 0;
                    bool miss = false;
                    for (int a = 0; a < 3 && !miss; ++a) {
                        const double lo = cell[a] + bx.from[a] / 16.0, hi = cell[a] + std::min<int>(bx.to[a], 16) / 16.0;
                        if (d[a] == 0.0) {
                            miss = origin[a] < lo || origin[a] > hi;
                            continue;
                        }
                        double t0 = (lo - origin[a]) / d[a], t1 = (hi - origin[a]) / d[a];
                        int sign = d[a] > 0 ? -1 : 1; // entering through the low face when moving +
                        if (t0 > t1) std::swap(t0, t1);
                        if (t0 > tEnter) {
                            tEnter = t0;
                            enterAxis = a;
                            enterSign = sign;
                        }
                        tExit = std::min(tExit, t1);
                        miss = tEnter > tExit;
                    }
                    if (!miss && tExit >= 0.0 && tEnter < best && tEnter <= maxDistance) {
                        best = std::max(0.0, tEnter);
                        bestAxis = enterAxis;
                        bestSign = enterSign;
                    }
                }
                if (best == inf) continue;
                return RayHit{p, enter[bestAxis][bestSign < 0 ? 1 : 0], best};
            }
        }
        // Moving +axis enters through the block's negative face, and vice versa.
        return RayHit{p, enter[axis][step[axis] > 0 ? 1 : 0], t};
    }
}

} // namespace mc::world
