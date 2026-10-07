#include "rendering/WorldRenderer.h"

#include "core/Files.h"
#include "rendering/ChunkMesher.h"
#include "world/Blocks.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

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
    m_padded.resize(world::kPaddedVolume);
    m_vertices.reserve(kMaxSectionVertices);
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
    const auto& registry = world::blockRegistry();
    for (const world::SectionPos& pos : m_dirty) {
        const world::Chunk* chunk = world.chunk({pos.x, pos.z});
        if (!chunk) {
            m_chunks.removeSection(pos);
            continue;
        }
        // Empty sections have no faces of their own (neighbours mesh their sides).
        if (chunk->section(pos.y - kMinSectionY).isEmpty()) {
            m_chunks.removeSection(pos);
            continue;
        }
        world::snapshotSection(world, pos, m_padded.data());
        meshSection(m_padded.data(), registry, m_models, m_vertices);
        m_chunks.uploadSection(pos, m_vertices);
    }
    m_dirty.clear();
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
