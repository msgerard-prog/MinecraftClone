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
// A chunk is lit once it and its 8 neighbours are loaded; a block edit relights the
// 3x3 chunks around it. Light jobs read shared, immutable sections, so the main
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
    void update(const std::vector<ChunkPos>& loaded, const std::vector<ChunkPos>& unloaded,
                const std::vector<BlockPos>& edited, std::vector<ChunkPos>& newlyLit,
                std::vector<SectionPos>& relitSections, std::vector<BlockPos>& editsReady);

    // Light jobs queued or running.
    int pending() const {
        return static_cast<int>(m_editQueue.size() + m_queue.size() - m_head) + m_inFlight;
    }

private:
    struct Job {
        ChunkPos pos;
        uint32_t version = 0;
        ChunkNeighbourhood input;
        ChunkLight before; // the chunk's light at submission (compared on the worker)
        ChunkLight output;
        uint32_t changed = 0; // bit per section whose light differs from `before`
    };
    void request(ChunkPos pos, bool edit);
    bool neighbourhoodLoaded(ChunkPos pos) const;
    bool submit(ChunkPos pos, std::vector<BlockPos>& editsReady);
    void run();

    World& m_world;
    // Two FIFO queues: edits first (latency), then streaming. Streaming entries are
    // consumed from m_head and compacted when the queue drains (no per-pop erase).
    std::vector<ChunkPos> m_editQueue;
    std::vector<ChunkPos> m_queue;
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
