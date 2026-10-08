#pragma once

#include "rendering/Shader.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace mc::gfx {

struct Camera;
class PackStack;

// Clouds (M22.2; wiki: Cloud), the Fancy kind: translucent boxes 12 x 4 x 12 blocks at
// Y 192.33, one per opaque pixel of the 256x256 clouds texture (it tiles), drifting
// west (-X) by 0.03 blocks a tick of game time. Top faces are lit fully, sides 0.9 /
// 0.8, bottoms 0.7; only faces between a cloud cell and an empty one are built. The
// mesh is rebuilt when the camera's cloud cell changes, then shifted by a uniform.
class CloudRenderer {
public:
    static constexpr float kHeight = 192.33f, kThickness = 4.0f, kCell = 12.0f; // (wiki: Cloud - Y 192.33)
    static constexpr float kSpeed = 0.03f; // blocks per tick, toward -X
    static constexpr int kMaxRadius = 32;  // cells (384 blocks)

    CloudRenderer() = default;
    ~CloudRenderer();
    CloudRenderer(const CloudRenderer&) = delete;
    CloudRenderer& operator=(const CloudRenderer&) = delete;

    bool init(const PackStack& packs);
    // `time`: game ticks + partial tick; `range`: blocks (the render distance);
    // `color` rgb + alpha from the time of day and weather.
    void draw(const Camera& camera, const glm::mat4& viewProj, double time, float range, const glm::vec4& color);

    // GL-free (tests): is the cloud texture's pixel (x, z) (wrapped) a cloud?
    bool cloudAt(int x, int z) const { return m_cells[size_t((z & 255) * 256 + (x & 255))] != 0; }

private:
    struct Vertex {
        float x, y, z, shade;
    };
    void rebuild(int cellX, int cellZ, int radius);

    Shader m_shader;
    uint32_t m_vao = 0, m_vbo = 0;
    std::array<uint8_t, 256 * 256> m_cells{};
    std::vector<Vertex> m_vertices; // reserved once (max radius)
    int m_builtX = INT32_MIN, m_builtZ = INT32_MIN, m_builtRadius = -1;
    size_t m_count = 0;
};

} // namespace mc::gfx
