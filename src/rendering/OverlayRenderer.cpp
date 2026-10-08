#include "rendering/OverlayRenderer.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mc::gfx {

namespace {

// An axis-aligned box as 12 triangles.
void addBox(std::vector<glm::vec3>& out, glm::vec3 a, glm::vec3 b) {
    const glm::vec3 c[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
                            {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
    const int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                             {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (const auto& f : faces) {
        out.insert(out.end(), {c[f[0]], c[f[1]], c[f[2]], c[f[0]], c[f[2]], c[f[3]]});
    }
}

} // namespace

OverlayRenderer::~OverlayRenderer() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool OverlayRenderer::init() {
    if (!m_shader.load("overlay")) return false;
    std::vector<glm::vec3> v;
    // Outline: the 12 edges of a slightly enlarged unit cube (no z-fighting with the
    // block), each as a thin box. Vanilla draws a dark 40% outline.
    constexpr float g = 0.002f; // grow
    constexpr float t = 0.008f; // half thickness
    const float lo = -g, hi = 1.0f + g;
    for (float y : {lo, hi})
        for (float z : {lo, hi})
            addBox(v, {lo, y - t, z - t}, {hi, y + t, z + t}); // along x
    for (float x : {lo, hi})
        for (float z : {lo, hi})
            addBox(v, {x - t, lo, z - t}, {x + t, hi, z + t}); // along y
    for (float x : {lo, hi})
        for (float y : {lo, hi})
            addBox(v, {x - t, y - t, lo}, {x + t, y + t, hi}); // along z
    m_outlineVertices = static_cast<int>(v.size());
    // Crosshair in GUI pixels around the screen centre: vanilla's 15x15 cross with
    // 1-pixel lines (scaled by the GUI scale in draw()).
    addBox(v, {-7.5f, -0.5f, 0}, {7.5f, 0.5f, 0});
    addBox(v, {-0.5f, -7.5f, 0}, {0.5f, -0.5f, 0});
    addBox(v, {-0.5f, 0.5f, 0}, {0.5f, 7.5f, 0});
    m_crosshairVertices = static_cast<int>(v.size()) - m_outlineVertices;

    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, static_cast<GLsizeiptr>(v.size() * sizeof(glm::vec3)), v.data(), 0);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(glm::vec3));
    return true;
}

void OverlayRenderer::draw(const Camera& camera, int width, int height,
                           const std::optional<world::BlockPos>& target, const glm::vec3& boxMin,
                           const glm::vec3& boxMax) {
    m_shader.bind();
    glBindVertexArray(m_vao);
    glEnable(GL_BLEND);

    if (target) {
        // Camera-relative placement (double precision on the CPU).
        const glm::vec3 offset(glm::dvec3(target->x, target->y, target->z) - camera.position);
        // (the unit outline stretched over the block's shape bounds: slabs, stairs...)
        const glm::mat4 m = glm::scale(glm::translate(camera.viewProjectionAtOrigin(float(width) / float(height)),
                                                      offset + boxMin),
                                       boxMax - boxMin);
        glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m));
        glUniform4f(1, 0.0f, 0.0f, 0.0f, 0.4f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, m_outlineVertices);
        glDepthMask(GL_TRUE);
    }

    // Crosshair: inverts what's behind it (vanilla blends ONE_MINUS_DST_COLOR).
    // Vanilla "auto" GUI scale: the largest whole scale that still fits 320x240.
    const float scale = static_cast<float>(std::max(1, std::min(width / 320, height / 240)));
    // Snap the 15x15 sprite to whole GUI pixels (vanilla draws GUI on a pixel grid).
    const float guiW = std::floor(width / scale), guiH = std::floor(height / scale);
    const float left = std::floor((guiW - 15.0f) / 2.0f), top = std::floor((guiH - 15.0f) / 2.0f);
    const float cx = (left + 7.5f) * scale - width / 2.0f; // sprite centre vs screen centre, px
    const float cy = height / 2.0f - (top + 7.5f) * scale;
    const glm::mat4 px =
        glm::scale(glm::translate(glm::mat4(1.0f), {2.0f * cx / width, 2.0f * cy / height, 0.0f}),
                   {2.0f * scale / width, 2.0f * scale / height, 1.0f});
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(px));
    glUniform4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, m_outlineVertices, m_crosshairVertices);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

} // namespace mc::gfx
