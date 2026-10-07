#pragma once

#include "core/WorkQueue.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"

#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

namespace mc::world {

// Streams chunks around a centre (the player): generates missing chunks nearest
// first on worker threads, inserts finished ones into the World (main thread), and
// unloads chunks that fell out of range. Generation radius is renderDistance + 1 so
// every chunk inside the render distance has all 8 neighbours (needed for meshing);
// unloading waits until renderDistance + 3 so chunks don't flicker at the edge.
class ChunkLoader {
public:
    ChunkLoader(World& world, const TerrainGenerator& generator, int threads);
    ~ChunkLoader();
    ChunkLoader(const ChunkLoader&) = delete;
    ChunkLoader& operator=(const ChunkLoader&) = delete;

    void setRenderDistance(int chunks);
    int renderDistance() const { return m_renderDistance; }

    // Main thread, once per frame. Appends chunks inserted / removed this call to
    // `loaded` / `unloaded` (cleared first) so the renderer can follow.
    void update(ChunkPos center, std::vector<ChunkPos>& loaded, std::vector<ChunkPos>& unloaded);

    // Chunks wanted but not yet in the World (queued + generating).
    int pending() const { return static_cast<int>(m_queue.size()) + m_inFlight; }

    // True if (dx, dz) from the centre is inside a circular radius (vanilla 1.18+
    // loads/renders a rounded circle, not a square).
    static bool inRadius(int dx, int dz, int radius) {
        return dx * dx + dz * dz <= radius * radius + radius;
    }

private:
    void rebuildQueue(ChunkPos center);
    void run();

    World& m_world;
    const TerrainGenerator& m_generator;
    int m_renderDistance = 12;
    bool m_haveCenter = false;
    ChunkPos m_center{};
    std::vector<ChunkPos> m_queue;            // wanted chunks, sorted far -> near (pop from back)
    std::unordered_set<ChunkPos> m_requested; // in flight
    int m_inFlight = 0;
    int m_maxInFlight = 8;
    WorkQueue<ChunkPos> m_jobs;
    WorkQueue<std::unique_ptr<Chunk>> m_done;
    std::vector<std::thread> m_threads;
};

} // namespace mc::world
