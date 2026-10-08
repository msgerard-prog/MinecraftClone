// Water mobs (M25.2; wiki: Cod, Salmon, Tropical Fish, Pufferfish, Squid, Glow Squid,
// Spawn). Part of Mobs: they swim about in water, flee players (fish), flop and
// suffocate on land; pufferfish puff up and sting.
#include "gameplay/Mobs.h"

#include "gameplay/FluidContact.h"
#include "world/Blocks.h"
#include "world/Potions.h"
#include "world/Raycast.h"
#include "world/Weather.h"

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
    // Burning out of water: 1 a second until it runs out (review fix: this AI returns
    // before the generic fire countdown).
    if (m.fireTicks > 0) {
        if (m.fireTicks % 20 == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        --m.fireTicks;
    }
    // Air (wiki: Fish, Squid): 300 ticks out of water, then 2 damage a second; dolphins
    // dry out after 2 minutes (never in rain), guardians never (review fixes).
    const bool guardian = m.type == MobType::Guardian || m.type == MobType::ElderGuardian;
    const BlockPos feetCell{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)), int(std::floor(m.pos.z))};
    const bool rained = (m.type == MobType::Dolphin || m.type == MobType::Axolotl) && ctx.weather &&
                        rainingAt(ctx.world, *ctx.weather, feetCell);
    // (M26.3c: axolotls last 5 minutes out of water - wiki: Axolotl)
    if (fluid.water || guardian || rained) {
        m.airTicks = m.type == MobType::Dolphin ? 2400 : m.type == MobType::Axolotl ? 6000 : 300;
    } else if (--m.airTicks <= -20) {
        m.airTicks = 0;
        m.health -= 2.0f;
        m.hurtTime = 10;
    }
    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 toPlayer = playerPos - m.pos;
    const double playerDist2 = glm::dot(toPlayer, toPlayer);
    // A tadpole grows into a frog in 20 minutes (M26.3c; wiki: Tadpole): warm where it
    // grows up in a warm biome, cold in a cold one, else temperate.
    if (m.type == MobType::Tadpole && m.age < 0 && ++m.age >= 0) {
        const Chunk* c = ctx.world.chunk(feetCell.chunk());
        const Biome b = c && c->biomes() ? c->biomes()->at(blockToLocal(feetCell.x), feetCell.y, blockToLocal(feetCell.z),
                                                            ctx.world.height())
                                         : Biome::Plains;
        const bool warm = b == Biome::Desert || b == Biome::Savanna || b == Biome::Jungle || b == Biome::SparseJungle ||
                          b == Biome::Badlands || b == Biome::WoodedBadlands || b == Biome::ErodedBadlands ||
                          b == Biome::WarmOcean || b == Biome::NetherWastes || b == Biome::MangroveSwamp ||
                          b == Biome::BambooJungle || b == Biome::SavannaPlateau || b == Biome::WindsweptSavanna;
        const bool cold = b == Biome::SnowyPlains || b == Biome::SnowyTaiga || b == Biome::IceSpikes ||
                          b == Biome::FrozenRiver || b == Biome::FrozenOcean || b == Biome::SnowySlopes ||
                          b == Biome::Grove || b == Biome::FrozenPeaks || b == Biome::JaggedPeaks ||
                          b == Biome::SnowyBeach || b == Biome::DeepFrozenOcean;
        m.type = MobType::Frog;
        m.woolColour = uint8_t(warm ? 1 : cold ? 2 : 0);
        m.health = mobInfo(MobType::Frog).maxHealth;
        m.age = 0;
        return true;
    }
    if (m.type == MobType::Nautilus) { // (M26.5a) breeding like animals; provoked, it bites back
        animalUpkeep(ctx, m);
        if (double unused = 0.0; fluid.water && animalGoal(ctx, m, unused)) m.goalTicks = 0;
        if (m.angry && --m.angerTicks <= 0) m.angry = false;
        if (m.angry && !m.tamed && ctx.survival && !ctx.playerDead && playerDist2 < 16.0 * 16.0) {
            m.goal = playerPos + glm::dvec3(0.0, 0.5, 0.0);
            m.goalTicks = 0;
            if (m.attackCooldown > 0) --m.attackCooldown;
            if (m.attackCooldown == 0 &&
                box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.3), ctx.player.box().max + glm::dvec3(0.3)})) {
                if (ctx.vitals.attacked(mobInfo(m.type).attackDamage, &m.pos)) setPlayerAttacker(m.uuidHi);
                m.attackCooldown = 20;
            }
        }
    }
    if (m.type == MobType::Axolotl) {
        // Playing dead (wiki: Axolotl): a hurt axolotl may lie still for 10 s, healing.
        // (hurtTime was 10 at the hit; the tick counted it down once before the AI)
        if (m.hurtTime == 9 && m.health < maxHealthOf(m) && m.spellTicks == 0 && ctx.rng.nextInt(3) == 0)
            m.spellTicks = 200;
        if (m.spellTicks > 0) {
            --m.spellTicks;
            if (m.spellTicks % 50 == 0) m.health = std::min(maxHealthOf(m), m.health + 1.0f); // (Regeneration I)
            m.vel *= 0.5;
            physics(ctx.world, m, glm::dvec3(0.0), false);
            return true;
        }
        // Growing up and breeding like any animal (to a partner in love).
        animalUpkeep(ctx, m);
        if (double unused = 0.0; fluid.water && m.targetUuid == 0 && animalGoal(ctx, m, unused)) m.goalTicks = 0;
        // Hunting in the water: fish, squid, tadpoles, drowned and guardians within 8.
        if (fluid.water && m.targetUuid == 0 && !m.isBaby() && ctx.rng.nextInt(20) == 0) {
            const ChunkPos c{blockToChunk(feetCell.x), blockToChunk(feetCell.z)};
            double best = 8.0 * 8.0;
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                        for (const MobData& o : ch->mobs())
                            if ((isFish(o.type) || o.type == MobType::Squid || o.type == MobType::GlowSquid ||
                                 o.type == MobType::Tadpole || o.type == MobType::Drowned || o.type == MobType::Guardian ||
                                 o.type == MobType::ElderGuardian) &&
                                o.health > 0.0f && glm::dot(o.pos - m.pos, o.pos - m.pos) < best) {
                                best = glm::dot(o.pos - m.pos, o.pos - m.pos);
                                m.targetUuid = o.uuidHi;
                            }
        }
        if (m.targetUuid != 0) {
            MobData* t = mobByUuid(ctx.world, m.pos, m.targetUuid);
            if (!t || t->health <= 0.0f || glm::length(t->pos - m.pos) > 16.0) {
                // Its prey is gone: a player swimming nearby gets Regeneration I for 5 s
                // (our reading of the wiki's "helping the player").
                if (t && t->health <= 0.0f && ctx.player.inWater() && playerDist2 < 20.0 * 20.0)
                    ctx.vitals.addEffect(Effect::Regeneration, 0, 100);
                m.targetUuid = 0;
            } else {
                m.goal = t->pos;
                m.goalTicks = 0;
                if (m.attackCooldown > 0) --m.attackCooldown;
                if (m.attackCooldown == 0 && box(m).intersects(Aabb{box(*t).min - glm::dvec3(0.3), box(*t).max + glm::dvec3(0.3)})) {
                    t->health -= mobInfo(MobType::Axolotl).attackDamage;
                    t->hurtTime = 10;
                    m.attackCooldown = 20;
                }
            }
        }
        // On land it walks slowly back toward water (wiki: Axolotl).
        if (!fluid.water) {
            if (!waterAt(ctx.world, m.goal) && ctx.rng.nextInt(10) == 0)
                for (int k = 0; k < 8; ++k) {
                    const glm::dvec3 g = m.pos + glm::dvec3(ctx.rng.nextDouble() * 12 - 6, ctx.rng.nextDouble() * 3 - 2,
                                                            ctx.rng.nextDouble() * 12 - 6);
                    if (waterAt(ctx.world, g)) {
                        m.goal = g;
                        break;
                    }
                }
            glm::dvec3 d = m.goal - m.pos;
            d.y = 0.0;
            const double l = glm::length(d);
            const glm::dvec3 wish = l > 0.3 ? d / l * 0.05 : glm::dvec3(0.0);
            if (l > 0.3) m.yaw = m.headYaw = approachAngle(m.yaw, yawTo(m.pos, m.goal), 10.0f);
            physics(ctx.world, m, wish, m.climbing && m.onGround);
            return true;
        }
    }
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
    // Guardians (M25.5; wiki: Guardian, Elder Guardian): a player within 16 blocks in
    // sight is locked on - the guardian stops and its beam charges for 4 s (elder 3 s),
    // then hits for 6 (elder 8). Elders give players within 50 blocks Mining Fatigue III
    // for 5 minutes, checked every minute.
    if (m.type == MobType::Guardian || m.type == MobType::ElderGuardian) {
        const bool elder = m.type == MobType::ElderGuardian;
        if (elder && ++m.spellTicks >= 1200) {
            m.spellTicks = 0;
            int left = 0; // (re-applied when under a minute of III is left - wiki)
            for (const auto& e : ctx.vitals.effects())
                if (e.type == Effect::MiningFatigue && e.amplifier >= 2) left = e.duration;
            if (ctx.survival && !ctx.playerDead && playerDist2 < 50.0 * 50.0 && left < 1200)
                ctx.vitals.addEffect(Effect::MiningFatigue, 2, 6000);
        }
        const glm::dvec3 eye = ctx.player.eyePosition(1.0);
        const glm::dvec3 from = m.pos + glm::dvec3(0.0, info.height * 0.5, 0.0);
        // (range 15, elder 14; after a shot it swims 3 s before locking on again: the
        // negative charge counts that pause up)
        const double range = elder ? 14.0 : 15.0;
        bool locked = ctx.survival && !ctx.playerDead && playerDist2 < range * range && m.chargeTicks >= 0;
        if (locked) {
            const glm::dvec3 d = eye - from;
            const double len = glm::length(d);
            locked = len < 1e-6 || !raycastBlocks(ctx.world, from, d / len, len);
        }
        if (locked) {
            m.hasBeam = true;
            m.beam = eye - glm::dvec3(0.0, 0.3, 0.0);
            m.yaw = m.headYaw = yawTo(m.pos, eye);
            m.vel *= 0.8;
            if (++m.chargeTicks >= (elder ? 60 : 80)) {
                // 6 + 2 magic (elder 8 + 4) on Normal (wiki: Guardian › Laser; ours counts
                // the magic part like the rest).
                ctx.vitals.attacked(elder ? 12.0f : 8.0f, &m.pos);
                m.chargeTicks = -60;
                m.hasBeam = false;
            }
            if (!fluid.water && m.onGround && ctx.rng.nextInt(10) == 0) m.vel.y = 0.4; // (flopping still)
            physics(ctx.world, m, glm::dvec3(0.0), false);
            return true;
        }
        m.hasBeam = false;
        if (m.chargeTicks < 0) ++m.chargeTicks; // (the pause after a shot)
        else m.chargeTicks = 0;
    }
    // A dolphin gives a player swimming within 5 blocks Dolphin's Grace (wiki: Dolphin).
    if (m.type == MobType::Dolphin && ctx.player.inWater() && playerDist2 < 5.0 * 5.0 && !ctx.playerDead)
        ctx.vitals.addEffect(Effect::DolphinsGrace, 0, 100);
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
                if (waterAt(ctx.world, g) &&
                    (m.type != MobType::Dolphin || !waterAt(ctx.world, g + glm::dvec3(0.0, 4.0, 0.0)))) {
                    m.goal = g; // (dolphins keep within a few blocks of the surface, for air)
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
    // Guardians (M25.5): only in a monument's water - here, water beside its prismarine
    // walls (wiki: Guardian › Spawning: within the monument's bounds), groups of 2-4.
    if (y >= 39 && y <= 62 && m_hostiles < 70) {
        static const BlockId bricks = *r.findBlock("prismarine_bricks"), dark = *r.findBlock("dark_prismarine");
        bool monument = false;
        for (int d = 1; d <= 3 && !monument; ++d)
            for (const glm::ivec3 o : {glm::ivec3{d, 0, 0}, glm::ivec3{-d, 0, 0}, glm::ivec3{0, 0, d}, glm::ivec3{0, 0, -d},
                                        glm::ivec3{0, -d, 0}}) {
                const BlockId b = r.blockOf(ctx.world.getBlock({x + o.x, y + o.y, z + o.z}));
                if (b == bricks || b == dark) monument = true;
            }
        if (monument) {
            for (int i = 0, n = 2 + int(ctx.rng.nextInt(3)); i < n && m_hostiles < 70; ++i)
                if (add(ctx.world, make(MobType::Guardian, {x + 0.5, double(y) + 0.1, z + 0.5}, ctx.rng))) ++m_hostiles;
            return;
        }
    }
    // Drowned (M25.3; wiki: Drowned › Spawning): monsters of dark ocean and river water
    // (block light 0, sky light after night darkening at most a random 0..7), under the
    // monster cap; 1 in 16 holds a trident.
    const bool sea = biome == Biome::River || biome == Biome::FrozenRiver || biome == Biome::Ocean ||
                     biome == Biome::DeepOcean || biome == Biome::ColdOcean || biome == Biome::DeepColdOcean ||
                     biome == Biome::LukewarmOcean || biome == Biome::DeepLukewarmOcean || biome == Biome::WarmOcean ||
                     biome == Biome::FrozenOcean || biome == Biome::DeepFrozenOcean;
    // (wiki weights: rivers 100, oceans 5 and only below y 58, frozen rivers 1 - so
    // rivers 8% of attempts here, oceans 1 in 20 of that, frozen rivers 1 in 100)
    const bool river = biome == Biome::River, frozenRiver = biome == Biome::FrozenRiver;
    const uint32_t drownedOdds = river ? 1000u : frozenRiver ? 10u : y < 58 ? 50u : 0u; // per 12500
    if (sea && drownedOdds > 0 && ctx.rng.nextInt(12500) < drownedOdds && m_hostiles < 70 && c->blockLight(lx, y, lz) == 0 &&
        c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken) <= static_cast<int>(ctx.rng.nextInt(8))) {
        MobData d = make(MobType::Drowned, {x + 0.5, double(y), z + 0.5}, ctx.rng);
        d.heldTrident = ctx.rng.nextInt(16) == 0;
        if (add(ctx.world, d)) ++m_hostiles;
        return;
    }
    const bool lush = biome == Biome::LushCaves;
    if ((y < 30 || lush) && c->skyLight(lx, y, lz) == 0 && c->blockLight(lx, y, lz) == 0) { // dark caves
        // Axolotls (M26.3c; wiki: groups of 4-6 in lush caves' water - M27.2c; worlds from
        // before cave biomes: deep cave water below y 0), else glow squid.
        if ((lush || y < 0) && m_axolotls < 5 && ctx.rng.nextInt(3) == 0) kind = MobType::Axolotl, group = 4 + int(ctx.rng.nextInt(3));
        else if (m_glowSquid < 5) kind = MobType::GlowSquid, group = 4 + int(ctx.rng.nextInt(3)); // (wiki: 4-6)
    } else if (y >= 38 && y <= 58 && sea && biome != Biome::River && biome != Biome::FrozenRiver && roll < 4 &&
               m_squid < 5) {
        kind = MobType::Nautilus, group = 1 + int(ctx.rng.nextInt(3)); // (M26.5a; wiki: oceans, y 38-58, 1-3)
    } else if (y < 50 || y > 63) {
        return; // (fish, squid and dolphins: y 50-63 only - wiki)
    } else if (roll < 30) { // the creature list
        // Squid in every ocean and river; dolphins with them in the warmer oceans (wiki:
        // ocean 1:1, lukewarm and warm 10:2, deep lukewarm 8:2), groups of 1-2.
        const bool dolphinBiome = biome == Biome::Ocean || biome == Biome::DeepOcean || biome == Biome::LukewarmOcean ||
                                  biome == Biome::DeepLukewarmOcean || biome == Biome::WarmOcean;
        const uint32_t dolphinIn12 = biome == Biome::Ocean || biome == Biome::DeepOcean ? 6
                                     : biome == Biome::DeepLukewarmOcean                  ? 2 * 12 / 10
                                                                                          : 2;
        if (m_squid < 5) {
            if (dolphinBiome && ctx.rng.nextInt(12) < dolphinIn12) kind = MobType::Dolphin, group = 1 + int(ctx.rng.nextInt(2));
            else kind = MobType::Squid, group = 1 + int(ctx.rng.nextInt(4));
        }
    } else if (m_fish < 20) { // the ambient list (wiki: each ocean's spawn table)
        const uint32_t w = ctx.rng.nextInt(100);
        switch (biome) {
        case Biome::WarmOcean: kind = w < 60 ? MobType::TropicalFish : MobType::Pufferfish; break;
        case Biome::LukewarmOcean: // (tropical 25, cod 15, pufferfish 5)
            kind = w < 56 ? MobType::TropicalFish : w < 89 ? MobType::Cod : MobType::Pufferfish;
            break;
        case Biome::DeepLukewarmOcean: // (tropical 25, cod 8, pufferfish 5)
            kind = w < 66 ? MobType::TropicalFish : w < 87 ? MobType::Cod : MobType::Pufferfish;
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
        else if (kind == MobType::Squid || kind == MobType::Dolphin) ++m_squid;
        else ++m_fish;
    }
}

} // namespace mc
