#pragma once

#include "rendering/SpriteImage.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::gfx {

class PackStack;

// Stitches every PNG in a pack folder ("assets/minecraft/textures/block/") into one
// GL texture, like vanilla's block atlas. Sprites sit on a power-of-two grid of equal
// cells; the cell is as big as the largest sprite (HD resource packs) and smaller
// sprites are scaled up nearest-neighbour. Mipmaps (up to 4 levels, the 1.21
// default) never mix two sprites. Animated sprites (vertical strips) cycle through
// their frames, one atlas upload per frame change, as vanilla does each tick.
class TextureAtlas {
public:
    static constexpr int kMinCellSize = 16;
    static constexpr int kMaxMipLevels = 4; // 1.21.11 default (Fancy preset; Fast uses 2)
    // Vanilla's missing-texture sprite: magenta/black checkerboard. Always sprite 0.
    static constexpr std::string_view kMissing = "missingno";

    TextureAtlas() = default;
    ~TextureAtlas();
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    // Load time only. `folder` is a pack path ending in '/'; sprite names are file stems
    // (prefixed with the folder's `prefix`, e.g. "item/stick").
    struct AtlasFolder {
        std::string_view folder;
        std::string_view prefix;
    };
    bool build(const PackStack& packs, std::string_view folder);
    bool build(const PackStack& packs, std::span<const AtlasFolder> folders);

    // Advance animations by one game tick (20 per second). Main thread.
    void tick();

    // Grid index of a sprite (row-major, `columns()` per row); unknown names log a
    // warning and return the missing sprite (index 0). Load/bake time only.
    bool has(std::string_view name) const; // (no warning when missing)
    int spriteIndex(std::string_view name) const;
    int columns() const { return m_columns; }
    int cellSize() const { return m_cellSize; }
    int spriteCount() const { return m_spriteCount; }
    int animatedCount() const { return static_cast<int>(m_animations.size()); }
    uint32_t texture() const { return m_texture; }

    // RGBA pixels of the 16x16 missing-texture sprite (GL-free, unit-tested).
    static std::vector<uint8_t> missingSpritePixels();

private:
    struct Animation {
        int sprite = 0;
        int frametime = 1;
        int frame = 0;
        int ticksLeft = 1;
        std::vector<std::vector<Image>> mips; // [frame][level]
    };
    void uploadFrame(const Animation& anim) const;

    uint32_t m_texture = 0;
    int m_columns = 0;
    int m_cellSize = kMinCellSize;
    int m_mipLevels = kMaxMipLevels;
    int m_spriteCount = 0;
    std::unordered_map<std::string, int> m_indices;
    std::vector<Animation> m_animations;
};

} // namespace mc::gfx
