#include "gameplay/BlockCollision.h"

#include "world/BlockShapes.h"
#include "world/Blocks.h"

#include <cmath>

namespace mc {

void gatherBlockBoxes(const world::World& world, const Aabb& region, std::vector<Aabb>& out, bool unloadedSolid) {
    const auto& reg = world::blockRegistry();
    out.clear();
    const int x0 = int(std::floor(region.min.x)), x1 = int(std::floor(region.max.x));
    const int y0 = int(std::floor(region.min.y)), y1 = int(std::floor(region.max.y));
    const int z0 = int(std::floor(region.min.z)), z1 = int(std::floor(region.max.z));
    for (int y = y0 - 1; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                if (unloadedSolid && !world.chunk({world::blockToChunk(x), world::blockToChunk(z)})) {
                    if (y >= y0) out.push_back({{double(x), double(y), double(z)}, {x + 1.0, y + 1.0, z + 1.0}});
                    continue;
                }
                const world::BlockStateId s = world.getBlock({x, y, z});
                if (!reg.collides(s)) continue;
                const world::BlockShape& sh = world::collisionShape(s);
                for (int i = 0; i < sh.count; ++i) {
                    const world::ShapeBox& b = sh.boxes[size_t(i)];
                    if (y < y0 && b.to[1] <= 16) continue; // (the layer below only for tall shapes)
                    out.push_back({{x + b.from[0] / 16.0, y + b.from[1] / 16.0, z + b.from[2] / 16.0},
                                   {x + b.to[0] / 16.0, y + b.to[1] / 16.0, z + b.to[2] / 16.0}});
                }
            }
}

} // namespace mc
