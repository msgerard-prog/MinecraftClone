#include "rendering/TextureAtlas.h"

#include "core/Log.h"
#include "rendering/PackedVertex.h"
#include "rendering/ResourcePack.h"

#include <glad/gl.h>

#include <algorithm>
#include <bit>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace mc::gfx {

namespace {

struct LoadedSprite {
    std::string name;
    Image image;               // first frame (or the whole sprite)
    std::vector<Image> frames; // all frames when animated
    int frametime = 1;
};

} // namespace

std::optional<Image> decodePng(const std::vector<uint8_t>& bytes) {
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* data =
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!data) return std::nullopt;
    Image img{w, h, std::vector<uint8_t>(data, data + size_t(w) * h * 4)};
    stbi_image_free(data);
    return img;
}

std::vector<uint8_t> TextureAtlas::missingSpritePixels() {
    // Vanilla: 2x2 checker of #F800F8 and black (wiki: Missing textures and models).
    constexpr int n = kMinCellSize;
    std::vector<uint8_t> px(n * n * 4);
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

bool TextureAtlas::build(const PackStack& packs, std::string_view folder) {
    std::vector<LoadedSprite> sprites;
    sprites.push_back(
        {std::string(kMissing), {kMinCellSize, kMinCellSize, missingSpritePixels()}, {}, 1});

    // Sorted file list (union over packs), so placement is the same on every run.
    for (const std::string& file : packs.list(folder, ".png")) {
        const std::string path = std::string(folder) + file;
        const auto bytes = packs.read(path);
        auto img = bytes ? decodePng(*bytes) : std::nullopt;
        const std::string name = file.substr(0, file.size() - 4);
        if (!img) {
            MC_LOG_WARN("Atlas: can't decode %s", path.c_str());
            continue;
        }
        LoadedSprite s{name, {}, {}, 1};
        const int w = img->width;
        if (!std::has_single_bit(static_cast<unsigned>(w)) || img->height % w != 0) {
            MC_LOG_WARN("Atlas: %s is %dx%d; expected a power-of-two square or a strip of "
                        "square frames; skipped",
                        path.c_str(), w, img->height);
            continue;
        }
        const int frames = img->height / w;
        if (frames > 1) {
            // A strip is animated only with an .mcmeta "animation" section (vanilla).
            const auto meta = packs.read(path + ".mcmeta");
            const auto anim = meta ? parseAnimationMeta(std::string_view(
                                         reinterpret_cast<const char*>(meta->data()), meta->size()))
                                   : std::nullopt;
            if (anim) {
                for (int f = 0; f < frames; ++f)
                    s.frames.push_back(stripFrame(*img, f));
                s.frametime = anim->frametime;
            }
            s.image = stripFrame(*img, 0);
        } else {
            s.image = std::move(*img);
        }
        sprites.push_back(std::move(s));
    }
    if (sprites.size() > kMaxSprites) {
        MC_LOG_ERROR("Atlas: %zu sprites exceed the %u-sprite limit; extra sprites dropped",
                     sprites.size(), kMaxSprites);
        sprites.resize(kMaxSprites);
    }

    // Cell = largest sprite; mip levels limited so a 1-pixel level still fits a cell.
    m_cellSize = kMinCellSize;
    for (const auto& s : sprites)
        m_cellSize = std::max(m_cellSize, s.image.width);
    m_mipLevels = std::min(kMaxMipLevels, std::countr_zero(static_cast<unsigned>(m_cellSize)));
    auto toCell = [&](const Image& img) {
        return img.width == m_cellSize ? img : upscaleNearest(img, m_cellSize / img.width);
    };

    int cols = 1;
    while (cols * cols < static_cast<int>(sprites.size()))
        cols *= 2;
    m_columns = cols;
    const int width = cols * m_cellSize;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (width > maxSize) {
        MC_LOG_ERROR("Atlas: %dx%d exceeds GL_MAX_TEXTURE_SIZE %d", width, width, maxSize);
        return false;
    }

    std::vector<uint8_t> atlas(size_t(width) * width * 4, 0);
    m_indices.clear();
    m_animations.clear();
    for (size_t i = 0; i < sprites.size(); ++i) {
        const int sx = static_cast<int>(i) % cols * m_cellSize;
        const int sy = static_cast<int>(i) / cols * m_cellSize;
        const Image cell = toCell(sprites[i].image);
        for (int row = 0; row < m_cellSize; ++row) {
            std::memcpy(&atlas[(size_t(sy + row) * width + sx) * 4], cell.at(0, row),
                        size_t(m_cellSize) * 4);
        }
        m_indices[sprites[i].name] = static_cast<int>(i);
        if (!sprites[i].frames.empty()) {
            Animation anim;
            anim.sprite = static_cast<int>(i);
            anim.frametime = sprites[i].frametime;
            anim.ticksLeft = anim.frametime;
            for (const Image& frame : sprites[i].frames) {
                std::vector<Image> levels{toCell(frame)};
                for (int l = 1; l <= m_mipLevels; ++l)
                    levels.push_back(halve(levels.back()));
                anim.mips.push_back(std::move(levels));
            }
            m_animations.push_back(std::move(anim));
        }
    }
    m_spriteCount = static_cast<int>(sprites.size());

    if (m_texture) glDeleteTextures(1, &m_texture);
    glCreateTextures(GL_TEXTURE_2D, 1, &m_texture);
    glTextureStorage2D(m_texture, m_mipLevels + 1, GL_RGBA8, width, width);
    // Row 0 of `atlas` is the top of each image; the shader's v0 is the top row.
    glTextureSubImage2D(m_texture, 0, 0, 0, width, width, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data());
    glGenerateTextureMipmap(m_texture);
    // Pixel-art look: nearest when magnified; mipmapped when far away (as vanilla).
    glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTextureParameteri(m_texture, GL_TEXTURE_MAX_LEVEL, m_mipLevels);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    MC_LOG_INFO("Atlas: %d sprites (%d animated), %dpx cells, %dx%d, %d mip levels", m_spriteCount,
                animatedCount(), m_cellSize, width, width, m_mipLevels);
    return true;
}

void TextureAtlas::uploadFrame(const Animation& anim) const {
    const int sx = anim.sprite % m_columns * m_cellSize;
    const int sy = anim.sprite / m_columns * m_cellSize;
    const auto& levels = anim.mips[static_cast<size_t>(anim.frame)];
    for (int l = 0; l < static_cast<int>(levels.size()); ++l) {
        glTextureSubImage2D(m_texture, l, sx >> l, sy >> l, levels[l].width, levels[l].height,
                            GL_RGBA, GL_UNSIGNED_BYTE, levels[l].pixels.data());
    }
}

void TextureAtlas::tick() {
    for (Animation& anim : m_animations) {
        if (--anim.ticksLeft > 0) continue;
        anim.ticksLeft = anim.frametime;
        anim.frame = (anim.frame + 1) % static_cast<int>(anim.mips.size());
        uploadFrame(anim);
    }
}

int TextureAtlas::spriteIndex(std::string_view name) const {
    const auto it = m_indices.find(std::string(name));
    if (it != m_indices.end()) return it->second;
    MC_LOG_WARN("Atlas: missing sprite '%.*s'", static_cast<int>(name.size()), name.data());
    return 0; // missingno is always first
}

} // namespace mc::gfx
