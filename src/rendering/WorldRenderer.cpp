#include "rendering/WorldRenderer.h"

#include "core/Files.h"
#include "core/Window.h"
#include "core/Log.h"
#include "rendering/Fog.h"
#include "rendering/ResourcePack.h"
#include "world/Biome.h"
#include "world/Blocks.h"
#include "world/DayTime.h"
#include "world/Rotation.h"

#include <glad/gl.h>

#include <algorithm>
#include <bit>
#include <glm/gtc/type_ptr.hpp>
#include <thread>

namespace mc::gfx {

namespace {

// Vanilla's daytime sky colour at plains biome (#78A7FF), until biomes exist (M8).


} // namespace

WorldRenderer::~WorldRenderer() {
    m_workers.reset(); // join mesh threads before the models they read go away
    if (m_queries[0]) glDeleteQueries(kQueryRing, m_queries);
    if (m_tintPalette) glDeleteBuffers(1, &m_tintPalette);
}

bool WorldRenderer::init(const std::string& resourcePacksDir) {
    if (!m_blockShader.load("block")) return false;
    // Pack stack: our placeholders at the bottom (the repo root holds `assets/`), then
    // the user's packs on top.
    m_packs = std::make_unique<PackStack>();
    PackStack& packs = *m_packs;
    packs.add(ResourcePack::open(std::filesystem::path(MC_ASSETS_DIR).parent_path()));
    packs.addAllIn(resourcePacksDir);
    // Blocks and items share one atlas (as vanilla's): item sprites are "item/<name>".
    const TextureAtlas::AtlasFolder folders[] = {{"assets/minecraft/textures/block/", ""},
                                                 {"assets/minecraft/textures/item/", "item/"}};
    if (!m_atlas.build(packs, folders)) return false;
    m_models.bake(world::blockRegistry(), m_atlas);
    if (!m_chunks.init() || !m_translucent.init() || !m_sky.init(packs)) return false;
    if (!m_clouds.init(packs)) MC_LOG_WARN("Clouds disabled");
    // Tint palette (shader binding 1): grass, foliage, water per biome slot, plus the
    // fixed birch/spruce foliage slots (wiki: Leaves).
    {
        auto rgb = [](uint32_t c) {
            return glm::vec4(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f,
                             (c & 255) / 255.0f, 1.0f);
        };
        std::vector<glm::vec4> palette(256 * 3, glm::vec4(1.0f));
        for (int b = 0; b < static_cast<int>(world::Biome::Count); ++b) {
            const auto& info = world::biomeInfo(static_cast<world::Biome>(b));
            palette[size_t(b) * 3 + 0] = rgb(info.grass);
            palette[size_t(b) * 3 + 1] = rgb(info.foliage);
            palette[size_t(b) * 3 + 2] = rgb(info.water);
        }
        palette[size_t(world::kBirchFoliageSlot) * 3 + 1] = rgb(0x80A755);
        palette[size_t(world::kSpruceFoliageSlot) * 3 + 1] = rgb(0x619961);
        for (int p = 0; p < 16; ++p)
            palette[size_t(world::kRedstoneSlot0 + p) * 3 + 1] = rgb(world::redstoneColor(p));
        glCreateBuffers(1, &m_tintPalette);
        glNamedBufferStorage(m_tintPalette, GLsizeiptr(palette.size() * sizeof(glm::vec4)),
                             palette.data(), 0);
    }
    setDayTime(6000, 0.0f); // noon until the game says otherwise
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

void WorldRenderer::setDayTime(int64_t dayTime, float partialTick, float rain, float thunder) {
    const double angle = world::celestialAngle(dayTime, partialTick);
    m_skyDarken = static_cast<float>(world::skyDarken(angle, rain, thunder));
    m_rain = rain;
    m_thunder = thunder;
    // The sky: the biome's colour times the daylight (wiki: Sky). Rain greys it toward
    // 60% of its luminance, thunder darkens it toward 20% (vanilla: each blends 75% of
    // the way at full strength).
    const float day = static_cast<float>(world::daylight(angle));
    m_skyColor = m_biomeSky * day;
    const auto grey = [&](float strength, float level) {
        const float l = glm::dot(m_skyColor, glm::vec3(0.3f, 0.59f, 0.11f)) * level;
        m_skyColor = glm::mix(m_skyColor, glm::vec3(l), strength * 0.75f);
    };
    if (rain > 0.0f) grey(rain, 0.6f);
    if (thunder > 0.0f) grey(thunder, 0.2f);
    // The fog: the biome's fog colour, darkened at night but never to black (vanilla
    // keeps 6% of red/green and 9% of blue: a deep blue night horizon).
    m_fogBase = m_biomeFog * glm::vec3(day * 0.94f + 0.06f, day * 0.94f + 0.06f, day * 0.91f + 0.09f);
    const world::SunriseColor sr = world::sunriseColor(angle);
    m_sunrise = {sr.r, sr.g, sr.b, sr.a * (1.0f - rain)};
    m_sunSide = {world::sunDirection(angle).x >= 0.0 ? 1.0f : -1.0f, 0.0f};
    m_skyState = {angle, static_cast<float>(world::starBrightness(angle)), world::moonPhase(dayTime), true,
                  1.0f - rain};
    // Clouds: white by day, dim grey at night (vanilla keeps 10% red/green, 15% blue),
    // greyer in rain and dark in thunder (wiki: Cloud - rgb 30,30,30 in storms).
    glm::vec3 cloud(day * 0.9f + 0.1f, day * 0.9f + 0.1f, day * 0.85f + 0.15f);
    const auto greyCloud = [&](float strength, float level) {
        const float l = glm::dot(cloud, glm::vec3(0.3f, 0.59f, 0.11f)) * level;
        cloud = glm::mix(cloud, glm::vec3(l), strength * 0.95f);
    };
    if (rain > 0.0f) greyCloud(rain, 0.6f);
    if (thunder > 0.0f) greyCloud(thunder, 0.2f);
    m_cloudColor = glm::vec4(cloud, 0.8f);
    if (m_dimension != world::Dimension::Overworld) {
        // No daylight: fixed fog colours (wiki: Nether Wastes fog #330808; the End's
        // fog #A080A0 x 0.15), no sun, moon, stars or glow.
        m_skyDarken = 0.0f;
        m_skyState.celestial = false;
        m_sunrise = glm::vec4(0.0f);
        m_skyColor = m_fogBase =
            m_dimension == world::Dimension::Nether ? m_netherFog : glm::vec3(0xA0, 0x80, 0xA0) / 255.0f * 0.15f;
    }
}

void WorldRenderer::setRenderDistance(int chunks) {
    m_renderDistance = chunks;
    const uint64_t columns = uint64_t(2 * chunks + 1) * uint64_t(2 * chunks + 1);
    m_chunks.reserve(static_cast<uint32_t>(std::bit_ceil(std::min<uint64_t>(columns * 2700, 1u << 26))));
}

void WorldRenderer::setDimension(world::Dimension d) {
    const bool changed = d != m_dimension;
    m_dimension = d;
    m_minSection = world::dimensionInfo(d).height.minSection(); // vanilla heights per dimension
    m_maxSection = world::dimensionInfo(d).height.maxSection();
    if (!changed) return;
    m_chunks.removeAll();
    m_translucent.removeAll();
    m_dirtyList.clear();
    m_sortedDirty = 0;
    for (auto it = m_states.begin(); it != m_states.end();) {
        ++it->second.version; // results still in flight are stale
        it->second.dirty = false;
        it = it->second.inFlight == 0 ? m_states.erase(it) : std::next(it);
    }
}

void WorldRenderer::markChunkDirty(const world::World& world, world::ChunkPos pos) {
    constexpr int kNeighbours[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : kNeighbours) {
        const world::ChunkPos p{pos.x + d[0], pos.z + d[1]};
        if (!world.chunk(p)) continue;
        for (int sy = m_minSection; sy <= m_maxSection; ++sy)
            markDirty({p.x, sy, p.z});
    }
}

void WorldRenderer::markAllDirty(const world::World& world) {
    world.forEachChunk([&](const world::Chunk& c) {
        for (int sy = m_minSection; sy <= m_maxSection; ++sy) {
            markDirty({c.pos().x, sy, c.pos().z});
        }
    });
}

void WorldRenderer::markChunkSections(const world::World& world, world::ChunkPos pos) {
    // Only non-empty sections: a freshly meshable chunk has no old meshes to clear,
    // and empty sections have no faces of their own (~70% fewer jobs).
    const world::Chunk* chunk = world.chunk(pos);
    if (!chunk) return;
    for (int sy = m_minSection; sy <= m_maxSection; ++sy) {
        if (!chunk->section(sy - m_minSection).isEmpty()) markDirty({pos.x, sy, pos.z});
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
                    if (p.y < m_minSection || p.y > m_maxSection) continue;
                    if (!m_meshTracker.isMeshed({p.x, p.z})) continue;
                    markDirty(p);
                }
    }
}

void WorldRenderer::onChunksUnloaded(const std::vector<world::ChunkPos>& unloaded) {
    m_meshTracker.onUnloaded(unloaded);
    // Every dimension's section range, not just the current one: on a dimension switch
    // the old dimension's chunks are unloaded after the range has changed.
    constexpr int kLowest = world::kOverworldHeight.minSection();
    constexpr int kHighest = std::max({world::kOverworldHeight.maxSection(), world::kNetherHeight.maxSection(),
                                       world::kEndHeight.maxSection()});
    for (const world::ChunkPos& p : unloaded) {
        for (int sy = kLowest; sy <= kHighest; ++sy) {
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
            if (p.y < m_minSection || p.y > m_maxSection) continue;
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
        // Empty sections have no faces of their own (neighbours mesh their sides); a
        // section outside this dimension's height (left from the last one) has none.
        const bool outside = pos.y < m_minSection || pos.y > m_maxSection;
        if (!chunk || outside || chunk->section(pos.y - m_minSection).isEmpty()) {
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
            job->refs = {}; // don't keep partly captured sections alive in the pool
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
    // The frame's fog colour (vanilla FogRenderer): looking toward a rising or setting
    // sun tints it with the glow; it leans toward the sky colour at short render
    // distances; rain and thunder darken it.
    m_fogColor = m_fogBase;
    if (m_dimension == world::Dimension::Overworld) {
        const glm::vec3 look = world::lookVector(camera.yaw, camera.pitch);
        if (m_sunrise.a > 0.0f && m_renderDistance >= 4) {
            const float toward = std::max(0.0f, look.x * m_sunSide.x + look.z * m_sunSide.y);
            m_fogColor = glm::mix(m_fogColor, glm::vec3(m_sunrise), m_sunrise.a * toward);
        }
        const float v = 1.0f - std::pow(0.25f + 0.75f * float(std::min(m_renderDistance, 32)) / 32.0f, 0.25f);
        m_fogColor += (m_skyColor - m_fogColor) * v;
        if (m_rain > 0.0f) m_fogColor *= glm::vec3(1.0f - m_rain * 0.5f, 1.0f - m_rain * 0.5f, 1.0f - m_rain * 0.4f);
        if (m_thunder > 0.0f) m_fogColor *= 1.0f - m_thunder * 0.5f;
    }
    glClearColor(m_fogColor.r, m_fogColor.g, m_fogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const float aspect = float(framebufferWidth) / float(framebufferHeight);
    if (m_dimension == world::Dimension::Overworld)
        m_sky.drawGradient(camera, aspect, m_skyColor, m_fogColor, m_sunrise, m_sunSide);
    else if (m_dimension == world::Dimension::End)
        m_sky.drawEndSky(camera, aspect);
    m_sky.draw(camera, aspect, m_skyState);
    m_blockShader.bind();
    // Fixed locations/bindings: see docs/architecture.md › Rendering.
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1i(1, m_atlas.columns());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_tintPalette); // biome tints
    // Distance fog toward the sky colour so the edge of the loaded world fades out.
    FogRange fog = terrainFog(m_renderDistance);
    if (m_dimension == world::Dimension::Nether) fog = netherFog(m_renderDistance);
    glUniform2f(4, fog.start, fog.end);
    glUniform3fv(5, 1, glm::value_ptr(m_fogColor));
    glUniform1f(7, m_skyDarken);
    // Dimension light: the Nether's ambient light lifts darkness (0.1); the End's
    // lightmap is forced bright (wiki: Dimension type › ambient_light; Light).
    glUniform1f(8, m_nightVision ? 1.0f : world::dimensionInfo(m_dimension).ambientLight);
    glUniform1f(9, m_dimension == world::Dimension::End ? 1.0f : 0.0f);
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
    // Clouds (Overworld; none below render distance 4, wiki: Cloud), last: they're
    // translucent and almost always farther than water surfaces.
    if (m_dimension == world::Dimension::Overworld && m_renderDistance >= 4)
        m_clouds.draw(camera, viewProj, m_cloudTime, float(m_renderDistance * 16), m_cloudColor);
    glEndQuery(GL_TIME_ELAPSED);
    m_queryPending[q] = true;
    m_queryIndex = (q + 1) % kQueryRing;
}

} // namespace mc::gfx
