// The Wither (M26.4b; wiki: Wither). Part of Mobs.
//
// Built of four soul sand (or soul soil) blocks in a T with three wither skeleton skulls
// on top; the skull placed last brings it to life. It first charges for 11 seconds,
// unhurt, its health filling up, then bursts out with an explosion of power 7. Then it
// hovers above its target - the player, or any living thing that isn't undead - firing
// wither skulls (8 damage, Wither II, a small blast); hurt, it breaks the blocks around
// it. It regenerates 1 health a second; below half health its armour turns arrows.
// Killed, it drops a nether star.
#include "gameplay/Mobs.h"

#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

constexpr int kCharge = 220; // ticks of charging after it is built (wiki: 11 s)

bool isSoul(BlockId b) { return b == blocks::SoulSand || b == blocks::SoulSoil; }
bool isWitherSkull(BlockId b) { return b == blocks::WitherSkeletonSkull || b == blocks::WitherSkeletonWallSkull; }

} // namespace

bool Mobs::witherProof(BlockId b) {
    return b == blocks::Bedrock || b == blocks::EndPortal || b == blocks::EndPortalFrame || b == blocks::EndGateway;
}

bool Mobs::buildWither(World& world, const BlockPos& skull, Xoroshiro& rng) {
    const auto& r = blockRegistry();
    if (!isWitherSkull(r.blockOf(world.getBlock(skull)))) return false;
    // The skull row lies along x or z; the one just placed may be any of the three.
    for (const glm::ivec2 axis : {glm::ivec2{1, 0}, glm::ivec2{0, 1}})
        for (int offset = -1; offset <= 1; ++offset) {
            const BlockPos mid{skull.x - axis.x * offset, skull.y, skull.z - axis.y * offset};
            bool ok = true;
            for (int k = -1; k <= 1 && ok; ++k) {
                const BlockPos top{mid.x + axis.x * k, mid.y, mid.z + axis.y * k};
                const BlockPos sand{top.x, top.y - 1, top.z};
                ok = isWitherSkull(r.blockOf(world.getBlock(top))) && isSoul(r.blockOf(world.getBlock(sand)));
                // (the bottom corners beside the stem must be free: a T, not a block)
                if (ok && k != 0) ok = !r.collides(world.getBlock({top.x, top.y - 2, top.z}));
            }
            const BlockPos stem{mid.x, mid.y - 2, mid.z};
            if (!ok || !isSoul(r.blockOf(world.getBlock(stem)))) continue;
            for (int k = -1; k <= 1; ++k) {
                world.updateBlock({mid.x + axis.x * k, mid.y, mid.z + axis.y * k}, 0);
                world.updateBlock({mid.x + axis.x * k, mid.y - 1, mid.z + axis.y * k}, 0);
            }
            world.updateBlock(stem, 0);
            MobData w = make(MobType::Wither, {stem.x + 0.5, double(stem.y), stem.z + 0.5}, rng);
            w.health = 1.0f; // (charging: it fills up)
            w.spellTicks = kCharge;
            w.persistent = true;
            return add(world, w);
        }
    return false;
}

bool Mobs::witherAi(Context& ctx, MobData& m) {
    if (m.type != MobType::Wither) return false;
    const MobInfo& info = mobInfo(m.type);
    m.vel *= 0.6;
    if (m.spellTicks > 0) { // charging: still, unhurt, filling up
        m.health = std::min(info.maxHealth, m.health + (info.maxHealth - 1.0f) / float(kCharge));
        m.hurtTime = 0;
        if (--m.spellTicks == 0) { // ...then the blast that frees it
            ExplosionTargets t;
            t.tnt = ctx.tnt;
            if (ctx.survival && !ctx.playerDead) {
                t.player = &ctx.player;
                t.vitals = &ctx.vitals;
            }
            m_scratchEdits.clear();
            m_explosion.explode(ctx.world, m.pos + glm::dvec3(0.0, 1.75, 0.0), 7.0f, ctx.rng, ctx.items,
                                ctx.edits ? *ctx.edits : m_scratchEdits, t);
            m.health = info.maxHealth;
        }
        m.limbSwing += 0.1f;
        return true;
    }
    // Regeneration: 1 a second (wiki).
    if (++m.goalTicks % 20 == 0) m.health = std::min(info.maxHealth, m.health + 1.0f);
    // Hurt: it breaks what it's stuck in and around (3 wide, 4 high).
    if (m.hurtTime == 9) {
        const int bx = int(std::floor(m.pos.x)), by = int(std::floor(m.pos.y)), bz = int(std::floor(m.pos.z));
        for (int dy = 0; dy <= 3; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx) {
                    const BlockPos q{bx + dx, by + dy, bz + dz};
                    const BlockStateId s = ctx.world.getBlock(q);
                    if (s == 0 || witherProof(blockRegistry().blockOf(s)) || !ctx.world.isInHeight(q.y)) continue;
                    ctx.world.updateBlock(q, 0);
                    if (ctx.edits) ctx.edits->push_back(q);
                }
    }
    // Its target: the player (survival, within 64), else the nearest living thing that
    // isn't undead within 32.
    glm::dvec3 target(0.0);
    bool have = false;
    const glm::dvec3 player = ctx.player.position();
    if (ctx.survival && !ctx.playerDead && glm::length(player - m.pos) < 64.0) {
        target = player + glm::dvec3(0.0, 1.0, 0.0);
        have = true;
    } else if (MobData* o = m.targetUuid ? mobByUuid(ctx.world, m.pos, m.targetUuid) : nullptr; o && o->health > 0.0f) {
        target = o->pos + glm::dvec3(0.0, mobInfo(o->type).height * 0.5, 0.0);
        have = glm::length(target - m.pos) < 40.0;
    }
    if (!have) m.targetUuid = 0;
    if (!have && ctx.rng.nextInt(20) == 0) {
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
        double best = 32.0 * 32.0;
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (const MobData& o : ch->mobs()) {
                        const bool undead = isUndead(o.type);
                        if (undead || o.health <= 0.0f || o.type == MobType::Boat || o.type == MobType::Minecart ||
                            o.type == MobType::EndCrystal)
                            continue;
                        const double d = glm::dot(o.pos - m.pos, o.pos - m.pos);
                        if (d < best) {
                            best = d;
                            m.targetUuid = o.uuidHi;
                        }
                    }
    }
    // Hovering: some 5 blocks above the target and about 8 away; drifting when idle.
    glm::dvec3 goal = m.pos;
    if (have) {
        glm::dvec3 flat(m.pos.x - target.x, 0.0, m.pos.z - target.z);
        const double l = glm::length(flat);
        flat = l > 1e-6 ? flat / l : glm::dvec3(1, 0, 0);
        goal = target + flat * 8.0 + glm::dvec3(0.0, 5.0, 0.0);
        const glm::dvec3 to = target - m.pos;
        m.yaw = m.headYaw = float(std::atan2(-to.x, to.z) * 180.0 / 3.14159265358979);
        // A skull every 2 s or so, when it can see the target (wiki: Wither › Attacks).
        if (m.attackCooldown > 0) --m.attackCooldown;
        if (m.attackCooldown == 0 && ctx.projectiles) {
            const glm::dvec3 from = m.pos + glm::dvec3(0.0, 3.0, 0.0);
            const glm::dvec3 d = target - from;
            const double len = glm::length(d);
            if (len > 1e-6 && !raycastBlocks(ctx.world, from, d / len, len)) {
                ctx.projectiles->shoot(ProjectileKind::WitherSkull, from + d / len * 1.2, d / len, 0.9, 0.0, false, false,
                                       ctx.rng, m.uuidHi);
                m.attackCooldown = int16_t(30 + ctx.rng.nextInt(20));
            } else {
                m.attackCooldown = 10;
            }
        }
    } else {
        goal = m.pos + glm::dvec3(std::cos(m.goalTicks * 0.02) * 2.0, 0.0, std::sin(m.goalTicks * 0.02) * 2.0);
    }
    const glm::dvec3 d = goal - m.pos;
    const double len = glm::length(d);
    const glm::dvec3 wish = len > 0.5 ? d / len * std::min(info.speed * 0.5, len * 0.1) : glm::dvec3(0.0);
    m.limbSwing += 0.15f;
    physics(ctx.world, m, wish, false);
    return true;
}

} // namespace mc
