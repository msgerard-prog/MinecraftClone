// Monsters 2 (M16.5; wiki: Skeleton, Creeper, Spider, Enderman). Part of Mobs.
#include "gameplay/Mobs.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Raycast.h"
#include "world/Rotation.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

// Line of sight from a mob's eyes to the player's (no block in between).
bool sees(const World& w, const MobData& m, const Player& player) {
    const glm::dvec3 eye = m.pos + glm::dvec3(0.0, mobInfo(m.type).height * 0.85, 0.0);
    const glm::dvec3 target = player.eyePosition(1.0);
    const glm::dvec3 d = target - eye;
    const double len = glm::length(d);
    return len < 1e-6 || !raycastBlocks(w, eye, d / len, len);
}

bool solid(const World& w, int x, int y, int z) { return blockRegistry().collides(w.getBlock({x, y, z})); }

// Light where a mob stands, as spiders see it: max(block, sky - night darkening).
int lightAt(const World& w, const glm::dvec3& p, float skyDarken) {
    const BlockPos b{int(std::floor(p.x)), int(std::floor(p.y + 0.5)), int(std::floor(p.z))};
    const Chunk* c = w.chunk(b.chunk());
    if (!c || !c->lit() || !w.isInHeight(b.y)) return 0;
    const int sky = c->skyLight(blockToLocal(b.x), b.y, blockToLocal(b.z)) - int(skyDarken);
    return std::max<int>(c->blockLight(blockToLocal(b.x), b.y, blockToLocal(b.z)), sky);
}

// Blocks an enderman picks up (wiki: Enderman › Picking up blocks; those we have).
bool holdable(BlockId b) {
    switch (b) {
    case blocks::GrassBlock:
    case blocks::Dirt:
    case blocks::CoarseDirt:
    case blocks::Sand:
    case blocks::RedSand:
    case blocks::Gravel:
    case blocks::Clay:
    case blocks::Dandelion:
    case blocks::Poppy:
    case blocks::Cornflower:
    case blocks::AzureBluet:
    case blocks::OxeyeDaisy:
    case blocks::Podzol:
    case blocks::Tnt:
    case blocks::Mycelium:
    case blocks::Cactus: // (wiki: Enderman › #enderman_holdable)
    case blocks::Pumpkin:
    case blocks::BrownMushroom:
    case blocks::RedMushroom: return true;
    default: return false;
    }
}

} // namespace

bool Mobs::mayTarget(Context& ctx, const MobData& m) const {
    switch (m.type) {
    case MobType::Spider: // neutral in light 12+ unless provoked (wiki: Spider)
        return m.angry || lightAt(ctx.world, m.pos, ctx.skyDarken) < 12;
    case MobType::Enderman: return m.angry; // only when stared at or hit
    default: return true;
    }
}

bool Mobs::teleport(World& world, MobData& m, const glm::dvec3& around, Xoroshiro& rng) {
    // A random spot up to 32 blocks away (wiki: Enderman › Teleportation): drop down
    // to the first solid block, room for its 3-block height, not into fluids.
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int x = int(std::floor(around.x + (rng.nextDouble() - 0.5) * 64.0));
        int y = int(std::floor(around.y + (rng.nextDouble() - 0.5) * 64.0));
        const int z = int(std::floor(around.z + (rng.nextDouble() - 0.5) * 64.0));
        if (!world.chunk(BlockPos{x, 0, z}.chunk())) continue;
        y = std::clamp(y, world.height().minY + 1, world.height().maxY() - 3);
        while (y > world.height().minY + 1 && !solid(world, x, y - 1, z))
            --y;
        if (!solid(world, x, y - 1, z) || solid(world, x, y, z) || solid(world, x, y + 1, z) || solid(world, x, y + 2, z))
            continue;
        const BlockId at = blockRegistry().blockOf(world.getBlock({x, y, z}));
        if (at == blocks::Water || at == blocks::Lava) continue;
        m.pos = {x + 0.5, double(y), z + 0.5};
        m.prevPos = m.pos; // (a jump, not a slide)
        m.vel = glm::dvec3(0.0);
        m.pathLength = 0;
        return true;
    }
    return false;
}

void Mobs::monsterTick(Context& ctx, MobData& m, bool chase, double playerDist2) {
    const glm::dvec3 playerPos = ctx.player.position();
    const MobInfo& info = mobInfo(m.type);
    switch (m.type) {
    case MobType::Creeper: {
        // Starts swelling within 3 blocks of its target and keeps going while the
        // target stays within 7 and in sight; otherwise it calms down. Explodes after
        // 30 ticks of swelling, with power 3 (wiki: Creeper).
        const bool swelling = chase && playerDist2 <= 7.0 * 7.0 && (playerDist2 < 3.0 * 3.0 || m.fuse > 0) &&
                              sees(ctx.world, m, ctx.player);
        if (swelling) ++m.fuse;
        else if (m.fuse > 0) --m.fuse;
        if (m.fuse >= 30) {
            m_scratchEdits.clear();
            std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
            ExplosionTargets t;
            if (ctx.survival && !ctx.playerDead) {
                t.player = &ctx.player;
                t.vitals = &ctx.vitals;
            }
            m.health = 0.0f;
            m.deathTime = 19; // gone next tick, without loot (it blew itself up)
            m_explosion.explode(ctx.world, m.pos + glm::dvec3(0, 0.0625, 0), 3.0f, ctx.rng, ctx.items, changed, t);
        }
        break;
    }
    case MobType::Skeleton: {
        // Draws for 20 ticks while it sees the player within 15 blocks, then shoots
        // (speed 1.6, spread 6) and waits 40 more: one arrow every 3 s on Normal (wiki:
        // Skeleton).
        if (!chase || playerDist2 > 15.0 * 15.0 || !ctx.projectiles || !sees(ctx.world, m, ctx.player)) {
            m.shootTicks = 0;
            break;
        }
        if (m.attackCooldown > 0) break;
        if (++m.shootTicks >= 20) {
            m.shootTicks = 0;
            m.attackCooldown = 40;
            const glm::dvec3 from = m.pos + glm::dvec3(0, info.height * 0.85 - 0.1, 0);
            glm::dvec3 d = playerPos + glm::dvec3(0, 1.8 / 3.0, 0) - from;
            d.y += std::sqrt(d.x * d.x + d.z * d.z) * 0.2; // aim above for the drop
            // (Starts just outside its own box: vanilla's arrows ignore their shooter.)
            const glm::dvec3 start = from + glm::normalize(d) * (info.width * 0.5 + 0.2);
            ctx.projectiles->shoot(ProjectileKind::Arrow, start, d, 1.6, 6.0, false, false, ctx.rng, m.uuidHi);
        }
        break;
    }
    case MobType::Spider: {
        // Leaps at its target from a few blocks away (wiki: Spider).
        if (chase && m.onGround && playerDist2 > 2.0 * 2.0 && playerDist2 < 6.0 * 6.0 && ctx.rng.nextInt(10) == 0) {
            glm::dvec3 d = playerPos - m.pos;
            d.y = 0.0;
            const double l = glm::length(d);
            if (l > 1e-6) m.vel += d / l * 0.4;
            m.vel.y = 0.4;
        }
        // In bright light a calm spider forgets the player (1 in 100 a tick).
        if (m.targeting && !m.angry && lightAt(ctx.world, m.pos, ctx.skyDarken) >= 12 && ctx.rng.nextInt(100) == 0)
            m.targeting = false;
        break;
    }
    case MobType::Enderman: {
        // Stared at: the player's look ray meets its head, unblocked, within 64 blocks,
        // for 5 ticks in a row - then it is angry for 20-40 s (wiki: Enderman).
        const glm::dvec3 eye = ctx.player.eyePosition(1.0);
        const glm::dvec3 headCentre = m.pos + glm::dvec3(0, info.height - 0.25, 0);
        const double dist = glm::length(headCentre - eye);
        bool stared = false;
        if (ctx.survival && !ctx.playerDead && dist < 64.0 && dist > 1e-6) {
            const glm::dvec3 look(lookVector(ctx.player.yaw(), ctx.player.pitch()));
            const Aabb head{headCentre - glm::dvec3(0.3), headCentre + glm::dvec3(0.3)};
            double t0 = 0.0, t1 = 64.0; // the look ray against the head box
            for (int a = 0; a < 3 && t0 <= t1; ++a) {
                if (std::abs(look[a]) < 1e-12) {
                    if (eye[a] < head.min[a] || eye[a] > head.max[a]) t0 = t1 + 1.0;
                    continue;
                }
                double ta = (head.min[a] - eye[a]) / look[a], tb = (head.max[a] - eye[a]) / look[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
            }
            stared = t0 <= t1 && !raycastBlocks(ctx.world, eye, look, t0);
        }
        m.stareTicks = stared ? int16_t(m.stareTicks + 1) : int16_t(0);
        if (m.stareTicks >= 5) {
            m.angry = true;
            m.targeting = true;
            m.angerTicks = int16_t(400 + ctx.rng.nextInt(400));
        }
        if (m.angry && m.angerTicks > 0 && --m.angerTicks == 0) { // calms down
            m.angry = false;
            m.targeting = false;
        }
        // Water hurts it (1 a tick) and makes it teleport (wiki: Enderman).
        const FluidContact fluid = fluidContact(ctx.world, box(m));
        if (fluid.water) {
            if (m.hurtTime == 0) {
                m.health -= 1.0f;
                m.hurtTime = 10;
            }
            m.wantsTeleport = true;
        }
        if (m.wantsTeleport) {
            m.wantsTeleport = false;
            teleport(ctx.world, m, m.pos, ctx.rng);
        } else if (m.angry && playerDist2 > 16.0 * 16.0 && ctx.rng.nextInt(30) == 0) {
            teleport(ctx.world, m, playerPos, ctx.rng); // closes in on a far target
        }
        // Blocks: pick one up (1 in 20 a tick, within 2 blocks) or put it down (1 in 2000).
        const BlockPos at{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)), int(std::floor(m.pos.z))};
        if (!m.carried && ctx.rng.nextInt(20) == 0) {
            const BlockPos q{at.x + int(ctx.rng.nextInt(5)) - 2, at.y + int(ctx.rng.nextInt(3)),
                             at.z + int(ctx.rng.nextInt(5)) - 2};
            const BlockStateId s = ctx.world.getBlock(q);
            if (holdable(blockRegistry().blockOf(s))) {
                m.carried = s;
                ctx.world.updateBlock(q, 0);
                if (ctx.edits) ctx.edits->push_back(q);
            }
        } else if (m.carried && ctx.rng.nextInt(2000) == 0) {
            const BlockPos q{at.x + int(ctx.rng.nextInt(3)) - 1, at.y + int(ctx.rng.nextInt(3)) - 1,
                             at.z + int(ctx.rng.nextInt(3)) - 1};
            if (ctx.world.getBlock(q) == 0 && solid(ctx.world, q.x, q.y - 1, q.z)) {
                ctx.world.updateBlock(q, m.carried);
                if (ctx.edits) ctx.edits->push_back(q);
                m.carried = 0;
            }
        }
        break;
    }
    default: break;
    }
}

} // namespace mc
