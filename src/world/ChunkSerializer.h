#pragma once

#include "core/Nbt.h"
#include "world/Chunk.h"

#include <array>
#include <memory>

namespace mc::world {

// Data version written into saves (wiki: Data version): Java 1.21.1 = 3955. Tied to
// the 1.21 patch decision (ADR 0002, pending).
inline constexpr int32_t kDataVersion = 3955;

// A chunk's saveable state, shared read-only (copy-on-write sections), so the IO
// thread can serialise it while the main thread keeps playing.
struct ChunkSnapshot {
    ChunkPos pos;
    std::array<std::shared_ptr<const Section>, kSectionsPerChunk> sections;
    std::array<std::shared_ptr<const SectionLight>, kSectionsPerChunk> light; // may be null

    static ChunkSnapshot of(const Chunk& chunk);
};

// Java 1.21 chunk NBT (wiki: Chunk format): DataVersion, xPos/zPos/yPos, Status
// "minecraft:full", sections [ {Y, block_states {palette, data}, biomes, SkyLight,
// BlockLight} ]. Block states are written with a local palette of
// "minecraft:id" + Properties (vanilla packing: 64 / bits entries per long, at
// least 4 bits). Biomes are always plains until biomes exist (M8).
nbt::Compound chunkToNbt(const ChunkSnapshot& chunk);

// Fills `chunk` (already at the right position) from NBT. Unknown blocks become air
// (counted in `unknownBlocks`). Light is not read: it is recomputed on load.
// Returns false if the NBT isn't a chunk at that position.
bool chunkFromNbt(const nbt::Compound& nbt, Chunk& chunk, int* unknownBlocks = nullptr);

} // namespace mc::world
