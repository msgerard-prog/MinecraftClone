// Monsters 2 (M16.5; wiki: Skeleton, Creeper, Spider, Enderman). Part of Mobs.
#include "gameplay/Mobs.h"

#include "world/BlockUpdates.h"

#include "world/Potions.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Raycast.h"
#include "world/Rotation.h"
#include "world/Weather.h"

#include <cmath>

namespace mc {

namespace {
double centredRand(world::Xoroshiro& rng) { return rng.nextDouble() * 2.0 - 1.0; }
} // namespace

using namespace world;

namespace {

bool sees(const World& w, const MobData& m, const Player& player) { return Mobs::seesPlayer(w, m, player); }

} // namespace

bool Mobs::seesPlayer(const World& w, const MobData& m, const Player& player) {
    const glm::dvec3 eye = m.pos + glm::dvec3(0.0, mobInfo(m.type).height * 0.85, 0.0);
    const glm::dvec3 target = player.eyePosition(1.0);
    const glm::dvec3 d = target - eye;
    const double len = glm::length(d);
    return len < 1e-6 || !raycastBlocks(w, eye, d / len, len);
}

namespace {

bool solid(const World& w, int x, int y, int z) {
    return blockRegistry().collides(w.getBlock({x, y, z}));
}

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
    case blocks::CrimsonNylium: // (wiki: #enderman_holdable)
    case blocks::WarpedNylium:
    case blocks::CrimsonFungus:
    case blocks::WarpedFungus:
    case blocks::CrimsonRoots:
    case blocks::WarpedRoots:
    case blocks::Mycelium:
    case blocks::Cactus: // (wiki: Enderman › #enderman_holdable)
    case blocks::Pumpkin:
    case blocks::BrownMushroom:
    case blocks::RedMushroom:
        return true;
    default:
        return false;
    }
}

} // namespace

bool Mobs::mayTarget(Context& ctx, const MobData& m) const {
    switch (m.type) {
    case MobType::Spider: // neutral in light 12+ unless provoked (wiki: Spider)
    case MobType::CaveSpider:
        return m.angry || lightAt(ctx.world, m.pos, ctx.skyDarken) < 12;
    case MobType::Enderman:
        return m.angry; // only when stared at or hit
    case MobType::ZombifiedPiglin:
        return m.angry; // neutral until it (or one nearby) is hit
    // Drowned go after players in water, or anywhere at night (wiki: Drowned).
    case MobType::Drowned:
        return ctx.player.inWater() || ctx.skyDarken >= 4.0f || ctx.thundering;
    case MobType::Warden:
        return m.angerTicks >= 80; // (M27.3c: hunts only when angry enough)
    case MobType::Creeper: { // creepers keep away from cats and ocelots (wiki: Creeper › Behavior)
        if (m_felines == 0 && m_felinesLastTick == 0) return true;
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                         blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (const MobData& o : ch->mobs())
                        if ((o.type == MobType::Cat || o.type == MobType::Ocelot) &&
                            o.health > 0.0f && glm::dot(o.pos - m.pos, o.pos - m.pos) < 6.0 * 6.0)
                            return false;
        return true;
    }
    case MobType::Piglin: // hostile unless the player wears gold; babies never (wiki: Piglin)
        return !m.isBaby() && m.admireTicks == 0 && (m.angry || !ctx.wearsGold);
    default:
        return true;
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
        if (!solid(world, x, y - 1, z) || solid(world, x, y, z) || solid(world, x, y + 1, z) ||
            solid(world, x, y + 2, z))
            continue;
        const BlockId at = blockRegistry().blockOf(world.getBlock({x, y, z}));
        if (at == blocks::Water || at == blocks::Lava) continue;
        world.levelEvent(LevelEvent::Type::Portal, m.pos.x, m.pos.y,
                         m.pos.z); // (purple sparks at both ends)
        m.pos = {x + 0.5, double(y), z + 0.5};
        world.levelEvent(LevelEvent::Type::Portal, m.pos.x, m.pos.y, m.pos.z);
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
        const bool swelling = chase && playerDist2 <= 7.0 * 7.0 &&
                              (playerDist2 < 3.0 * 3.0 || m.fuse > 0) &&
                              sees(ctx.world, m, ctx.player);
        if (swelling && m.fuse == 0) // the hiss (vanilla: pitch 0.5)
            ctx.world.playSound(Sound::Fuse, m.pos.x, m.pos.y + 1.0, m.pos.z, 1.0f, 0.5f);
        if (swelling)
            ++m.fuse;
        else if (m.fuse > 0)
            --m.fuse;
        if (m.fuse >= 30) {
            m_scratchEdits.clear();
            std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
            ExplosionTargets t;
            t.tnt = ctx.tnt;
            if (ctx.survival && !ctx.playerDead) {
                t.player = &ctx.player;
                t.vitals = &ctx.vitals;
            }
            m.health = 0.0f;
            m.deathTime = 19; // gone next tick, without loot (it blew itself up)
            // (M26.4b) a charged creeper's blast: the mobs it kills drop their heads
            if (m.powered) {
                const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))),
                                  blockToChunk(int(std::floor(m.pos.z)))};
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (Chunk* ch = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                            for (MobData& o : ch->mobs())
                                if (&o != &m && glm::length(o.pos - m.pos) < 12.0)
                                    o.chargedBlast = 3;
            }
            t.breakBlocks = ctx.mobGriefing; // (M28.1: game rule)
            m_explosion.explode(ctx.world, m.pos + glm::dvec3(0, 0.0625, 0),
                                m.powered ? 6.0f : 3.0f, ctx.rng, ctx.items, changed, t);
        }
        break;
    }
    case MobType::Skeleton:
    case MobType::Illusioner: // (M29.1c: a bow like a skeleton's)
    case MobType::Stray:
    case MobType::Bogged:
    case MobType::Parched: {
        // Draws for 20 ticks while it sees the player within 15 blocks, then shoots
        // (speed 1.6, spread 6) and waits 40 more: one arrow every 3 s on Normal (wiki:
        // Skeleton). (M29.1a) Bogged shoot every 3.5 s, parched every 4.5 s; strays' arrows
        // slow for 30 s, bogged ones poison for 4 s, parched ones weaken for 30 s.
        if (!chase || playerDist2 > 15.0 * 15.0 || !ctx.projectiles ||
            !sees(ctx.world, m, ctx.player)) {
            m.shootTicks = 0;
            break;
        }
        if (m.attackCooldown > 0) break;
        // (M29.2a; wiki: Illusioner) now and then it blinds its target for 20 s.
        if (m.type == MobType::Illusioner && ctx.rng.nextInt(200) == 0 && ctx.survival)
            ctx.vitals.addEffect(Effect::Blindness, 0, 400);
        if (++m.shootTicks >= 20) {
            m.shootTicks = 0;
            m.attackCooldown = m.type == MobType::Bogged ? 50 : m.type == MobType::Parched ? 70 : 40;
            if (ctx.difficulty == 3) m.attackCooldown -= 20; // (wiki: a second faster on Hard)
            const glm::dvec3 from = m.pos + glm::dvec3(0, info.height * 0.85 - 0.1, 0);
            glm::dvec3 d = playerPos + glm::dvec3(0, 1.8 / 3.0, 0) - from;
            d.y += std::sqrt(d.x * d.x + d.z * d.z) * 0.2; // aim above for the drop
            // (Starts just outside its own box: vanilla's arrows ignore their shooter.)
            const glm::dvec3 start = from + glm::normalize(d) * (info.width * 0.5 + 0.2);
            if (ctx.projectiles->shoot(ProjectileKind::Arrow, start, d, 1.6, 6.0, false, false,
                                       ctx.rng, m.uuidHi)) {
                Projectile& a = ctx.projectiles->last();
                a.skeleton = true;
                if (m.type == MobType::Stray) a.hitEffect = {Effect::Slowness, 600};
                if (m.type == MobType::Bogged) a.hitEffect = {Effect::Poison, 80};
                if (m.type == MobType::Parched) a.hitEffect = {Effect::Weakness, 600};
            }
            ctx.world.playSound(Sound::BowShoot, m.pos.x, m.pos.y + 1.5, m.pos.z);
        }
        break;
    }
    case MobType::Drowned: {
        // With a trident: thrown at a target within 20 blocks every 1.5 s (wiki: Drowned -
        // its ranged attack).
        if (!m.heldTrident || !chase || playerDist2 > 20.0 * 20.0 || playerDist2 < 2.0 * 2.0 ||
            !ctx.projectiles || m.attackCooldown > 0 || !sees(ctx.world, m, ctx.player))
            break;
        m.attackCooldown = 30;
        const glm::dvec3 from = m.pos + glm::dvec3(0, info.height * 0.85, 0);
        glm::dvec3 d = playerPos + glm::dvec3(0, 0.9, 0) - from;
        d.y += std::sqrt(d.x * d.x + d.z * d.z) * 0.15;
        const glm::dvec3 start = from + glm::normalize(d) * (info.width * 0.5 + 0.3);
        static const ItemId trident = *itemRegistry().find("trident");
        if (ctx.projectiles->shoot(ProjectileKind::Trident, start, d, 1.6, 12.0, false, false,
                                   ctx.rng, m.uuidHi)) {
            ctx.projectiles->last().stack = {trident, 1};
            ctx.projectiles->last().pickup = false; // (a drowned's tridents can't be picked up)
        }
        ctx.world.playSound(Sound::BowShoot, m.pos.x, m.pos.y + 1.5, m.pos.z);
        break;
    }
    case MobType::Pillager: {
        // Loads its crossbow for 25 ticks, then fires a bolt (speed 2, spread 6), then
        // waits 40 more, at a player within 16 it sees (wiki: Pillager, Crossbow) - or
        // at the villager / golem it hunts (M24.5).
        glm::dvec3 aim = playerPos;
        bool haveAim =
            chase && m.targeting && playerDist2 <= 16.0 * 16.0 && sees(ctx.world, m, ctx.player);
        if (!haveAim && m.targetUuid)
            if (const MobData* t = mobByUuid(ctx.world, m.pos, m.targetUuid);
                t && t->health > 0.0f && glm::length(t->pos - m.pos) <= 16.0) {
                aim = t->pos + glm::dvec3(0.0, mobInfo(t->type).height * 0.5 - 1.8 / 3.0, 0.0);
                haveAim = true;
            }
        if (!haveAim || !ctx.projectiles) {
            m.shootTicks = 0;
            break;
        }
        if (m.attackCooldown > 0) break;
        if (++m.shootTicks >= 25) {
            m.shootTicks = 0;
            m.attackCooldown = 40;
            const glm::dvec3 from = m.pos + glm::dvec3(0, info.height * 0.85 - 0.1, 0);
            glm::dvec3 d = aim + glm::dvec3(0, 1.8 / 3.0, 0) - from;
            d.y += std::sqrt(d.x * d.x + d.z * d.z) * 0.12;
            const glm::dvec3 start = from + glm::normalize(d) * (info.width * 0.5 + 0.2);
            ctx.projectiles->shoot(ProjectileKind::Arrow, start, d, 2.0, 6.0, false, false, ctx.rng,
                                   m.uuidHi);
            ctx.world.playSound(Sound::BowShoot, m.pos.x, m.pos.y + 1.5, m.pos.z, 1.0f, 1.3f);
        }
        break;
    }
    case MobType::Evoker: {
        // Spells (wiki: Evoker): every 5 s fangs bite at a player within 16 (ours: 6
        // damage where the player stands a second later unless they moved 2 blocks
        // away), every 17 s three vexes join it (at most 8 around).
        if (m.spellTicks > 0) --m.spellTicks;
        if (m.chargeTicks > 0 && --m.chargeTicks == 0) { // (the fangs rise where it aimed: `beam`)
            if (glm::length(ctx.player.position() - m.beam) < 2.0)
                ctx.vitals.attacked(6.0f, &m.pos);
            ctx.world.levelEvent(LevelEvent::Type::Crit, m.beam.x, m.beam.y + 0.5, m.beam.z);
        }
        if (!chase || playerDist2 > 16.0 * 16.0) break;
        if (m.attackCooldown == 0) {
            m.attackCooldown = 100;
            m.chargeTicks = 20;
            m.beam = ctx.player.position();
        }
        if (m.spellTicks == 0) {
            m.spellTicks = 340;
            int vexes = 0;
            const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                             blockToChunk(int(std::floor(m.pos.z)))};
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                        for (const MobData& o : ch->mobs())
                            vexes += o.type == MobType::Vex && o.health > 0.0f;
            for (int i = 0; i < 3 && vexes < 8 && m_births.size() < m_births.capacity();
                 ++i, ++vexes) {
                MobData vex = make(
                    MobType::Vex,
                    m.pos + glm::dvec3(centredRand(ctx.rng) * 2.0, 1.0, centredRand(ctx.rng) * 2.0),
                    ctx.rng);
                vex.spellTicks = int16_t(20 * (30 + ctx.rng.nextInt(90))); // (lives 30-119 s)
                m_births.push_back(vex);
            }
        }
        break;
    }
    case MobType::Vex: // (wiki: Vex) its time up, it fades: 1 damage a second
        if (m.spellTicks > 0)
            --m.spellTicks;
        else if (ctx.rng.nextInt(20) == 0)
            m.health -= 1.0f;
        break;
    case MobType::Witch: {
        // Drinks when it needs to (wiki: Witch › Behavior): fire resistance while burning,
        // healing now and then when hurt (5% a tick); drinking takes 32 ticks.
        if (m.fireResistTicks > 0) {
            --m.fireResistTicks;
            m.fireTicks = 0;
        }
        if (m.drinkTicks > 0) {
            if (--m.drinkTicks == 0) {
                if (m.drinking == 1)
                    m.health = std::min(info.maxHealth, m.health + 4.0f); // healing
                if (m.drinking == 2) m.fireResistTicks = 3600;            // fire resistance
                m.drinking = 0;
            }
            break;
        }
        if (m.fireTicks > 0 && m.fireResistTicks == 0) {
            m.drinking = 2;
            m.drinkTicks = 32;
            break;
        }
        if (m.health < info.maxHealth && ctx.rng.nextFloat() < 0.05f) {
            m.drinking = 1;
            m.drinkTicks = 32;
            break;
        }
        // Throws a splash potion every 3 s at a player within 10 blocks it sees: slowness
        // from 8+ blocks, poison while the player has 8+ health, weakness up close
        // (1 in 4), else harming.
        if (!chase || playerDist2 > 10.0 * 10.0 || !ctx.projectiles ||
            !sees(ctx.world, m, ctx.player))
            break;
        if (m.attackCooldown > 0) break;
        m.attackCooldown = 60;
        const char* kind = "harming";
        if (playerDist2 >= 8.0 * 8.0 && ctx.vitals.effectLevel(Effect::Slowness) == 0)
            kind = "slowness";
        else if (ctx.vitals.health() >= 8.0f && ctx.vitals.effectLevel(Effect::Poison) == 0)
            kind = "poison";
        else if (playerDist2 <= 3.0 * 3.0 && ctx.vitals.effectLevel(Effect::Weakness) == 0 &&
                 ctx.rng.nextInt(4) == 0)
            kind = "weakness";
        const glm::dvec3 from = m.pos + glm::dvec3(0, info.height * 0.85 - 0.1, 0);
        glm::dvec3 d = playerPos + glm::dvec3(0, 1.0, 0) - from;
        d.y += std::sqrt(d.x * d.x + d.z * d.z) * 0.2; // (an arc)
        const glm::dvec3 start = from + glm::normalize(d) * (info.width * 0.5 + 0.3);
        if (ctx.projectiles->shoot(ProjectileKind::SplashPotion, start, d, 0.75, 8.0, false, false,
                                   ctx.rng, m.uuidHi)) {
            ctx.projectiles->last().potion =
                static_cast<uint8_t>(findPotion(kind).value_or(Potion::Harming));
            ctx.projectiles->last().pickup = false;
        }
        break;
    }
    case MobType::Spider:
    case MobType::CaveSpider: {
        // Leaps at its target from a few blocks away (wiki: Spider).
        if (chase && m.onGround && playerDist2 > 2.0 * 2.0 && playerDist2 < 6.0 * 6.0 &&
            ctx.rng.nextInt(10) == 0) {
            glm::dvec3 d = playerPos - m.pos;
            d.y = 0.0;
            const double l = glm::length(d);
            if (l > 1e-6) m.vel += d / l * 0.4;
            m.vel.y = 0.4;
        }
        // In bright light a calm spider forgets the player (1 in 100 a tick).
        if (m.targeting && !m.angry && lightAt(ctx.world, m.pos, ctx.skyDarken) >= 12 &&
            ctx.rng.nextInt(100) == 0)
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
        if (fluid.water ||
            (ctx.weather && rainingAt(ctx.world, *ctx.weather,
                                      {int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 2.9)),
                                       int(std::floor(m.pos.z))}))) {
            if (m.hurtTime == 0) {
                m.health -= 1.0f;
                m.hurtTime = 10;
            }
            m.wantsTeleport = true;
        }
        // Sunlight (M32.2; vanilla EnderMan.customServerAiStep): by day under the open sky it
        // teleports now and then - the brighter, the likelier - and forgets its target.
        {
            const BlockPos head{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 2.5)), int(std::floor(m.pos.z))};
            if (const Chunk* hc = ctx.world.chunk(head.chunk()); hc && hc->lit() && ctx.world.isInHeight(head.y)) {
                const int sky = hc->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z));
                const float bright = float(std::max(0, sky - int(ctx.skyDarken))) / 15.0f;
                if (bright > 0.5f && sky >= 15 && ctx.rng.nextFloat() * 30.0f < (bright - 0.4f) * 2.0f) {
                    m.angry = false;
                    m.targeting = false;
                    m.wantsTeleport = true;
                }
            }
        }
        // Hurt by something that isn't a creature (fire, lava, a cactus, a fall): it teleports
        // away (vanilla: 9 times in 10).
        if (m.hurtTime == 9 && !m.lastHurtByPlayer && ctx.rng.nextInt(10) != 0) m.wantsTeleport = true;
        if (m.wantsTeleport) {
            m.wantsTeleport = false;
            teleport(ctx.world, m, m.pos, ctx.rng);
        } else if (m.angry && playerDist2 > 16.0 * 16.0 && ctx.rng.nextInt(30) == 0) {
            teleport(ctx.world, m, playerPos, ctx.rng); // closes in on a far target
        }
        // Blocks: pick one up (1 in 20 a tick, within 2 blocks) or put it down (1 in 2000).
        const BlockPos at{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)),
                          int(std::floor(m.pos.z))};
        // (game rule mob_griefing off: endermen leave blocks alone)
        if (!ctx.mobGriefing) {
        } else if (!m.carried && ctx.rng.nextInt(20) == 0) {
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
    case MobType::Silverfish: {
        // Hurt, it calls out the silverfish hiding in infested blocks within 10 blocks
        // (5 up and down): those blocks break and their silverfish join in (wiki).
        if (m.hurtTime == 9) {
            const int bx = int(std::floor(m.pos.x)), by = int(std::floor(m.pos.y)),
                      bz = int(std::floor(m.pos.z));
            int woken = 0;
            for (int dy = -5; dy <= 5 && woken < 20; ++dy)
                for (int dz = -10; dz <= 10 && woken < 20; ++dz)
                    for (int dx = -10; dx <= 10 && woken < 20; ++dx) {
                        const BlockPos q{bx + dx, by + dy, bz + dz};
                        if (!BlockUpdates::isInfested(
                                blockRegistry().blockOf(ctx.world.getBlock(q))))
                            continue;
                        ctx.world.updateBlock(q, 0); // (BlockUpdates lets the silverfish out)
                        if (ctx.edits) ctx.edits->push_back(q);
                        ++woken;
                    }
        }
        // Idle, it now and then slips into a stone block beside it (wiki: Silverfish ›
        // Behavior): the block becomes infested and the silverfish is gone into it.
        if (!chase && m.hurtTime == 0 && ctx.mobGriefing && ctx.rng.nextInt(200) == 0) {
            static constexpr int kDir[6][3] = {{0, -1, 0}, {1, 0, 0},  {-1, 0, 0},
                                               {0, 0, 1},  {0, 0, -1}, {0, 1, 0}};
            const int* d = kDir[ctx.rng.nextInt(6)];
            const BlockPos q{int(std::floor(m.pos.x)) + d[0], int(std::floor(m.pos.y)) + d[1],
                             int(std::floor(m.pos.z)) + d[2]};
            const BlockStateId s = ctx.world.getBlock(q);
            if (const BlockId inf = BlockUpdates::infestedOf(blockRegistry().blockOf(s))) {
                BlockStateId into = blockRegistry().defaultState(inf);
                if (inf == blocks::InfestedDeepslate)
                    into = blockRegistry().set(into, properties::axis,
                                               blockRegistry().get(s, properties::axis));
                ctx.world.updateBlock(q, into);
                if (ctx.edits) ctx.edits->push_back(q);
                m.vanish = true;
                m.health = 0.0f;
                m.deathTime = 19;
            }
        }
        break;
    }
    case MobType::Breeze: {
        // The breeze (M26.4c; wiki: Breeze): within 16 blocks and in sight, it shoots a
        // wind charge every 2-3 s, and leaps about - up and toward or away from its target.
        if (!chase) break;
        if (m.chargeTicks > 0) --m.chargeTicks;
        const glm::dvec3 eye = m.pos + glm::dvec3(0.0, 1.3, 0.0);
        const glm::dvec3 to = playerPos + glm::dvec3(0.0, 1.0, 0.0) - eye;
        const double dist = glm::length(to);
        if (m.chargeTicks == 0 && ctx.projectiles && dist < 16.0 && dist > 1e-6 &&
            !raycastBlocks(ctx.world, eye, to / dist, dist)) {
            ctx.projectiles->shoot(ProjectileKind::WindCharge, eye + to / dist * 0.8, to / dist,
                                   0.7, 1.0, false, false, ctx.rng, m.uuidHi);
            m.chargeTicks = 32; // (wiki: a wind charge every 32 ticks)
        }
        if (m.onGround && ctx.rng.nextInt(40) == 0) {
            glm::dvec3 flat(to.x, 0.0, to.z);
            const double l = glm::length(flat);
            flat = l > 1e-6 ? flat / l : glm::dvec3(1, 0, 0);
            const double away = playerDist2 < 4.0 * 4.0 ? -1.0 : 1.0; // (backs off when close)
            m.vel += flat * (0.4 * away) + glm::dvec3(0.0, 0.7, 0.0);
        }
        break;
    }
    default:
        break;
    }
}

} // namespace mc
