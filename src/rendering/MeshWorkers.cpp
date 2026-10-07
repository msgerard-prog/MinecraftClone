#include "rendering/MeshWorkers.h"

#include "rendering/ChunkMesher.h"

namespace mc::gfx {

MeshWorkers::MeshWorkers(const world::BlockRegistry& registry, const BlockModels& models,
                         int threadCount, int jobCapacity)
    : m_registry(registry), m_models(models) {
    for (int i = 0; i < jobCapacity; ++i) {
        auto job = std::make_unique<MeshJob>();
        job->padded.resize(world::kPaddedVolume);
        job->vertices.reserve(4096); // grows to the largest mesh it has held, then reused
        m_free.push(std::move(job));
    }
    for (int i = 0; i < threadCount; ++i)
        m_threads.emplace_back([this] { run(); });
}

MeshWorkers::~MeshWorkers() {
    m_pending.close(/*discard=*/true); // don't mesh the backlog on exit
    for (auto& t : m_threads)
        t.join();
}

std::unique_ptr<MeshJob> MeshWorkers::acquireJob() {
    auto job = m_free.tryPop();
    return job ? std::move(*job) : nullptr;
}

void MeshWorkers::submit(std::unique_ptr<MeshJob> job) { m_pending.push(std::move(job)); }

std::unique_ptr<MeshJob> MeshWorkers::takeResult() {
    auto job = m_done.tryPop();
    return job ? std::move(*job) : nullptr;
}

void MeshWorkers::recycle(std::unique_ptr<MeshJob> job) { m_free.push(std::move(job)); }

void MeshWorkers::run() {
    while (auto job = m_pending.popWait()) {
        const world::SectionPos& p = (*job)->pos;
        meshSection((*job)->padded.data(), glm::ivec3(p.x * 16, p.y * 16, p.z * 16), m_registry,
                    m_models, (*job)->vertices);
        m_done.push(std::move(*job));
    }
}

} // namespace mc::gfx
