#pragma once

#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "world/Items.h"

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

private:
    std::vector<uint16_t> m_sprites; // per item; 0 = use the block model
};

} // namespace mc::gfx
