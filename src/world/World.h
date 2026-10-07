#pragma once

#include "world/Chunk.h"

#include <memory>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace mc::world {

// All loaded chunks. Only the main thread mutates it (see docs/architecture.md).
class World {
public:
    World() { m_ticking.reserve(4096); }

    Chunk& createChunk(ChunkPos pos); // replaces an existing chunk at pos
    // Takes ownership of a chunk built elsewhere (e.g. on a worldgen worker).
    Chunk& insertChunk(std::unique_ptr<Chunk> chunk);
    // Removes a chunk and hands it back (for reuse); nullptr if it wasn't loaded.
    std::unique_ptr<Chunk> removeChunk(ChunkPos pos);
    Chunk* chunk(ChunkPos pos);
    const Chunk* chunk(ChunkPos pos) const;
    size_t chunkCount() const { return m_chunks.size(); }

    // World coordinates. Unloaded chunks read as air; writes to them are ignored.
    BlockStateId getBlock(const BlockPos& p) const;
    void setBlock(const BlockPos& p, BlockStateId state);

    // Chunks with something that ticks (block entities, mobs), so game ticks never
    // scan every loaded chunk (vanilla keeps level-wide ticking lists too).
    // markTicking() after adding mobs/entities to a chunk; entries whose chunk was
    // unloaded or has nothing left are dropped while iterating.
    void markTicking(ChunkPos pos);
    template <typename Fn> void forEachTickingChunk(Fn&& fn) {
        size_t w = 0;
        for (size_t r = 0; r < m_ticking.size(); ++r) {
            Chunk* c = chunk(m_ticking[r]);
            if (!c) continue; // unloaded: drop it
            if (c->furnaces().empty() && c->mobs().empty()) {
                c->inTickingList = false; // nothing left: drop it
                continue;
            }
            m_ticking[w++] = m_ticking[r];
            fn(*c);
        }
        m_ticking.resize(w);
    }

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
    std::vector<ChunkPos> m_ticking;
};

} // namespace mc::world
