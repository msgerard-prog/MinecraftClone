#include "rendering/Mesh.h"

#include <glad/gl.h>

#include <cstddef>

namespace mc::gfx {

Mesh::~Mesh() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void Mesh::upload(std::span<const BlockVertex> vertices) {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (!m_vao) {
        // Vertex layout matches assets/shaders/block.vert locations 0..2.
        glCreateVertexArrays(1, &m_vao);
        glEnableVertexArrayAttrib(m_vao, 0);
        glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(BlockVertex, x));
        glVertexArrayAttribBinding(m_vao, 0, 0);
        glEnableVertexArrayAttrib(m_vao, 1);
        glVertexArrayAttribFormat(m_vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(BlockVertex, u));
        glVertexArrayAttribBinding(m_vao, 1, 0);
        glEnableVertexArrayAttrib(m_vao, 2);
        glVertexArrayAttribFormat(m_vao, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                  offsetof(BlockVertex, color));
        glVertexArrayAttribBinding(m_vao, 2, 0);
    }
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(BlockVertex));
    m_count = static_cast<int>(vertices.size());
}

void Mesh::draw() const {
    if (m_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_count);
}

} // namespace mc::gfx
