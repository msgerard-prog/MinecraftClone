#pragma once

#include "core/Nbt.h"
#include "world/Chunk.h"

#include <array>
#include <memory>
#include <vector>

namespace mc::world {

// Data version written into saves (wiki: Data version): Java 1.21.11 = 4671. Tied to
// the pinned patch (ADR 0002: 1.21.11).
inline constexpr int32_t kDataVersion = 4671; // Java Edition 1.21.11 (ADR 0002)

// A chunk's saveable state, shared read-only (copy-on-write sections), so the IO
// thread can serialise it while the main thread keeps playing.
struct ChunkSnapshot {
    ChunkPos pos;
    HeightRange height;                                                 // the dimension's
    std::array<std::shared_ptr<const Section>, kMaxSections> sections; // [0, height.sections())
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light; // may be null
    std::shared_ptr<const ChunkBiomes> biomes;
    std::vector<Chunk::FurnaceEntry> furnaces; // block entities
    std::vector<Chunk::ChestEntry> chests;
    std::vector<Chunk::SpawnerEntry> spawners;
    std::vector<MobData> mobs;                 // saved in entities/ (1.17+ layout)
    std::vector<Chunk::BlockTick> blockTicks;  // times relative to gameTime when saved
    int64_t gameTime = 0; // written as LastUpdate

    static ChunkSnapshot of(const Chunk& chunk, int64_t gameTime = 0);
};

// Java 1.21.11 chunk NBT (wiki: Chunk format): DataVersion, xPos/zPos/yPos, status
// "minecraft:full", sections [ {Y, block_states {palette, data}, biomes, SkyLight,
// BlockLight} ]. Block states are written with a local palette of
// "minecraft:id" + Properties (vanilla packing: 64 / bits entries per long, at
// least 4 bits). Biomes: a string palette per section with 64 entries packed at
// ceil(log2(palette size)) bits (no data for a single biome).
nbt::Compound chunkToNbt(const ChunkSnapshot& chunk);

// Fills `chunk` (already at the right position) from NBT. Unknown blocks become air
// (counted in `unknownBlocks`); unknown properties or values of a known block are
// ignored (that property keeps its default). Light is not read: it is recomputed on load.
// Returns false if the NBT isn't a chunk at that position. `legacyWorld`: the world's
// level.dat predates kCloneFormat 1 (chunks without "clone_format" get their placed
// leaves made persistent; see LevelData.h).
bool chunkFromNbt(const nbt::Compound& nbt, Chunk& chunk, int* unknownBlocks = nullptr, bool legacyWorld = false);

// The chunk's entities file (wiki: Entity format, 1.17+ entities/ region files):
// { DataVersion, Position [I; x, z], Entities [ {id, Pos, Motion, Rotation, Health,
// OnGround, fall_distance (1.21.5+ double; FallDistance read), Fire, Air, equipment,
// HurtTime, DeathTime, PersistenceRequired, UUID, ...} ] }.
nbt::Compound entitiesToNbt(const ChunkSnapshot& chunk);
void entitiesFromNbt(const nbt::Compound& nbt, Chunk& chunk);

} // namespace mc::world
