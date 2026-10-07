#pragma once

#include <cstdint>
#include <span>

namespace mc::gfx {

// Vertex used for blocks. 24 bytes; M2 will pack it smaller for chunk meshes.
struct BlockVertex {
    float x, y, z;
    float u, v;
    uint32_t color; // RGBA8: face shade x tint (vanilla multiplies both into the colour)
};

// Static GPU triangle list (uploaded once, drawn many times).
class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(std::span<const BlockVertex> vertices); // load time only
    void draw() const;
    int vertexCount() const { return m_count; }

private:
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    int m_count = 0;
};

} // namespace mc::gfx
