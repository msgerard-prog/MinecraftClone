#pragma once

#include "rendering/GuiBatch.h"
#include "rendering/Shader.h"

#include <cstdint>

namespace mc::gfx {

class PackStack;
class TextureAtlas;

// Draws a GuiBatch on top of the frame: one dynamic vertex buffer, one draw call.
// Textures: font and HUD sprites from the pack stack (our originals unless a pack
// overrides them), the block atlas for item icons.
class GuiRenderer {
public:
    static constexpr int kMaxQuads = 16384;

    GuiRenderer() = default;
    ~GuiRenderer();
    GuiRenderer(const GuiRenderer&) = delete;
    GuiRenderer& operator=(const GuiRenderer&) = delete;

    bool init(const PackStack& packs, const TextureAtlas& atlas);
    // Vanilla "auto" GUI scale: the largest whole scale that keeps 320x240 GUI pixels.
    static int guiScale(int width, int height);

    // A batch set up with this renderer's font and atlas layout (reserve done).
    GuiBatch& batch() { return m_batch; }
    // Upload and draw the batch over the framebuffer, then clear it.
    void draw(int framebufferWidth, int framebufferHeight);

private:
    Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_textures[5] = {}; // indexed by GuiTexture
    GuiBatch m_batch;
};

} // namespace mc::gfx
