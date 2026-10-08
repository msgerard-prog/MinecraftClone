#pragma once

#include "world/Chunk.h"
#include "world/Sounds.h"

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

// Something the player sees or hears happen (M22.3; vanilla's level events and
// particles sent to clients): gameplay code reports them, main turns them into
// particles (and sounds, M22.4) and clears the list each tick.
struct LevelEvent {
    enum class Type : uint8_t {
        BlockBreak,   // pos = block corner, data = the broken state
        BlockHit,     // pos = block corner, data = state | face << 16 (mining cracks)
        BlockPlace,   // pos = block corner, data = the placed state (sound only)
        Explosion,    // pos = centre, data = power x 10
        MobDeath,     // pos = feet, data = width x 100 | height x 100 << 16 (poof)
        PotionSplash, // pos = impact, data = 0xRRGGBB
        Crit,         // pos = where the hit landed
        Extinguish,   // pos = centre (fire or a burning thing put out: smoke)
        Portal,       // pos = where an entity teleported (portal/ender particles)
        Note,         // pos = above a note block, data = the note 0..24 (its colour)
    };
    Type type;
    double x, y, z;
    uint32_t data = 0;
};

// All loaded chunks. Only the main thread mutates it (see docs/architecture.md).
class World {
public:
    World() {
        m_ticking.reserve(4096);
        m_events.reserve(1024);
        m_sounds.reserve(512);
        m_newMobs.reserve(64);
        m_vibrations.reserve(64);
    }
    // Vibrations (M27.3c): every one BlockUpdates::vibrate hears, for wardens; Mobs::tick
    // reads and clears them (a vibration made after the mob pass is heard next tick).
    struct Vibration {
        glm::dvec3 pos;
        bool byPlayer;
    };
    void vibration(const glm::dvec3& p, bool byPlayer) {
        if (m_vibrations.size() < m_vibrations.capacity()) m_vibrations.push_back({p, byPlayer});
    }
    std::vector<Vibration>& vibrations() { return m_vibrations; }

    // Mobs that appear out of blocks (M26.5 review: bees let out of a broken hive) are
    // queued here and added by Mobs::tick after its pass - never pushed into a chunk's
    // mob list while it may be iterating it. Dropped beyond the reserved capacity.
    void queueMob(const MobData& m) {
        if (m_newMobs.size() < m_newMobs.capacity()) m_newMobs.push_back(m);
    }
    std::vector<MobData>& queuedMobs() { return m_newMobs; }

    // Sounds (M22.4, world/Sounds.h): queued for main to play; dropped when full.
    void playSound(Sound sound, double x, double y, double z, float volume = 1.0f, float pitch = 1.0f) {
        if (m_sounds.size() < m_sounds.capacity()) m_sounds.push_back({sound, x, y, z, volume, pitch});
    }
    std::vector<SoundEvent>& soundEvents() { return m_sounds; }

    // Level events (M22.3): dropped beyond the reserved capacity (hard rule 1).
    void levelEvent(LevelEvent::Type type, double x, double y, double z, uint32_t data = 0) {
        if (m_events.size() < m_events.capacity()) m_events.push_back({type, x, y, z, data});
    }
    std::vector<LevelEvent>& levelEvents() { return m_events; }

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
            if (c->furnaces().empty() && c->mobs().empty() && c->blockTicks().empty() && c->spawners().empty() &&
                c->brewingStands().empty() && c->comparators().empty() && c->hoppers().empty() &&
                c->campfires().empty() && c->beacons().empty() && c->jukeboxes().empty() && c->beehives().empty()) {
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
    std::vector<LevelEvent> m_events;
    std::vector<MobData> m_newMobs;
    std::vector<Vibration> m_vibrations;
    std::vector<SoundEvent> m_sounds;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>> m_chunks;
    std::vector<ChunkPos> m_ticking;
    BlockUpdateListener* m_listener = nullptr;
    bool m_hasSkyLight = true;
    bool m_ultrawarm = false;
    HeightRange m_height = kOverworldHeight;
    uint64_t m_chunkEpoch = 0;
};

} // namespace mc::world
