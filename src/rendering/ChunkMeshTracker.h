#pragma once

#include "world/World.h"

#include <span>
#include <unordered_set>
#include <vector>

namespace mc::gfx {

// Which streamed chunks have been given meshes. A chunk becomes meshable once it and
// all 8 neighbours are loaded (its border faces depend on them). GL-free (tested).
class ChunkMeshTracker {
public:
    static bool neighbourhoodLoaded(const world::World& world, world::ChunkPos pos);

    // For newly loaded chunks: appends every chunk that just became meshable
    // (the new chunk itself or any neighbour it completed) to `ready`, once.
    void onLoaded(const world::World& world, std::span<const world::ChunkPos> loaded,
                  std::vector<world::ChunkPos>& ready);
    // Unloaded chunks may be meshed again when they come back.
    void onUnloaded(std::span<const world::ChunkPos> unloaded);
    bool isMeshed(world::ChunkPos pos) const { return m_meshed.contains(pos); }
    size_t meshedCount() const { return m_meshed.size(); }

private:
    std::unordered_set<world::ChunkPos> m_meshed;
};

} // namespace mc::gfx
