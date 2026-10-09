#pragma once

#include "core/WorkQueue.h"
#include "world/LightEngine.h"
#include "world/SectionSnapshot.h"
#include "world/World.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace mc::world {

// Keeps chunk light up to date on worker threads (GL-free; the renderer only reacts).
// A chunk is lit once it and its 8 neighbours are loaded. A block edit whose 3x3 chunks
// are lit is corrected incrementally (M31.1: one job over the 9 chunks; it waits while any
// of them has a job running, and full jobs wait for it); elsewhere the edit relights the
// 3x3 chunks from scratch. Light jobs read shared, immutable sections, so the main
// thread can keep editing while they run.
//
// Edited blocks are handed back once the light around them is current (the edited
// chunk's relight is installed), so their sections are meshed with correct light
// instead of the stale light of the removed/placed block.
class LightManager {
public:
    LightManager(World& world, int threads);
    ~LightManager();
    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    // Main thread, once per frame. Inputs: chunks loaded / unloaded this frame and
    // blocks edited. Outputs (cleared first): chunks that became lit for the first
    // time, sections whose light changed in chunks that were already lit, and edited
    // blocks whose light is now up to date (re-mesh them).
    // `settling`: edits that may be drawn with stale light for a moment (flowing
    // fluids): they come straight back in `editsReady` and their relight waits in the
    // streaming queue instead of jumping ahead of it.
    void update(const std::vector<ChunkPos>& loaded, const std::vector<ChunkPos>& unloaded,
                const std::vector<BlockPos>& edited, const std::vector<BlockPos>& settling,
                std::vector<ChunkPos>& newlyLit, std::vector<SectionPos>& relitSections,
                std::vector<BlockPos>& editsReady);

    // Light jobs queued or running for streaming and edits (fluids settling in the
    // background are left out: flowing springs would keep it from ever reaching 0).
    int pending() const {
        return static_cast<int>(m_editQueue.size() + m_queue.size() - m_head + m_incWaiting.size()) + m_inFlight;
    }

private:
    struct Job {
        ChunkPos pos;
        uint32_t version = 0;
        ChunkNeighbourhood input;
        ChunkLight before; // the chunk's light at submission (compared on the worker)
        ChunkLight output;
        uint32_t changed = 0; // bit per section whose light differs from `before`
        // (M31.1) an incremental update of the 3x3 chunks around `pos` after `inc.edits`
        bool incremental = false;
        IncrementalLightInput inc;
        IncrementalLightOutput incOut;
    };
    // (M31.1) edits waiting for an incremental update (their chunks busy, or this frame's).
    std::vector<BlockPos> m_incWaiting;
    std::vector<BlockPos> m_incKeep;
    std::vector<BlockPos> m_fullEdits; // edits whose chunks aren't all lit: the old 3x3 relight
    std::vector<ChunkPos> m_retry; // full jobs held back while a chunk is being updated
    enum class Incremental : uint8_t { Ready, Busy, Unlit };
    Incremental incrementalState(ChunkPos centre) const;
    void submitIncremental(ChunkPos centre, const BlockPos* edits, size_t count);
    enum Priority : uint8_t { kSettle = 0, kStream = 1, kEdit = 2 };
    void request(ChunkPos pos, Priority priority);
    bool neighbourhoodLoaded(ChunkPos pos) const;
    bool submit(ChunkPos pos, std::vector<BlockPos>& editsReady);
    void run();

    World& m_world;
    // Three FIFO queues: edits first (latency), then streaming, then settling (fluid
    // flow; only once nothing streams, so its repeated requests merge while waiting).
    // Streaming entries are consumed from m_head and compacted when the queue drains
    // (no per-pop erase).
    std::vector<ChunkPos> m_editQueue;
    std::vector<ChunkPos> m_queue;
    std::vector<ChunkPos> m_settleQueue;
    // Chunks fluid flowed in, waiting for their settle slot: each chunk is relit for
    // flow at most every kSettleFrames frames, so a running spring costs one relight
    // per chunk per interval instead of one per flowing block.
    std::vector<ChunkPos> m_settleWanted;
    uint32_t m_frame = 0;
    static constexpr uint32_t kSettleFrames = 20;
    size_t m_head = 0;
    std::vector<BlockPos> m_pendingEdits; // waiting for their chunk's relight
    uint32_t m_nextVersion = 1;            // global: versions never repeat
    int m_inFlight = 0;
    int m_maxInFlight = 8;
    std::vector<std::unique_ptr<Job>> m_free; // job pool (no per-submit allocation)
    WorkQueue<std::unique_ptr<Job>> m_jobs;
    WorkQueue<std::unique_ptr<Job>> m_done;
    std::vector<std::thread> m_threads;
};

} // namespace mc::world
