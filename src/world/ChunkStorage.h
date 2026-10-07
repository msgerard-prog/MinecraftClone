#pragma once

#include "world/ChunkSerializer.h"
#include "world/RegionFile.h"

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace mc::world {

// A world's chunk storage: <world>/region/r.<x>.<z>.mca (Anvil). Loads run on the
// caller's thread (chunk-loader workers); saves are queued to one IO thread that
// serialises shared snapshots, so the main thread never waits on disk. A chunk
// queued for saving is served from its snapshot if it is loaded again before the
// write lands. Thread-safe.
class ChunkStorage {
public:
    explicit ChunkStorage(std::filesystem::path worldDir);
    ~ChunkStorage(); // writes everything still queued
    ChunkStorage(const ChunkStorage&) = delete;
    ChunkStorage& operator=(const ChunkStorage&) = delete;

    // Fills `chunk` (at its position) from disk; false if it was never saved.
    bool load(Chunk& chunk);
    void save(ChunkSnapshot snapshot);
    // Blocks until every queued save is on disk.
    void flush();
    int queued() const;

private:
    RegionFile* region(ChunkPos pos, bool create, bool entities = false); // m_fileMutex held
    void run();

    std::filesystem::path m_dir;
    mutable std::mutex m_mutex; // queue + pending
    std::condition_variable m_wake, m_idle;
    std::deque<ChunkPos> m_queue;
    struct Pending {
        ChunkSnapshot snapshot; // latest
        bool queued = false;    // in m_queue (re-queued if saved again while writing)
        bool failed = false;    // last write failed: kept in memory, retried on next save
    };
    std::unordered_map<ChunkPos, Pending> m_pending;
    bool m_writing = false;
    bool m_stop = false;
    std::mutex m_fileMutex; // region files
    std::map<std::pair<int, int>, std::unique_ptr<RegionFile>> m_regions;
    std::map<std::pair<int, int>, std::unique_ptr<RegionFile>> m_entityRegions; // entities/
    std::thread m_thread;
};

} // namespace mc::world
