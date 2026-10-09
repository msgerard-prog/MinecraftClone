#include "gameplay/FluidContact.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

// The fluid in a cell: waterlogged blocks hold water (M25.1).
BlockId fluidIn(BlockStateId s) {
    return blockRegistry().waterlogged(s) ? BlockId(blocks::Water) : blockRegistry().blockOf(s);
}

// Surface height above the cell's bottom: amount / 9; a full cell under the same fluid.
double surface(const World& w, int x, int y, int z, BlockId kind) {
    const BlockStateId s = w.getBlock({x, y, z});
    if (fluidIn(s) != kind) return -1.0;
    if (fluidIn(w.getBlock({x, y + 1, z})) == kind) return 1.0;
    return BlockUpdates::fluidAmount(s) / 9.0;
}

} // namespace

FluidContact fluidContact(const World& world, const Aabb& box) {
    FluidContact c;
    const auto& reg = blockRegistry();
    constexpr double e = 0.001;
    for (int x = int(std::floor(box.min.x + e)); x <= int(std::floor(box.max.x - e)); ++x)
        for (int y = int(std::floor(box.min.y + e)); y <= int(std::floor(box.max.y - e)); ++y)
            for (int z = int(std::floor(box.min.z + e)); z <= int(std::floor(box.max.z - e)); ++z) {
                // One lookup per cell; only fluid cells read their neighbours.
                const BlockStateId s = world.getBlock({x, y, z});
                const BlockId kind = fluidIn(s);
                if (isFire(kind)) c.fire = true, c.soulFire = c.soulFire || kind == blocks::SoulFire;
                if (reg.blockOf(s) == blocks::BubbleColumn) {
                    c.bubble = reg.get(s, properties::drag) == 0 ? -1 : 1;
                    c.bubbleSurface = c.bubbleSurface || world.getBlock({x, y + 1, z}) == 0;
                }
                if (kind != blocks::Water && kind != blocks::Lava) continue;
                const double h = fluidIn(world.getBlock({x, y + 1, z})) == kind
                                     ? 1.0
                                     : BlockUpdates::fluidAmount(s) / 9.0;
                if (y + h < box.min.y) continue;
                (kind == blocks::Water ? c.water : c.lava) = true;
                c.height = std::max(c.height, y + h - box.min.y);
                if (kind != blocks::Water) continue;
                // The current: toward lower neighbouring surfaces (vanilla getFlow);
                // an open neighbour over the same fluid counts as a drop.
                static constexpr int kDirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
                for (const auto& d : kDirs) {
                    double hn = surface(world, x + d[0], y, z + d[1], kind);
                    if (hn < 0.0) {
                        if (reg.collides(world.getBlock({x + d[0], y, z + d[1]}))) continue;
                        const double below = surface(world, x + d[0], y - 1, z + d[1], kind);
                        if (below < 0.0) continue;
                        hn = below - 8.0 / 9.0;
                    }
                    c.flow += glm::dvec3(d[0], 0.0, d[1]) * (h - hn);
                }
            }
    const double len = glm::length(c.flow);
    c.flow = len > 1e-6 ? c.flow / len : glm::dvec3(0.0);
    return c;
}

void applyBubbleColumn(const FluidContact& c, glm::dvec3& velocity) {
    if (c.bubble > 0) velocity.y = std::min(c.bubbleSurface ? 1.8 : 0.7, velocity.y + (c.bubbleSurface ? 0.1 : 0.06));
    if (c.bubble < 0) velocity.y = std::max(c.bubbleSurface ? -0.9 : -0.3, velocity.y - 0.03);
}

bool pointInFluid(const World& world, const glm::dvec3& p, BlockId block) {
    const int x = int(std::floor(p.x)), y = int(std::floor(p.y)), z = int(std::floor(p.z));
    const double h = surface(world, x, y, z, block);
    return h >= 0.0 && p.y < y + h;
}

bool eyesUnderWater(const World& world, const glm::dvec3& eye) {
    const BlockPos at{int(std::floor(eye.x)), int(std::floor(eye.y)), int(std::floor(eye.z))};
    if (blockRegistry().blockOf(world.getBlock(at)) == blocks::BubbleColumn) return false;
    return pointInFluid(world, eye, blocks::Water);
}

} // namespace mc
