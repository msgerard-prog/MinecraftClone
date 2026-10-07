#include "world/ChunkStorage.h"

#include "core/Log.h"

#include <chrono>

namespace mc::world {

ChunkStorage::ChunkStorage(std::filesystem::path worldDir)
    : m_dir(std::move(worldDir)), m_thread([this] { run(); }) {}

ChunkStorage::~ChunkStorage() {
    flush();
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    m_thread.join();
}

RegionFile* ChunkStorage::region(ChunkPos pos, bool create) {
    const std::pair key{pos.x >> 5, pos.z >> 5};
    auto it = m_regions.find(key);
    if (it != m_regions.end()) return it->second.get();
    auto file = std::make_unique<RegionFile>();
    const auto path = m_dir / "region" /
                      ("r." + std::to_string(key.first) + "." + std::to_string(key.second) + ".mca");
    if (!file->open(path, create)) {
        // Loads never create files: a missing region just means "never saved".
        if (create) MC_LOG_ERROR("Can't open region file %s", path.string().c_str());
        return nullptr;
    }
    if (m_regions.size() >= 64) m_regions.erase(m_regions.begin()); // bound open handles
    return m_regions.emplace(key, std::move(file)).first->second.get();
}

bool ChunkStorage::load(Chunk& chunk) {
    const ChunkPos pos = chunk.pos();
    {
        std::lock_guard lock(m_mutex);
        if (auto it = m_pending.find(pos); it != m_pending.end()) {
            // Not written yet: copy the queued snapshot's sections.
            const ChunkSnapshot& snap = it->second.snapshot;
            for (int s = 0; s < kSectionsPerChunk; ++s)
                chunk.mutableSection(s) = *snap.sections[size_t(s)];
            if (snap.biomes) chunk.setBiomes(snap.biomes);
            chunk.furnaces() = snap.furnaces;
            chunk.clearDirty();
            return true;
        }
    }
    std::optional<std::vector<uint8_t>> bytes;
    {
        std::lock_guard lock(m_fileMutex);
        RegionFile* r = region(pos, /*create=*/false);
        if (!r || !r->has(RegionFile::index(pos.x, pos.z))) return false;
        bytes = r->read(RegionFile::index(pos.x, pos.z));
    }
    if (!bytes) {
        MC_LOG_WARN("Chunk %d,%d is unreadable; regenerating it", pos.x, pos.z);
        return false;
    }
    std::optional<nbt::Compound> nbt;
    try { // malformed data must not take the worker (and the game) down
        nbt = nbt::read(*bytes);
    } catch (const std::bad_alloc&) {
        nbt.reset();
    }
    int unknown = 0;
    if (!nbt || !chunkFromNbt(*nbt, chunk, &unknown)) {
        MC_LOG_WARN("Chunk %d,%d has invalid data; regenerating it", pos.x, pos.z);
        return false;
    }
    if (unknown > 0) MC_LOG_WARN("Chunk %d,%d: %d unknown block states became air", pos.x, pos.z, unknown);
    chunk.clearDirty();
    return true;
}

void ChunkStorage::save(ChunkSnapshot snapshot) {
    {
        std::lock_guard lock(m_mutex);
        const ChunkPos pos = snapshot.pos;
        Pending& p = m_pending[pos];
        p.snapshot = std::move(snapshot);
        p.failed = false;
        // Queue it unless it is already waiting. If the IO thread is writing an older
        // snapshot right now, this queues a second write (regression: that newer
        // snapshot used to be dropped).
        if (!p.queued) {
            p.queued = true;
            m_queue.push_back(pos);
        }
    }
    m_wake.notify_one();
}

int ChunkStorage::queued() const {
    std::lock_guard lock(m_mutex);
    int n = 0;
    for (const auto& [pos, p] : m_pending)
        n += p.failed ? 0 : 1;
    return n;
}

void ChunkStorage::flush() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [&] { return m_queue.empty() && !m_writing; });
}

void ChunkStorage::run() {
    std::unique_lock lock(m_mutex);
    for (;;) {
        m_wake.wait(lock, [&] { return m_stop || !m_queue.empty(); });
        if (m_queue.empty()) {
            if (m_stop) return;
            continue;
        }
        const ChunkPos pos = m_queue.front();
        m_queue.pop_front();
        Pending& pending = m_pending.at(pos);
        pending.queued = false;
        ChunkSnapshot snap = pending.snapshot; // copy (shared pointers): stays pending until written
        m_writing = true;
        lock.unlock();

        const auto bytes = nbt::write(chunkToNbt(snap));
        const auto now = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                                   std::chrono::system_clock::now().time_since_epoch())
                                                   .count());
        bool ok = false;
        {
            std::lock_guard files(m_fileMutex);
            RegionFile* r = region(pos, /*create=*/true);
            ok = r && r->write(RegionFile::index(pos.x, pos.z), bytes, now);
            if (!ok) MC_LOG_ERROR("Failed to save chunk %d,%d (kept in memory)", pos.x, pos.z);
        }

        lock.lock();
        // Done with it unless a newer snapshot arrived meanwhile (then it is queued
        // again) or the write failed (then reloads keep using the snapshot).
        if (auto it = m_pending.find(pos); it != m_pending.end() && !it->second.queued) {
            if (ok) m_pending.erase(it);
            else it->second.failed = true;
        }
        m_writing = false;
        if (m_queue.empty()) m_idle.notify_all();
    }
}

} // namespace mc::world
