#pragma once

#include "rendering/Camera.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/TextureAtlas.h"

#include <span>

namespace mc::gfx {

// Owns the GL state for drawing the world: block shader, block atlas, world meshes.
// main.cpp drives it; all GL calls for a frame happen in here (hard rule 7).
class WorldRenderer {
public:
    // Load time: GL state, shaders, atlas. Call after initOpenGl().
    bool init();

    // Replaces the static world mesh (M1 test scene; per-section meshes in M2).
    void setWorldMesh(std::span<const BlockVertex> vertices);

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    const TextureAtlas& atlas() const { return m_atlas; }

private:
    Shader m_blockShader;
    TextureAtlas m_atlas;
    Mesh m_world;
};

} // namespace mc::gfx
