#include "rendering/TextureAtlas.h"

#include "core/Log.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace mc::gfx {

namespace {

using Pixels = std::vector<uint8_t>; // RGBA, kSpriteSize^2 * 4

bool loadSprite(const std::filesystem::path& file, Pixels& out) {
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* data = stbi_load(file.string().c_str(), &w, &h, &channels, 4);
    if (!data) {
        MC_LOG_WARN("Atlas: can't read %s: %s", file.string().c_str(), stbi_failure_reason());
        return false;
    }
    constexpr int n = TextureAtlas::kSpriteSize;
    // Animated sprites are vertical strips of n x n frames; use frame 0 until
    // animation support lands.
    const bool ok = w == n && h >= n && h % n == 0;
    if (ok) {
        out.assign(data, data + n * n * 4);
    } else {
        MC_LOG_WARN("Atlas: %s is %dx%d, expected 16x16 (or a 16-wide strip); skipped",
                    file.string().c_str(), w, h);
    }
    stbi_image_free(data);
    return ok;
}

} // namespace

std::vector<uint8_t> TextureAtlas::missingSpritePixels() {
    // Vanilla: 2x2 checker of #F800F8 and black (wiki: Missing textures and models).
    constexpr int n = TextureAtlas::kSpriteSize;
    Pixels px(n * n * 4);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const bool magenta = (x < n / 2) != (y < n / 2);
            uint8_t* p = &px[(y * n + x) * 4];
            p[0] = magenta ? 248 : 0;
            p[1] = 0;
            p[2] = magenta ? 248 : 0;
            p[3] = 255;
        }
    }
    return px;
}

TextureAtlas::~TextureAtlas() {
    if (m_texture) glDeleteTextures(1, &m_texture);
}

bool TextureAtlas::build(const std::string& folder) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            files.push_back(entry.path());
        }
    }
    if (ec) {
        MC_LOG_ERROR("Atlas: can't open folder %s", folder.c_str());
        return false;
    }
    // Sorted so sprite placement is the same on every run and machine.
    std::sort(files.begin(), files.end());

    std::vector<std::pair<std::string, Pixels>> sprites;
    sprites.emplace_back(std::string(kMissing), missingSpritePixels());
    for (const auto& file : files) {
        Pixels px;
        if (loadSprite(file, px)) sprites.emplace_back(file.stem().string(), std::move(px));
    }

    // Smallest power-of-two grid that fits all sprites.
    int cols = 1;
    while (cols * cols < static_cast<int>(sprites.size()))
        cols *= 2;
    m_width = cols * kSpriteSize;
    std::vector<uint8_t> atlas(static_cast<size_t>(m_width) * m_width * 4, 0);

    m_sprites.clear();
    for (size_t i = 0; i < sprites.size(); ++i) {
        const int sx = static_cast<int>(i) % cols * kSpriteSize;
        const int sy = static_cast<int>(i) / cols * kSpriteSize;
        for (int row = 0; row < kSpriteSize; ++row) {
            std::memcpy(&atlas[((sy + row) * m_width + sx) * 4],
                        &sprites[i].second[row * kSpriteSize * 4], kSpriteSize * 4);
        }
        const float scale = 1.0f / static_cast<float>(m_width);
        m_sprites[sprites[i].first] = {sx * scale, sy * scale, (sx + kSpriteSize) * scale,
                                       (sy + kSpriteSize) * scale};
    }

    if (m_texture) glDeleteTextures(1, &m_texture);
    glCreateTextures(GL_TEXTURE_2D, 1, &m_texture);
    glTextureStorage2D(m_texture, kMipLevels + 1, GL_RGBA8, m_width, m_width);
    // Row 0 of `atlas` is the top of each image, so v0 = top (see UvRect).
    glTextureSubImage2D(m_texture, 0, 0, 0, m_width, m_width, GL_RGBA, GL_UNSIGNED_BYTE,
                        atlas.data());
    glGenerateTextureMipmap(m_texture);
    // Pixel-art look: nearest when magnified; mipmapped when far away (as vanilla).
    glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTextureParameteri(m_texture, GL_TEXTURE_MAX_LEVEL, kMipLevels);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    MC_LOG_INFO("Atlas: %d sprites, %dx%d", spriteCount(), m_width, m_width);
    return true;
}

UvRect TextureAtlas::sprite(std::string_view name) const {
    const auto it = m_sprites.find(std::string(name));
    if (it != m_sprites.end()) return it->second;
    MC_LOG_WARN("Atlas: missing sprite '%.*s'", static_cast<int>(name.size()), name.data());
    return m_sprites.at(std::string(kMissing));
}

} // namespace mc::gfx
