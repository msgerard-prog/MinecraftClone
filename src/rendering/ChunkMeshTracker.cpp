#include "rendering/ChunkMeshTracker.h"

namespace mc::gfx {

bool ChunkMeshTracker::neighbourhoodLoaded(const world::World& world, world::ChunkPos pos) {
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const world::Chunk* c = world.chunk({pos.x + dx, pos.z + dz}); !c || !c->lit())
                return false;
    return true;
}

void ChunkMeshTracker::onLoaded(const world::World& world, std::span<const world::ChunkPos> loaded,
                                std::vector<world::ChunkPos>& ready) {
    for (const world::ChunkPos& p : loaded) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const world::ChunkPos q{p.x + dx, p.z + dz};
                if (!m_meshed.contains(q) && neighbourhoodLoaded(world, q)) {
                    m_meshed.insert(q);
                    ready.push_back(q);
                }
            }
        }
    }
}

void ChunkMeshTracker::onUnloaded(std::span<const world::ChunkPos> unloaded) {
    for (const world::ChunkPos& p : unloaded)
        m_meshed.erase(p);
}

} // namespace mc::gfx
