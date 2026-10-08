#pragma once

#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "world/Items.h"

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace mc::gfx {

class TextureAtlas;

// How each item looks in the GUI: block items as their block model (isometric or
// flat for plants/torches), other items as their "item/<name>" atlas sprite.
// Built once at load; GL-free.
class ItemIcons {
public:
    void build(const TextureAtlas& atlas);
    // Item sprite (0: none, use the block model).
    uint16_t sprite(world::ItemId id) const { return id < m_sprites.size() ? m_sprites[id] : 0; }
    // The test/helper path: an explicit sprite per item.
    void setSprite(world::ItemId id, uint16_t sprite);

    // Draws a 16x16 item at (x, y) with vanilla's count (bottom right, when > 1) and
    // durability bar (damaged tools: green -> red).
    void draw(GuiBatch& batch, const BlockModels& models, const world::ItemStack& stack, float x,
              float y, uint32_t grassTint) const;

    // Turning icons (M28.2a; wiki: Compass, Recovery Compass, Clock): compasses point to
    // the world spawn (Overworld), their lodestone or the last death, from the player's
    // position and facing; clocks show the sky's turn. Elsewhere they spin. Main sets
    // this each frame (like vanilla's client-side item model properties).
    struct Dials {
        glm::dvec3 player{0.0};
        float yaw = 0.0f; // vanilla degrees (0 = south)
        uint8_t dimension = 0;
        glm::ivec3 spawn{0};
        bool hasDeath = false;
        glm::ivec3 death{0};
        uint8_t deathDimension = 0;
        double celestial = 0.0; // world::celestialAngle (0 = noon)
        double seconds = 0.0;   // for spinning needles
    };
    void setDials(const Dials& d) { m_dials = d; }
    // The needle's frame (of 32) pointing at `target`: 0 straight ahead, clockwise.
    static int compassFrame(const glm::dvec3& player, float yaw, const glm::dvec3& target);
    // The clock's frame (of 64): 0 at noon.
    static int clockFrame(double celestial);
    // (M28.3d) a banner pattern's mask (its top half) for the loom's buttons; atlas texel
    // origin of a sprite and the cell size.
    uint16_t bannerMask(int pattern, int half = 0) const {
        return pattern >= 0 && pattern * 2 + half < int(m_bannerMasks.size()) ? m_bannerMasks[size_t(pattern * 2 + half)] : 0;
    }
    int spriteU(uint16_t sprite) const { return int(sprite % m_columns) * m_cell; }
    int spriteV(uint16_t sprite) const { return int(sprite / m_columns) * m_cell; }
    int cellSize() const { return m_cell; }

private:
    uint16_t dialSprite(const world::ItemStack& stack, uint16_t sprite) const;
    Dials m_dials;
    world::ItemId m_compass = 0, m_recovery = 0, m_clock = 0;
    std::array<uint16_t, 32> m_compassFrames{}, m_recoveryFrames{};
    std::array<uint16_t, 64> m_clockFrames{};
    std::vector<uint16_t> m_bannerMasks;
    int m_columns = 1, m_cell = 16;
    std::vector<uint16_t> m_sprites; // per item; 0 = use the block model
    uint16_t m_potionOverlay = 0;
};

} // namespace mc::gfx
