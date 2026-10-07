#include "rendering/WorldRenderer.h"

#include "core/Files.h"
#include "core/Window.h"
#include "rendering/Fog.h"
#include "rendering/ResourcePack.h"
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
// Vanilla's default water colour (#3F76E4), until biomes exist (M8).
constexpr glm::vec3 kWater(0x3F / 255.0f, 0x76 / 255.0f, 0xE4 / 255.0f);

constexpr int kMinSectionY = world::kMinY >> 4; // -4
constexpr int kMaxSectionY = world::kMaxY >> 4; // 19

} // namespace

WorldRenderer::~WorldRenderer() {
    m_workers.reset(); // join mesh threads before the models they read go away
    if (m_queries[0]) glDeleteQueries(kQueryRing, m_queries);
}

bool WorldRenderer::init(const std::string& resourcePacksDir) {
    if (!m_blockShader.load("block")) return false;
    // Pack stack: our placeholders at the bottom (the repo root holds `assets/`), then
    // the user's packs on top.
    PackStack packs;
    packs.add(ResourcePack::open(std::filesystem::path(MC_ASSETS_DIR).parent_path()));
    packs.addAllIn(resourcePacksDir);
    if (!m_atlas.build(packs, "assets/minecraft/textures/block/")) return false;
    m_models.bake(world::blockRegistry(), m_atlas);
    if (!m_chunks.init() || !m_translucent.init()) return false;
    glCreateQueries(GL_TIME_ELAPSED, kQueryRing, m_queries);
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

void WorldRenderer::markChunkSections(const world::World& world, world::ChunkPos pos) {
    // Only non-empty sections: a freshly meshable chunk has no old meshes to clear,
    // and empty sections have no faces of their own (~70% fewer jobs).
    const world::Chunk* chunk = world.chunk(pos);
    if (!chunk) return;
    for (int sy = kMinSectionY; sy <= kMaxSectionY; ++sy) {
        if (!chunk->section(sy - kMinSectionY).isEmpty()) markDirty({pos.x, sy, pos.z});
    }
}

void WorldRenderer::onChunksLit(const world::World& world,
                                const std::vector<world::ChunkPos>& lit) {
    m_streaming = true;
    m_ready.clear();
    m_meshTracker.onLoaded(world, lit, m_ready);
    for (const world::ChunkPos& p : m_ready)
        markChunkSections(world, p);
}

void WorldRenderer::onLightChanged(const std::vector<world::SectionPos>& sections) {
    // A section's mesh reads light from its 26 neighbours: re-mesh all of them.
    for (const world::SectionPos& s : sections) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx) {
                    const world::SectionPos p{s.x + dx, s.y + dy, s.z + dz};
                    if (p.y < kMinSectionY || p.y > kMaxSectionY) continue;
                    if (!m_meshTracker.isMeshed({p.x, p.z})) continue;
                    markDirty(p);
                }
    }
}

void WorldRenderer::onChunksUnloaded(const std::vector<world::ChunkPos>& unloaded) {
    m_meshTracker.onUnloaded(unloaded);
    for (const world::ChunkPos& p : unloaded) {
        for (int sy = kMinSectionY; sy <= kMaxSectionY; ++sy) {
            const world::SectionPos s{p.x, sy, p.z};
            m_chunks.removeSection(s);
            m_translucent.removeSection(s);
            if (const auto it = m_states.find(s); it != m_states.end()) {
                ++it->second.version; // drop any result still in flight
                eraseIfIdle(s);
            }
        }
    }
}

void WorldRenderer::onBlocksChanged(const std::vector<world::BlockPos>& changed) {
    for (const world::BlockPos& b : changed) {
        const world::SectionPos s{world::blockToChunk(b.x), b.y >> 4, world::blockToChunk(b.z)};
        const int lx = world::blockToLocal(b.x), ly = world::blockToLocal(b.y),
                  lz = world::blockToLocal(b.z);
        const world::SectionPos around[7] = {
            s,
            {s.x + (lx == 0 ? -1 : 0), s.y, s.z},
            {s.x + (lx == 15 ? 1 : 0), s.y, s.z},
            {s.x, s.y + (ly == 0 ? -1 : 0), s.z},
            {s.x, s.y + (ly == 15 ? 1 : 0), s.z},
            {s.x, s.y, s.z + (lz == 0 ? -1 : 0)},
            {s.x, s.y, s.z + (lz == 15 ? 1 : 0)},
        };
        for (const world::SectionPos& p : around) {
            if (p.y < kMinSectionY || p.y > kMaxSectionY) continue;
            // While streaming, only chunks that already have meshes are re-meshed (a
            // chunk without all neighbours would show walls).
            if (m_streaming && !m_meshTracker.isMeshed({p.x, p.z})) continue;
            markDirty(p);
        }
    }
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
                m_chunks.uploadSection(job->pos, job->mesh.opaque);
                m_translucent.uploadSection(job->pos, job->mesh.translucent);
            }
            eraseIfIdle(job->pos);
        }
        m_workers->recycle(std::move(job));
    }

    // 2. Dispatch, nearest sections first (the list is sorted far -> near, so the
    //    nearest is at the back), within a time budget and the in-flight cap.
    if (m_dirtyUnsorted) {
        // Far -> near (nearest at the back). Only the newly appended tail is sorted,
        // then merged with the already-sorted prefix through a reused scratch buffer.
        auto distance2 = [&](const world::SectionPos& p) {
            const glm::dvec3 c(p.x * 16.0 + 8.0, p.y * 16.0 + 8.0, p.z * 16.0 + 8.0);
            const glm::dvec3 d = c - cameraPos;
            return glm::dot(d, d);
        };
        auto farFirst = [&](const world::SectionPos& a, const world::SectionPos& b) {
            return distance2(a) > distance2(b);
        };
        const auto mid = m_dirtyList.begin() +
                         static_cast<std::ptrdiff_t>(std::min(m_sortedDirty, m_dirtyList.size()));
        std::sort(mid, m_dirtyList.end(), farFirst);
        m_mergeScratch.resize(m_dirtyList.size());
        std::merge(m_dirtyList.begin(), mid, mid, m_dirtyList.end(), m_mergeScratch.begin(),
                   farFirst);
        m_dirtyList.swap(m_mergeScratch);
        m_sortedDirty = m_dirtyList.size();
        m_dirtyUnsorted = false;
    }
    constexpr double kBudgetSeconds = 0.002; // main-thread snapshot time per frame
    const double start = timeSeconds();
    while (!m_dirtyList.empty() && m_inFlight < m_maxInFlight &&
           timeSeconds() - start < kBudgetSeconds) {
        const world::SectionPos pos = m_dirtyList.back();
        m_dirtyList.pop_back();
        m_sortedDirty = std::min(m_sortedDirty, m_dirtyList.size());
        SectionState& st = m_states[pos];
        st.dirty = false;

        const world::Chunk* chunk = world.chunk({pos.x, pos.z});
        // Empty sections have no faces of their own (neighbours mesh their sides).
        if (!chunk || chunk->section(pos.y - kMinSectionY).isEmpty()) {
            ++st.version; // drops any in-flight result for it
            m_chunks.removeSection(pos);
            m_translucent.removeSection(pos);
            eraseIfIdle(pos);
            continue;
        }
        auto job = m_workers->acquireJob(); // never null while under the cap
        job->pos = pos;
        job->version = ++st.version;
        ++st.inFlight;
        if (!world::captureSection(world, pos, job->refs)) { // neighbours gone: skip
            --st.inFlight;
            m_workers->recycle(std::move(job));
            eraseIfIdle(pos);
            continue;
        }
        m_workers->submit(std::move(job));
        ++m_inFlight;
    }
}

void WorldRenderer::drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    const glm::mat4 viewProj =
        camera.viewProjectionAtOrigin(float(framebufferWidth) / float(framebufferHeight));
    // GPU timing: read the query issued kQueryRing frames ago (finished by now), then
    // reuse its slot for this frame.
    const int q = m_queryIndex;
    if (m_queryPending[q]) {
        // Allowed per-frame glGet (rendering/CLAUDE.md): availability first, so a
        // result that isn't ready is skipped instead of stalling the CPU.
        GLint available = 0;
        glGetQueryObjectiv(m_queries[q], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(m_queries[q], GL_QUERY_RESULT, &ns);
            const double ms = static_cast<double>(ns) / 1e6;
            m_gpuTotalMs += ms;
            m_gpuMaxMs = std::max(m_gpuMaxMs, ms);
            ++m_gpuSamples;
        }
    }
    glBeginQuery(GL_TIME_ELAPSED, m_queries[q]);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(kSkyR, kSkyG, kSkyB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_blockShader.bind();
    // Fixed locations/bindings: see docs/architecture.md › Rendering.
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1i(1, m_atlas.columns());
    glUniform3fv(2, 1, glm::value_ptr(kPlainsGrass));
    glUniform3fv(3, 1, glm::value_ptr(kWater));
    // Distance fog toward the sky colour so the edge of the loaded world fades out.
    const FogRange fog = terrainFog(m_renderDistance);
    glUniform2f(4, fog.start, fog.end);
    glUniform3f(5, kSkyR, kSkyG, kSkyB);
    glUniform1f(7, m_skyDarken);
    glBindTextureUnit(0, m_atlas.texture());

    // Opaque pass.
    // Nothing beyond the fog end is visible: skip those sections (vanilla doesn't draw
    // past the render distance either; chunks stay loaded a little further out).
    const float maxDistance = fog.end;
    glUniform1f(6, 0.5f); // cutout: torches, glass edges
    m_chunks.draw(camera, viewProj, false, maxDistance);
    glUniform1f(6, 0.0f);

    // Translucent pass: blended, no depth writes, far sections first. Back faces stay
    // culled; fluids emit reversed copies of the faces vanilla shows from both sides.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_translucent.draw(camera, viewProj, /*backToFront=*/true, maxDistance);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEndQuery(GL_TIME_ELAPSED);
    m_queryPending[q] = true;
    m_queryIndex = (q + 1) % kQueryRing;
}

} // namespace mc::gfx
