#include "world/ChunkLoader.h"

#include <algorithm>

namespace mc::world {

ChunkLoader::ChunkLoader(World& world, const TerrainGenerator& generator, int threads)
    : m_world(world), m_generator(generator), m_maxInFlight(std::max(2, threads * 3)) {
    m_requested.reserve(static_cast<size_t>(m_maxInFlight));
    for (int i = 0; i < threads; ++i)
        m_threads.emplace_back([this] { run(); });
}

ChunkLoader::~ChunkLoader() {
    m_jobs.close(/*discard=*/true);
    for (auto& t : m_threads)
        t.join();
}

void ChunkLoader::setRenderDistance(int chunks) {
    m_renderDistance = std::max(2, chunks);
    m_haveCenter = false; // force a queue rebuild
}

bool ChunkLoader::wanted(int dx, int dz, int renderDistance) {
    for (int oz = -1; oz <= 1; ++oz)
        for (int ox = -1; ox <= 1; ++ox)
            if (inRadius(dx + ox, dz + oz, renderDistance)) return true;
    return false;
}

bool ChunkLoader::isRequested(ChunkPos p) const {
    return std::find(m_requested.begin(), m_requested.end(), p) != m_requested.end();
}

void ChunkLoader::run() {
    // Workers only touch the generator (immutable) and the chunk they build.
    while (auto pos = m_jobs.popWait()) {
        std::unique_ptr<Chunk> chunk;
        if (auto reused = m_recycled.tryPop()) {
            chunk = std::move(*reused);
            chunk->reset(*pos);
        } else {
            chunk = std::make_unique<Chunk>(*pos);
        }
        m_generator.generate(*chunk);
        m_done.push(std::move(chunk));
    }
}

void ChunkLoader::rebuildQueue(ChunkPos center) {
    const int r = m_renderDistance + 1;
    m_queue.clear();
    for (int dz = -r; dz <= r; ++dz) {
        for (int dx = -r; dx <= r; ++dx) {
            if (!wanted(dx, dz, m_renderDistance)) continue;
            const ChunkPos p{center.x + dx, center.z + dz};
            if (!m_world.chunk(p) && !isRequested(p)) m_queue.push_back(p);
        }
    }
    std::sort(m_queue.begin(), m_queue.end(), [&](const ChunkPos& a, const ChunkPos& b) {
        const int da = (a.x - center.x) * (a.x - center.x) + (a.z - center.z) * (a.z - center.z);
        const int db = (b.x - center.x) * (b.x - center.x) + (b.z - center.z) * (b.z - center.z);
        return da > db; // nearest at the back
    });
}

void ChunkLoader::update(ChunkPos center, std::vector<ChunkPos>& loaded,
                         std::vector<ChunkPos>& unloaded) {
    loaded.clear();
    unloaded.clear();

    // 1. Integrate finished chunks (only if still wanted; otherwise recycle).
    while (auto chunk = m_done.tryPop()) {
        const ChunkPos p = (*chunk)->pos();
        m_requested.erase(std::find(m_requested.begin(), m_requested.end(), p));
        if (wanted(p.x - center.x, p.z - center.z, m_renderDistance)) {
            m_world.insertChunk(std::move(*chunk));
            loaded.push_back(p);
        } else {
            m_recycled.push(std::move(*chunk));
        }
    }

    // 2. Re-plan when the centre chunk changes (or after a render distance change).
    if (!m_haveCenter || !(center == m_center)) {
        m_center = center;
        m_haveCenter = true;
        rebuildQueue(center);
        // Unload chunks 2+ chunks beyond the wanted area (hysteresis).
        const int keep = m_renderDistance + 3;
        m_far.clear(); // reused buffer: no allocation on the frame path
        m_world.forEachChunk([&](const Chunk& c) {
            if (!inRadius(c.pos().x - center.x, c.pos().z - center.z, keep))
                m_far.push_back(c.pos());
        });
        for (const ChunkPos& p : m_far) {
            if (auto chunk = m_world.removeChunk(p)) m_recycled.push(std::move(chunk));
            unloaded.push_back(p);
        }
    }

    // 3. Submit the nearest wanted chunks, keeping a bounded number in flight.
    while (!m_queue.empty() && static_cast<int>(m_requested.size()) < m_maxInFlight) {
        const ChunkPos p = m_queue.back();
        m_queue.pop_back();
        if (m_world.chunk(p) || isRequested(p)) continue;
        m_requested.push_back(p);
        m_jobs.push(p);
    }
}

} // namespace mc::world
