#pragma once

#include "world/Biome.h"
#include "world/BlockEntity.h"
#include "world/Mob.h"
#include "world/Coords.h"
#include "world/Light.h"
#include "world/Section.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

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
//
// Sections and their light are shared, immutable snapshots (copy-on-write): worker
// threads (lighting, meshing) hold references to them while the main thread keeps
// editing - an edit copies a section first if anyone else still holds it.
class Chunk {
public:
    explicit Chunk(ChunkPos pos) : m_pos(pos) {
        for (auto& s : m_sections)
            s = std::make_shared<Section>();
        m_mobs.reserve(4); // mobs walking in don't allocate during the tick (usually)
    }

    ChunkPos pos() const { return m_pos; }
    // Reuse this chunk object for another position (the generator overwrites every
    // section; light is recomputed).
    void reset(ChunkPos pos) {
        m_pos = pos;
        m_lit = false;
        m_dirty = false;
        m_biomes = defaultBiomes();
        m_furnaces.clear();
        m_mobs.clear();
        m_blockTicks.clear();
        ticksRelative = false;
        inTickingList = false;
        lightJob = {};
        for (auto& l : m_light)
            l.reset();
    }

    // Local x/z (0..15), world y. Out-of-height reads return air; writes are ignored.
    BlockStateId get(int x, int y, int z) const {
        if (!isInBuildHeight(y)) return 0;
        return m_sections[sectionIndex(y)]->get(x, blockToLocal(y), z);
    }
    void set(int x, int y, int z, BlockStateId state) {
        if (!isInBuildHeight(y)) return;
        mutableSection(sectionIndex(y)).set(x, blockToLocal(y), z, state);
    }

    const Section& section(int index) const { return *m_sections[index]; }
    // Write access: copies the section first if a worker still holds the old one.
    // Invariant: only the main thread copies these shared_ptrs (workers only read
    // and release theirs), so use_count() == 1 means no worker can still see it; the
    // acquire fence pairs with the worker's releasing decrement.
    Section& mutableSection(int index) {
        auto& s = m_sections[index];
        if (s.use_count() > 1) s = std::make_shared<Section>(*s);
        std::atomic_thread_fence(std::memory_order_acquire);
        m_dirty = true;
        return *s;
    }
    // A reference that stays valid (unchanged) while the chunk is edited.
    std::shared_ptr<const Section> shareSection(int index) const { return m_sections[index]; }

    // Light (computed by LightEngine; until then the chunk is not lit).
    bool lit() const { return m_lit; }
    const std::shared_ptr<const SectionLight>& light(int index) const { return m_light[index]; }
    void setLight(std::array<std::shared_ptr<const SectionLight>, kSectionsPerChunk> light) {
        m_light = std::move(light);
        m_lit = true;
    }
    // Biomes (4x4x4 cells), immutable and shared; plains until a generator sets them.
    const std::shared_ptr<const ChunkBiomes>& biomes() const { return m_biomes; }
    void setBiomes(std::shared_ptr<const ChunkBiomes> b) {
        m_biomes = std::move(b);
        m_dirty = true;
    }
    static const std::shared_ptr<const ChunkBiomes>& defaultBiomes() {
        static const auto plains = std::make_shared<const ChunkBiomes>();
        return plains;
    }

    // Block entities (M9: furnaces), keyed by position inside the chunk. Main thread.
    struct FurnaceEntry {
        int x, y, z; // local x/z, world y
        FurnaceData data;
    };
    FurnaceData* furnace(int x, int y, int z) {
        for (auto& f : m_furnaces)
            if (f.x == x && f.y == y && f.z == z) return &f.data;
        return nullptr;
    }
    FurnaceData& addFurnace(int x, int y, int z) {
        if (FurnaceData* f = furnace(x, y, z)) return *f;
        m_dirty = true;
        m_furnaces.push_back({x, y, z, {}});
        return m_furnaces.back().data;
    }
    void removeBlockEntity(int x, int y, int z) {
        std::erase_if(m_furnaces, [&](const FurnaceEntry& f) { return f.x == x && f.y == y && f.z == z; });
    }
    std::vector<FurnaceEntry>& furnaces() { return m_furnaces; }
    const std::vector<FurnaceEntry>& furnaces() const { return m_furnaces; }
    void markDirty() { m_dirty = true; }

    // Mobs standing in this chunk (gameplay moves them between chunks as they walk;
    // saved in the world's entities/ region files).
    std::vector<MobData>& mobs() { return m_mobs; }
    const std::vector<MobData>& mobs() const { return m_mobs; }

    // Scheduled block ticks in this chunk (wiki: Tick › Scheduled tick; redstone
    // delays). Ordered by time, then priority, then scheduling order.
    struct BlockTick {
        int8_t x, z;      // local
        int16_t y;        // world
        int8_t priority;  // lower runs first
        BlockId block;    // runs only if this block is still there
        int64_t time;     // game time it runs at (a delay while `ticksRelative`)
        uint64_t order;   // scheduling order within the same time and priority
    };
    std::vector<BlockTick>& blockTicks() { return m_blockTicks; }
    const std::vector<BlockTick>& blockTicks() const { return m_blockTicks; }
    // Loaded from disk: tick times are delays until the chunk's first game tick.
    bool ticksRelative = false;

    // Changed since it was generated / loaded / last saved (needs saving).
    bool dirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // World bookkeeping: listed in World's ticking chunks (main thread only).
    bool inTickingList = false;

    // LightManager bookkeeping (main thread only).
    struct LightJobState {
        uint32_t version = 0; // latest submitted job (0: none); globally unique
        bool queued = false;  // waiting in a LightManager queue
    } lightJob;

    // Light at local x/z, world y (above the world: full sky light).
    uint8_t skyLight(int x, int y, int z) const;
    uint8_t blockLight(int x, int y, int z) const;

private:
    ChunkPos m_pos;
    std::array<std::shared_ptr<Section>, kSectionsPerChunk> m_sections;
    std::array<std::shared_ptr<const SectionLight>, kSectionsPerChunk> m_light;
    bool m_lit = false;
    bool m_dirty = false;
    std::shared_ptr<const ChunkBiomes> m_biomes = defaultBiomes();
    std::vector<FurnaceEntry> m_furnaces;
    std::vector<MobData> m_mobs;
    std::vector<BlockTick> m_blockTicks;
};

} // namespace mc::world

template <> struct std::hash<mc::world::ChunkPos> {
    size_t operator()(const mc::world::ChunkPos& p) const noexcept {
        return std::hash<int64_t>{}(p.key());
    }
};
