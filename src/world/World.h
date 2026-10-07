#pragma once

#include "world/Chunk.h"

#include <memory>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace mc::world {

// Told about block changes made through World::updateBlock (redstone, supports).
class BlockUpdateListener {
public:
    virtual ~BlockUpdateListener() = default;
    virtual void onBlockChanged(const BlockPos& p, BlockStateId old, BlockStateId now) = 0;
};

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
    // A gameplay change (players breaking/placing): sets the block, then notifies the
    // listener so neighbours react (vanilla Level.setBlock with block updates).
    void updateBlock(const BlockPos& p, BlockStateId state) {
        const BlockStateId old = getBlock(p);
        if (old == state || !chunk(p.chunk())) return;
        setBlock(p, state);
        if (m_listener) m_listener->onBlockChanged(p, old, state);
    }
    void setListener(BlockUpdateListener* listener) { m_listener = listener; }
    // Neighbour updates for a change already made with setBlock (/fill).
    void notifyChanged(const BlockPos& p, BlockStateId old, BlockStateId now) {
        if (m_listener) m_listener->onBlockChanged(p, old, now);
    }

    // The dimension's vertical extent (vanilla LevelHeightAccessor): every chunk of this
    // world has it. Set before chunks are created (all chunks are removed when it changes).
    const HeightRange& height() const { return m_height; }
    void setHeight(HeightRange h) { m_height = h; }
    bool isInHeight(int32_t y) const { return m_height.contains(y); }

    // Dimension property: the Nether and the End have no sky light.
    bool hasSkyLight() const { return m_hasSkyLight; }
    // Changes whenever a chunk is added or removed (lets callers cache Chunk pointers).
    uint64_t chunkEpoch() const { return m_chunkEpoch; }
    void setHasSkyLight(bool v) { m_hasSkyLight = v; }
    // The Nether (wiki: Dimension type › ultrawarm): lava flows faster and farther,
    // water can't be placed.
    bool isUltrawarm() const { return m_ultrawarm; }
    void setUltrawarm(bool v) { m_ultrawarm = v; }

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
            if (c->furnaces().empty() && c->mobs().empty() && c->blockTicks().empty() && c->spawners().empty()) {
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
    BlockUpdateListener* m_listener = nullptr;
    bool m_hasSkyLight = true;
    bool m_ultrawarm = false;
    HeightRange m_height = kOverworldHeight;
    uint64_t m_chunkEpoch = 0;
};

} // namespace mc::world
