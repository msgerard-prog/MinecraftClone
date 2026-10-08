// Happy ghasts (M26.5b; wiki: Happy Ghast, Dried Ghast, Harness). Part of Mobs.
//
// A dried ghast block left in water soaks up three stages and becomes a ghastling;
// snowballs speed its 20 minutes of growing up by a tenth each. A grown happy ghast
// floats about slowly, follows players holding snowballs, and wears a harness: then a
// rider steers it where they look, jump lifts it. It heals 1 every 30 s, 1 a second in
// rain under the open sky.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/Items.h"
#include "world/Rotation.h"
#include "world/Weather.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace mc {

using namespace world;

Mobs::Use Mobs::happyGhastInteract(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    const std::string_view id = held != kNoItem ? itemRegistry().item(held).id : std::string_view{};
    if (id == "minecraft:snowball") { // a ghastling grows a tenth faster; adults just like it
        if (!m.isBaby()) return Use::None;
        m.age += -m.age / 10;
        return Use::Fed;
    }
    if (m.isBaby()) return Use::None;
    if (id == "minecraft:shears" && m.decor > 0) {
        if (const auto h = itemRegistry().find(std::string(kDyeColours[(m.decor - 1) % 16]) + "_harness"))
            items.spawn(m.pos + glm::dvec3(0.0, 2.0, 0.0), {*h, 1}, rng);
        m.decor = 0;
        return Use::Sheared;
    }
    if (id.ends_with("_harness") && m.decor == 0) {
        for (int c = 0; c < 16; ++c)
            if (id == std::string("minecraft:") + kDyeColours[c] + "_harness") m.decor = uint8_t(c + 1);
        return Use::Fed;
    }
    if (m.decor > 0 && !m.ridden) { // harnessed: climb on
        m.ridden = true;
        return Use::Ride;
    }
    return Use::None;
}

bool Mobs::happyGhastAi(Context& ctx, MobData& m) {
    if (m.type != MobType::HappyGhast) return false;
    animalUpkeep(ctx, m); // (growing up)
    // Healing: 1 every 30 s; 1 a second in rain with the sky open (wiki).
    const BlockPos top{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 4.0)), int(std::floor(m.pos.z))};
    const bool rained = ctx.weather && ctx.weather->raining && rainingAt(ctx.world, *ctx.weather, top);
    const bool clouds = m.pos.y >= 187.0 && m.pos.y <= 196.0; // (and at cloud level - wiki)
    if (++m.goalTicks % (rained || clouds ? 20 : 600) == 0) m.health = std::min(maxHealthOf(m), m.health + 1.0f);
    if (m.ridden) return false; // (mountTick steers it)
    // Following a player holding a snowball within 16 blocks, keeping a little off;
    // otherwise drifting slowly to a spot nearby now and then.
    const glm::dvec3 player = ctx.player.position() + glm::dvec3(0.0, 1.0, 0.0);
    static const ItemId snowball = itemRegistry().find("snowball").value_or(kNoItem);
    glm::dvec3 goal = m.goal;
    const bool harness = !m.isBaby() && ctx.heldItem != kNoItem && itemRegistry().item(ctx.heldItem).id.ends_with("_harness");
    if (!ctx.playerDead && ((snowball != kNoItem && ctx.heldItem == snowball) || harness) &&
        glm::length(player - m.pos) < 16.0) {
        const glm::dvec3 off = m.pos - player;
        const double l = glm::length(off);
        goal = player + (l > 1e-6 ? off / l : glm::dvec3(1, 0, 0)) * (m.isBaby() ? 2.0 : 5.0);
    } else if (m.goalTicks % 200 == 0 || glm::length(m.goal - m.pos) < 1.0) {
        m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 16 - 8, ctx.rng.nextDouble() * 6 - 3, ctx.rng.nextDouble() * 16 - 8);
        goal = m.goal;
    }
    const glm::dvec3 d = goal - m.pos;
    const double len = glm::length(d);
    const glm::dvec3 wish = len > 0.5 ? d / len * 0.06 : glm::dvec3(0.0);
    if (len > 0.5) m.yaw = m.headYaw = float(std::atan2(-d.x, d.z) * 180.0 / 3.14159265358979);
    physics(ctx.world, m, wish, false);
    return true;
}

} // namespace mc
