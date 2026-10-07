#pragma once

#include "world/Coords.h"
#include "world/Section.h"

#include <array>
#include <cstdint>
#include <functional>

namespace mc::world {

struct ChunkPos {
    int32_t x = 0;
    int32_t z = 0;
    bool operator==(const ChunkPos&) const = default;
    // Vanilla packs chunk positions into one long the same way (ChunkPos.toLong).
    int64_t key() const {
        return (static_cast<int64_t>(z) << 32) | static_cast<int64_t>(static_cast<uint32_t>(x));
    }
};

struct BlockPos {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    bool operator==(const BlockPos&) const = default;
    ChunkPos chunk() const { return {blockToChunk(x), blockToChunk(z)}; }
};

// A 16 x 384 x 16 column: 24 sections from Y -64 (section 0) to Y 319 (section 23).
class Chunk {
public:
    explicit Chunk(ChunkPos pos) : m_pos(pos) {}

    ChunkPos pos() const { return m_pos; }
    // Reuse this chunk object for another position (sections keep their capacity;
    // the generator overwrites every section).
    void reset(ChunkPos pos) { m_pos = pos; }

    // Local x/z (0..15), world y. Out-of-height reads return air; writes are ignored.
    BlockStateId get(int x, int y, int z) const {
        if (!isInBuildHeight(y)) return 0;
        return m_sections[sectionIndex(y)].get(x, blockToLocal(y), z);
    }
    void set(int x, int y, int z, BlockStateId state) {
        if (!isInBuildHeight(y)) return;
        m_sections[sectionIndex(y)].set(x, blockToLocal(y), z, state);
    }

    Section& section(int index) { return m_sections[index]; }
    const Section& section(int index) const { return m_sections[index]; }

private:
    ChunkPos m_pos;
    std::array<Section, kSectionsPerChunk> m_sections;
};

} // namespace mc::world

template <> struct std::hash<mc::world::ChunkPos> {
    size_t operator()(const mc::world::ChunkPos& p) const noexcept {
        return std::hash<int64_t>{}(p.key());
    }
};
