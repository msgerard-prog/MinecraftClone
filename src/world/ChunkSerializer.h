#pragma once

#include "core/Nbt.h"
#include "world/Chunk.h"

#include <array>
#include <memory>
#include <vector>

namespace mc::world {

// Data version written into saves (wiki: Data version): Java 26.3 = 5023 (M34, the user's
// choice 2026-10-09: saves follow the 26.x content; 1.21.11 was 4671, 26.1 4786).
inline constexpr int32_t kDataVersion = 5023; // Java Edition 26.3 (ADR 0002)

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
    std::vector<Chunk::BrewingEntry> brewing;
    std::vector<Chunk::SignEntry> signs;
    std::vector<Chunk::BannerEntry> banners; // (M28.3d)
    std::vector<Chunk::CampfireEntry> campfires;
    std::vector<Chunk::BeaconEntry> beacons;
    std::vector<Chunk::JukeboxEntry> jukeboxes;
    std::vector<Chunk::CommandBlockEntry> commandBlocks; // (M29.7)
    std::vector<Chunk::BrushableEntry> brushables;
    std::vector<Chunk::BeehiveEntry> beehives; // (M26.3b)
    std::vector<Chunk::ComparatorEntry> comparators;
    std::vector<Chunk::HopperEntry> hoppers;
    std::vector<Chunk::DispenserEntry> dispensers;
    std::vector<MobData> mobs;                 // saved in entities/ (1.17+ layout)
    std::vector<Chunk::MobStoreEntry> mobStores; // (M26.2) their chests
    std::vector<Chunk::DroppedItem> droppedItems; // (M30.4)
    std::vector<Chunk::DroppedOrb> droppedOrbs;
    std::vector<Chunk::ParkedEntity> parkedEntities; // (M32.3)
    std::vector<Chunk::BlockTick> blockTicks;  // times relative to gameTime when saved
    int64_t gameTime = 0; // written as LastUpdate
    int64_t inhabitedTicks = 0; // (M32.2) InhabitedTime

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

// One item stack as vanilla saves it (id, count, components; Slot when slot >= 0), for
// level.dat's lists too.
nbt::Compound itemToNbt(const ItemStack& s, int slot);
ItemStack itemFromNbtPublic(const nbt::Compound& c);

// The chunk's entities file (wiki: Entity format, 1.17+ entities/ region files):
// { DataVersion, Position [I; x, z], Entities [ {id, Pos, Motion, Rotation, Health,
// OnGround, fall_distance (1.21.5+ double; FallDistance read), Fire, Air, equipment,
// HurtTime, DeathTime, PersistenceRequired, UUID, ...} ] }.
// The player's UUID, written as tamed pets' Owner (M26.1). Set once per session.
void setPlayerUuid(uint64_t hi, uint64_t lo);
nbt::Compound entitiesToNbt(const ChunkSnapshot& chunk);
void entitiesFromNbt(const nbt::Compound& nbt, Chunk& chunk);

} // namespace mc::world
