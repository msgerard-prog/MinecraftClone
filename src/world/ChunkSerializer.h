#pragma once

#include "core/Nbt.h"
#include "world/Chunk.h"

#include <array>
#include <memory>
#include <vector>

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
    std::shared_ptr<const ChunkBiomes> biomes;
    std::vector<Chunk::FurnaceEntry> furnaces; // block entities
    int64_t gameTime = 0; // written as LastUpdate

    static ChunkSnapshot of(const Chunk& chunk, int64_t gameTime = 0);
};

// Java 1.21 chunk NBT (wiki: Chunk format): DataVersion, xPos/zPos/yPos, Status
// "minecraft:full", sections [ {Y, block_states {palette, data}, biomes, SkyLight,
// BlockLight} ]. Block states are written with a local palette of
// "minecraft:id" + Properties (vanilla packing: 64 / bits entries per long, at
// least 4 bits). Biomes: a string palette per section with 64 entries packed at
// ceil(log2(palette size)) bits (no data for a single biome).
nbt::Compound chunkToNbt(const ChunkSnapshot& chunk);

// Fills `chunk` (already at the right position) from NBT. Unknown blocks become air
// (counted in `unknownBlocks`); unknown properties or values of a known block are
// ignored (that property keeps its default). Light is not read: it is recomputed on load.
// Returns false if the NBT isn't a chunk at that position.
bool chunkFromNbt(const nbt::Compound& nbt, Chunk& chunk, int* unknownBlocks = nullptr);

} // namespace mc::world
