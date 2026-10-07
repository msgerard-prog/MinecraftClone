#include "rendering/CloudRenderer.h"

#include "core/Log.h"
#include "rendering/Camera.h"
#include "rendering/ResourcePack.h"
#include "rendering/SpriteImage.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <climits>
#include <cmath>

namespace mc::gfx {

CloudRenderer::~CloudRenderer() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool CloudRenderer::init(const PackStack& packs) {
    if (!m_shader.load("clouds")) return false;
    const auto bytes = packs.read("assets/minecraft/textures/environment/clouds.png");
    const auto img = bytes ? decodePng(*bytes) : std::nullopt;
    if (!img || img->width <= 0 || img->height <= 0) {
        MC_LOG_WARN("Clouds: can't load environment/clouds.png - no clouds");
    } else {
        // Sample the texture onto our 256x256 grid (packs may ship other sizes).
        for (int z = 0; z < 256; ++z)
            for (int x = 0; x < 256; ++x) {
                const int sx = x * img->width / 256, sz = z * img->height / 256;
                m_cells[size_t(z * 256 + x)] = img->pixels[size_t((sz * img->width + sx) * 4 + 3)] > 127 ? 1 : 0;
            }
    }
    const size_t maxCells = size_t(2 * kMaxRadius + 1) * size_t(2 * kMaxRadius + 1);
    m_vertices.reserve(maxCells * 6 * 6);
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, GLsizeiptr(m_vertices.capacity() * sizeof(Vertex)), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, x));
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glEnableVertexArrayAttrib(m_vao, 1);
    glVertexArrayAttribFormat(m_vao, 1, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, shade));
    glVertexArrayAttribBinding(m_vao, 1, 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(Vertex));
    return true;
}

void CloudRenderer::rebuild(int cellX, int cellZ, int radius) {
    m_vertices.clear();
    // Positions relative to the corner of the camera's cell, at the cloud bottom.
    auto face = [&](const glm::vec3 (&p)[4], float shade) {
        for (const int k : {0, 1, 2, 0, 2, 3})
            m_vertices.push_back({p[k].x, p[k].y, p[k].z, shade});
    };
    const float c = kCell, h = kThickness;
    for (int dz = -radius; dz <= radius; ++dz)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz > radius * radius) continue;
            const int gx = cellX + dx, gz = cellZ + dz;
            if (!cloudAt(gx, gz)) continue;
            const float x0 = float(dx) * c, z0 = float(dz) * c, x1 = x0 + c, z1 = z0 + c;
            face({{x0, h, z0}, {x0, h, z1}, {x1, h, z1}, {x1, h, z0}}, 1.0f);   // top
            face({{x0, 0, z0}, {x1, 0, z0}, {x1, 0, z1}, {x0, 0, z1}}, 0.7f);   // bottom
            if (!cloudAt(gx - 1, gz)) face({{x0, 0, z0}, {x0, 0, z1}, {x0, h, z1}, {x0, h, z0}}, 0.9f);
            if (!cloudAt(gx + 1, gz)) face({{x1, 0, z1}, {x1, 0, z0}, {x1, h, z0}, {x1, h, z1}}, 0.9f);
            if (!cloudAt(gx, gz - 1)) face({{x1, 0, z0}, {x0, 0, z0}, {x0, h, z0}, {x1, h, z0}}, 0.8f);
            if (!cloudAt(gx, gz + 1)) face({{x0, 0, z1}, {x1, 0, z1}, {x1, h, z1}, {x0, h, z1}}, 0.8f);
        }
    m_count = m_vertices.size();
    if (m_count) glNamedBufferSubData(m_vbo, 0, GLsizeiptr(m_count * sizeof(Vertex)), m_vertices.data());
    m_builtX = cellX;
    m_builtZ = cellZ;
    m_builtRadius = radius;
}

void CloudRenderer::draw(const Camera& camera, const glm::mat4& viewProj, double time, float range,
                         const glm::vec4& color) {
    if (!m_vao) return;
    // Cloud space: world x plus the drift (the pattern moves west as time passes).
    const double drift = time * double(kSpeed);
    const double sx = camera.position.x + drift, sz = camera.position.z;
    const int cellX = int(std::floor(sx / kCell)), cellZ = int(std::floor(sz / kCell));
    const int radius = std::clamp(int(range / kCell) + 1, 1, kMaxRadius);
    if (cellX != m_builtX || cellZ != m_builtZ || radius != m_builtRadius) rebuild(cellX, cellZ, radius);
    if (m_count == 0) return;
    const glm::vec3 offset(float(double(cellX) * kCell - sx), float(double(kHeight) - camera.position.y),
                           float(double(cellZ) * kCell - sz));
    m_shader.bind();
    glBindVertexArray(m_vao);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(1, 1, glm::value_ptr(offset));
    glUniform4fv(2, 1, glm::value_ptr(color));
    glUniform2f(3, range * 0.6f, range); // fade out toward the edge (horizontal distance)
    glDisable(GL_CULL_FACE);
    // Two passes so a translucent box doesn't show its own far faces: depth first,
    // then colour where the depth matches (vanilla does the same for Fancy clouds).
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_count));
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_count));
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

} // namespace mc::gfx
