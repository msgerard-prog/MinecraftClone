#include "rendering/WorldRenderer.h"

#include "core/Files.h"
#include "world/Blocks.h"

#include <glad/gl.h>

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <thread>

namespace mc::gfx {

namespace {

// Vanilla's daytime sky colour at plains biome (#78A7FF).
constexpr float kSkyR = 0x78 / 255.0f;
constexpr float kSkyG = 0xA7 / 255.0f;
constexpr float kSkyB = 0xFF / 255.0f;
// Vanilla plains grass colour (#91BD59), until biomes exist (M8).
constexpr glm::vec3 kPlainsGrass(0x91 / 255.0f, 0xBD / 255.0f, 0x59 / 255.0f);

constexpr int kMinSectionY = world::kMinY >> 4; // -4
constexpr int kMaxSectionY = world::kMaxY >> 4; // 19

} // namespace

bool WorldRenderer::init() {
    if (!m_blockShader.load("block")) return false;
    if (!m_atlas.build(assetPath("minecraft/textures/block"))) return false;
    m_models.bake(world::blockRegistry(), m_atlas);
    if (!m_chunks.init()) return false;
    // Leave one core for the main thread.
    const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    m_workers = std::make_unique<MeshWorkers>(world::blockRegistry(), m_models, threads);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // back faces (clockwise from the camera) are never visible
    return true;
}

void WorldRenderer::markChunkDirty(const world::World& world, world::ChunkPos pos) {
    constexpr int kNeighbours[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : kNeighbours) {
        const world::ChunkPos p{pos.x + d[0], pos.z + d[1]};
        if (!world.chunk(p)) continue;
        for (int sy = kMinSectionY; sy <= kMaxSectionY; ++sy)
            m_dirty.insert({p.x, sy, p.z});
    }
}

void WorldRenderer::markAllDirty(const world::World& world) {
    world.forEachChunk([&](const world::Chunk& c) {
        for (int sy = kMinSectionY; sy <= kMaxSectionY; ++sy) {
            m_dirty.insert({c.pos().x, sy, c.pos().z});
        }
    });
}

void WorldRenderer::update(const world::World& world) {
    // 1. Dispatch: snapshot each dirty section on the main thread (the only thread
    //    that may read the World), mesh it on a worker.
    for (const world::SectionPos& pos : m_dirty) {
        const world::Chunk* chunk = world.chunk({pos.x, pos.z});
        // Empty sections have no faces of their own (neighbours mesh their sides).
        if (!chunk || chunk->section(pos.y - kMinSectionY).isEmpty()) {
            ++m_versions[pos]; // drops any in-flight result for it
            m_chunks.removeSection(pos);
            continue;
        }
        auto job = m_workers->acquireJob();
        job->pos = pos;
        job->version = ++m_versions[pos];
        world::snapshotSection(world, pos, job->padded.data());
        m_workers->submit(std::move(job));
        ++m_inFlight;
    }
    m_dirty.clear();

    // 2. Upload finished meshes. A result whose version is older than the latest
    //    submission is stale (the section changed again) and is dropped.
    while (auto job = m_workers->takeResult()) {
        --m_inFlight;
        if (m_versions[job->pos] == job->version) m_chunks.uploadSection(job->pos, job->vertices);
        m_workers->recycle(std::move(job));
    }
}

void WorldRenderer::drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    const glm::mat4 viewProj =
        camera.viewProjectionAtOrigin(float(framebufferWidth) / float(framebufferHeight));
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(kSkyR, kSkyG, kSkyB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_blockShader.bind();
    // Fixed locations/bindings: see docs/architecture.md › Rendering.
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1i(1, m_atlas.columns());
    glUniform3fv(2, 1, glm::value_ptr(kPlainsGrass));
    glBindTextureUnit(0, m_atlas.texture());
    m_chunks.draw(camera, viewProj);
}

} // namespace mc::gfx
