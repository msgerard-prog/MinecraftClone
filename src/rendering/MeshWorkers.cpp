#include "rendering/MeshWorkers.h"

#include "rendering/ChunkMesher.h"

namespace mc::gfx {

MeshWorkers::MeshWorkers(const world::BlockRegistry& registry, const BlockModels& models,
                         int threadCount, int jobCapacity)
    : m_registry(registry), m_models(models) {
    for (int i = 0; i < jobCapacity; ++i) {
        auto job = std::make_unique<MeshJob>();
        job->padded.resize(world::kPaddedVolume);
        job->sky.resize(world::kPaddedVolume);
        job->blockLight.resize(world::kPaddedVolume);
        job->mesh.opaque.reserve(4096); // grows to the largest mesh it has held, then reused
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
        MeshJob& j = **job;
        world::buildPadded(j.refs, j.padded.data(), j.sky.data(), j.blockLight.data());
        const auto biomes = std::move(j.refs.biomes);
        const int minSection = j.refs.minSection;
        j.refs = {}; // release the shared sections early
        const world::SectionPos& p = j.pos;
        const int section = p.y - minSection; // index in the chunk (biome cells)
        meshSection(j.padded.data(), j.sky.data(), j.blockLight.data(),
                    glm::ivec3(p.x * 16, p.y * 16, p.z * 16), m_registry, m_models, j.mesh,
                    biomes ? &biomes->cells[size_t(section * world::ChunkBiomes::kPerSection)]
                           : nullptr);
        m_done.push(std::move(*job));
    }
}

} // namespace mc::gfx
