#include "rendering/WorldRenderer.h"

#include "core/Files.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace mc::gfx {

namespace {

// Vanilla's daytime sky colour at plains biome (#78A7FF).
constexpr float kSkyR = 0x78 / 255.0f;
constexpr float kSkyG = 0xA7 / 255.0f;
constexpr float kSkyB = 0xFF / 255.0f;

} // namespace

bool WorldRenderer::init() {
    if (!m_blockShader.load("block")) return false;
    if (!m_atlas.build(assetPath("minecraft/textures/block"))) return false;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // back faces (clockwise from the camera) are never visible
    return true;
}

void WorldRenderer::setWorldMesh(std::span<const BlockVertex> vertices) {
    m_world.upload(vertices);
}

void WorldRenderer::drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    const glm::mat4 viewProj =
        camera.viewProjection(float(framebufferWidth) / float(framebufferHeight));
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(kSkyR, kSkyG, kSkyB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_blockShader.bind();
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj)); // location 0: uViewProj
    glBindTextureUnit(0, m_atlas.texture());                      // unit 0: block atlas
    m_world.draw();
}

} // namespace mc::gfx
