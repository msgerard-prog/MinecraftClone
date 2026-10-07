// Nether mobs (M19.2; wiki: Ghast, Blaze, Magma Cube, Zombified Piglin, Spawn). Part
// of Mobs.
#include "gameplay/Mobs.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Loot.h"
#include "world/Raycast.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

namespace {

bool seesFrom(const World& w, const glm::dvec3& from, const glm::dvec3& to) {
    const glm::dvec3 d = to - from;
    const double len = glm::length(d);
    return len < 1e-6 || !raycastBlocks(w, from, d / len, len);
}

float yawTo(const glm::dvec3& from, const glm::dvec3& to) {
    // Vanilla yaw: 0 faces +Z (south), 90 faces -X.
    return static_cast<float>(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 / std::numbers::pi);
}

float approachAngle(float from, float to, float maxStep) { // (as in Mobs.cpp)
    float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    d = std::clamp(d, -maxStep, maxStep);
    return from + d;
}

bool roomAt(const World& w, const Aabb& box) {
    for (int x = int(std::floor(box.min.x)); x <= int(std::floor(box.max.x - 1e-7)); ++x)
        for (int y = int(std::floor(box.min.y)); y <= int(std::floor(box.max.y - 1e-7)); ++y)
            for (int z = int(std::floor(box.min.z)); z <= int(std::floor(box.max.z - 1e-7)); ++z) {
                const BlockStateId s = w.getBlock({x, y, z});
                const BlockId b = blockRegistry().blockOf(s);
                if (blockRegistry().collides(s) || b == blocks::Water || b == blocks::Lava) return false;
            }
    return true;
}

} // namespace

bool Mobs::netherAi(Context& ctx, MobData& m) {
    if (m.type == MobType::Piglin) {
        // Admiring a gold ingot for 6 s (still, peaceful), then a barter thrown out
        // toward the player (wiki: Bartering); otherwise it goes for gold on the ground
        // within 8 blocks.
        static const ItemId gold = *itemRegistry().find("gold_ingot");
        if (m.admireTicks > 0) {
            m.targeting = false;
            physics(ctx.world, m, glm::dvec3(0.0), false);
            if (--m.admireTicks == 0 && !m.isBaby()) {
                const ItemStack loot = rollOne(LootTable::PiglinBartering, ctx.rng);
                if (!loot.empty()) {
                    ItemEntity* e = ctx.items.spawn(m.pos + glm::dvec3(0, 1.0, 0), loot, ctx.rng);
                    if (e) {
                        const glm::dvec3 d = ctx.player.position() - m.pos;
                        const double l = glm::length(glm::dvec2(d.x, d.z));
                        if (l > 1e-6) e->vel = glm::dvec3(d.x / l * 0.3, 0.3, d.z / l * 0.3);
                    }
                }
            }
            return true;
        }
        if (!m.isBaby() && !m.targeting)
            if (const ItemEntity* g = ctx.items.nearest(m.pos, gold, 8.0)) {
                if (glm::length(g->pos - m.pos) < 1.5) {
                    ctx.items.takeOne(g);
                    m.admireTicks = 120;
                    return true;
                }
                m.goal = g->pos;
                m.goalTicks = 0;
            }
        return false;
    }
    if (m.type == MobType::Strider) {
        // Strolls (lava is ground to it), wants warped fungus partners like any
        // animal; water hurts it (wiki: Strider).
        animalUpkeep(ctx, m);
        double speed = mobInfo(m.type).speed * 0.5;
        if (!animalGoal(ctx, m, speed) && (++m.goalTicks > 160 || glm::length(m.goal - m.pos) < 1.0)) {
            m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 16 - 8, 0, ctx.rng.nextDouble() * 16 - 8);
            m.goalTicks = 0;
        }
        const glm::dvec2 d(m.goal.x - m.pos.x, m.goal.z - m.pos.z);
        glm::dvec3 wish(0.0);
        if (glm::length(d) > 0.5) {
            wish = glm::dvec3(d.x, 0, d.y) / glm::length(d) * speed;
            m.yaw = m.headYaw = approachAngle(m.yaw, yawTo(m.pos, m.goal), 8.0f);
        }
        const FluidContact fluid = fluidContact(ctx.world, box(m));
        if (fluid.water && m.hurtTime == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        physics(ctx.world, m, wish, false);
        return true;
    }
    if (m.type == MobType::ZombifiedPiglin) { // anger runs out (wiki: 20-55 s)
        if (m.angry && --m.angerTicks <= 0) {
            m.angry = false;
            m.targeting = false;
        }
        return false; // walks, paths and fights like a zombie (generic AI)
    }
    if (m.type != MobType::Ghast && m.type != MobType::Blaze && m.type != MobType::MagmaCube) return false;
    const MobInfo& info = mobInfo(m.type);
    const glm::dvec3 eye = ctx.player.eyePosition(1.0);
    const glm::dvec3 centre = m.pos + glm::dvec3(0.0, info.height * 0.5, 0.0);
    const glm::dvec3 toPlayer = eye - centre;
    const double horiz = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    const bool canTarget = ctx.survival && !ctx.playerDead;
    glm::dvec3 wish(0.0);

    if (m.type == MobType::Ghast) {
        // Targets a player within 64 blocks across and 4 up or down that it can see;
        // charges for a second, then fires (every 3 s: the charge restarts at -40).
        const bool target = canTarget && horiz < 64.0 && std::abs(eye.y - m.pos.y) <= 4.0 &&
                            seesFrom(ctx.world, centre, eye);
        if (target) {
            m.yaw = m.headYaw = yawTo(m.pos, eye);
            if (++m.chargeTicks >= 20) {
                const glm::dvec3 dir = glm::normalize(toPlayer);
                if (ctx.projectiles)
                    ctx.projectiles->shoot(ProjectileKind::GhastFireball, centre + dir * 2.5, dir, 0.6, 0.0, false,
                                           false, ctx.rng, m.uuidHi);
                m.chargeTicks = -40;
            }
        } else if (m.chargeTicks > 0) {
            --m.chargeTicks;
        }
        // Floats to random spots up to 16 blocks away (wiki: Ghast - "floats around").
        if (++m.goalTicks > 100 || glm::length(m.goal - m.pos) < 2.0) {
            m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 32 - 16, ctx.rng.nextDouble() * 32 - 16,
                                        ctx.rng.nextDouble() * 32 - 16);
            m.goal.y = std::clamp(m.goal.y, 8.0, 116.0);
            m.goalTicks = 0;
        }
        const glm::dvec3 d = m.goal - m.pos;
        if (glm::length(d) > 1e-6) wish = d / glm::length(d) * 0.1;
        if (!target) m.yaw = m.headYaw = approachAngle(m.yaw, yawTo(m.pos, m.goal), 5.0f);
        const glm::dvec3 before = m.pos;
        physics(ctx.world, m, wish, false);
        if (glm::length(m.pos - before) < 0.01) m.goalTicks = 1000; // blocked: a new spot
    } else if (m.type == MobType::Blaze) {
        // Hovers near the player at eye height; melee for 6 when close; otherwise
        // charges 3 s and fires three small fireballs 0.3 s apart (wiki: Blaze).
        const double d = glm::length(toPlayer);
        const bool target = canTarget && d < 48.0 && seesFrom(ctx.world, centre, eye);
        if (target) {
            m.yaw = m.headYaw = yawTo(m.pos, eye);
            glm::dvec3 go(toPlayer.x, eye.y + 0.5 - (m.pos.y + info.height * 0.5), toPlayer.z);
            if (horiz < 4.0) go.x = go.z = 0.0;
            if (glm::length(go) > 1e-6) wish = go / glm::length(go) * 0.12;
            if (m.attackCooldown > 0) --m.attackCooldown;
            if (d < 2.0 && m.attackCooldown == 0 && box(m).intersects(ctx.player.box())) {
                if (ctx.vitals.attacked(info.attackDamage, &m.pos)) ctx.player.knockback(toPlayer.x, toPlayer.z);
                m.attackCooldown = 20;
            } else if (d >= 2.0) {
                ++m.chargeTicks;
                if (m.chargeTicks == 60) m.volley = 3;
                if (m.volley > 0 && m.chargeTicks >= 60 && (m.chargeTicks - 60) % 6 == 0) {
                    const glm::dvec3 dir = glm::normalize(toPlayer);
                    if (ctx.projectiles)
                        ctx.projectiles->shoot(ProjectileKind::BlazeFireball, centre + dir * 0.8, dir, 0.9,
                                               std::sqrt(d) * 0.5, false, false, ctx.rng, m.uuidHi);
                    if (--m.volley == 0) m.chargeTicks = 0;
                }
            }
        } else {
            m.chargeTicks = 0;
            m.volley = 0;
            // Idle: drifts about slowly, sinking gently (vanilla blazes fall slowly).
            if (++m.goalTicks > 120 || glm::length(m.goal - m.pos) < 1.0) {
                m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 12 - 6, ctx.rng.nextDouble() * 4 - 2.5,
                                            ctx.rng.nextDouble() * 12 - 6);
                m.goalTicks = 0;
            }
            const glm::dvec3 g = m.goal - m.pos;
            if (glm::length(g) > 1e-6) wish = g / glm::length(g) * 0.05;
            m.yaw = m.headYaw = approachAngle(m.yaw, yawTo(m.pos, m.goal), 6.0f);
        }
        physics(ctx.world, m, wish, false);
        // Water hurts blazes (wiki: Blaze - 1 a tick in water or rain).
        if (fluidContact(ctx.world, box(m)).water && m.hurtTime == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
    } else { // magma cube
        // Jumps every 40-120 ticks, every 13-40 while chasing a player it saw within 16
        // blocks; bigger cubes jump higher; touching the player hurts it by size
        // (wiki: Magma Cube - 3, 4, 6).
        if (!canTarget || toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z > 16.0 * 16.0) m.targeting = false;
        else if (!m.targeting && ++m.sightCheck >= 10) {
            m.sightCheck = 0;
            m.targeting = seesFrom(ctx.world, centre, eye);
        }
        if (m.onGround) {
            if (--m.jumpTicks <= 0) {
                const float yaw = m.targeting ? yawTo(m.pos, eye) : ctx.rng.nextFloat() * 360.0f - 180.0f;
                m.yaw = m.headYaw = yaw;
                const double r = yaw * std::numbers::pi / 180.0;
                m.vel.y = 0.42 + 0.1 * m.size;
                m.goal = glm::dvec3(-std::sin(r), 0.0, std::cos(r)); // (the jump's direction)
                m.jumpTicks = int16_t(m.targeting ? 13 + ctx.rng.nextInt(28) : 40 + ctx.rng.nextInt(81));
            } else {
                m.goal = glm::dvec3(0.0);
            }
        }
        wish = glm::dvec3(m.goal.x, 0.0, m.goal.z) * (0.08 + 0.03 * m.size);
        physics(ctx.world, m, wish, false);
        if (m.attackCooldown > 0) --m.attackCooldown;
        if (canTarget && m.attackCooldown == 0 && box(m).intersects(ctx.player.box())) {
            const float damage = m.size >= 4 ? 6.0f : m.size == 2 ? 4.0f : 3.0f;
            if (ctx.vitals.attacked(damage, &m.pos)) ctx.player.knockback(toPlayer.x, toPlayer.z);
            m.attackCooldown = 10;
        }
    }
    if (m.fireTicks > 0) m.fireTicks = 0; // (fire immune)
    return true;
}

void Mobs::spawnNether(Context& ctx) {
    // Striders (wiki: Strider - groups of 2-4 on lava with air above): one try every
    // 20 ticks on the lava surface near the player, while fewer than 8 are around.
    if (ctx.rng.nextInt(20) == 0) {
        const glm::dvec3 p = ctx.player.position();
        const int x = int(std::floor(p.x)) + static_cast<int>(ctx.rng.nextInt(97)) - 48;
        const int z = int(std::floor(p.z)) + static_cast<int>(ctx.rng.nextInt(97)) - 48;
        int striders = 0;
        ctx.world.forEachTickingChunk([&](Chunk& c) {
            for (const MobData& m : c.mobs())
                striders += m.type == MobType::Strider;
        });
        for (int y = 31; y >= 20 && striders < 8; --y) { // the lava sea's surface (Y 31)
            if (blockRegistry().blockOf(ctx.world.getBlock({x, y, z})) != blocks::Lava) continue;
            if (ctx.world.getBlock({x, y + 1, z}) != 0 || ctx.world.getBlock({x, y + 2, z}) != 0) break;
            const glm::dvec3 d(x + 0.5 - p.x, y - p.y, z + 0.5 - p.z);
            if (glm::dot(d, d) < 24.0 * 24.0) break;
            const int group = 2 + static_cast<int>(ctx.rng.nextInt(3));
            for (int i = 0; i < group; ++i)
                add(ctx.world, make(MobType::Strider, {x + 0.5 + i * 1.2, y + 1.0, z + 0.5}, ctx.rng));
            break;
        }
    }
    // Nether monsters by biome (wiki: Spawn › Java Edition, the Nether's spawn weights):
    // nether wastes - zombified piglin 100 (groups of 4), ghast 50 (1), magma cube 2,
    // enderman 1; soul sand valley - ghast 50, skeleton 20 (5), enderman 1; basalt
    // deltas - magma cube 100 (2-5), ghast 40; crimson forest - zombified piglin 1;
    // warped forest - enderman 1; piglins (wastes 15 in 4s, crimson 5 in 3-4) and
    // hoglins (crimson 9 in 3-4). Striders: 2-4 on lava with air above, everywhere.
    // One attempt a tick near the player; block light 11 or less (ours); the mob cap 70.
    if (m_hostiles >= 70) return;
    const glm::dvec3 p = ctx.player.position();
    const int x = int(std::floor(p.x)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int z = int(std::floor(p.z)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int y = int(std::floor(p.y)) + static_cast<int>(ctx.rng.nextInt(65)) - 32;
    if (!ctx.world.isInHeight(y - 1) || !ctx.world.isInHeight(y + 4)) return;
    const double dx = x + 0.5 - p.x, dz = z + 0.5 - p.z, dy = y - p.y;
    if (dx * dx + dy * dy + dz * dz < 24.0 * 24.0) return;
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit() || !c->biomes()) return;
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    if (c->blockLight(lx, y, lz) > 11) return;
    if (!blockRegistry().collides(ctx.world.getBlock({x, y - 1, z}))) return;
    struct Entry {
        MobType type;
        int weight, minGroup, maxGroup;
    };
    const Biome biome = c->biomes()->at(lx, y, lz, ctx.world.height());
    std::array<Entry, 5> table{};
    int n = 0;
    switch (biome) {
    case Biome::SoulSandValley:
        table = {{{MobType::Ghast, 50, 1, 1}, {MobType::Skeleton, 20, 5, 5}, {MobType::Enderman, 1, 4, 4}}};
        n = 3;
        break;
    case Biome::BasaltDeltas:
        table = {{{MobType::MagmaCube, 100, 2, 5}, {MobType::Ghast, 40, 1, 1}}};
        n = 2;
        break;
    case Biome::CrimsonForest:
        table = {{{MobType::ZombifiedPiglin, 1, 2, 4}, {MobType::Piglin, 5, 3, 4}, {MobType::Hoglin, 9, 3, 4}}};
        n = 3;
        break;
    case Biome::WarpedForest:
        table = {{{MobType::Enderman, 1, 4, 4}}};
        n = 1;
        break;
    default: // nether wastes
        table = {{{MobType::ZombifiedPiglin, 100, 4, 4},
                  {MobType::Ghast, 50, 1, 1},
                  {MobType::MagmaCube, 2, 4, 4},
                  {MobType::Enderman, 1, 4, 4},
                  {MobType::Piglin, 15, 4, 4}}};
        n = 5;
        break;
    }
    int total = 0;
    for (int i = 0; i < n; ++i)
        total += table[size_t(i)].weight;
    int roll = static_cast<int>(ctx.rng.nextInt(uint32_t(total)));
    Entry e = table[0];
    for (int i = 0; i < n; ++i) {
        if (roll < table[size_t(i)].weight) {
            e = table[size_t(i)];
            break;
        }
        roll -= table[size_t(i)].weight;
    }
    const int group = e.minGroup + static_cast<int>(ctx.rng.nextInt(uint32_t(e.maxGroup - e.minGroup + 1)));
    for (int i = 0; i < group && m_hostiles < 70; ++i) {
        const int gx = x + (i == 0 ? 0 : static_cast<int>(ctx.rng.nextInt(5)) - 2);
        const int gz = z + (i == 0 ? 0 : static_cast<int>(ctx.rng.nextInt(5)) - 2);
        MobData m = make(e.type, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
        // Ghasts need a 5x5x4 room (wiki); the others room for their box.
        const Aabb need = e.type == MobType::Ghast ? Aabb{{gx - 2.0, double(y), gz - 2.0}, {gx + 3.0, y + 4.0, gz + 3.0}}
                                                   : box(m);
        if (!roomAt(ctx.world, need)) continue;
        if (e.type == MobType::ZombifiedPiglin && ctx.rng.nextInt(20) == 0) m.age = -24000; // 5% babies (wiki)
        if (e.type == MobType::Piglin && ctx.rng.nextInt(5) == 0) m.age = -24000; // (babies never grow up)
        if (add(ctx.world, m)) ++m_hostiles;
    }
}

} // namespace mc
