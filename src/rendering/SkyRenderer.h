#pragma once

#include "rendering/Camera.h"
#include "rendering/Shader.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace mc::gfx {

class PackStack;

// What the sky looks like this frame (from world/DayTime.h).
struct SkyState {
    double celestialAngle = 0.0; // 0 = sun at the zenith
    float starBrightness = 0.0f; // 0 .. 0.5
    int moonPhase = 0;           // 0..7
    bool celestial = true;       // sun, moon and stars (Overworld only)
    float visibility = 1.0f;     // 1 - rain strength: rain hides the sun, moon and stars
};

// Sun, moon and stars (wiki: Sun, Moon, Daylight cycle). Drawn first each frame, on
// top of the cleared sky colour, with depth off and additive blending: the textures
// are black where there is no light. The sun rises in the east, sets in the west.
class SkyRenderer {
public:
    SkyRenderer() = default;
    ~SkyRenderer();
    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    // Load time: shader, sun/moon textures (built-in, overridable by packs), stars.
    bool init(const PackStack& packs);
    void draw(const Camera& camera, float aspect, const SkyState& sky);

private:
    Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_sunTexture = 0;
    uint32_t m_moonTexture = 0;
    uint32_t m_whiteTexture = 0; // stars
    int m_starVertices = 0;
};

} // namespace mc::gfx
