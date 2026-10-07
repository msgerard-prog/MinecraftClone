#include "rendering/ItemIcons.h"

#include "rendering/TextureAtlas.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mc::gfx {

void ItemIcons::build(const TextureAtlas& atlas) {
    const auto& items = world::itemRegistry();
    m_sprites.assign(items.count(), 0);
    for (size_t i = 0; i < items.count(); ++i) {
        const world::ItemDef& def = items.item(static_cast<world::ItemId>(i));
        if (!def.texture.empty()) m_sprites[i] = static_cast<uint16_t>(atlas.spriteIndex(def.texture));
    }
}

void ItemIcons::setSprite(world::ItemId id, uint16_t sprite) {
    if (m_sprites.size() <= id) m_sprites.resize(size_t(id) + 1, 0);
    m_sprites[id] = sprite;
}

void ItemIcons::draw(GuiBatch& batch, const BlockModels& models, const world::ItemStack& stack,
                     float x, float y, uint32_t grassTint) const {
    if (stack.empty()) return;
    const world::ItemDef& def = world::itemRegistry().item(stack.item);
    const uint16_t sprite = stack.item < m_sprites.size() ? m_sprites[stack.item] : 0;
    if (sprite) {
        batch.atlasSprite(sprite, x, y);
    } else if (def.block) {
        const world::BlockStateId state =
            stack.state ? stack.state : world::blockRegistry().defaultState(def.block);
        if (state < models.size()) batch.blockIcon(models[state], x, y, grassTint);
    }
    // Durability bar (vanilla: 13 px, 2 px tall, under the item).
    if (def.durability > 0 && stack.damage > 0) {
        const float left = std::max(0.0f, 1.0f - float(stack.damage) / float(def.durability));
        const float w = std::round(13.0f * left);
        // Hue from green (full) to red (worn out).
        const float h = left / 3.0f; // 0..1/3 of the hue circle
        const float r = std::clamp(std::abs(h * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
        const float g = std::clamp(2.0f - std::abs(h * 6.0f - 2.0f), 0.0f, 1.0f);
        batch.fill(x + 2, y + 13, 13, 2, rgba(0, 0, 0));
        batch.fill(x + 2, y + 13, w, 1, rgba(uint8_t(r * 255), uint8_t(g * 255), 0));
    }
    if (stack.count > 1) {
        char text[8];
        std::snprintf(text, sizeof(text), "%d", stack.count);
        const int w = batch.textWidth(text);
        batch.text(text, x + 19 - 2 - static_cast<float>(w), y + 6 + 3, argb(0xFFFFFFFF));
    }
}

} // namespace mc::gfx
