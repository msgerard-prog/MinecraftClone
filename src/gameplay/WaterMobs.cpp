// Water mobs (M25.2; wiki: Cod, Salmon, Tropical Fish, Pufferfish, Squid, Glow Squid,
// Spawn). Part of Mobs: they swim about in water, flee players (fish), flop and
// suffocate on land; pufferfish puff up and sting.
#include "gameplay/Mobs.h"

#include "gameplay/FluidContact.h"
#include "world/Blocks.h"
#include "world/Potions.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

namespace {

float yawTo(const glm::dvec3& from, const glm::dvec3& to) {
    return static_cast<float>(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 / std::numbers::pi);
}

float approachAngle(float from, float to, float maxStep) {
    float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    return from + std::clamp(d, -maxStep, maxStep);
}

bool waterAt(const World& w, const glm::dvec3& p) {
    const BlockStateId s = w.getBlock({int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))});
    return blockRegistry().blockOf(s) == blocks::Water || blockRegistry().waterlogged(s);
}

} // namespace

bool Mobs::waterAi(Context& ctx, MobData& m) {
    const MobInfo& info = mobInfo(m.type);
    if (!info.swims) return false;
    const FluidContact fluid = fluidContact(ctx.world, box(m));
    // Air (wiki: Fish, Squid): 300 ticks out of water, then 2 damage a second.
    if (fluid.water) {
        m.airTicks = 300;
    } else if (--m.airTicks <= -20) {
        m.airTicks = 0;
        m.health -= 2.0f;
        m.hurtTime = 10;
    }
    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 toPlayer = playerPos - m.pos;
    const double playerDist2 = glm::dot(toPlayer, toPlayer);
    if (m.type == MobType::Pufferfish) {
        // Puffs up (state 0 -> 1 -> 2) when a player is within ~2.5 blocks, deflates
        // a while after; touching a puffed one stings: 1 + state damage and Poison for
        // 3 s per state (wiki: Pufferfish).
        const bool threat = ctx.survival && !ctx.playerDead && playerDist2 < 2.5 * 2.5;
        if (threat) {
            m.chargeTicks = 100;
            if (m.size < 2 && ctx.rng.nextInt(5) == 0) ++m.size;
        } else if (m.chargeTicks > 0) {
            --m.chargeTicks;
        } else if (m.size > 0 && ctx.rng.nextInt(10) == 0) {
            --m.size;
        }
        if (m.size > 0 && threat && m.attackCooldown == 0 &&
            box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.3), ctx.player.box().max + glm::dvec3(0.3)})) {
            if (ctx.vitals.attacked(1.0f + float(m.size), &m.pos)) ctx.vitals.addEffect(Effect::Poison, 0, 60 * m.size);
            m.attackCooldown = 20;
        }
        if (m.attackCooldown > 0) --m.attackCooldown;
    }
    glm::dvec3 wish(0.0);
    if (fluid.water) {
        // Fish flee a player within 8 blocks (wiki: avoid-entity goal); otherwise a
        // random spot in the water nearby every few seconds.
        const bool flee = isFish(m.type) && m.type != MobType::Pufferfish && playerDist2 < 8.0 * 8.0 && ctx.survival;
        if (flee) {
            const glm::dvec3 away = playerDist2 > 1e-6 ? -toPlayer / std::sqrt(playerDist2) : glm::dvec3(1, 0, 0);
            const glm::dvec3 target = m.pos + away * 4.0;
            if (waterAt(ctx.world, target)) m.goal = target, m.goalTicks = 0;
        }
        if (++m.goalTicks > 60 + int(ctx.rng.nextInt(80)) || glm::length(m.goal - m.pos) < 0.6 ||
            !waterAt(ctx.world, m.goal)) {
            m.goalTicks = 0;
            m.goal = m.pos;
            for (int tries = 0; tries < 4; ++tries) {
                const glm::dvec3 g = m.pos + glm::dvec3(ctx.rng.nextDouble() * 12 - 6, ctx.rng.nextDouble() * 6 - 3,
                                                        ctx.rng.nextDouble() * 12 - 6);
                if (waterAt(ctx.world, g)) {
                    m.goal = g;
                    break;
                }
            }
        }
        const glm::dvec3 d = m.goal - m.pos;
        const double dl = glm::length(d);
        if (dl > 0.3) {
            const double speed = info.speed * (flee ? 1.6 : 1.0);
            wish = d / dl * speed;
            m.yaw = m.headYaw = approachAngle(m.yaw, yawTo(m.pos, m.goal), 12.0f);
        }
    } else if (m.onGround && ctx.rng.nextInt(10) == 0) {
        // Flopping on land (wiki: Fish - they flop around): a little hop to a side.
        m.vel = {ctx.rng.nextDouble() * 0.2 - 0.1, 0.4, ctx.rng.nextDouble() * 0.2 - 0.1};
        m.yaw = ctx.rng.nextFloat() * 360.0f;
    }
    physics(ctx.world, m, wish, false);
    return true;
}

void Mobs::spawnWater(Context& ctx) {
    // One attempt a tick (wiki: Spawn): a water spot 24-64 blocks from the player with
    // water above it. Squid ("water creatures", cap 5) in oceans and rivers, fish
    // ("water ambient", cap 20) by the biome's list, glow squid in dark water below
    // y 30 ("underground water creatures", cap 5).
    const glm::dvec3 p = ctx.player.position();
    const int x = int(std::floor(p.x)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int z = int(std::floor(p.z)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int y = int(std::floor(p.y)) + static_cast<int>(ctx.rng.nextInt(65)) - 32;
    if (!ctx.world.isInHeight(y) || !ctx.world.isInHeight(y + 1)) return;
    const double dx = x + 0.5 - p.x, dz = z + 0.5 - p.z, dy = y - p.y;
    if (dx * dx + dy * dy + dz * dz < 24.0 * 24.0) return;
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit() || !c->biomes()) return;
    const auto& r = blockRegistry();
    const BlockStateId here = ctx.world.getBlock({x, y, z}), above = ctx.world.getBlock({x, y + 1, z});
    if (r.blockOf(here) != blocks::Water || r.blockOf(above) != blocks::Water) return;
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    const Biome biome = c->biomes()->at(lx, y, lz, ctx.world.height());
    MobType kind = MobType::Count;
    int group = 1;
    const uint32_t roll = ctx.rng.nextInt(100);
    // Drowned (M25.3; wiki: Drowned › Spawning): monsters of dark ocean and river water
    // (block light 0, sky light after night darkening at most a random 0..7), under the
    // monster cap; 1 in 16 holds a trident.
    const bool sea = biome == Biome::River || biome == Biome::FrozenRiver || biome == Biome::Ocean ||
                     biome == Biome::DeepOcean || biome == Biome::ColdOcean || biome == Biome::DeepColdOcean ||
                     biome == Biome::LukewarmOcean || biome == Biome::DeepLukewarmOcean || biome == Biome::WarmOcean ||
                     biome == Biome::FrozenOcean || biome == Biome::DeepFrozenOcean;
    if (sea && roll < 8 && m_hostiles < 70 && c->blockLight(lx, y, lz) == 0 &&
        c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken) <= static_cast<int>(ctx.rng.nextInt(8))) {
        MobData d = make(MobType::Drowned, {x + 0.5, double(y), z + 0.5}, ctx.rng);
        d.heldTrident = ctx.rng.nextInt(16) == 0;
        if (add(ctx.world, d)) ++m_hostiles;
        return;
    }
    if (y < 30 && c->skyLight(lx, y, lz) == 0 && c->blockLight(lx, y, lz) == 0) { // dark caves
        if (m_glowSquid < 5) kind = MobType::GlowSquid, group = 2 + int(ctx.rng.nextInt(3));
    } else if (roll < 30) { // the creature list
        const bool squidBiome = biome != Biome::WarmOcean && (biome == Biome::River || biome == Biome::Ocean ||
                                biome == Biome::DeepOcean || biome == Biome::ColdOcean || biome == Biome::DeepColdOcean ||
                                biome == Biome::LukewarmOcean || biome == Biome::DeepLukewarmOcean ||
                                biome == Biome::FrozenOcean || biome == Biome::DeepFrozenOcean);
        if (squidBiome && m_squid < 5) kind = MobType::Squid, group = 1 + int(ctx.rng.nextInt(4));
    } else if (m_fish < 20) { // the ambient list (wiki: each ocean's spawn table)
        const uint32_t w = ctx.rng.nextInt(100);
        switch (biome) {
        case Biome::WarmOcean: kind = w < 60 ? MobType::TropicalFish : MobType::Pufferfish; break;
        case Biome::LukewarmOcean:
        case Biome::DeepLukewarmOcean:
            kind = w < 55 ? MobType::TropicalFish : w < 80 ? MobType::Cod : MobType::Pufferfish;
            break;
        case Biome::Ocean:
        case Biome::DeepOcean: kind = MobType::Cod; break;
        case Biome::ColdOcean:
        case Biome::DeepColdOcean: kind = w < 50 ? MobType::Cod : MobType::Salmon; break;
        case Biome::FrozenOcean:
        case Biome::DeepFrozenOcean:
        case Biome::River:
        case Biome::FrozenRiver: kind = MobType::Salmon; break;
        default: break;
        }
        group = kind == MobType::Cod            ? 3 + int(ctx.rng.nextInt(4))
                : kind == MobType::Salmon       ? 1 + int(ctx.rng.nextInt(5))
                : kind == MobType::TropicalFish ? 8
                : kind == MobType::Pufferfish   ? 1 + int(ctx.rng.nextInt(3))
                                                : 1;
    }
    if (kind == MobType::Count) return;
    // A tropical fish school shares one look (wiki: Tropical Fish - they school by variant).
    const MobData look = make(kind, {x + 0.5, double(y), z + 0.5}, ctx.rng);
    for (int i = 0; i < group; ++i) {
        const int gx = x + static_cast<int>(ctx.rng.nextInt(5)) - 2, gz = z + static_cast<int>(ctx.rng.nextInt(5)) - 2;
        if (r.blockOf(ctx.world.getBlock({gx, y, gz})) != blocks::Water) continue;
        MobData mob = make(kind, {gx + 0.5, double(y) + 0.2, gz + 0.5}, ctx.rng);
        mob.size = kind == MobType::TropicalFish ? look.size : mob.size;
        mob.woolColour = look.woolColour;
        mob.color2 = look.color2;
        if (!add(ctx.world, mob)) continue;
        if (kind == MobType::GlowSquid) ++m_glowSquid;
        else if (kind == MobType::Squid) ++m_squid;
        else ++m_fish;
    }
}

} // namespace mc
