#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::gfx {

// Texture coordinates of one sprite inside the atlas (v0 = top row of the image).
struct UvRect {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};

// Stitches every 16x16 PNG in a folder into one GL texture, like vanilla's block
// atlas. Sprites sit on a power-of-two grid, so mipmaps (4 levels, the 1.21 default)
// never mix two sprites.
// Not yet vanilla: animated sprites show frame 0 only, and sprites that aren't 16
// wide (higher-resolution resource packs) are skipped. Fix when the first animated
// texture or resource-pack loading arrives.
class TextureAtlas {
public:
    static constexpr int kSpriteSize = 16;
    static constexpr int kMipLevels = 4;
    // Vanilla's missing-texture sprite: magenta/black checkerboard.
    static constexpr std::string_view kMissing = "missingno";

    TextureAtlas() = default;
    ~TextureAtlas();
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    // Load-time only. `folder` is absolute; sprite names are file stems ("stone").
    bool build(const std::string& folder);

    // Looks up a sprite by name (load/mesh-build time); unknown names log a warning and
    // return the missing sprite.
    UvRect sprite(std::string_view name) const;

    // RGBA pixels of the missing-texture sprite (GL-free, unit-tested).
    static std::vector<uint8_t> missingSpritePixels();

    uint32_t texture() const { return m_texture; }
    int spriteCount() const { return static_cast<int>(m_sprites.size()); }
    int width() const { return m_width; }

private:
    uint32_t m_texture = 0;
    int m_width = 0;
    std::unordered_map<std::string, UvRect> m_sprites;
};

} // namespace mc::gfx
