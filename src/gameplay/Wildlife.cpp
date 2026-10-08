// Wildlife (M26.3; wiki: Rabbit, Fox, Polar Bear, Panda, Goat, Armadillo). Part of Mobs.
//
// - Rabbits hop everywhere, run from players (unless tempted by carrots or dandelions),
//   wolves and foxes, and raid carrot crops. Their coat follows the biome.
// - Foxes sleep through the day, hunt chickens and rabbits, carry one item in their
//   mouth (picked up from the ground), eat sweet berries off bushes and keep away from
//   players who don't sneak. Foxes bred by the player trust them.
// - Polar bears are neutral: a hit, or a player coming near their cub, makes them fight.
// - Pandas have two genes (main, hidden) that give a personality: lazy ones dawdle,
//   worried ones run from players, aggressive ones hit back, weak ones have half the
//   health, brown ones are brown; cubs sneeze out slime balls.
// - Goats ram whatever stands near (every 30-300 s): a run-up and a charge that knocks
//   its target away; ramming stone, logs, ores or packed ice breaks off a horn.
// - Armadillos roll up when a sprinting or riding player, an undead mob or a hit scares
//   them, take less damage rolled up, and shed a scute every 5-10 minutes.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/BlockUpdates.h"
#include "world/Items.h"
#include "world/Rotation.h"
#include "world/Sounds.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

ItemId itemNamed(const char* name) { return itemRegistry().find(name).value_or(kNoItem); }
bool solid(const World& w, int x, int y, int z) { return blockRegistry().collides(w.getBlock({x, y, z})); }
bool isDay(int64_t dayTime) { return (dayTime % 24000) < 12000; }
ChunkPos chunkOf(const glm::dvec3& p) { return {blockToChunk(int(std::floor(p.x))), blockToChunk(int(std::floor(p.z)))}; }

// The nearest live mob within `range` that `want` accepts (3x3 chunks).
template <typename F> MobData* nearestMob(World& world, const MobData& self, double range, F&& want) {
    MobData* best = nullptr;
    double bestD = range * range;
    const ChunkPos c = chunkOf(self.pos);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (MobData& o : ch->mobs()) {
                    if (&o == &self || o.health <= 0.0f || !want(o)) continue;
                    const double d = glm::dot(o.pos - self.pos, o.pos - self.pos);
                    if (d < bestD) {
                        bestD = d;
                        best = &o;
                    }
                }
    return best;
}

// Runs from `from`: a goal `dist` blocks the other way.
void flee(MobData& m, const glm::dvec3& from, double dist) {
    glm::dvec3 away = m.pos - from;
    away.y = 0.0;
    const double l = glm::length(away);
    m.goal = m.pos + (l > 1e-6 ? away / l : glm::dvec3(1, 0, 0)) * dist;
}

} // namespace

void Mobs::initWildlife(MobData& m, Xoroshiro& rng) {
    switch (m.type) {
    case MobType::Panda: { // (wiki: Panda › Genes - normal 163/256, lazy, worried, playful and
        // aggressive 1/16 each, weak 25/256, brown 1/64; kPandaGenes order)
        auto gene = [&] {
            const uint32_t r = rng.nextInt(256);
            return uint8_t(r < 163 ? 0 : r < 179 ? 1 : r < 195 ? 2 : r < 211 ? 3 : r < 227 ? 6 : r < 252 ? 5 : 4);
        };
        m.woolColour = gene();
        m.color2 = gene();
        if (pandaPersonality(m.woolColour, m.color2) == 5) { // weak: half the health
            m.maxHealth = 10.0f;
            m.health = 10.0f;
        }
        break;
    }
    case MobType::Goat:
        m.powered = rng.nextInt(50) == 0; // (a screaming goat, 2%)
        m.chargeTicks = int16_t(600 + rng.nextInt(5400));
        break;
    case MobType::Armadillo: m.eggTicks = 6000 + int(rng.nextInt(6000)); break;
    case MobType::Tadpole: // (grows into a frog in 20 minutes; never despawns)
        m.age = -24000;
        m.persistent = true;
        break;
    case MobType::Axolotl: m.woolColour = uint8_t(rng.nextInt(4)); break; // (blue only from breeding)
    default: break;
    }
}

void Mobs::wildlifeOffspring(const MobData& a, const MobData& b, MobData& baby, Xoroshiro& rng) {
    switch (baby.type) {
    case MobType::Panda: { // one gene from each parent, now and then a new one (wiki)
        baby.woolColour = rng.nextInt(2) ? a.woolColour : a.color2;
        baby.color2 = rng.nextInt(2) ? b.woolColour : b.color2;
        // Each gene mutates 1 in 32: normal 5/16, weak 5/16, brown 2/16, the others 1/16 each.
        auto mutate = [&](uint8_t& g) {
            if (rng.nextInt(32) != 0) return;
            const uint32_t r = rng.nextInt(16);
            g = uint8_t(r < 5 ? 0 : r < 10 ? 5 : r < 12 ? 4 : r == 12 ? 1 : r == 13 ? 2 : r == 14 ? 3 : 6);
        };
        mutate(baby.woolColour);
        mutate(baby.color2);
        baby.maxHealth = pandaPersonality(baby.woolColour, baby.color2) == 5 ? 10.0f : 0.0f;
        baby.health = maxHealthOf(baby);
        break;
    }
    case MobType::Fox: // a fox born of the player's breeding trusts them (wiki: Fox)
        baby.tamed = true;
        baby.woolColour = rng.nextInt(2) ? a.woolColour : b.woolColour;
        break;
    case MobType::Rabbit: baby.woolColour = rng.nextInt(2) ? a.woolColour : b.woolColour; break;
    case MobType::Goat: // (wiki: about half the kids of a screaming parent scream, else 2%)
        baby.powered = (a.powered || b.powered) ? rng.nextInt(2) == 0 : rng.nextInt(50) == 0;
        break;
    case MobType::Axolotl: // a parent's colour, or 1 in 1200 the rare blue (wiki: Axolotl)
        baby.woolColour = rng.nextInt(1200) == 0 ? 4 : rng.nextInt(2) ? a.woolColour : b.woolColour;
        break;
    default: break;
    }
}

bool Mobs::wildlifeGoal(Context& ctx, MobData& m, double& speed) {
    const glm::dvec3 p = ctx.player.position();
    const double pd = glm::length(p - m.pos);
    switch (m.type) {
    case MobType::Rabbit: {
        // Away from players (unless they hold its food), wolves and foxes.
        if (!ctx.playerDead && ctx.survival && pd < 8.0 && !isFood(m.type, ctx.heldItem)) {
            flee(m, p, 8.0);
            speed *= 2.2;
            return true;
        }
        if (m.panicTicks == 0 && ctx.rng.nextInt(10) == 0)
            if (const MobData* hunter = nearestMob(ctx.world, m, 6.0, [](const MobData& o) {
                    return (o.type == MobType::Wolf && !o.tamed) || o.type == MobType::Fox;
                })) {
                flee(m, hunter->pos, 8.0);
                m.panicTicks = 40;
                return true;
            }
        // Raids a carrot crop now and then: walks there and nibbles a stage off.
        if (m.workTarget.y == kNoPoint && !m.isBaby() && ctx.rng.nextInt(200) == 0) {
            const int bx = int(std::floor(m.pos.x)), by = int(std::floor(m.pos.y)), bz = int(std::floor(m.pos.z));
            for (int dy = -1; dy <= 1 && m.workTarget.y == kNoPoint; ++dy)
                for (int dz = -4; dz <= 4 && m.workTarget.y == kNoPoint; ++dz)
                    for (int dx = -4; dx <= 4; ++dx)
                        if (blockRegistry().blockOf(ctx.world.getBlock({bx + dx, by + dy, bz + dz})) == blocks::Carrots) {
                            m.workTarget = {bx + dx, by + dy, bz + dz};
                            break;
                        }
        }
        if (m.workTarget.y != kNoPoint) {
            const BlockPos t{m.workTarget.x, m.workTarget.y, m.workTarget.z};
            const BlockStateId s = ctx.world.getBlock(t);
            if (blockRegistry().blockOf(s) != blocks::Carrots) {
                m.workTarget.y = kNoPoint;
                return false;
            }
            m.goal = {t.x + 0.5, double(t.y), t.z + 0.5};
            if (glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.0) {
                const int age = blockRegistry().get(s, properties::age7);
                ctx.world.updateBlock(t, age > 0 ? blockRegistry().set(s, properties::age7, age - 1) : BlockStateId{0});
                if (ctx.edits) ctx.edits->push_back(t);
                m.workTarget.y = kNoPoint;
            }
            return true;
        }
        return false;
    }
    case MobType::Fox: {
        // Asleep through the day, unless a player who isn't sneaking comes close or it
        // was hurt; it wakes at night (wiki: Fox › Sleeping).
        const bool near = !ctx.playerDead && pd < 6.0 && !ctx.player.sneaking() && !m.tamed;
        if (isDay(ctx.dayTime) && !near && m.panicTicks == 0 && m.targetUuid == 0 && !m.isBaby()) {
            m.sitting = true; // (drawn curled up)
            m.goal = m.pos;
            m.vel.x = m.vel.z = 0.0;
            return true;
        }
        m.sitting = false;
        if (!m.tamed && !ctx.playerDead && pd < 16.0 && !ctx.player.sneaking() && !isFood(m.type, ctx.heldItem)) {
            flee(m, p, 10.0);
            speed *= 1.5;
            return true;
        }
        // Hunting by night: a chicken or a rabbit within 16 blocks.
        if (m.targetUuid == 0 && !isDay(ctx.dayTime) && !m.isBaby() && ctx.rng.nextInt(200) == 0)
            if (const MobData* prey = nearestMob(ctx.world, m, 16.0, [](const MobData& o) {
                    return o.type == MobType::Chicken || o.type == MobType::Rabbit;
                }))
                m.targetUuid = prey->uuidHi;
        if (m.targetUuid != 0) {
            MobData* t = mobByUuid(ctx.world, m.pos, m.targetUuid);
            if (!t || t->health <= 0.0f || glm::length(t->pos - m.pos) > 24.0) {
                m.targetUuid = 0;
            } else {
                m.goal = t->pos;
                speed *= 1.4;
                if (m.attackCooldown > 0) --m.attackCooldown;
                if (m.attackCooldown == 0 &&
                    glm::length(glm::dvec2(t->pos.x - m.pos.x, t->pos.z - m.pos.z)) < mobInfo(t->type).width * 0.5 + 0.8) {
                    t->health -= mobInfo(MobType::Fox).attackDamage;
                    t->hurtTime = 10;
                    t->panicTicks = 100;
                    m.attackCooldown = 20;
                }
                return true;
            }
        }
        // Sweet berries now and then: to a ripe bush within 8 blocks, picked.
        if (m.workTarget.y == kNoPoint && ctx.rng.nextInt(400) == 0) {
            const int bx = int(std::floor(m.pos.x)), by = int(std::floor(m.pos.y)), bz = int(std::floor(m.pos.z));
            for (int dy = -1; dy <= 1 && m.workTarget.y == kNoPoint; ++dy)
                for (int dz = -8; dz <= 8 && m.workTarget.y == kNoPoint; dz += 2)
                    for (int dx = -8; dx <= 8; dx += 2) {
                        const BlockStateId s = ctx.world.getBlock({bx + dx, by + dy, bz + dz});
                        if (blockRegistry().blockOf(s) == blocks::SweetBerryBush && blockRegistry().get(s, properties::age3) >= 2) {
                            m.workTarget = {bx + dx, by + dy, bz + dz};
                            break;
                        }
                    }
        }
        if (m.workTarget.y != kNoPoint) {
            const BlockPos t{m.workTarget.x, m.workTarget.y, m.workTarget.z};
            m.goal = {t.x + 0.5, double(t.y), t.z + 0.5};
            if (glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.2) {
                static const ItemId berries = itemNamed("sweet_berries");
                const int n = BlockUpdates::pickBerries(ctx.world, t, ctx.rng);
                if (ctx.edits && n > 0) ctx.edits->push_back(t);
                // One into its mouth (if empty), the rest on the ground (wiki: Fox › Food).
                int left = n;
                if (left > 0 && m.mouthItem == kNoItem) {
                    m.mouthItem = berries;
                    --left;
                }
                if (left > 0) ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {berries, uint8_t(left)}, ctx.rng);
                m.workTarget.y = kNoPoint;
            }
            return true;
        }
        return false;
    }
    case MobType::PolarBear: {
        // A grown bear with a cub nearby fights a player who comes close (wiki).
        if (!m.isBaby() && !m.angry && ctx.survival && !ctx.playerDead && pd < 8.0 && ctx.rng.nextInt(10) == 0 &&
            nearestMob(ctx.world, m, 16.0, [](const MobData& o) { return o.type == MobType::PolarBear && o.isBaby(); })) {
            m.angry = true;
            m.angerTicks = 400;
        }
        return false;
    }
    case MobType::Panda: {
        const int personality = pandaPersonality(m.woolColour, m.color2);
        if (personality == 1) speed *= 0.5; // lazy
        if (personality == 2 && !ctx.playerDead && pd < 8.0 && !ctx.player.sneaking()) { // worried
            flee(m, p, 10.0);
            speed *= 1.5;
            return true;
        }
        // Bamboo on the ground: walks over and eats it.
        static const ItemId bamboo = itemRegistry().blockItem(blocks::Bamboo);
        if (m.eatTicks == 0 && ctx.rng.nextInt(20) == 0)
            if (const ItemEntity* e = ctx.items.nearest(m.pos, bamboo, 6.0)) {
                m.goal = e->pos;
                if (glm::length(e->pos - m.pos) < 1.5) {
                    ctx.items.takeOne(e);
                    m.eatTicks = 100;
                    m.health = std::min(maxHealthOf(m), m.health + 1.0f);
                }
                return true;
            }
        if (m.eatTicks > 0) {
            m.goal = m.pos;
            return true;
        }
        return false;
    }
    case MobType::Frog: {
        // A frog carrying spawn lays it on water nearby (wiki: Frog › Breeding).
        if (m.hasEgg) {
            if (m.workTarget.y == kNoPoint && ctx.rng.nextInt(10) == 0)
                for (int k = 0; k < 16; ++k) {
                    const BlockPos w{int(std::floor(m.pos.x)) + int(ctx.rng.nextInt(17)) - 8, int(std::floor(m.pos.y)) - 1,
                                     int(std::floor(m.pos.z)) + int(ctx.rng.nextInt(17)) - 8};
                    for (int dy = 1; dy >= -2; --dy) {
                        const BlockPos q{w.x, w.y + dy, w.z};
                        if (blockRegistry().blockOf(ctx.world.getBlock(q)) == blocks::Water &&
                            ctx.world.getBlock({q.x, q.y + 1, q.z}) == 0) {
                            m.workTarget = {q.x, q.y + 1, q.z};
                            break;
                        }
                    }
                    if (m.workTarget.y != kNoPoint) break;
                }
            if (m.workTarget.y != kNoPoint) {
                const BlockPos t{m.workTarget.x, m.workTarget.y, m.workTarget.z};
                m.goal = {t.x + 0.5, double(t.y), t.z + 0.5};
                if (glm::length(m.goal - m.pos) < 1.6) {
                    if (ctx.world.getBlock(t) == 0 &&
                        blockRegistry().blockOf(ctx.world.getBlock({t.x, t.y - 1, t.z})) == blocks::Water) {
                        ctx.world.updateBlock(t, blockRegistry().defaultState(blocks::Frogspawn));
                        if (ctx.edits) ctx.edits->push_back(t);
                        m.hasEgg = false;
                    }
                    m.workTarget.y = kNoPoint;
                }
                return true;
            }
        }
        // Frogs eat small slimes and magma cubes with their tongue from up to 3 blocks: a
        // slime leaves a slime ball, a magma cube the froglight of the frog's kind (wiki).
        if (m.targetUuid == 0 && !m.isBaby() && ctx.rng.nextInt(40) == 0)
            if (const MobData* prey = nearestMob(ctx.world, m, 10.0, [](const MobData& o) {
                    return (o.type == MobType::Slime || o.type == MobType::MagmaCube) && o.size == 1;
                }))
                m.targetUuid = prey->uuidHi;
        if (m.targetUuid != 0) {
            MobData* t = mobByUuid(ctx.world, m.pos, m.targetUuid);
            if (!t || t->health <= 0.0f || glm::length(t->pos - m.pos) > 16.0) {
                m.targetUuid = 0;
                return false;
            }
            m.goal = t->pos;
            if (glm::length(t->pos - m.pos) < 3.0) {
                static const ItemId slimeBall = itemNamed("slime_ball");
                static constexpr BlockId kLights[3] = {blocks::OchreFroglight, blocks::PearlescentFroglight,
                                                       blocks::VerdantFroglight};
                const ItemStack drop = t->type == MobType::Slime
                                           ? ItemStack{slimeBall, 1}
                                           : ItemStack{itemRegistry().blockItem(kLights[m.woolColour % 3]), 1};
                ctx.items.spawn(t->pos + glm::dvec3(0, 0.3, 0), drop, ctx.rng);
                t->health = 0.0f;
                t->deathTime = 19; // (swallowed: no other drops)
                t->vanish = true;
                m.targetUuid = 0;
                m.goal = m.pos;
            }
            return true;
        }
        return false;
    }
    case MobType::Armadillo:
        if (m.sitting) { // rolled up: still
            m.goal = m.pos;
            m.vel.x = m.vel.z = 0.0;
            return true;
        }
        return false;
    default: return false;
    }
}

void Mobs::wildlifeTick(Context& ctx, MobData& m, bool blockedAhead) {
    switch (m.type) {
    case MobType::Fox: {
        // Picks up an item lying within a block, if its mouth is empty; eats food it carries.
        if (m.mouthItem == kNoItem && ctx.rng.nextInt(10) == 0)
            for (const ItemEntity& e : ctx.items.items())
                if (e.pickupDelay == 0 && e.stack.count > 0 && glm::length(e.pos - m.pos) < 1.2) {
                    m.mouthItem = e.stack.item;
                    ctx.items.takeOne(&e);
                    break;
                }
        if (m.mouthItem != kNoItem && itemRegistry().item(m.mouthItem).food > 0 && ctx.rng.nextInt(600) == 0) {
            m.health = std::min(maxHealthOf(m), m.health + float(itemRegistry().item(m.mouthItem).food));
            m.mouthItem = kNoItem;
        }
        break;
    }
    case MobType::Panda: {
        if (m.eatTicks > 0) --m.eatTicks;
        // Cubs sneeze now and then (weak ones often) and a slime ball flies out (wiki).
        const int personality = pandaPersonality(m.woolColour, m.color2);
        if (m.isBaby() && ctx.rng.nextInt(personality == 5 ? 500 : 6000) == 0) {
            static const ItemId slime = itemNamed("slime_ball");
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.6, 0), {slime, 1}, ctx.rng);
            ctx.world.playSound(mobSound(m.type, MobSound::Ambient), m.pos.x, m.pos.y, m.pos.z, 1.0f, 1.6f);
        }
        break;
    }
    case MobType::Goat: {
        // Ramming (wiki: Goat › Ramming): every 30-300 s (screaming goats 5-15 s) it picks
        // a player or another mob 4-16 blocks off, backs up for a second, then charges.
        if (m.phase == 0) {
            if (m.isBaby() || --m.chargeTicks > 0) break;
            const glm::dvec3 p = ctx.player.position();
            const double pd = glm::length(p - m.pos);
            glm::dvec3 target(0.0);
            bool have = false;
            if (ctx.survival && !ctx.playerDead && pd > 4.0 && pd < 16.0) {
                target = p;
                have = true;
            } else if (const MobData* o = nearestMob(ctx.world, m, 16.0, [](const MobData& x) {
                           return x.type != MobType::Goat && !isMount(x.type) && x.type != MobType::Boat &&
                                  x.type != MobType::Minecart && !mobInfo(x.type).swims;
                       })) {
                target = o->pos;
                have = glm::length(o->pos - m.pos) > 4.0;
            }
            m.chargeTicks = int16_t(m.powered ? 100 + ctx.rng.nextInt(200) : 600 + ctx.rng.nextInt(5400));
            if (!have) break;
            m.phase = 1;
            m.phaseTicks = 20;
            m.goal = target;
            m.beam = target; // (where it aims)
            break;
        }
        const glm::dvec3 to(m.beam.x - m.pos.x, 0.0, m.beam.z - m.pos.z);
        const double dist = glm::length(to);
        const glm::dvec3 dir = dist > 1e-6 ? to / dist : glm::dvec3(0, 0, 1);
        m.yaw = m.headYaw = float(std::atan2(-dir.x, dir.z) * 180.0 / 3.14159265358979);
        if (m.phase == 1) { // the run-up: head down, still
            m.vel.x = m.vel.z = 0.0;
            m.goal = m.pos;
            if (--m.phaseTicks <= 0) {
                m.phase = 2;
                m.phaseTicks = 30;
            }
            break;
        }
        // The charge: fast along the aimed line.
        m.goal = m.pos + dir * 2.0;
        m.vel.x = dir.x * 0.5;
        m.vel.z = dir.z * 0.5;
        bool done = --m.phaseTicks <= 0;
        const Aabb me{box(m).min - glm::dvec3(0.2), box(m).max + glm::dvec3(0.2)};
        if (ctx.survival && !ctx.playerDead && me.intersects(ctx.player.box())) {
            if (ctx.vitals.attacked(mobInfo(m.type).attackDamage, &m.pos)) {
                const glm::dvec3 push = dir * 2.5; // (thrown well back)
                ctx.player.setVelocity(ctx.player.velocity() + glm::dvec3(push.x, 0.5, push.z));
            }
            ctx.world.playSound(Sound::GoatRam, m.pos.x, m.pos.y + 0.6, m.pos.z);
            done = true;
        } else if (MobData* o = nearestMob(ctx.world, m, 1.6, [](const MobData& x) { return x.type != MobType::Goat; })) {
            o->health -= mobInfo(m.type).attackDamage;
            o->hurtTime = 10;
            o->vel += glm::dvec3(dir.x * 1.5, 0.4, dir.z * 1.5);
            ctx.world.playSound(Sound::GoatRam, m.pos.x, m.pos.y + 0.6, m.pos.z);
            done = true;
        } else if (blockedAhead && m.phaseTicks < 28) {
            // A wall: stone, logs, ores and packed ice break a horn off (wiki: Goat Horn).
            const glm::dvec3 front = m.pos + dir * (mobInfo(m.type).width * 0.5 + 0.3) + glm::dvec3(0, 0.5, 0);
            const BlockId b = blockRegistry().blockOf(
                ctx.world.getBlock({int(std::floor(front.x)), int(std::floor(front.y)), int(std::floor(front.z))}));
            const std::string& id = blockRegistry().block(b).id;
            // (wiki: stone, logs, coal/copper/iron/emerald ore and packed ice - not deepslate ores)
            const bool hard = id == "minecraft:stone" || id.ends_with("_log") || id == "minecraft:coal_ore" ||
                              id == "minecraft:copper_ore" || id == "minecraft:iron_ore" ||
                              id == "minecraft:emerald_ore" || id == "minecraft:packed_ice";
            if (hard && m.horns != 0) {
                const uint8_t which = (m.horns & 1) && ((m.horns & 2) == 0 || ctx.rng.nextInt(2)) ? 1 : 2;
                m.horns = uint8_t(m.horns & ~which);
                static const ItemId horn = itemNamed("goat_horn");
                // (vanilla: regular goats drop the first four instruments, screaming ones the others)
                ItemStack h{horn, 1};
                h.damage = uint16_t((m.powered ? 4 : 0) + ctx.rng.nextInt(4));
                ctx.items.spawn(front - dir * 0.5, h, ctx.rng);
            }
            ctx.world.playSound(Sound::GoatRam, m.pos.x, m.pos.y + 0.6, m.pos.z);
            done = true;
        }
        if (done) {
            m.phase = 0;
            m.goal = m.pos;
        }
        break;
    }
    case MobType::Armadillo: {
        // A scute every 5-10 minutes (wiki: Armadillo Scute).
        if (!m.isBaby() && --m.eggTicks <= 0) {
            static const ItemId scute = itemNamed("armadillo_scute");
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {scute, 1}, ctx.rng);
            m.eggTicks = 6000 + int(ctx.rng.nextInt(6000));
        }
        // Scared: a hit, a sprinting or riding player within 7, or undead within 7.
        bool scared = m.hurtTime > 0;
        const double pd = glm::length(ctx.player.position() - m.pos);
        if (!scared && !ctx.playerDead && pd < 7.0 && ctx.player.sprinting()) scared = true;
        if (!scared && ctx.rng.nextInt(10) == 0)
            scared = nearestMob(ctx.world, m, 7.0, [](const MobData& o) {
                         return isZombie(o.type) || o.type == MobType::Skeleton || o.type == MobType::ZombifiedPiglin;
                     }) != nullptr;
        if (scared) {
            m.sitting = true;
            m.goalTicks = 80; // (stays rolled at least 4 s after the last scare)
        } else if (m.sitting && --m.goalTicks <= 0) {
            m.sitting = false;
        }
        break;
    }
    default: break;
    }
}

void Mobs::spawnWildlife(Context& ctx, Biome biome, BlockId ground, int x, int y, int z) {
    // wiki: each mob's Spawning section (groups, biomes); variants by biome.
    MobType kind = MobType::Count;
    int group = 1;
    uint8_t variant = 0;
    const bool snowy = biome == Biome::SnowyPlains || biome == Biome::SnowyTaiga || biome == Biome::Grove ||
                       biome == Biome::SnowySlopes || biome == Biome::IceSpikes;
    switch (biome) {
    case Biome::Desert:
        if (ground != blocks::Sand) return;
        kind = MobType::Rabbit, variant = 4; // gold
        break;
    case Biome::FlowerForest:
    case Biome::Meadow:
    case Biome::CherryGrove:
        if (ground != blocks::GrassBlock) return;
        kind = MobType::Rabbit;
        break;
    case Biome::Taiga:
    case Biome::OldGrowthSpruceTaiga:
    case Biome::SnowyTaiga:
    case Biome::Grove:
        kind = ctx.rng.nextInt(3) == 0 ? MobType::Rabbit : MobType::Fox;
        if (kind == MobType::Fox) variant = snowy ? 1 : 0;
        break;
    case Biome::SnowyPlains:
    case Biome::IceSpikes:
        kind = ctx.rng.nextInt(3) == 0 ? MobType::PolarBear : MobType::Rabbit;
        break;
    case Biome::SnowySlopes:
    case Biome::JaggedPeaks:
    case Biome::FrozenPeaks:
        kind = MobType::Goat;
        break;
    case Biome::Jungle:
        if (ctx.rng.nextInt(4) != 0) return; // (rare outside bamboo jungles, which we don't have)
        kind = MobType::Panda;
        break;
    case Biome::Savanna:
    case Biome::Badlands:
    case Biome::WoodedBadlands:
        kind = MobType::Armadillo;
        break;
    case Biome::Swamp: // (M26.3c; wiki: Frog - swamps, groups of 2-5, temperate there)
        kind = MobType::Frog;
        break;
    default: return;
    }
    if (kind == MobType::Rabbit && variant == 0) { // white in the snow, else brown / black / salt and pepper
        const uint32_t r = ctx.rng.nextInt(10);
        variant = snowy ? (r < 8 ? 1 : 3) : (r < 5 ? 0 : r < 9 ? 2 : 5);
    }
    group = kind == MobType::Rabbit ? 2 + int(ctx.rng.nextInt(2))
            : kind == MobType::Fox  ? 2 + int(ctx.rng.nextInt(3))
            : kind == MobType::Goat ? 1 + int(ctx.rng.nextInt(3))
            : kind == MobType::Frog ? 2 + int(ctx.rng.nextInt(4))
            : kind == MobType::Armadillo && biome == Biome::Savanna ? 2 + int(ctx.rng.nextInt(2)) // (badlands 1-2)
                                    : 1 + int(ctx.rng.nextInt(2));
    for (int i = 0; i < group; ++i) {
        const int gx = x + int(ctx.rng.nextInt(5)) - 2, gz = z + int(ctx.rng.nextInt(5)) - 2;
        if (!solid(ctx.world, gx, y - 1, gz) || solid(ctx.world, gx, y, gz) || solid(ctx.world, gx, y + 1, gz)) continue;
        MobData m = make(kind, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
        if (kind == MobType::Rabbit || kind == MobType::Fox) m.woolColour = variant;
        if (kind == MobType::PolarBear && i > 0) m.age = -24000; // (a cub with its mother)
        if (add(ctx.world, m)) ++m_creatures;
    }
}

} // namespace mc
