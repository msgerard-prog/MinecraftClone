#pragma once

#include "rendering/BlockModels.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mc::gfx {

// Textures the GUI samples (vertex field `texture`; see assets/shaders/gui.frag).
enum class GuiTexture : uint32_t { White = 0, Font = 1, Hotbar = 2, Selection = 3, Atlas = 4, Icons = 5, Map = 6 };

// Survival HUD icons: 9x9 cells in the Icons strip (u = index * 9).
enum class HudIcon : int {
    HeartFull,
    HeartHalf,
    HeartContainer,
    FoodFull,
    FoodHalf,
    FoodEmpty,
    Air,
    AirBursting,
    ArmorFull,
    ArmorHalf,
    ArmorEmpty,
    HeartGoldFull, // (M29.2a: Absorption)
    HeartGoldHalf,
    Count
};

// GUI vertex: position in GUI pixels (origin top-left), UV in texels of its texture.
struct GuiVertex {
    float x, y;
    float u, v;
    uint32_t color; // RGBA8, multiplies the texel (0xAABBGGRR in memory order R,G,B,A)
    uint32_t texture;
};

constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24);
}
// Vanilla-style ARGB hex (0xAARRGGBB) as used by text colours.
constexpr uint32_t argb(uint32_t c) {
    return rgba(uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), uint8_t(c >> 24));
}

// Glyph advance widths of the bitmap font (vanilla: ink width + 1; space 4).
struct FontMetrics {
    std::array<uint8_t, 256> advance{};
    int cell = 8;       // glyph cell size in font texels (8 for a 128px sheet)
    int textureSize = 128;
    // From the font sheet's alpha: rightmost inked column per glyph.
    static FontMetrics fromImage(const uint8_t* rgba, int size);
};

// Where the block atlas keeps its sprites (texel grid), for block icons.
struct AtlasLayout {
    int columns = 1;
    int cellSize = 16;
};

// GL-free GUI geometry for one frame, in GUI pixels (screen / GUI scale). Reserve
// once; clear() keeps capacity, so building a frame doesn't allocate.
class GuiBatch {
public:
    static constexpr int kLineHeight = 9; // vanilla font line height

    // Reserves and caps the batch: quads beyond this are dropped (never reallocates).
    void reserveQuads(size_t quads) {
        m_vertices.reserve(quads * 6);
        m_maxVertices = quads * 6;
    }
    void clear() { m_vertices.clear(); }
    const std::vector<GuiVertex>& vertices() const { return m_vertices; }

    void setFont(const FontMetrics& f) { m_font = f; }
    void setAtlas(const AtlasLayout& a) { m_atlas = a; }

    // Solid rectangle.
    void fill(float x, float y, float w, float h, uint32_t color);
    // Texture rectangle (u, v, uw, vh in texels).
    void sprite(GuiTexture tex, float x, float y, float w, float h, float u, float v, float uw,
                float vh, uint32_t color = rgba(255, 255, 255));
    // Text with vanilla's drop shadow (1 px down-right, colour / 4). Returns the width.
    int text(std::string_view s, float x, float y, uint32_t color, bool shadow = true,
             float scale = 1.0f);
    int textWidth(std::string_view s) const;
    // A block as a 16x16 GUI item: isometric cube (top, south and east faces shaded
    // 1.0 / 0.8 / 0.6), or its sprite flat for non-cube models (torch).
    void blockIcon(const BakedModel& model, float x, float y, uint32_t grassTint);
    // An atlas sprite (item textures) drawn flat at 16x16.
    void atlasSprite(uint16_t sprite, float x, float y, uint32_t color = rgba(255, 255, 255));
    // (M32.3) an atlas sprite centred at (cx, cy), halfW x halfH (a negative halfW mirrors it,
    // as a sprite turned past edge-on).
    void atlasSpriteCentred(uint16_t sprite, float cx, float cy, float halfW, float halfH,
                            uint32_t color = rgba(255, 255, 255));

private:
    void quad(const float (&px)[4][2], const float (&uv)[4][2], uint32_t color, GuiTexture tex);

    std::vector<GuiVertex> m_vertices;
    size_t m_maxVertices = 0; // 0: uncapped (tests)
    FontMetrics m_font;

public:
    const FontMetrics& font() const { return m_font; }

private:
    AtlasLayout m_atlas;
};

} // namespace mc::gfx
