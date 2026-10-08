#include "rendering/GuiRenderer.h"

#include "core/Log.h"
#include "rendering/ResourcePack.h"
#include "rendering/SpriteImage.h"
#include "rendering/TextureAtlas.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <string>

namespace mc::gfx {

namespace {

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

Image loadImage(const PackStack& packs, const char* path, int fallbackW, int fallbackH) {
    const auto bytes = packs.read(path);
    if (auto img = bytes ? decodePng(*bytes) : std::nullopt) return *img;
    MC_LOG_WARN("GUI: can't load %s", path);
    return Image{fallbackW, fallbackH,
                 std::vector<uint8_t>(size_t(fallbackW) * fallbackH * 4, 0)};
}

} // namespace

GuiRenderer::~GuiRenderer() {
    // The atlas texture (index 4) belongs to TextureAtlas.
    glDeleteTextures(4, m_textures);
    glDeleteTextures(2, &m_textures[5]); // (the HUD icons and the map)
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

int GuiRenderer::guiScale(int width, int height) {
    const int fits = std::max(1, std::min(width / 320, height / 240)); // vanilla auto: the largest that fits
    return s_scaleSetting > 0 ? std::min(s_scaleSetting, fits) : fits;
}

bool GuiRenderer::init(const PackStack& packs, const TextureAtlas& atlas) {
    if (!m_shader.load("gui")) return false;
    const Image font = loadImage(packs, "assets/minecraft/textures/font/ascii.png", 128, 128);
    m_textures[0] = makeTexture(Image{1, 1, {255, 255, 255, 255}});
    m_textures[6] = makeTexture(Image{128, 128, std::vector<uint8_t>(128 * 128 * 4, 0)}); // (M28.2b)
    m_textures[1] = makeTexture(font);
    m_textures[2] =
        makeTexture(loadImage(packs, "assets/minecraft/textures/gui/sprites/hud/hotbar.png", 182, 22));
    m_textures[3] = makeTexture(
        loadImage(packs, "assets/minecraft/textures/gui/sprites/hud/hotbar_selection.png", 24, 23));
    m_textures[4] = atlas.texture();
    // Survival icons: one 9-pixel cell each, side by side (vanilla sprite paths).
    {
        constexpr int n = static_cast<int>(HudIcon::Count);
        Image strip{9 * n, 9, std::vector<uint8_t>(size_t(9 * n) * 9 * 4, 0)};
        for (int i = 0; i < n; ++i) {
            const Image icon = loadImage(
                packs, (std::string("assets/minecraft/textures/gui/sprites/") + kHudIconPaths[i]).c_str(), 9, 9);
            for (int y = 0; y < std::min(9, icon.height); ++y)
                for (int x = 0; x < std::min(9, icon.width); ++x)
                    std::copy_n(icon.at(x, y), 4, &strip.pixels[(size_t(y) * strip.width + i * 9 + x) * 4]);
        }
        m_textures[5] = makeTexture(strip);
    }

    // Font sheets are square 16x16 grids (any resolution).
    if (font.width == font.height && font.width >= 16)
        m_batch.setFont(FontMetrics::fromImage(font.pixels.data(), font.width));
    m_batch.setAtlas({atlas.columns(), atlas.cellSize()});
    m_batch.reserveQuads(kMaxQuads);

    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, GLsizeiptr(kMaxQuads) * 6 * sizeof(GuiVertex), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 2, GL_FLOAT, GL_FALSE, offsetof(GuiVertex, x));
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glEnableVertexArrayAttrib(m_vao, 1);
    glVertexArrayAttribFormat(m_vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(GuiVertex, u));
    glVertexArrayAttribBinding(m_vao, 1, 0);
    glEnableVertexArrayAttrib(m_vao, 2);
    glVertexArrayAttribFormat(m_vao, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(GuiVertex, color));
    glVertexArrayAttribBinding(m_vao, 2, 0);
    glEnableVertexArrayAttrib(m_vao, 3);
    glVertexArrayAttribIFormat(m_vao, 3, 1, GL_UNSIGNED_INT, offsetof(GuiVertex, texture));
    glVertexArrayAttribBinding(m_vao, 3, 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(GuiVertex));
    return true;
}

void GuiRenderer::uploadMap(const uint8_t* rgba) {
    glTextureSubImage2D(m_textures[6], 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

void GuiRenderer::draw(int framebufferWidth, int framebufferHeight) {
    const auto& v = m_batch.vertices();
    const size_t count = std::min(v.size(), size_t(kMaxQuads) * 6);
    if (count == 0) return;
    glNamedBufferSubData(m_vbo, 0, GLsizeiptr(count * sizeof(GuiVertex)), v.data());

    const int scale = guiScale(framebufferWidth, framebufferHeight);
    m_shader.bind();
    glBindVertexArray(m_vao);
    // GUI pixels -> NDC, origin top-left.
    glUniform2f(0, float(framebufferWidth) / float(scale), float(framebufferHeight) / float(scale));
    for (int i = 0; i < 7; ++i)
        glBindTextureUnit(GLuint(i), m_textures[i]);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    m_batch.clear();
}

} // namespace mc::gfx
