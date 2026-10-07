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
    // The Overworld sky dome (M22.2): sky colour overhead fading to the fog colour at
    // the horizon, and the sunrise/sunset glow toward the sun (rgb + alpha in sunrise,
    // sunSide = the sun's horizontal direction). Drawn first, depth off.
    void drawGradient(const Camera& camera, float aspect, const glm::vec3& sky, const glm::vec3& fog,
                      const glm::vec4& sunrise, const glm::vec2& sunSide);
    // The End's sky (M22.2; wiki: The End): a dark tiled texture on a box around the
    // camera, tinted #282828.
    void drawEndSky(const Camera& camera, float aspect);

private:
    Shader m_shader;
    Shader m_gradientShader;
    uint32_t m_emptyVao = 0;     // (the gradient's full-screen triangle has no buffer)
    uint32_t m_endSkyTexture = 0;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_sunTexture = 0;
    uint32_t m_moonTexture = 0;
    uint32_t m_whiteTexture = 0; // stars
    int m_starVertices = 0;
    int m_endSkyFirst = 0; // 36 vertices after the stars
};

} // namespace mc::gfx
