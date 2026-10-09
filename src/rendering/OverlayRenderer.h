#pragma once

#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "world/Chunk.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>

namespace mc::gfx {

// World overlays drawn after the terrain: the targeted-block outline and the
// crosshair (a stand-in until the UI layer in M6).
class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer();
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    bool init();
    void draw(const Camera& camera, int framebufferWidth, int framebufferHeight,
              const std::optional<world::BlockPos>& target, const glm::vec3& boxMin = glm::vec3(0.0f),
              const glm::vec3& boxMax = glm::vec3(1.0f));
    // The crosshair (M30.1: drawn after the first-person hand, only in first person).
    void drawCrosshair(int framebufferWidth, int framebufferHeight);

private:
    Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    int m_outlineVertices = 0;   // first range in the VBO
    int m_crosshairVertices = 0; // second range
};

} // namespace mc::gfx
