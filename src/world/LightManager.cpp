#include "world/LightManager.h"

#include <algorithm>

namespace mc::world {

LightManager::LightManager(World& world, int threads)
    : m_world(world), m_maxInFlight(std::max(2, threads * 3)) {
    for (int i = 0; i < m_maxInFlight; ++i)
        m_free.push_back(std::make_unique<Job>());
    m_editQueue.reserve(1024);
    m_queue.reserve(1024);
    m_settleQueue.reserve(1024);
    m_settleWanted.reserve(1024);
    m_pendingEdits.reserve(4096); // fires and decaying leaves edit continuously
    m_incWaiting.reserve(4096);
    m_incKeep.reserve(4096);
    m_fullEdits.reserve(4096);
    m_retry.reserve(256);
    for (auto& j : m_free) j->inc.edits.reserve(4096); // (a TNT frame's edits fit)
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
        if (j.incremental) { // (M31.1)
            updateLightIncremental(j.inc, j.incOut);
            j.inc.blocks = {};
            j.inc.light = {};
            m_done.push(std::move(*job));
            continue;
        }
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

void LightManager::request(ChunkPos pos, Priority priority) {
    Chunk* c = m_world.chunk(pos);
    if (!c) return;
    // A more urgent request moves a waiting chunk up to its queue (the older entry
    // is dropped at submit: the chunk is no longer queued by then).
    if (c->lightJob.queued && c->lightJob.priority >= priority) return;
    c->lightJob.queued = true;
    c->lightJob.priority = priority;
    (priority == kEdit ? m_editQueue : priority == kStream ? m_queue : m_settleQueue).push_back(pos);
}

LightManager::Incremental LightManager::incrementalState(ChunkPos centre) const {
    bool busy = false;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = m_world.chunk({centre.x + dx, centre.z + dz});
            if (!c || !c->lit()) return Incremental::Unlit;
            busy = busy || c->lightJob.inFlight > 0; // (a queued full job computes from the newest blocks anyway)
        }
    return busy ? Incremental::Busy : Incremental::Ready;
}

void LightManager::submitIncremental(ChunkPos centre, const BlockPos* edits, size_t count) {
    auto job = std::move(m_free.back());
    m_free.pop_back();
    job->incremental = true;
    job->pos = centre;
    job->version = m_nextVersion++;
    ChunkNeighbourhood::capture(m_world, centre, job->inc.blocks);
    for (int i = 0; i < 9; ++i) {
        Chunk* c = m_world.chunk({centre.x + i % 3 - 1, centre.z + i / 3 - 1});
        for (int s = 0; s < c->sectionCount(); ++s) job->inc.light[size_t(i)][size_t(s)] = c->light(s);
        ++c->lightJob.inFlight;
        c->lightJob.incremental = job->version;
    }
    job->inc.edits.assign(edits, edits + count);
    m_jobs.push(std::move(job));
    ++m_inFlight;
}

bool LightManager::submit(ChunkPos pos, std::vector<BlockPos>& editsReady) {
    Chunk* c = m_world.chunk(pos);
    if (!c || !c->lightJob.queued) return true; // unloaded (or a stale duplicate): drop
    if (c->lightJob.inFlight > 0 && c->lightJob.incremental != 0) { // (M31.1) wait for the update
        m_retry.push_back(pos);
        return true;
    }
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
    job->incremental = false;
    {
        const uint32_t lastSettle = c->lightJob.lastSettle;
        const bool settleWanted = c->lightJob.settleWanted;
        const uint8_t inFlight = c->lightJob.inFlight;
        c->lightJob = {job->version, false};
        c->lightJob.lastSettle = lastSettle;
        c->lightJob.settleWanted = settleWanted;
        c->lightJob.inFlight = uint8_t(inFlight + 1);
    }
    m_jobs.push(std::move(job));
    ++m_inFlight;
    return true;
}

void LightManager::update(const std::vector<ChunkPos>& loaded, const std::vector<ChunkPos>& unloaded,
                          const std::vector<BlockPos>& edited, const std::vector<BlockPos>& settling,
                          std::vector<ChunkPos>& newlyLit, std::vector<SectionPos>& relitSections,
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
    for (const ChunkPos& p : loaded) // (M31 review) jobs submitted before this load aren't its
        if (Chunk* c = m_world.chunk(p)) c->lightJob.epoch = m_nextVersion;
    for (const ChunkPos& p : loaded) {
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{p.x + dx, p.z + dz};
                const Chunk* c = m_world.chunk(q);
                if (c && !c->lit() && c->lightJob.version == 0 && neighbourhoodLoaded(q))
                    request(q, kStream);
            }
    }
    // Settling edits: re-meshed now; their chunks (3x3) relit in the background, each
    // at most every kSettleFrames frames (a later request waits for its slot, so the
    // final state always gets relit).
    ++m_frame;
    for (const BlockPos& b : settling) {
        editsReady.push_back(b);
        const ChunkPos c = b.chunk();
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const ChunkPos q{c.x + dx, c.z + dz};
                Chunk* ch = m_world.chunk(q);
                if (!ch || !(ch->lit() || ch->lightJob.version != 0) || ch->lightJob.settleWanted) continue;
                ch->lightJob.settleWanted = true;
                m_settleWanted.push_back(q);
            }
    }
    std::erase_if(m_settleWanted, [&](const ChunkPos& q) {
        Chunk* ch = m_world.chunk(q);
        if (!ch || !ch->lightJob.settleWanted) return true; // unloaded (reset)
        if (m_frame - ch->lightJob.lastSettle < kSettleFrames) return false;
        ch->lightJob.settleWanted = false;
        ch->lightJob.lastSettle = m_frame;
        request(q, kSettle);
        return true;
    });

    // Results: install if still current; report what changed.
    while (auto done = m_done.tryPop()) {
        auto job = std::move(*done);
        --m_inFlight;
        if (job->incremental) { // (M31.1) the 3x3 chunks' corrected sections
            for (int i = 0; i < 9; ++i) {
                const ChunkPos cp{job->pos.x + i % 3 - 1, job->pos.z + i / 3 - 1};
                Chunk* c = m_world.chunk(cp);
                if (!c || c->lightJob.incremental != job->version) continue; // (reloaded since)
                if (c->lightJob.inFlight > 0) --c->lightJob.inFlight;
                c->lightJob.incremental = 0;
                if (job->incOut.changed[size_t(i)] == 0) continue;
                ChunkLight now;
                for (int s = 0; s < c->sectionCount(); ++s) {
                    now[size_t(s)] = (job->incOut.changed[size_t(i)] & (1u << s)) ? job->incOut.light[size_t(i)][size_t(s)]
                                                                                 : c->light(s);
                    if (job->incOut.changed[size_t(i)] & (1u << s))
                        relitSections.push_back({cp.x, c->height().minSection() + s, cp.z});
                }
                c->setLight(std::move(now));
            }
            for (const BlockPos& b : job->inc.edits) editsReady.push_back(b);
            job->inc.edits.clear();
            job->incOut = {};
            m_free.push_back(std::move(job));
            continue;
        }
        Chunk* chunk = m_world.chunk(job->pos);
        if (chunk && chunk->lightJob.inFlight > 0 && job->version >= chunk->lightJob.epoch) --chunk->lightJob.inFlight;
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
            if (!chunk->lightJob.queued || chunk->lightJob.priority == kSettle) { // (a settling relight may wait long)
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

    // (M31 review) After installing finished jobs and before streaming takes the free ones:
    // edits get the jobs first and see chunks whose jobs just ended as idle.
    // (M31.1) Edits are corrected incrementally where the 3x3 chunks around them are lit;
    // edits in busy chunks wait for their jobs; elsewhere (chunks still streaming in)
    // the whole 3x3 is relit as before.
    for (const BlockPos& b : edited) m_incWaiting.push_back(b);
    m_incKeep.clear();
    if (!m_incWaiting.empty()) {
        std::sort(m_incWaiting.begin(), m_incWaiting.end(), // (grouping only: no order to keep, no buffer)
                         [](const BlockPos& a, const BlockPos& b) { return a.chunk().key() < b.chunk().key(); });
        size_t i = 0;
        while (i < m_incWaiting.size()) {
            const ChunkPos centre = m_incWaiting[i].chunk();
            size_t j = i;
            while (j < m_incWaiting.size() && m_incWaiting[j].chunk() == centre) ++j;
            const Incremental st = incrementalState(centre);
            if (st == Incremental::Ready && !m_free.empty()) {
                submitIncremental(centre, m_incWaiting.data() + i, j - i);
            } else if (st == Incremental::Unlit) {
                for (size_t k = i; k < j; ++k) m_fullEdits.push_back(m_incWaiting[k]);
            } else {
                for (size_t k = i; k < j; ++k) m_incKeep.push_back(m_incWaiting[k]);
            }
            i = j;
        }
        m_incWaiting.swap(m_incKeep);
    }
    // An edit can change light up to 15 blocks away: relight the 3x3 chunks around
    // it - every chunk that is lit or has a light job (whose input is now stale).
    for (const BlockPos& b : m_fullEdits) {
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
                if (ch && (ch->lit() || ch->lightJob.version != 0)) request(q, kEdit);
            }
    }

    m_fullEdits.clear();
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
    // Settling goes after streaming; while chunks still stream only one a frame (so
    // flowing lava doesn't stay dark during a long flight).
    const size_t budget = m_head < m_queue.size() ? 1 : m_settleQueue.size();
    size_t st = 0;
    while (st < m_settleQueue.size() && st < budget && !m_free.empty())
        submit(m_settleQueue[st++], editsReady);
    m_settleQueue.erase(m_settleQueue.begin(), m_settleQueue.begin() + static_cast<std::ptrdiff_t>(st));
    // (M31.1) full jobs held back by an incremental update go first next frame.
    for (const ChunkPos& p : m_retry) { // (back to the queue it came from)
        const Chunk* c = m_world.chunk(p);
        const uint8_t prio = c ? c->lightJob.priority : uint8_t(kEdit);
        (prio == kEdit ? m_editQueue : prio == kStream ? m_queue : m_settleQueue).push_back(p);
    }
    m_retry.clear();
}

} // namespace mc::world
