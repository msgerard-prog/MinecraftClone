#pragma once

#include "world/Chunk.h"

#include <memory>
#include <unordered_map>

namespace mc::world {

// All loaded chunks. Only the main thread mutates it (see docs/architecture.md).
class World {
public:
    Chunk& createChunk(ChunkPos pos); // replaces an existing chunk at pos
    Chunk* chunk(ChunkPos pos);
    const Chunk* chunk(ChunkPos pos) const;
    size_t chunkCount() const { return m_chunks.size(); }

    // World coordinates. Unloaded chunks read as air; writes to them are ignored.
    BlockStateId getBlock(const BlockPos& p) const;
    void setBlock(const BlockPos& p, BlockStateId state);

    template <typename Fn> void forEachChunk(Fn&& fn) {
        for (auto& [key, c] : m_chunks)
            fn(*c);
    }
    template <typename Fn> void forEachChunk(Fn&& fn) const {
        for (const auto& [key, c] : m_chunks)
            fn(static_cast<const Chunk&>(*c));
    }

private:
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>> m_chunks;
};

} // namespace mc::world
