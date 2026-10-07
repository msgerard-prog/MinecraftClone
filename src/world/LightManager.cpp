#include "world/LightManager.h"

#include <algorithm>

namespace mc::world {

LightManager::LightManager(World& world, int threads)
    : m_world(world), m_maxInFlight(std::max(2, threads * 3)) {
    for (int i = 0; i < m_maxInFlight; ++i)
        m_free.push_back(std::make_unique<Job>());
    m_editQueue.reserve(64);
    m_queue.reserve(1024);
    m_pendingEdits.reserve(64);
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
        Job& j = **job;
        j.output = computeChunkLight(j.input);
        const int sections = j.input.height.sections();
        j.input = {}; // release the shared sections early
        // Compare with the old light here, not on the main thread; unchanged sections
        // keep their old instance (and the new copy is freed on this thread).
        j.changed = 0;
        for (int s = 0; s < sections; ++s) {
            const auto& before = j.before[s];
            if (before && (before == j.output[s] || *before == *j.output[s])) {
                j.output[s] = before;
            } else {
                j.changed |= 1u << s;
            }
        }
        j.before = {};
        m_done.push(std::move(*job));
    }
}

bool LightManager::neighbourhoodLoaded(ChunkPos pos) const {
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!m_world.chunk({pos.x + dx, pos.z + dz})) return false;
    return true;
}

void LightManager::request(ChunkPos pos, bool edit) {
    Chunk* c = m_world.chunk(pos);
    if (!c || c->lightJob.queued) return;
    c->lightJob.queued = true;
    (edit ? m_editQueue : m_queue).push_back(pos);
}

bool LightManager::submit(ChunkPos pos, std::vector<BlockPos>& editsReady) {
    Chunk* c = m_world.chunk(pos);
    if (!c || !c->lightJob.queued) return true; // unloaded (or a stale duplicate): drop
    auto job = std::move(m_free.back());
    m_free.pop_back();
    if (!ChunkNeighbourhood::capture(m_world, pos, job->input)) {
        // A neighbour unloaded since the request; it is requested again on reload.
        job->input = {};
        m_free.push_back(std::move(job));
        c->lightJob.queued = false;
        if (c->lightJob.version == 0) { // no job will come: don't hold its edits
            std::erase_if(m_pendingEdits, [&](const BlockPos& b) {
                if (b.chunk() != pos) return false;
                editsReady.push_back(b);
                return true;
            });
        }
        return true;
    }
    for (int s = 0; s < c->sectionCount(); ++s)
        job->before[s] = c->light(s);
    job->pos = pos;
    job->version = m_nextVersion++;
    c->lightJob = {job->version, false};
    m_jobs.push(std::move(job));
    ++m_inFlight;
    return true;
}

void LightManager::update(const std::vector<ChunkPos>& loaded,
                          const std::vector<ChunkPos>& unloaded,
                          const std::vector<BlockPos>& edited, std::vector<ChunkPos>& newlyLit,
                          std::vector<SectionPos>& relitSections,
                          std::vector<BlockPos>& editsReady) {
    newlyLit.clear();
    relitSections.clear();
    editsReady.clear();

    // Unloaded chunks: their queue entries are dropped at submit time (chunk gone or
    // reset), and their edits are moot.
    if (!unloaded.empty()) {
        std::erase_if(m_pendingEdits, [&](const BlockPos& b) { return !m_world.chunk(b.chunk()); });
    }
    // A new chunk may complete its own neighbourhood or a neighbour's: request
    // exactly the chunks whose 3x3 is now complete (so nothing waits in the queue).
    for (const ChunkPos& p : loaded) {
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{p.x + dx, p.z + dz};
                const Chunk* c = m_world.chunk(q);
                if (c && !c->lit() && c->lightJob.version == 0 && neighbourhoodLoaded(q))
                    request(q, false);
            }
    }
    // An edit can change light up to 15 blocks away: relight the 3x3 chunks around
    // it - every chunk that is lit or has a light job (whose input is now stale).
    for (const BlockPos& b : edited) {
        const ChunkPos c = b.chunk();
        const Chunk* centre = m_world.chunk(c);
        if (!centre || (!centre->lit() && centre->lightJob.version == 0)) {
            editsReady.push_back(b); // not lit yet: nothing to wait for
        } else {
            m_pendingEdits.push_back(b);
        }
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{c.x + dx, c.z + dz};
                const Chunk* ch = m_world.chunk(q);
                if (ch && (ch->lit() || ch->lightJob.version != 0)) request(q, true);
            }
    }

    // Results: install if still current; report what changed.
    while (auto done = m_done.tryPop()) {
        auto job = std::move(*done);
        --m_inFlight;
        Chunk* chunk = m_world.chunk(job->pos);
        if (chunk && chunk->lightJob.version == job->version) {
            const bool first = !chunk->lit();
            if (!first) {
                for (int s = 0; s < chunk->sectionCount(); ++s)
                    if (job->changed & (1u << s))
                        relitSections.push_back({job->pos.x, chunk->height().minSection() + s, job->pos.z});
            }
            chunk->setLight(job->output);
            if (first) newlyLit.push_back(job->pos);
            // Edits in this chunk now have current light, unless another job for it
            // is still queued (a later edit): then they wait for that one.
            if (!chunk->lightJob.queued) {
                std::erase_if(m_pendingEdits, [&](const BlockPos& b) {
                    if (b.chunk() != job->pos) return false;
                    editsReady.push_back(b);
                    return true;
                });
            }
        }
        job->output = {};
        m_free.push_back(std::move(job));
    }

    // Submit: edits first, then streaming, up to the in-flight cap.
    size_t e = 0;
    while (e < m_editQueue.size() && !m_free.empty())
        submit(m_editQueue[e++], editsReady);
    m_editQueue.erase(m_editQueue.begin(), m_editQueue.begin() + static_cast<std::ptrdiff_t>(e));
    while (m_head < m_queue.size() && !m_free.empty())
        submit(m_queue[m_head++], editsReady);
    if (m_head == m_queue.size() || m_head > 4096) { // drop the consumed prefix
        m_queue.erase(m_queue.begin(), m_queue.begin() + static_cast<std::ptrdiff_t>(m_head));
        m_head = 0;
    }
}

} // namespace mc::world
