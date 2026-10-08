#include "rendering/ItemIcons.h"

#include "rendering/TextureAtlas.h"
#include "world/Banners.h"
#include "world/Blocks.h"
#include "world/ItemExtras.h"
#include "world/Potions.h"

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
    m_potionOverlay = static_cast<uint16_t>(atlas.spriteIndex("item/potion_overlay"));
    char name[48];
    for (int f = 0; f < 32; ++f) {
        std::snprintf(name, sizeof(name), "item/compass_%02d", f);
        m_compassFrames[size_t(f)] = static_cast<uint16_t>(atlas.spriteIndex(name));
        std::snprintf(name, sizeof(name), "item/recovery_compass_%02d", f);
        m_recoveryFrames[size_t(f)] = static_cast<uint16_t>(atlas.spriteIndex(name));
    }
    for (int f = 0; f < 64; ++f) {
        std::snprintf(name, sizeof(name), "item/clock_%02d", f);
        m_clockFrames[size_t(f)] = static_cast<uint16_t>(atlas.spriteIndex(name));
    }
    m_columns = atlas.columns();
    m_cell = atlas.cellSize();
    m_bannerMasks.clear();
    for (const world::BannerPattern& p : world::kBannerPatterns)
        for (const char* half : {":0,0", ":0,1"})
            m_bannerMasks.push_back(static_cast<uint16_t>(atlas.spriteIndex("clone_banner/" + std::string(p.name) + half)));
    m_compass = items.find("compass").value_or(0);
    m_recovery = items.find("recovery_compass").value_or(0);
    m_clock = items.find("clock").value_or(0);
}

int ItemIcons::compassFrame(const glm::dvec3& player, float yaw, const glm::dvec3& target) {
    // Headings in vanilla's yaw degrees (0 = south/+Z, 90 = west/-X): the target's minus
    // the player's facing is how far right of straight ahead it lies.
    const double dx = target.x - player.x, dz = target.z - player.z;
    const double heading = std::atan2(-dx, dz) * 180.0 / 3.14159265358979323846;
    const double rel = heading - double(yaw);
    const int f = int(std::lround(rel / 360.0 * 32.0));
    return ((f % 32) + 32) % 32;
}

int ItemIcons::clockFrame(double celestial) {
    const int f = int(std::lround(celestial * 64.0));
    return ((f % 64) + 64) % 64;
}

uint16_t ItemIcons::dialSprite(const world::ItemStack& stack, uint16_t sprite) const {
    const Dials& d = m_dials;
    const int spin = int(d.seconds * 24.0); // (a needle with nothing to find turns round and round)
    if (stack.item == m_clock && m_clock)
        return m_clockFrames[size_t(d.dimension == 0 ? clockFrame(d.celestial) : spin % 64)];
    if (stack.item == m_recovery && m_recovery) {
        if (!d.hasDeath || d.deathDimension != d.dimension) return m_recoveryFrames[size_t(spin % 32)];
        return m_recoveryFrames[size_t(compassFrame(d.player, d.yaw, glm::dvec3(d.death) + 0.5))];
    }
    if (stack.item == m_compass && m_compass) {
        if (stack.extra) { // a lodestone compass
            const auto t = world::lodestoneTarget(stack.extra);
            if (!t || !t->hasTarget || t->dimension != d.dimension) return m_compassFrames[size_t(spin % 32)];
            return m_compassFrames[size_t(compassFrame(d.player, d.yaw, glm::dvec3(t->pos.x, t->pos.y, t->pos.z) + 0.5))];
        }
        if (d.dimension != 0) return m_compassFrames[size_t(spin % 32)];
        return m_compassFrames[size_t(compassFrame(d.player, d.yaw, glm::dvec3(d.spawn) + 0.5))];
    }
    return sprite;
}

void ItemIcons::setSprite(world::ItemId id, uint16_t sprite) {
    if (m_sprites.size() <= id) m_sprites.resize(size_t(id) + 1, 0);
    m_sprites[id] = sprite;
}

void ItemIcons::draw(GuiBatch& batch, const BlockModels& models, const world::ItemStack& stack,
                     float x, float y, uint32_t grassTint) const {
    if (stack.empty()) return;
    const world::ItemDef& def = world::itemRegistry().item(stack.item);
    const uint16_t sprite = dialSprite(stack, stack.item < m_sprites.size() ? m_sprites[stack.item] : 0);
    if (sprite && stack.potion && m_potionOverlay) {
        // Potions (M19.4): the liquid, tinted by the potion's colour, under the bottle.
        const uint32_t c = world::potionColour(static_cast<world::Potion>(stack.potion));
        batch.atlasSprite(m_potionOverlay, x, y, rgba(uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)));
        batch.atlasSprite(sprite, x, y);
    } else if (sprite) {
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
