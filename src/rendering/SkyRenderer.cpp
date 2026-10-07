#include "rendering/SkyRenderer.h"

#include "core/Log.h"
#include "rendering/ResourcePack.h"
#include "rendering/SpriteImage.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace mc::gfx {

namespace {

struct SkyVertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

// Vertex ranges in the shared buffer.
constexpr int kSunFirst = 0;
constexpr int kMoonFirst = 6;
constexpr int kStarsFirst = 12;

// A quad facing down at height `y` (seen from below), half size `h`.
void addQuad(std::vector<SkyVertex>& v, float y, float h) {
    const SkyVertex a{{-h, y, -h}, {0, 0}}, b{{h, y, -h}, {1, 0}}, c{{h, y, h}, {1, 1}},
        d{{-h, y, h}, {0, 1}};
    v.insert(v.end(), {a, b, c, a, c, d});
}

// Star field: ~1500 random directions, small quads at distance 100 facing the origin,
// fixed seed (the sky is the same in every world). Count and size are estimated from
// observation, not documented; vanilla's star positions are not reproduced.
void addStars(std::vector<SkyVertex>& v) {
    std::mt19937 rng(0x5EED5u); // our own fixed seed
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), size(0.15f, 0.25f),
        spin(0.0f, 2.0f * std::numbers::pi_v<float>);
    for (int i = 0; i < 1500; ++i) {
        glm::vec3 d(u(rng), u(rng), u(rng));
        const float len2 = glm::dot(d, d);
        if (len2 < 0.01f || len2 > 1.0f) continue; // uniform inside the unit ball
        d = glm::normalize(d);
        const glm::vec3 c = d * 100.0f;
        // Tangent basis, rotated randomly so the squares aren't all aligned.
        const glm::vec3 ref = std::abs(d.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 t = glm::normalize(glm::cross(ref, d));
        glm::vec3 b = glm::cross(d, t);
        const float a = spin(rng), s = size(rng);
        const glm::vec3 t2 = (t * std::cos(a) + b * std::sin(a)) * s;
        const glm::vec3 b2 = (b * std::cos(a) - t * std::sin(a)) * s;
        const SkyVertex p0{c - t2 - b2, {0, 0}}, p1{c + t2 - b2, {0, 0}}, p2{c + t2 + b2, {0, 0}},
            p3{c - t2 + b2, {0, 0}};
        v.insert(v.end(), {p0, p1, p2, p0, p2, p3});
    }
}

uint32_t makeTexture(const Image& img) {
    uint32_t tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA8, img.width, img.height);
    glTextureSubImage2D(tex, 0, 0, 0, img.width, img.height, GL_RGBA, GL_UNSIGNED_BYTE,
                        img.pixels.data());
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

uint32_t loadTexture(const PackStack& packs, const char* path) {
    const auto bytes = packs.read(path);
    auto img = bytes ? decodePng(*bytes) : std::nullopt;
    if (!img) {
        MC_LOG_WARN("Sky: can't load %s", path);
        img = Image{1, 1, {0, 0, 0, 255}}; // black adds nothing
    }
    return makeTexture(*img);
}

} // namespace

SkyRenderer::~SkyRenderer() {
    const uint32_t textures[] = {m_sunTexture, m_moonTexture, m_whiteTexture};
    glDeleteTextures(3, textures);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool SkyRenderer::init(const PackStack& packs) {
    if (!m_shader.load("sky")) return false;
    std::vector<SkyVertex> v;
    // Apparent sizes estimated from observation (~33 and ~23 degrees across).
    addQuad(v, 100.0f, 30.0f); // sun
    addQuad(v, 100.0f, 20.0f); // moon (placed opposite the sun in draw())
    addStars(v);
    m_starVertices = static_cast<int>(v.size()) - kStarsFirst;

    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, static_cast<GLsizeiptr>(v.size() * sizeof(SkyVertex)), v.data(),
                         0);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(SkyVertex, pos));
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glEnableVertexArrayAttrib(m_vao, 1);
    glVertexArrayAttribFormat(m_vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(SkyVertex, uv));
    glVertexArrayAttribBinding(m_vao, 1, 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(SkyVertex));

    m_sunTexture = loadTexture(packs, "assets/minecraft/textures/environment/sun.png");
    m_moonTexture = loadTexture(packs, "assets/minecraft/textures/environment/moon_phases.png");
    m_whiteTexture = makeTexture(Image{1, 1, {255, 255, 255, 255}});
    return true;
}

void SkyRenderer::draw(const Camera& camera, float aspect, const SkyState& sky) {
    if (!sky.celestial) return; // the Nether and the End have no sun, moon or stars
    // Sky rotation: about the north-south axis, sun at +Y at angle 0; at sunrise
    // (angle 0.75) it is in the east (+X), at sunset (0.25) in the west.
    const float turn = static_cast<float>(sky.celestialAngle * 2.0 * std::numbers::pi);
    const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), turn, glm::vec3(0, 0, 1));
    const glm::mat4 vp = camera.viewProjectionAtOrigin(aspect) * rot;

    m_shader.bind();
    glBindVertexArray(m_vao);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(vp));

    // Stars first (behind the sun and moon), fading in at dusk.
    const float stars = sky.starBrightness * sky.visibility;
    if (stars > 0.0f) {
        glUniform4f(1, stars, stars, stars, 1.0f);
        glUniform4f(2, 0.0f, 0.0f, 0.0f, 0.0f);
        glBindTextureUnit(0, m_whiteTexture);
        glDrawArrays(GL_TRIANGLES, kStarsFirst, m_starVertices);
    }

    glUniform4f(1, 1.0f, 1.0f, 1.0f, sky.visibility);
    glUniform4f(2, 0.0f, 0.0f, 1.0f, 1.0f); // uv offset, scale: whole texture
    glBindTextureUnit(0, m_sunTexture);
    glDrawArrays(GL_TRIANGLES, kSunFirst, 6);

    // Moon: opposite the sun, one cell of the 4x2 phase sheet.
    const glm::mat4 moonVp = vp * glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float>,
                                              glm::vec3(0, 0, 1));
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(moonVp));
    glUniform4f(2, static_cast<float>(sky.moonPhase % 4) * 0.25f,
                (sky.moonPhase >= 4 ? 0.5f : 0.0f), 0.25f, 0.5f);
    glBindTextureUnit(0, m_moonTexture);
    glDrawArrays(GL_TRIANGLES, kMoonFirst, 6);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace mc::gfx
