#pragma once

#include "core/WorkQueue.h"
#include "rendering/BlockModels.h"
#include "rendering/ChunkMesher.h"
#include "rendering/PackedVertex.h"
#include "world/BlockRegistry.h"
#include "world/SectionSnapshot.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace mc::gfx {

// One unit of meshing work. Jobs (and their buffers) are recycled, so steady-state
// meshing does not allocate.
struct MeshJob {
    world::SectionPos pos;
    uint32_t version = 0;                    // stale results are dropped by the owner
    std::vector<world::BlockStateId> padded; // input: 18^3 snapshot
    SectionMesh mesh;                        // output (opaque + translucent)
};

// Worker threads that run meshSection on snapshots. GL-free: uploads happen on the
// main thread (WorldRenderer). Registry and models must outlive the workers and stay
// unchanged while they run.
class MeshWorkers {
public:
    // Allocates `jobCapacity` jobs up front; no jobs are created later.
    MeshWorkers(const world::BlockRegistry& registry, const BlockModels& models, int threadCount,
                int jobCapacity);
    ~MeshWorkers();
    MeshWorkers(const MeshWorkers&) = delete;
    MeshWorkers& operator=(const MeshWorkers&) = delete;

    // A free job, or nullptr if all `jobCapacity` jobs are in use.
    std::unique_ptr<MeshJob> acquireJob();
    void submit(std::unique_ptr<MeshJob> job);
    // Finished job, or nullptr if none is ready. Return it with recycle() when done.
    std::unique_ptr<MeshJob> takeResult();
    void recycle(std::unique_ptr<MeshJob> job);

    int threadCount() const { return static_cast<int>(m_threads.size()); }

private:
    void run();

    const world::BlockRegistry& m_registry;
    const BlockModels& m_models;
    WorkQueue<std::unique_ptr<MeshJob>> m_pending;
    WorkQueue<std::unique_ptr<MeshJob>> m_done;
    WorkQueue<std::unique_ptr<MeshJob>> m_free;
    std::vector<std::thread> m_threads;
};

} // namespace mc::gfx
