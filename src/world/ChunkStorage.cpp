#include "world/ChunkStorage.h"

#include "core/Log.h"

#include <chrono>

namespace mc::world {

ChunkStorage::ChunkStorage(std::filesystem::path worldDir, bool legacyWorld)
    : m_dir(std::move(worldDir)), m_legacyWorld(legacyWorld), m_thread([this] { run(); }) {}

ChunkStorage::~ChunkStorage() {
    flush();
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    m_thread.join();
}

RegionFile* ChunkStorage::region(ChunkPos pos, bool create, bool entities) {
    const std::pair key{pos.x >> 5, pos.z >> 5};
    auto& regions = entities ? m_entityRegions : m_regions;
    auto it = regions.find(key);
    if (it != regions.end()) return it->second.get();
    // Known-missing files (most worlds have few entities/ regions): no filesystem
    // check per chunk load. Creating the file clears the entry.
    auto& missing = entities ? m_missingEntityRegions : m_missingRegions;
    if (!create && missing.contains(key)) return nullptr;
    auto file = std::make_unique<RegionFile>();
    const auto path = m_dir / (entities ? "entities" : "region") /
                      ("r." + std::to_string(key.first) + "." + std::to_string(key.second) + ".mca");
    if (!file->open(path, create)) {
        // Loads never create files: a missing region just means "never saved".
        if (create) MC_LOG_ERROR("Can't open region file %s", path.string().c_str());
        else missing.insert(key);
        return nullptr;
    }
    missing.erase(key);
    if (regions.size() >= 64) regions.erase(regions.begin()); // bound open handles
    return regions.emplace(key, std::move(file)).first->second.get();
}

bool ChunkStorage::load(Chunk& chunk) {
    const ChunkPos pos = chunk.pos();
    {
        std::lock_guard lock(m_mutex);
        if (auto it = m_pending.find(pos); it != m_pending.end()) {
            // Not written yet: copy the queued snapshot's sections.
            const ChunkSnapshot& snap = it->second.snapshot;
            for (int s = 0; s < chunk.sectionCount(); ++s)
                chunk.mutableSection(s) = *snap.sections[size_t(s)];
            if (snap.biomes) chunk.setBiomes(snap.biomes);
            chunk.furnaces() = snap.furnaces;
            chunk.chests() = snap.chests;
            chunk.spawners() = snap.spawners;
            chunk.brewingStands() = snap.brewing;
            chunk.comparators() = snap.comparators;
            chunk.hoppers() = snap.hoppers;
            chunk.dispensers() = snap.dispensers;
            chunk.mobs() = snap.mobs;
            chunk.blockTicks() = snap.blockTicks; // delays (see ChunkSnapshot::of)
            chunk.ticksRelative = !snap.blockTicks.empty();
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
    if (!nbt || !chunkFromNbt(*nbt, chunk, &unknown, m_legacyWorld)) {
        MC_LOG_WARN("Chunk %d,%d has invalid data; regenerating it", pos.x, pos.z);
        return false;
    }
    if (unknown > 0) MC_LOG_WARN("Chunk %d,%d: %d unknown block states became air", pos.x, pos.z, unknown);
    // Entities live in their own region files (1.17+); a chunk without one has none.
    {
        std::optional<std::vector<uint8_t>> ebytes;
        {
            std::lock_guard lock(m_fileMutex);
            RegionFile* r = region(pos, /*create=*/false, /*entities=*/true);
            if (r && r->has(RegionFile::index(pos.x, pos.z))) ebytes = r->read(RegionFile::index(pos.x, pos.z));
        }
        if (ebytes) {
            try {
                if (auto en = nbt::read(*ebytes)) entitiesFromNbt(*en, chunk);
            } catch (const std::bad_alloc&) {
            }
        }
    }
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
            // Entities file: written when there are mobs, or to clear an old one.
            RegionFile* er = region(pos, /*create=*/!snap.mobs.empty(), /*entities=*/true);
            if (er && (!snap.mobs.empty() || er->has(RegionFile::index(pos.x, pos.z)))) {
                const auto ebytes = nbt::write(entitiesToNbt(snap));
                ok = ok && er->write(RegionFile::index(pos.x, pos.z), ebytes, now);
            }
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
