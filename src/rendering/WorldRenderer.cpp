#include "rendering/WorldRenderer.h"

#include "core/Files.h"
#include "core/Window.h"
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
    // Half the cores: leaves room for the main thread and the GL driver's own thread.
    const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    m_maxInFlight = threads * 4; // bounds job memory and per-frame dispatch work
    m_workers =
        std::make_unique<MeshWorkers>(world::blockRegistry(), m_models, threads, m_maxInFlight);
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
            markDirty({p.x, sy, p.z});
    }
}

void WorldRenderer::markAllDirty(const world::World& world) {
    world.forEachChunk([&](const world::Chunk& c) {
        for (int sy = kMinSectionY; sy <= kMaxSectionY; ++sy) {
            markDirty({c.pos().x, sy, c.pos().z});
        }
    });
}

void WorldRenderer::markDirty(world::SectionPos pos) {
    SectionState& st = m_states[pos];
    if (st.dirty) return;
    st.dirty = true;
    m_dirtyList.push_back(pos);
    m_dirtyUnsorted = true;
}

void WorldRenderer::eraseIfIdle(world::SectionPos pos) {
    const auto it = m_states.find(pos);
    if (it != m_states.end() && it->second.inFlight == 0 && !it->second.dirty) m_states.erase(it);
}

void WorldRenderer::update(const world::World& world, const glm::dvec3& cameraPos) {
    // 1. Upload finished meshes. A result older than the section's latest submission
    //    is stale (the section changed again meanwhile) and is dropped.
    while (auto job = m_workers->takeResult()) {
        --m_inFlight;
        const auto it = m_states.find(job->pos);
        if (it != m_states.end()) {
            --it->second.inFlight;
            if (it->second.version == job->version) {
                m_chunks.uploadSection(job->pos, job->vertices);
            }
            eraseIfIdle(job->pos);
        }
        m_workers->recycle(std::move(job));
    }

    // 2. Dispatch, nearest sections first (the list is sorted far -> near, so the
    //    nearest is at the back), within a time budget and the in-flight cap.
    if (m_dirtyUnsorted) {
        auto distance2 = [&](const world::SectionPos& p) {
            const glm::dvec3 c(p.x * 16.0 + 8.0, p.y * 16.0 + 8.0, p.z * 16.0 + 8.0);
            const glm::dvec3 d = c - cameraPos;
            return glm::dot(d, d);
        };
        std::sort(m_dirtyList.begin(), m_dirtyList.end(),
                  [&](const auto& a, const auto& b) { return distance2(a) > distance2(b); });
        m_dirtyUnsorted = false;
    }
    constexpr double kBudgetSeconds = 0.002; // main-thread snapshot time per frame
    const double start = timeSeconds();
    while (!m_dirtyList.empty() && m_inFlight < m_maxInFlight &&
           timeSeconds() - start < kBudgetSeconds) {
        const world::SectionPos pos = m_dirtyList.back();
        m_dirtyList.pop_back();
        SectionState& st = m_states[pos];
        st.dirty = false;

        const world::Chunk* chunk = world.chunk({pos.x, pos.z});
        // Empty sections have no faces of their own (neighbours mesh their sides).
        if (!chunk || chunk->section(pos.y - kMinSectionY).isEmpty()) {
            ++st.version; // drops any in-flight result for it
            m_chunks.removeSection(pos);
            eraseIfIdle(pos);
            continue;
        }
        auto job = m_workers->acquireJob(); // never null while under the cap
        job->pos = pos;
        job->version = ++st.version;
        ++st.inFlight;
        world::snapshotSection(world, pos, job->padded.data());
        m_workers->submit(std::move(job));
        ++m_inFlight;
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
