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
    // The held map's 128x128 picture (M28.2b; RGBA, row by row), drawn as GuiTexture::Map.
    void uploadMap(const uint8_t* rgba);
    static constexpr const char* kHudIconPaths[] = {
        "hud/heart/full.png", "hud/heart/half.png", "hud/heart/container.png",
        "hud/food_full.png",  "hud/food_half.png",  "hud/food_empty.png", "hud/air.png", "hud/air_bursting.png",
        "hud/armor_full.png", "hud/armor_half.png", "hud/armor_empty.png",
        "hud/heart/absorbing_full.png", "hud/heart/absorbing_half.png"}; // (M29.2a)
    // Vanilla "auto" GUI scale: the largest whole scale that keeps 320x240 GUI pixels.
    static int guiScale(int width, int height);
    // The GUI Scale option (M22.5): 0 = auto, else that scale when it fits.
    static void setScaleSetting(int scale) { s_scaleSetting = scale; }

    // A batch set up with this renderer's font and atlas layout (reserve done).
    GuiBatch& batch() { return m_batch; }
    // Upload and draw the batch over the framebuffer, then clear it.
    void draw(int framebufferWidth, int framebufferHeight);

private:
    static inline int s_scaleSetting = 0;
    Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_textures[7] = {}; // indexed by GuiTexture
    GuiBatch m_batch;
};

} // namespace mc::gfx
