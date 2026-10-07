#pragma once

#include "core/WorkQueue.h"
#include "world/ChunkStorage.h"
#include "world/ChunkGenerator.h"
#include "world/World.h"

#include <memory>
#include <thread>
#include <vector>

namespace mc::world {

// Streams chunks around a centre (the player): generates missing chunks nearest
// first on worker threads, inserts finished ones into the World (main thread), and
// unloads chunks that fell out of range.
//
// Wanted set: every chunk inside the render circle plus two rings of neighbours
// (lighting needs a 3x3 neighbourhood, meshing needs lit neighbours). Unloading waits until a chunk
// is 2 chunks beyond that, so chunks don't flicker at the edge. Unloaded chunks are recycled:
// workers reuse them instead of allocating, and the main thread frees nothing.
class ChunkLoader {
public:
    // `storage` (optional): chunks saved before are loaded instead of generated, and
    // changed chunks are saved when they unload.
    ChunkLoader(World& world, const ChunkGenerator& generator, int threads,
                ChunkStorage* storage = nullptr);
    ~ChunkLoader();
    ChunkLoader(const ChunkLoader&) = delete;
    ChunkLoader& operator=(const ChunkLoader&) = delete;

    void setRenderDistance(int chunks);
    // The current game time (unloaded chunks save their scheduled ticks relative to it).
    void setGameTime(int64_t t) { m_gameTime = t; }
    int renderDistance() const { return m_renderDistance; }

    // Main thread, once per frame. Appends chunks inserted / removed this call to
    // `loaded` / `unloaded` (cleared first) so the renderer can follow.
    void update(ChunkPos center, std::vector<ChunkPos>& loaded, std::vector<ChunkPos>& unloaded);

    // Chunks wanted but not yet in the World (queued + generating).
    int pending() const {
        return static_cast<int>(m_queue.size()) + static_cast<int>(m_requested.size());
    }

    // Rounded circle (vanilla 1.18+ loads and renders a cylinder, not a square).
    static bool inRadius(int dx, int dz, int radius) {
        return dx * dx + dz * dz <= radius * radius + radius;
    }
    // True if the chunk at (dx, dz) from the centre is inside the render circle or
    // within 2 chunks of a chunk that is.
    static bool wanted(int dx, int dz, int renderDistance);

private:
    int64_t m_gameTime = 0;
    void rebuildQueue(ChunkPos center);
    bool isRequested(ChunkPos p) const;
    void run();

    World& m_world;
    const ChunkGenerator& m_generator;
    ChunkStorage* m_storage = nullptr;
    int m_renderDistance = 12;
    bool m_haveCenter = false;
    ChunkPos m_center{};
    std::vector<ChunkPos> m_queue;     // wanted chunks, sorted far -> near (pop from back)
    std::vector<ChunkPos> m_requested; // in flight (small: at most m_maxInFlight)
    std::vector<ChunkPos> m_far;       // reused: chunks to unload
    int m_maxInFlight = 8;
    WorkQueue<ChunkPos> m_jobs;
    WorkQueue<std::unique_ptr<Chunk>> m_done;
    WorkQueue<std::unique_ptr<Chunk>> m_recycled; // unloaded chunks for workers to reuse
    std::vector<std::thread> m_threads;
};

} // namespace mc::world
