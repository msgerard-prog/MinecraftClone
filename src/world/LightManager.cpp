#include "world/LightManager.h"

#include <algorithm>

namespace mc::world {

LightManager::LightManager(World& world, int threads)
    : m_world(world), m_maxInFlight(std::max(2, threads * 3)) {
    for (int i = 0; i < threads; ++i)
        m_threads.emplace_back([this] { run(); });
}

LightManager::~LightManager() {
    m_jobs.close(/*discard=*/true);
    for (auto& t : m_threads)
        t.join();
}

void LightManager::run() {
    while (auto job = m_jobs.popWait()) {
        (*job)->output = computeChunkLight((*job)->input);
        (*job)->input = {}; // release the shared sections early
        m_done.push(std::move(*job));
    }
}

bool LightManager::neighbourhoodLoaded(ChunkPos pos) const {
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!m_world.chunk({pos.x + dx, pos.z + dz})) return false;
    return true;
}

void LightManager::request(ChunkPos pos) {
    if (m_queued.insert(pos).second) m_queue.push_back(pos);
}

void LightManager::update(const std::vector<ChunkPos>& loaded,
                          const std::vector<ChunkPos>& unloaded,
                          const std::vector<BlockPos>& edited, std::vector<ChunkPos>& newlyLit,
                          std::vector<SectionPos>& relitSections) {
    newlyLit.clear();
    relitSections.clear();

    for (const ChunkPos& p : unloaded) {
        m_versions.erase(p);
        if (m_queued.erase(p))
            m_queue.erase(std::remove(m_queue.begin(), m_queue.end(), p), m_queue.end());
    }
    // A new chunk may complete the neighbourhood of itself or any neighbour.
    for (const ChunkPos& p : loaded) {
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{p.x + dx, p.z + dz};
                const Chunk* c = m_world.chunk(q);
                if (c && !c->lit() && !m_versions.contains(q)) request(q);
            }
    }
    // An edit can change light up to 15 blocks away: relight the 3x3 chunks around it.
    for (const BlockPos& b : edited) {
        const ChunkPos c = b.chunk();
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{c.x + dx, c.z + dz};
                if (const Chunk* ch = m_world.chunk(q); ch && ch->lit()) request(q);
            }
    }

    // Results: install if still current; report what changed.
    while (auto job = m_done.tryPop()) {
        --m_inFlight;
        const auto it = m_versions.find((*job)->pos);
        Chunk* chunk = m_world.chunk((*job)->pos);
        if (it == m_versions.end() || it->second != (*job)->version || !chunk) continue;
        const bool first = !chunk->lit();
        if (!first) {
            for (int s = 0; s < kSectionsPerChunk; ++s) {
                const auto& before = chunk->light(s);
                if (!before || !(*before == *(*job)->output[s])) {
                    relitSections.push_back({(*job)->pos.x, (kMinY >> 4) + s, (*job)->pos.z});
                }
            }
        }
        chunk->setLight(std::move((*job)->output));
        if (first) newlyLit.push_back((*job)->pos);
    }

    // Submit: chunks whose 3x3 neighbourhood is loaded, up to the in-flight cap.
    // Chunks still missing neighbours (e.g. the outer ring of the loaded area) wait
    // without counting as pending work.
    m_waiting = 0;
    size_t i = 0;
    while (i < m_queue.size() && m_inFlight < m_maxInFlight) {
        const ChunkPos p = m_queue[i];
        if (!neighbourhoodLoaded(p)) {
            ++m_waiting;
            ++i;
            continue;
        }
        auto job = std::make_unique<Job>();
        ChunkNeighbourhood::capture(m_world, p, job->input);
        job->pos = p;
        job->version = ++m_versions[p];
        m_jobs.push(std::move(job));
        ++m_inFlight;
        m_queued.erase(p);
        m_queue.erase(m_queue.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

} // namespace mc::world
