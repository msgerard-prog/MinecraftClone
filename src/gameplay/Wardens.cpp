// The warden (M27.3c; wiki: Warden). Part of Mobs.
//
// A shrieker's fourth warning calls a warden: it digs its way out of the ground (6.7 s,
// unhurt), then wanders blind. Vibrations within 16 blocks draw it to look; each one a
// player made - and a player within 6 blocks it sniffs out, or hits it - angers it (35
// each, 100 for a hit). At 80 it hunts the player: a 30-damage blow up close, and from
// farther off (up to 15 blocks) a sonic boom that goes through walls and armor for 10
// every 2 s. Every 6 s it darkens the world of players within 20 (Darkness). Its anger
// fades by 1 a second; left alone a minute, it digs back down.
#include "gameplay/Mobs.h"

#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

constexpr int kEmerge = 134, kDig = 100;

} // namespace

bool Mobs::wardenTick(Context& ctx, MobData& m) {
    if (m.type != MobType::Warden) return false;
    if (m.phase == 0 || m.phase == 2) { // emerging or digging: still, unhurt
        m.vel.x = m.vel.z = 0.0;
        m.goal = m.pos;
        if (--m.phaseTicks <= 0) {
            if (m.phase == 0) {
                m.phase = 1;
            } else { // gone back into the ground
                m.health = 0.0f;
                m.deathTime = 19;
                m.lastHurtByPlayer = false;
            }
        }
        return true;
    }
    const glm::dvec3 player = ctx.player.position();
    const double dist = glm::length(player - m.pos);
    const bool reachable = ctx.survival && !ctx.playerDead;
    ++m.goalTicks; // (ticks since something disturbed it)
    // Vibrations it hears.
    for (const World::Vibration& v : ctx.world.vibrations()) {
        if (glm::dot(v.pos - m.pos, v.pos - m.pos) > 16.0 * 16.0) continue;
        m.goal = v.pos;
        m.goalTicks = 0;
        if (v.byPlayer && reachable) m.angerTicks = int16_t(std::min(150, m.angerTicks + 35));
    }
    // Sniffing: a player within 6 blocks every 6 s.
    if (++m.eggTicks % 120 == 0) {
        if (reachable && dist < 6.0) {
            m.angerTicks = int16_t(std::min(150, m.angerTicks + 35));
            m.goalTicks = 0;
        }
        if (reachable && dist < 20.0) ctx.vitals.addEffect(Effect::Darkness, 0, 260); // (its pulse of darkness)
    }
    if (m.angerTicks > 0 && m.eggTicks % 20 == 0) --m.angerTicks;
    // The sonic boom: angry, the player out of reach of its arms but within 15 (20 up or
    // down) - charged for 1.7 s, then 10 damage through anything, every 2 s.
    if (m.chargeTicks > 0) --m.chargeTicks;
    const bool angry = m.angerTicks >= 80 && reachable;
    const double flat = glm::length(glm::dvec2(player.x - m.pos.x, player.z - m.pos.z));
    if (m.spellTicks > 0) {
        m.goal = m.pos;
        if (--m.spellTicks == 0 && angry && flat <= 15.0 && std::abs(player.y - m.pos.y) <= 20.0) {
            ctx.vitals.damage(10.0f);
            setPlayerAttacker(m.uuidHi);
            const glm::dvec2 push = flat > 1e-6 ? glm::dvec2(player.x - m.pos.x, player.z - m.pos.z) / flat : glm::dvec2(0.0);
            ctx.player.knockback(-push.x, -push.y, 1.2);
            ctx.world.levelEvent(LevelEvent::Type::Explosion, (m.pos.x + player.x) * 0.5, m.pos.y + 1.5,
                                 (m.pos.z + player.z) * 0.5, 0);
        }
        return true;
    }
    if (angry && m.chargeTicks == 0 && dist > 3.0 && flat <= 15.0 && std::abs(player.y - m.pos.y) <= 20.0) {
        m.spellTicks = 34;
        m.chargeTicks = 40;
        m.yaw = m.headYaw = float(std::atan2(-(player.x - m.pos.x), player.z - m.pos.z) * 180.0 / 3.14159265358979);
        return true;
    }
    // Left alone for a minute: back into the ground.
    if (m.angerTicks == 0 && m.goalTicks > 1200) {
        m.phase = 2;
        m.phaseTicks = kDig;
        return true;
    }
    return false; // (the monster goals: hunting when angry - mayTarget - else wardenGoal)
}

bool Mobs::wardenGoal(Context& ctx, MobData& m, double& speed) {
    (void)ctx;
    if (m.type != MobType::Warden) return false;
    // Going to look where it last heard something (slowly), else standing about.
    if (glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.0) m.goal = m.pos;
    speed *= 0.7;
    return true;
}

bool Mobs::summonWarden(World& world, const BlockPos& shrieker, Xoroshiro& rng) {
    // None if one is already within 48 blocks; else out of the ground within 5 blocks.
    const ChunkPos c = shrieker.chunk();
    const glm::dvec3 s(shrieker.x + 0.5, shrieker.y, shrieker.z + 0.5);
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx)
            if (const Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs())
                    if (o.type == MobType::Warden && o.health > 0.0f && glm::length(o.pos - s) < 48.0) return false;
    const auto& r = blockRegistry();
    for (int tries = 0; tries < 30; ++tries) {
        const int x = shrieker.x + int(rng.nextInt(11)) - 5, z = shrieker.z + int(rng.nextInt(11)) - 5;
        for (int y = shrieker.y + 3; y >= shrieker.y - 6; --y) {
            if (!r.collides(world.getBlock({x, y - 1, z}))) continue;
            if (r.collides(world.getBlock({x, y, z})) || r.collides(world.getBlock({x, y + 1, z})) ||
                r.collides(world.getBlock({x, y + 2, z})))
                break;
            MobData w = make(MobType::Warden, {x + 0.5, double(y), z + 0.5}, rng);
            w.phase = 0;
            w.phaseTicks = kEmerge;
            w.goal = s;
            w.persistent = true;
            return add(world, w);
        }
    }
    return false;
}

} // namespace mc
