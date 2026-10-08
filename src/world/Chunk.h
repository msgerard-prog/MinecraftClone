#pragma once

#include "world/Biome.h"
#include "world/BlockEntity.h"
#include "world/Mob.h"
#include "world/Coords.h"
#include "world/Light.h"
#include "world/TickSet.h"
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
    // A column of its dimension's height (vanilla: the level's min_y and height).
    explicit Chunk(ChunkPos pos, HeightRange height = kOverworldHeight) : m_pos(pos), m_height(height) {
        for (int i = 0; i < m_height.sections(); ++i)
            m_sections[size_t(i)] = std::make_shared<Section>();
        m_mobs.reserve(4); // mobs walking in don't allocate during the tick (usually)
    }

    ChunkPos pos() const { return m_pos; }
    const HeightRange& height() const { return m_height; }
    int sectionCount() const { return m_height.sections(); }
    // Reuse this chunk object for another position (the generator overwrites every
    // section; light is recomputed), possibly in another dimension's height.
    void reset(ChunkPos pos, HeightRange height) {
        if (!(height == m_height)) {
            m_height = height;
            for (int i = 0; i < kMaxSections; ++i)
                m_sections[size_t(i)] = i < m_height.sections() ? std::make_shared<Section>() : nullptr;
        }
        m_pos = pos;
        m_lit = false;
        m_dirty = false;
        m_biomes = defaultBiomes();
        m_furnaces.clear();
        m_chests.clear();
        m_spawners.clear();
        m_brewing.clear();
        m_comparators.clear();
        m_hoppers.clear();
        m_dispensers.clear();
        m_signs.clear();
        m_mobs.clear();
        m_blockTicks.clear();
        m_tickSet.clear();
        m_tickSetValid = true;
        ticksRelative = false;
        inTickingList = false;
        lightJob = {};
        for (auto& l : m_light)
            l.reset();
    }

    // Local x/z (0..15), world y. Out-of-height reads return air; writes are ignored.
    BlockStateId get(int x, int y, int z) const {
        if (!m_height.contains(y)) return 0;
        return m_sections[size_t(m_height.sectionIndex(y))]->get(x, blockToLocal(y), z);
    }
    void set(int x, int y, int z, BlockStateId state) {
        if (!m_height.contains(y)) return;
        mutableSection(m_height.sectionIndex(y)).set(x, blockToLocal(y), z, state);
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
    void setLight(std::array<std::shared_ptr<const SectionLight>, kMaxSections> light) {
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
    struct ChestEntry {
        int x, y, z; // local x/z, world y
        ChestData data;
    };
    ChestData* chest(int x, int y, int z) {
        for (auto& c : m_chests)
            if (c.x == x && c.y == y && c.z == z) return &c.data;
        return nullptr;
    }
    const ChestData* chest(int x, int y, int z) const {
        for (const auto& c : m_chests)
            if (c.x == x && c.y == y && c.z == z) return &c.data;
        return nullptr;
    }
    ChestData& addChest(int x, int y, int z) {
        if (ChestData* c = chest(x, y, z)) return *c;
        m_dirty = true;
        m_chests.push_back({x, y, z, {}});
        return m_chests.back().data;
    }
    std::vector<ChestEntry>& chests() { return m_chests; }
    const std::vector<ChestEntry>& chests() const { return m_chests; }
    struct SpawnerEntry {
        int x, y, z; // local x/z, world y
        SpawnerData data;
    };
    SpawnerData* spawner(int x, int y, int z) {
        for (auto& s : m_spawners)
            if (s.x == x && s.y == y && s.z == z) return &s.data;
        return nullptr;
    }
    SpawnerData& addSpawner(int x, int y, int z) {
        if (SpawnerData* s = spawner(x, y, z)) return *s;
        m_dirty = true;
        m_spawners.push_back({x, y, z, {}});
        return m_spawners.back().data;
    }
    std::vector<SpawnerEntry>& spawners() { return m_spawners; }
    const std::vector<SpawnerEntry>& spawners() const { return m_spawners; }
    struct BrewingEntry {
        int x, y, z; // local x/z, world y
        BrewingData data;
    };
    BrewingData* brewing(int x, int y, int z) {
        for (auto& b : m_brewing)
            if (b.x == x && b.y == y && b.z == z) return &b.data;
        return nullptr;
    }
    BrewingData& addBrewing(int x, int y, int z) {
        if (BrewingData* b = brewing(x, y, z)) return *b;
        m_dirty = true;
        m_brewing.push_back({x, y, z, {}});
        return m_brewing.back().data;
    }
    std::vector<BrewingEntry>& brewingStands() { return m_brewing; }
    const std::vector<BrewingEntry>& brewingStands() const { return m_brewing; }
    struct ComparatorEntry {
        int x, y, z; // local x/z, world y
        ComparatorData data;
    };
    ComparatorData* comparator(int x, int y, int z) {
        for (auto& c : m_comparators)
            if (c.x == x && c.y == y && c.z == z) return &c.data;
        return nullptr;
    }
    ComparatorData& addComparator(int x, int y, int z) {
        if (ComparatorData* c = comparator(x, y, z)) return *c;
        m_dirty = true;
        m_comparators.push_back({x, y, z, {}});
        return m_comparators.back().data;
    }
    std::vector<ComparatorEntry>& comparators() { return m_comparators; }
    const std::vector<ComparatorEntry>& comparators() const { return m_comparators; }
    struct HopperEntry {
        int x, y, z; // local x/z, world y
        HopperData data;
    };
    HopperData* hopper(int x, int y, int z) {
        for (auto& h : m_hoppers)
            if (h.x == x && h.y == y && h.z == z) return &h.data;
        return nullptr;
    }
    HopperData& addHopper(int x, int y, int z) {
        if (HopperData* h = hopper(x, y, z)) return *h;
        m_dirty = true;
        m_hoppers.push_back({x, y, z, {}});
        return m_hoppers.back().data;
    }
    std::vector<HopperEntry>& hoppers() { return m_hoppers; }
    const std::vector<HopperEntry>& hoppers() const { return m_hoppers; }
    struct DispenserEntry {
        int x, y, z; // local x/z, world y (dispensers and droppers)
        DispenserData data;
    };
    DispenserData* dispenser(int x, int y, int z) {
        for (auto& d : m_dispensers)
            if (d.x == x && d.y == y && d.z == z) return &d.data;
        return nullptr;
    }
    DispenserData& addDispenser(int x, int y, int z) {
        if (DispenserData* d = dispenser(x, y, z)) return *d;
        m_dirty = true;
        m_dispensers.push_back({x, y, z, {}});
        return m_dispensers.back().data;
    }
    std::vector<DispenserEntry>& dispensers() { return m_dispensers; }
    const std::vector<DispenserEntry>& dispensers() const { return m_dispensers; }
    struct SignEntry { // (M23.3c)
        int x, y, z; // local x/z, world y
        SignData data;
    };
    SignData* sign(int x, int y, int z) {
        for (auto& s : m_signs)
            if (s.x == x && s.y == y && s.z == z) return &s.data;
        return nullptr;
    }
    const SignData* sign(int x, int y, int z) const {
        for (const auto& s : m_signs)
            if (s.x == x && s.y == y && s.z == z) return &s.data;
        return nullptr;
    }
    SignData& addSign(int x, int y, int z) {
        if (SignData* s = sign(x, y, z)) return *s;
        m_dirty = true;
        m_signs.push_back({x, y, z, {}});
        return m_signs.back().data;
    }
    std::vector<SignEntry>& signs() { return m_signs; }
    const std::vector<SignEntry>& signs() const { return m_signs; }
    void removeBlockEntity(int x, int y, int z) {
        std::erase_if(m_hoppers, [&](const HopperEntry& h) { return h.x == x && h.y == y && h.z == z; });
        std::erase_if(m_dispensers, [&](const DispenserEntry& d) { return d.x == x && d.y == y && d.z == z; });
        std::erase_if(m_signs, [&](const SignEntry& s) { return s.x == x && s.y == y && s.z == z; });
        std::erase_if(m_comparators, [&](const ComparatorEntry& c) { return c.x == x && c.y == y && c.z == z; });
        std::erase_if(m_brewing, [&](const BrewingEntry& b) { return b.x == x && b.y == y && b.z == z; });
        std::erase_if(m_spawners, [&](const SpawnerEntry& s) { return s.x == x && s.y == y && s.z == z; });
        std::erase_if(m_furnaces, [&](const FurnaceEntry& f) { return f.x == x && f.y == y && f.z == z; });
        std::erase_if(m_chests, [&](const ChestEntry& c) { return c.x == x && c.y == y && c.z == z; });
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
    // Mutable access for bulk edits (loading): the pending-tick set is rebuilt lazily.
    std::vector<BlockTick>& blockTicks() {
        m_tickSetValid = false;
        return m_blockTicks;
    }
    const std::vector<BlockTick>& blockTicks() const { return m_blockTicks; }
    // Hot paths (BlockUpdates): add a tick, ask whether one is pending in O(1), and
    // remove the ticks due by `now` (passing each to `fn`). Returns true if any ran.
    static uint64_t tickKey(int x, int y, int z, BlockId block) {
        return (uint64_t(uint32_t(y) & 0xFFFFu) << 24) | (uint64_t(z) << 20) | (uint64_t(x) << 16) | block;
    }
    void addTick(const BlockTick& t) {
        tickSet().insert(tickKey(t.x, t.y, t.z, t.block));
        m_blockTicks.push_back(t);
    }
    bool hasTick(int x, int y, int z, BlockId block) { return tickSet().contains(tickKey(x, y, z, block)); }
    template <typename Fn> bool takeDueTicks(int64_t now, Fn&& fn) {
        TickSet& set = tickSet();
        return std::erase_if(m_blockTicks, [&](const BlockTick& t) {
                   if (t.time > now) return false;
                   set.erase(tickKey(t.x, t.y, t.z, t.block));
                   fn(t);
                   return true;
               }) > 0;
    }
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
        uint8_t priority = 0; // of the queue it waits in: 0 settling, 1 streaming, 2 edits
        bool settleWanted = false;  // fluid flowed here; relight when its turn comes
        uint32_t lastSettle = 0;    // LightManager frame of its last settling request
    } lightJob;

    // Light at local x/z, world y (above the world: full sky light).
    uint8_t skyLight(int x, int y, int z) const;
    uint8_t blockLight(int x, int y, int z) const;

private:
    ChunkPos m_pos;
    HeightRange m_height;
    std::array<std::shared_ptr<Section>, kMaxSections> m_sections;      // [0, sectionCount)
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> m_light;
    bool m_lit = false;
    bool m_dirty = false;
    std::shared_ptr<const ChunkBiomes> m_biomes = defaultBiomes();
    std::vector<FurnaceEntry> m_furnaces;
    std::vector<ChestEntry> m_chests;
    std::vector<SpawnerEntry> m_spawners;
    std::vector<ComparatorEntry> m_comparators;
    std::vector<HopperEntry> m_hoppers;
    std::vector<DispenserEntry> m_dispensers;
    std::vector<SignEntry> m_signs;
    std::vector<BrewingEntry> m_brewing;
    std::vector<MobData> m_mobs;
    std::vector<BlockTick> m_blockTicks;
    TickSet m_tickSet; // keys of m_blockTicks (valid unless edited in bulk)
    bool m_tickSetValid = true;
    TickSet& tickSet() {
        if (!m_tickSetValid) {
            m_tickSet.clear();
            for (const BlockTick& t : m_blockTicks)
                m_tickSet.insert(tickKey(t.x, t.y, t.z, t.block));
            m_tickSetValid = true;
        }
        return m_tickSet;
    }
};

} // namespace mc::world

template <> struct std::hash<mc::world::ChunkPos> {
    size_t operator()(const mc::world::ChunkPos& p) const noexcept {
        return std::hash<int64_t>{}(p.key());
    }
};
