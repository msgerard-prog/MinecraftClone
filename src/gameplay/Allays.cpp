// Allays (M26.5a; wiki: Allay). Part of Mobs.
//
// Give an allay an item and it fetches more of the same: it looks for dropped stacks of
// it within 32 blocks of the player it follows (within 64), picks them up (a stack at
// most) and drops them by that player - or, for 30 s after hearing a note block play
// within 16 blocks, by the note block. An empty hand takes its item back. It dances
// beside a playing jukebox, and a dancing allay given an amethyst shard duplicates
// (then both wait 5 minutes). Hurt, it heals 2 a second.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/Items.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

Mobs::Use Mobs::allayInteract(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    const auto& reg = itemRegistry();
    const glm::dvec3 at = m.pos + glm::dvec3(0.0, 0.3, 0.0);
    static const ItemId shard = reg.find("amethyst_shard").value_or(kNoItem);
    if (held != kNoItem && held == shard && m.peek == 1 && m.age == 0) { // a dancing allay duplicates
        m.hasEgg = true; // (allayAi adds the new one)
        m.age = 6000;    // (5 minutes before the next)
        return Use::Fed;
    }
    if (held != kNoItem && m.mouthItem == kNoItem) {
        m.mouthItem = held;
        return Use::Fed; // (the allay keeps one)
    }
    if (held == kNoItem && m.mouthItem != kNoItem) { // taken back, with what it carried
        items.spawn(at, {m.mouthItem, 1}, rng);
        if (m.allayCount > 0) items.spawn(at, {m.mouthItem, m.allayCount}, rng);
        m.mouthItem = kNoItem;
        m.allayCount = 0;
        return Use::Sat;
    }
    return Use::None;
}

bool Mobs::allayAi(Context& ctx, MobData& m) {
    if (m.type != MobType::Allay) return false;
    if (m.age > 0) --m.age;
    if (m.hurtTime == 0 && m.health < maxHealthOf(m) && ++m.goalTicks % 10 == 0) m.health = std::min(maxHealthOf(m), m.health + 1.0f);
    // Duplicating (given a shard while dancing).
    if (m.hasEgg) {
        m.hasEgg = false;
        MobData twin = make(MobType::Allay, m.pos + glm::dvec3(0.3, 0.2, 0.0), ctx.rng);
        twin.age = 6000;
        twin.persistent = true;
        if (m_births.size() < m_births.capacity()) m_births.push_back(twin);
    }
    // Dancing within 10 blocks of a playing jukebox (as parrots do).
    const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
    if (ctx.rng.nextInt(20) == 0 || m.peek == 1) {
        bool music = false;
        for (int dz = -1; dz <= 1 && !music; ++dz)
            for (int dx = -1; dx <= 1 && !music; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                    for (const auto& j : ch->jukeboxes())
                        if (j.data.playing && glm::length(glm::dvec3((c0.x + dx) * 16 + j.x + 0.5, j.y + 0.5,
                                                                     (c0.z + dz) * 16 + j.z + 0.5) - m.pos) < 10.0) {
                            music = true;
                            break;
                        }
        m.peek = music ? 1 : 0;
    }
    // A note block played within 16 blocks: deliver there for 30 s.
    for (const LevelEvent& e : ctx.world.levelEvents())
        if (e.type == LevelEvent::Type::Note && glm::length(glm::dvec3(e.x, e.y, e.z) - m.pos) < 16.0) {
            m.workTarget = {int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))};
            m.spellTicks = 600;
        }
    if (m.spellTicks > 0 && --m.spellTicks == 0) m.workTarget.y = kNoPoint;
    const glm::dvec3 player = ctx.player.position() + glm::dvec3(0.0, 1.2, 0.0);
    const bool noteBlock = m.spellTicks > 0 && m.workTarget.y != kNoPoint;
    const bool followPlayer = !ctx.playerDead && glm::length(player - m.pos) < 64.0;
    const glm::dvec3 anchor = noteBlock ? glm::dvec3(m.workTarget.x + 0.5, m.workTarget.y + 1.5, m.workTarget.z + 0.5)
                                        : player;
    glm::dvec3 goal = m.pos;
    double speed = 0.15;
    if (m.peek == 1) { // dancing: turning on the spot
        m.yaw += 18.0f;
        physics(ctx.world, m, glm::dvec3(0.0), false);
        return true;
    }
    if (m.attackCooldown > 0) --m.attackCooldown;
    bool fetching = false;
    // (it carries one stack: up to the item's own stack size - 16 pearls, 1 sword)
    const int carry = m.mouthItem != kNoItem ? std::max<int>(1, itemRegistry().item(m.mouthItem).maxStack) : 0;
    if (m.mouthItem != kNoItem && m.allayCount < carry && (noteBlock || followPlayer)) {
        // The nearest stack of its item within 32 blocks of what it follows.
        const ItemEntity* best = nullptr;
        double bestD = 1e18;
        for (const ItemEntity& e : ctx.items.items())
            if (e.stack.item == m.mouthItem && e.stack.count > 0 && e.pickupDelay == 0 &&
                glm::dot(e.pos - anchor, e.pos - anchor) < 32.0 * 32.0 && glm::dot(e.pos - m.pos, e.pos - m.pos) < bestD) {
                bestD = glm::dot(e.pos - m.pos, e.pos - m.pos);
                best = &e;
            }
        if (best) {
            fetching = true;
            goal = best->pos + glm::dvec3(0.0, 0.3, 0.0);
            speed = 0.25;
            if (glm::length(best->pos - m.pos) < 1.3) { // (picked up: as much as it can carry)
                while (m.allayCount < carry && best && best->stack.count > 0) {
                    const bool last = best->stack.count == 1;
                    ctx.items.takeOne(best);
                    ++m.allayCount;
                    if (last) best = nullptr;
                }
            }
        }
    }
    if (!fetching && m.allayCount > 0 && (noteBlock || followPlayer)) { // deliver
        goal = anchor;
        speed = 0.25;
        if (glm::length(anchor - m.pos) < 3.0 && m.attackCooldown == 0) {
            ItemEntity* e = ctx.items.spawn(m.pos, {m.mouthItem, m.allayCount}, ctx.rng, 20);
            if (e) e->vel = (anchor - m.pos) * 0.1;
            m.allayCount = 0;
            m.attackCooldown = 60; // (3 s between deliveries)
        }
    } else if (!fetching && (noteBlock || followPlayer)) { // keep close, drifting
        if (glm::length(anchor - m.pos) > 4.0) goal = anchor;
        else if (++m.goalTicks % 60 == 0)
            m.goal = anchor + glm::dvec3(ctx.rng.nextDouble() * 4 - 2, ctx.rng.nextDouble() * 2 - 1, ctx.rng.nextDouble() * 4 - 2);
        if (glm::length(anchor - m.pos) <= 4.0) goal = m.goal;
    }
    const glm::dvec3 d = goal - m.pos;
    const double len = glm::length(d);
    const glm::dvec3 wish = len > 0.3 ? d / len * std::min(speed, len) : glm::dvec3(0.0);
    if (len > 0.3) m.yaw = m.headYaw = float(std::atan2(-d.x, d.z) * 180.0 / 3.14159265358979);
    m.limbSwing += 0.8f; // (wings)
    physics(ctx.world, m, wish, false);
    return true;
}

} // namespace mc
