#include "rendering/GuiBatch.h"

#include <cmath>

namespace mc::gfx {

FontMetrics FontMetrics::fromImage(const uint8_t* pixels, int size) {
    FontMetrics m;
    m.textureSize = size;
    m.cell = size / 16;
    for (int code = 0; code < 256; ++code) {
        const int ox = (code % 16) * m.cell, oy = (code / 16) * m.cell;
        int right = -1;
        for (int y = 0; y < m.cell; ++y)
            for (int x = 0; x < m.cell; ++x)
                if (pixels[((oy + y) * size + ox + x) * 4 + 3] > 0) right = std::max(right, x);
        // Advance in GUI pixels (8 per cell regardless of the sheet's resolution).
        const int inkWidth = (right + 1) * 8 / m.cell;
        m.advance[code] = static_cast<uint8_t>(right < 0 ? 0 : inkWidth + 1);
    }
    m.advance[' '] = 4;
    return m;
}

void GuiBatch::quad(const float (&px)[4][2], const float (&uv)[4][2], uint32_t color,
                    GuiTexture tex) {
    // Hard cap: never grow past the reserved capacity (no per-frame allocation).
    if (m_maxVertices && m_vertices.size() + 6 > m_maxVertices) return;
    const auto t = static_cast<uint32_t>(tex);
    auto v = [&](int i) { return GuiVertex{px[i][0], px[i][1], uv[i][0], uv[i][1], color, t}; };
    m_vertices.push_back(v(0));
    m_vertices.push_back(v(1));
    m_vertices.push_back(v(2));
    m_vertices.push_back(v(0));
    m_vertices.push_back(v(2));
    m_vertices.push_back(v(3));
}

void GuiBatch::sprite(GuiTexture tex, float x, float y, float w, float h, float u, float v,
                      float uw, float vh, uint32_t color) {
    const float px[4][2] = {{x, y}, {x, y + h}, {x + w, y + h}, {x + w, y}};
    const float uv[4][2] = {{u, v}, {u, v + vh}, {u + uw, v + vh}, {u + uw, v}};
    quad(px, uv, color, tex);
}

void GuiBatch::fill(float x, float y, float w, float h, uint32_t color) {
    sprite(GuiTexture::White, x, y, w, h, 0, 0, 1, 1, color);
}

int GuiBatch::textWidth(std::string_view s) const {
    int w = 0;
    for (char c : s)
        w += m_font.advance[static_cast<uint8_t>(c)];
    return w;
}

int GuiBatch::text(std::string_view s, float x, float y, uint32_t color, bool shadow, float scale) {
    if (shadow) {
        // Vanilla shadow: the colour's RGB / 4, same alpha.
        const uint32_t sc = (color & 0xFF000000u) | ((color & 0x00FCFCFCu) >> 2);
        text(s, x + scale, y + scale, sc, false, scale);
    }
    const float texel = static_cast<float>(m_font.cell) / 8.0f; // texels per GUI px
    float pen = x;
    for (char c : s) {
        const auto code = static_cast<uint8_t>(c);
        if (c != ' ') {
            const float u = static_cast<float>((code % 16) * m_font.cell);
            const float v = static_cast<float>((code / 16) * m_font.cell);
            sprite(GuiTexture::Font, pen, y, 8 * scale, 8 * scale, u, v, 8 * texel, 8 * texel, color);
        }
        pen += m_font.advance[code] * scale;
    }
    return static_cast<int>(pen - x);
}

void GuiBatch::atlasSprite(uint16_t sprite, float x, float y, uint32_t color) {
    const float cell = static_cast<float>(m_atlas.cellSize);
    const float u0 = static_cast<float>(sprite % m_atlas.columns) * cell;
    const float v0 = static_cast<float>(sprite / m_atlas.columns) * cell;
    this->sprite(GuiTexture::Atlas, x, y, 16, 16, u0, v0, cell, cell, color);
}

void GuiBatch::blockIcon(const BakedModel& model, float x, float y, uint32_t grassTint) {
    if (!model.visible) return;
    const float cell = static_cast<float>(m_atlas.cellSize);
    auto spriteUv = [&](uint16_t sprite, float (&uv)[4][2], int rotation, bool mirror) {
        const float u0 = static_cast<float>(sprite % m_atlas.columns) * cell;
        const float v0 = static_cast<float>(sprite / m_atlas.columns) * cell;
        // Corners in quad order: top-left, bottom-left, bottom-right, top-right.
        float c[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
        if (mirror)
            for (auto& p : c)
                p[0] = 1 - p[0];
        for (int i = 0; i < 4; ++i) {
            const auto& p = c[(i + rotation) % 4];
            uv[i][0] = u0 + p[0] * cell;
            uv[i][1] = v0 + p[1] * cell;
        }
    };
    if (model.boxCount > 0 || model.cross) { // flat item sprite (vanilla: torch, plants = their texture)
        float uv[4][2];
        spriteUv(model.cross ? model.crossSprite : model.boxes[0].faces[int(world::Direction::North)].sprite, uv, 0,
                 false);
        const float px[4][2] = {{x, y}, {x, y + 16}, {x + 16, y + 16}, {x + 16, y}};
        quad(px, uv, model.cross && model.crossTint == Tint::Grass ? grassTint : rgba(255, 255, 255),
             GuiTexture::Atlas);
        return;
    }
    // Isometric cube fitting 16x16: top rhombus and two side parallelograms.
    const float cx = x + 8, h = 16.0f;
    const float top[4][2] = {{cx, y + 0.5f}, {x + 1, y + 4.25f}, {cx, y + 8}, {x + 15, y + 4.25f}};
    const float left[4][2] = {{x + 1, y + 4.25f}, {x + 1, y + 12}, {cx, y + h - 0.5f}, {cx, y + 8}};
    const float right[4][2] = {{cx, y + 8}, {cx, y + h - 0.5f}, {x + 15, y + 12}, {x + 15, y + 4.25f}};
    const BakedVariant& var = model.variants[0];
    auto face = [&](world::Direction d, const float (&px)[4][2], float shade) {
        const BakedFace& f = var.faces[int(d)];
        float uv[4][2];
        spriteUv(f.sprite, uv, f.rotation, f.mirror);
        uint32_t c = rgba(255, 255, 255);
        if (f.tint == Tint::Grass) c = grassTint;
        const auto ch = [&](int shift) {
            return static_cast<uint8_t>(std::lround(((c >> shift) & 0xFF) * shade));
        };
        quad(px, uv, rgba(ch(0), ch(8), ch(16)), GuiTexture::Atlas);
    };
    face(world::Direction::Up, top, 1.0f);
    face(world::Direction::South, left, 0.8f);
    face(world::Direction::East, right, 0.6f);
}

} // namespace mc::gfx
