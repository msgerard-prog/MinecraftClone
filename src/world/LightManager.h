#pragma once

#include "core/WorkQueue.h"
#include "world/LightEngine.h"
#include "world/SectionSnapshot.h"
#include "world/World.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mc::world {

// Keeps chunk light up to date on worker threads (GL-free; the renderer only reacts).
// A chunk is lit once it and its 8 neighbours are loaded; a block edit relights the
// 3x3 chunks around it. Light jobs read shared, immutable sections, so the main
// thread can keep editing while they run.
class LightManager {
public:
    LightManager(World& world, int threads);
    ~LightManager();
    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    // Main thread, once per frame. Inputs: chunks loaded / unloaded this frame and
    // blocks edited. Outputs (cleared first): chunks that became lit for the first
    // time, and sections whose light changed in chunks that were already lit.
    void update(const std::vector<ChunkPos>& loaded, const std::vector<ChunkPos>& unloaded,
                const std::vector<BlockPos>& edited, std::vector<ChunkPos>& newlyLit,
                std::vector<SectionPos>& relitSections);

    // Light jobs queued or running (chunks waiting for missing neighbours excluded).
    int pending() const { return static_cast<int>(m_queue.size()) - m_waiting + m_inFlight; }

private:
    struct Job {
        ChunkPos pos;
        uint32_t version = 0;
        ChunkNeighbourhood input;
        ChunkLight output;
    };
    void request(ChunkPos pos);
    bool neighbourhoodLoaded(ChunkPos pos) const;
    void run();

    World& m_world;
    std::unordered_map<ChunkPos, uint32_t> m_versions; // latest requested
    std::vector<ChunkPos> m_queue;                     // waiting for a free worker slot
    std::unordered_set<ChunkPos> m_queued;             // same contents, fast lookup
    int m_inFlight = 0;
    int m_waiting = 0; // queued chunks found missing neighbours in the last update
    int m_maxInFlight = 8;
    WorkQueue<std::unique_ptr<Job>> m_jobs;
    WorkQueue<std::unique_ptr<Job>> m_done;
    std::vector<std::thread> m_threads;
};

} // namespace mc::world
