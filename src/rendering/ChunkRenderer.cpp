#include "rendering/ChunkRenderer.h"

#include "core/Log.h"
#include "rendering/ChunkMesher.h"
#include "rendering/Frustum.h"

#include <glad/gl.h>

#include <algorithm>

#include <vector>

namespace mc::gfx {

namespace {

constexpr uint32_t kMaxQuadsPerSection = static_cast<uint32_t>(kMaxSectionVertices / 4);
constexpr uint32_t kInitialArenaQuads = 1u << 20; // 1M quads = 32 MiB

} // namespace

void sortDrawsBackToFront(std::span<const DrawCommand> commands, std::span<const glm::vec4> offsets,
                          std::span<DrawSortItem> scratch, std::span<DrawCommand> outCommands,
                          std::span<glm::vec4> outOffsets) {
    const size_t n = commands.size();
    for (size_t i = 0; i < n; ++i) {
        const glm::vec3 c = glm::vec3(offsets[i]) + glm::vec3(8.0f); // section centre
        scratch[i] = {glm::dot(c, c), static_cast<uint32_t>(i)};
    }
    std::sort(
        scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(n),
        [](const DrawSortItem& a, const DrawSortItem& b) { return a.distance2 > b.distance2; });
    for (size_t i = 0; i < n; ++i) {
        outCommands[i] = commands[scratch[i].index];
        outCommands[i].baseInstance = static_cast<uint32_t>(i);
        outOffsets[i] = offsets[scratch[i].index];
    }
}

ChunkRenderer::~ChunkRenderer() {
    const GLuint buffers[] = {m_indexBuffer, m_arena, m_commandBuffer, m_offsetBuffer};
    for (GLuint b : buffers)
        if (b) glDeleteBuffers(1, &b);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool ChunkRenderer::init() {
    // Shared quad index buffer, big enough for the worst-case section.
    std::vector<uint32_t> indices(static_cast<size_t>(kMaxQuadsPerSection) * 6);
    for (uint32_t q = 0; q < kMaxQuadsPerSection; ++q) {
        const uint32_t v = q * 4;
        const uint32_t quad[6] = {v, v + 1, v + 2, v, v + 2, v + 3};
        for (int i = 0; i < 6; ++i)
            indices[q * 6 + i] = quad[i];
    }
    glCreateBuffers(1, &m_indexBuffer);
    glNamedBufferStorage(m_indexBuffer, static_cast<GLsizeiptr>(indices.size() * 4), indices.data(),
                         0);

    glCreateVertexArrays(1, &m_vao);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribIFormat(m_vao, 0, 2, GL_UNSIGNED_INT, 0); // uvec2 aPacked
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glVertexArrayElementBuffer(m_vao, m_indexBuffer);

    growArena(kInitialArenaQuads);
    ensureDrawCapacity(1024);
    return true;
}

void ChunkRenderer::growArena(uint32_t minQuads) {
    uint32_t capacity = m_arenaAlloc.capacity() ? m_arenaAlloc.capacity() : kInitialArenaQuads;
    while (capacity < minQuads)
        capacity *= 2;
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, static_cast<GLsizeiptr>(capacity) * 4 * sizeof(PackedVertex),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);
    if (m_arena) {
        // Keep existing meshes: copy the old arena into the start of the new one.
        glCopyNamedBufferSubData(m_arena, buffer, 0, 0,
                                 static_cast<GLsizeiptr>(m_arenaAlloc.capacity()) * 4 *
                                     sizeof(PackedVertex));
        glDeleteBuffers(1, &m_arena);
        m_arenaAlloc.grow(capacity);
        MC_LOG_INFO("Chunk arena grown to %u quads", capacity);
    } else {
        m_arenaAlloc.reset(capacity);
    }
    m_arena = buffer;
    glVertexArrayVertexBuffer(m_vao, 0, m_arena, 0, sizeof(PackedVertex));
}

void ChunkRenderer::ensureDrawCapacity(size_t sections) {
    if (sections <= m_drawCapacity) return;
    size_t capacity = m_drawCapacity ? m_drawCapacity : 1024;
    while (capacity < sections)
        capacity *= 2;
    if (m_commandBuffer) glDeleteBuffers(1, &m_commandBuffer);
    if (m_offsetBuffer) glDeleteBuffers(1, &m_offsetBuffer);
    glCreateBuffers(1, &m_commandBuffer);
    glNamedBufferStorage(m_commandBuffer, static_cast<GLsizeiptr>(capacity * sizeof(DrawCommand)),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);
    glCreateBuffers(1, &m_offsetBuffer);
    glNamedBufferStorage(m_offsetBuffer, static_cast<GLsizeiptr>(capacity * sizeof(glm::vec4)),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_commands.resize(capacity);
    m_offsets.resize(capacity);
    m_sort.resize(capacity);
    m_sortedCommands.resize(capacity);
    m_sortedOffsets.resize(capacity);
    m_drawCapacity = capacity;
}

void ChunkRenderer::removeSection(world::SectionPos pos) {
    const auto it = m_sections.find(pos);
    if (it == m_sections.end()) return;
    m_arenaAlloc.free(it->second.range);
    m_quadsTotal -= it->second.range.size;
    m_sections.erase(it);
}

void ChunkRenderer::uploadSection(world::SectionPos pos, std::span<const PackedVertex> vertices) {
    removeSection(pos);
    if (vertices.empty()) return;
    const auto quads = static_cast<uint32_t>(vertices.size() / 4);
    auto range = m_arenaAlloc.allocate(quads);
    if (!range) {
        growArena(m_arenaAlloc.capacity() + quads);
        range = m_arenaAlloc.allocate(quads);
    }
    glNamedBufferSubData(m_arena, static_cast<GLintptr>(range->offset) * 4 * sizeof(PackedVertex),
                         static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data());
    m_sections[pos] = {*range};
    m_quadsTotal += quads;
    ensureDrawCapacity(m_sections.size());
}

void ChunkRenderer::draw(const Camera& camera, const glm::mat4& viewProjAtOrigin, bool backToFront,
                         float maxDistance) {
    const Frustum frustum = Frustum::fromMatrix(viewProjAtOrigin);
    uint32_t drawCount = 0;
    uint64_t quads = 0;
    for (const auto& [pos, entry] : m_sections) {
        // Section origin relative to the camera, in double, then narrowed.
        const glm::dvec3 origin(pos.x * 16.0, pos.y * 16.0, pos.z * 16.0);
        const glm::vec3 offset(origin - camera.position);
        if (!frustum.intersectsBox(offset, offset + glm::vec3(16.0f))) continue;
        // Horizontal distance to the section centre; +12 covers the half-diagonal so a
        // section reaching into the visible range is kept.
        const glm::vec2 h(offset.x + 8.0f, offset.z + 8.0f);
        if (glm::dot(h, h) > (maxDistance + 12.0f) * (maxDistance + 12.0f)) continue;
        m_commands[drawCount] = {entry.range.size * 6, 1, 0,
                                 static_cast<int32_t>(entry.range.offset * 4), drawCount};
        m_offsets[drawCount] = glm::vec4(offset, 0.0f);
        ++drawCount;
        quads += entry.range.size;
    }
    m_stats = {static_cast<int>(m_sections.size()), static_cast<int>(drawCount), quads,
               m_quadsTotal, m_arenaAlloc.capacity()};
    if (drawCount == 0) return;

    const DrawCommand* commands = m_commands.data();
    const glm::vec4* offsets = m_offsets.data();
    if (backToFront) {
        sortDrawsBackToFront(std::span<const DrawCommand>(m_commands.data(), drawCount),
                             std::span<const glm::vec4>(m_offsets.data(), drawCount),
                             std::span<DrawSortItem>(m_sort.data(), drawCount),
                             std::span<DrawCommand>(m_sortedCommands.data(), drawCount),
                             std::span<glm::vec4>(m_sortedOffsets.data(), drawCount));
        commands = m_sortedCommands.data();
        offsets = m_sortedOffsets.data();
    }
    glNamedBufferSubData(m_commandBuffer, 0, drawCount * sizeof(DrawCommand), commands);
    glNamedBufferSubData(m_offsetBuffer, 0, drawCount * sizeof(glm::vec4), offsets);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_commandBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_offsetBuffer); // binding 0: offsets
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(drawCount), 0);
}

} // namespace mc::gfx
