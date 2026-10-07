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

RegionFile* ChunkStorage::region(ChunkPos pos) {
    const std::pair key{pos.x >> 5, pos.z >> 5};
    auto it = m_regions.find(key);
    if (it != m_regions.end()) return it->second.get();
    auto file = std::make_unique<RegionFile>();
    const auto path = m_dir / "region" /
                      ("r." + std::to_string(key.first) + "." + std::to_string(key.second) + ".mca");
    if (!file->open(path)) {
        MC_LOG_ERROR("Can't open region file %s", path.string().c_str());
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
            for (int s = 0; s < kSectionsPerChunk; ++s)
                chunk.mutableSection(s) = *it->second.sections[size_t(s)];
            chunk.clearDirty();
            return true;
        }
    }
    std::optional<std::vector<uint8_t>> bytes;
    {
        std::lock_guard lock(m_fileMutex);
        RegionFile* r = region(pos);
        if (!r || !r->has(RegionFile::index(pos.x, pos.z))) return false;
        bytes = r->read(RegionFile::index(pos.x, pos.z));
    }
    if (!bytes) {
        MC_LOG_WARN("Chunk %d,%d is unreadable; regenerating it", pos.x, pos.z);
        return false;
    }
    const auto nbt = nbt::read(*bytes);
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
        if (m_pending.insert_or_assign(pos, std::move(snapshot)).second) m_queue.push_back(pos);
    }
    m_wake.notify_one();
}

int ChunkStorage::queued() const {
    std::lock_guard lock(m_mutex);
    return static_cast<int>(m_pending.size());
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
        ChunkSnapshot snap = m_pending.at(pos); // copy (shared pointers): stays pending until written
        m_writing = true;
        lock.unlock();

        const auto bytes = nbt::write(chunkToNbt(snap));
        const auto now = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                                   std::chrono::system_clock::now().time_since_epoch())
                                                   .count());
        {
            std::lock_guard files(m_fileMutex);
            RegionFile* r = region(pos);
            if (!r || !r->write(RegionFile::index(pos.x, pos.z), bytes, now))
                MC_LOG_ERROR("Failed to save chunk %d,%d", pos.x, pos.z);
        }

        lock.lock();
        // Drop the pending entry unless a newer snapshot replaced it meanwhile
        // (then it was queued again).
        if (auto it = m_pending.find(pos); it != m_pending.end() &&
                                           it->second.sections == snap.sections &&
                                           std::find(m_queue.begin(), m_queue.end(), pos) == m_queue.end())
            m_pending.erase(it);
        m_writing = false;
        if (m_queue.empty()) m_idle.notify_all();
    }
}

} // namespace mc::world
