// Armor stands (M28.3b; wiki: Armor Stand): part of Mobs. Placed facing the player (in
// 45-degree steps), they fall like blocks would, wear armor and heads put on them with a
// right-click (an empty hand takes back the piece at the height clicked) and break after
// two hits in quick succession (one in creative), dropping themselves and what they wore.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/World.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

Chunk* chunkOf(World& world, const MobData& m) {
    return world.chunk(
        {blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))});
}

// The slot an item goes to: its armor slot (heads and carved pumpkins on the head).
int slotFor(const ItemStack& s) {
    if (s.empty()) return -1;
    const ItemDef& d = itemRegistry().item(s.item);
    if (d.armorSlot >= 1 && d.armorSlot <= 4) return d.armorSlot - 1; // (1 head .. 4 feet)
    if (isMobHead(d.block) || d.id == "minecraft:carved_pumpkin") return 0;
    return -1;
}

} // namespace

void Mobs::refreshWorn(World& world, MobData& m) {
    const Chunk* c = chunkOf(world, m);
    const ItemContents* s = c ? c->mobStore(m.uuidHi) : nullptr;
    for (int i = 0; i < 4; ++i)
        m.worn[size_t(i)] = s && !(*s)[size_t(i)].empty()
                                ? armorMaterial(itemRegistry().item((*s)[size_t(i)].item).id)
                                : 0;
}

bool Mobs::placeArmorStand(World& world, const BlockPos& cell, float playerYaw, Xoroshiro& rng) {
    const auto& r = blockRegistry();
    if (r.collides(world.getBlock(cell)) ||
        r.collides(world.getBlock({cell.x, cell.y + 1, cell.z})))
        return false;
    MobData m = make(MobType::ArmorStand, {cell.x + 0.5, double(cell.y), cell.z + 0.5}, rng);
    // Facing the player, rounded to 45 degrees (wiki: Armor Stand › Placement).
    const float yaw = std::round((playerYaw + 180.0f) / 45.0f) * 45.0f;
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = yaw;
    m.persistent = true;
    return add(world, m);
}

bool Mobs::useArmorStand(World& world, MobData& m, ItemStack& held, double hitY) {
    Chunk* c = chunkOf(world, m);
    if (!c) return false;
    int slot = slotFor(held);
    if (held.empty()) { // which piece: by where on the stand the click landed (wiki)
        const double y = hitY - m.pos.y;
        slot = y >= 1.6 ? 0 : y >= 0.9 ? 1 : y >= 0.4 ? 2 : 3;
    }
    if (slot < 0) return false;
    ItemContents& s = c->addMobStore(m.uuidHi);
    if (held.empty() && s[size_t(slot)].empty()) return false;
    ItemStack put = held;
    if (!put.empty()) put.count = 1;
    ItemStack back = s[size_t(slot)];
    s[size_t(slot)] = put;
    if (!held.empty() &&
        held.count > 1) { // (a stack: one goes on, the old piece can't come back into the slot)
        if (!back.empty()) {
            s[size_t(slot)] = back;
            return false;
        }
        held.count = uint8_t(held.count - 1);
    } else {
        held = back;
    }
    refreshWorn(world, m);
    c->markDirty();
    return true;
}

bool Mobs::hitArmorStand(World& world, MobData& m, bool creative) {
    // The first hit makes it shake; another within 5 ticks knocks it over (wiki).
    if (!creative && m.chargeTicks == 0) {
        m.chargeTicks = 5;
        m.hurtTime = 5;
        if (Chunk* c = chunkOf(world, m)) c->markDirty();
        return false;
    }
    m.health = 0.0f;
    m.lastHurtByPlayer = true;
    if (creative) { // (gone, nothing dropped)
        m.deathTime = 19;
        if (Chunk* c = chunkOf(world, m)) c->removeMobStore(m.uuidHi);
    }
    return true;
}

void Mobs::armorStandTick(Context& ctx, MobData& m) {
    if (m.chargeTicks > 0) --m.chargeTicks;
    if (m.hurtTime > 0) --m.hurtTime;
    // It only falls: a stand at rest sleeps, checking the ground once a second (perf).
    const bool resting = m.onGround && m.vel == glm::dvec3(0.0);
    if (!resting || (++m.ambientTime & 15) == 0)
        physics(ctx.world, m, glm::dvec3(0.0), false); // (silent: its ambient clock is free)
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = m.yaw;
}

void Mobs::dropArmorStand(Context& ctx, MobData& m) {
    const glm::dvec3 at = m.pos + glm::dvec3(0.0, 0.8, 0.0);
    if (Chunk* c = chunkOf(ctx.world, m)) {
        if (ItemContents* s = c->mobStore(m.uuidHi)) {
            for (int i = 0; i < 4; ++i)
                if (!(*s)[size_t(i)].empty()) ctx.items.spawn(at, (*s)[size_t(i)], ctx.rng);
            c->removeMobStore(m.uuidHi);
        }
    }
    if (const auto id = itemRegistry().find("armor_stand")) ctx.items.spawn(at, {*id, 1}, ctx.rng);
}

} // namespace mc
