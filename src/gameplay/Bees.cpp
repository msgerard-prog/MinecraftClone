// Bees (M26.3b; wiki: Bee, Beehive, Bee Nest, Pollination). Part of Mobs.
//
// A bee belongs to a hive (its `home`). By day, when it isn't raining, it leaves to find
// a flower, hovers on it to gather pollen, then flies home - growing crops it passes over
// on the way (up to 10 a trip). Back home it goes inside: a bee stays 600 ticks, or
// 2400 when it brought pollen, which becomes a level of honey as it leaves again (5 is
// full). At night and in rain bees stay in. A hit bee, and the bees around it, sting:
// 2 damage and Poison; a bee that stung loses its stinger and dies soon after.
#include "gameplay/Mobs.h"

#include "world/Beehives.h"
#include "world/Blocks.h"
#include "world/Potions.h"
#include "world/Sounds.h"
#include "world/Weather.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

bool isFlower(BlockId b) {
    return b == blocks::Dandelion || b == blocks::Poppy || b == blocks::Cornflower ||
           b == blocks::AzureBluet || b == blocks::OxeyeDaisy || b == blocks::CherryLeaves;
}
bool isHive(BlockId b) { return b == blocks::BeeNest || b == blocks::Beehive; }
float yawTo(const glm::dvec3& from, const glm::dvec3& to) {
    return static_cast<float>(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 /
                              3.14159265358979);
}
// The air block in front of a hive (where bees go in and out).
glm::dvec3 hiveDoor(const World& world, const BlockPos& h) {
    const BlockStateId s = world.getBlock(h);
    const int f = blockRegistry().get(s, properties::facing); // north, south, west, east
    static constexpr glm::ivec2 kDir[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    const glm::ivec2 d = kDir[std::clamp(f, 0, 3)];
    return {h.x + 0.5 + d.x * 0.9, h.y + 0.2, h.z + 0.5 + d.y * 0.9};
}

} // namespace

bool Mobs::beeAi(Context& ctx, MobData& m) {
    if (m.type != MobType::Bee) return false;
    animalUpkeep(ctx, m); // (growing up, love)
    const glm::dvec3 playerPos = ctx.player.position();
    const bool day = (ctx.dayTime % 24000) < 12000;
    const bool raining = ctx.weather && ctx.weather->raining;
    const BlockPos home{m.home.x, m.home.y, m.home.z};
    const bool hasHome =
        m.home.y != kNoPoint && isHive(blockRegistry().blockOf(ctx.world.getBlock(home)));
    if (!hasHome) m.home.y = kNoPoint;
    if (m.angry && --m.angerTicks <= 0) m.angry = false;
    // A bee that stung dies within a minute or so (wiki: Bee › Stinging).
    if (m.stung && --m.despawnDelay <= 0) {
        m.health = 0.0f;
        m.hurtTime = 10;
    }
    // Homeless: look for a hive with room within 20 blocks now and then.
    if (!hasHome && ctx.rng.nextInt(100) == 0) {
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                         blockToChunk(int(std::floor(m.pos.z)))};
        double best = 20.0 * 20.0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (const auto& h : ch->beehives()) {
                        const glm::dvec3 hp((c.x + dx) * 16 + h.x + 0.5, h.y + 0.5,
                                            (c.z + dz) * 16 + h.z + 0.5);
                        const double d = glm::dot(hp - m.pos, hp - m.pos);
                        if (h.data.count < 3 && d < best) {
                            best = d;
                            m.home = {(c.x + dx) * 16 + h.x, h.y, (c.z + dz) * 16 + h.z};
                        }
                    }
    }
    glm::dvec3 target = m.goal;
    double speed = 0.12; // (blocks a tick while wandering)
    if (m.angry && !m.stung && ctx.survival && !ctx.playerDead &&
        glm::length(playerPos - m.pos) < 24.0) {
        // Stinging the player (wiki: Bee): 2 damage and Poison for 10 s (Normal).
        target = playerPos + glm::dvec3(0.0, 1.0, 0.0);
        speed = 0.25;
        const Aabb reach{ctx.player.box().min - glm::dvec3(0.3),
                         ctx.player.box().max + glm::dvec3(0.3)};
        if (m.attackCooldown > 0) --m.attackCooldown;
        if (m.attackCooldown == 0 && box(m).intersects(reach)) {
            // Poison 10 s on Normal, 18 s on Hard, none on Easy (wiki: Bee).
            if (ctx.vitals.attacked(mobInfo(m.type).attackDamage, &m.pos) && ctx.difficulty >= 2)
                ctx.vitals.addEffect(Effect::Poison, 0, ctx.difficulty == 3 ? 360 : 200);
            setPlayerAttacker(m.uuidHi);
            m.stung = true;
            m.angry = false;
            m.despawnDelay = 600 + int(ctx.rng.nextInt(600));
            m.attackCooldown = 20;
        }
    } else if (hasHome && (!day || raining || m.nectar)) {
        // Home: into the hive if there's room (pollen first comes out as honey later).
        target = hiveDoor(ctx.world, home);
        speed = 0.18;
        if (glm::length(target - m.pos) < 1.0) {
            Chunk* hc = ctx.world.chunk(home.chunk());
            BeehiveData* hd =
                hc ? hc->beehive(blockToLocal(home.x), home.y, blockToLocal(home.z)) : nullptr;
            if (hd && hd->count < 3) {
                HiveBee& b = hd->bees[hd->count++];
                b = {m.uuidHi, m.uuidLo, m.health, m.age, m.nectar, 0, m.nectar ? 2400 : 600};
                hc->markDirty();
                m.vanish = true; // (gone into the hive: no death)
                m.health = 0.0f;
                m.deathTime = 19;
                return true;
            }
            if (hd) m.home.y = kNoPoint; // (full: find another)
        }
        // Passing over crops with pollen grows them (wiki: Bee › Pollination).
        if (m.nectar && m.volley < 10 && ctx.rng.nextInt(30) == 0) {
            for (int dy = 1; dy <= 2; ++dy) {
                const BlockPos below{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)) - dy,
                                     int(std::floor(m.pos.z))};
                const BlockStateId s = ctx.world.getBlock(below);
                const BlockId b = blockRegistry().blockOf(s);
                const Property* age =
                    b == blocks::Wheat || b == blocks::Carrots || b == blocks::Potatoes
                        ? &properties::age7
                    : b == blocks::Beetroots || b == blocks::SweetBerryBush ? &properties::age3
                                                                            : nullptr;
                if (!age) continue;
                const int a = blockRegistry().get(s, *age), max = age == &properties::age7 ? 7 : 3;
                if (a < max) {
                    ctx.world.updateBlock(below, blockRegistry().set(s, *age, a + 1));
                    if (ctx.edits) ctx.edits->push_back(below);
                    ++m.volley;
                }
                break;
            }
        }
    } else if (!m.nectar && day && !raining) {
        // Pollen: to a flower within 22 blocks of home (looked for now and then), hovering
        // on it for 2-4 s.
        if (m.workTarget.y == kNoPoint && ctx.rng.nextInt(20) == 0) {
            const glm::ivec3 c = hasHome ? m.home : glm::ivec3(glm::floor(m.pos));
            for (int k = 0; k < 24; ++k) {
                const BlockPos p{c.x + int(ctx.rng.nextInt(23)) - 11,
                                 c.y + int(ctx.rng.nextInt(7)) - 3,
                                 c.z + int(ctx.rng.nextInt(23)) - 11};
                if (isFlower(blockRegistry().blockOf(ctx.world.getBlock(p)))) {
                    m.workTarget = {p.x, p.y, p.z};
                    m.spellTicks = int16_t(40 + ctx.rng.nextInt(40));
                    break;
                }
            }
        }
        if (m.workTarget.y != kNoPoint) {
            const BlockPos f{m.workTarget.x, m.workTarget.y, m.workTarget.z};
            if (!isFlower(blockRegistry().blockOf(ctx.world.getBlock(f)))) {
                m.workTarget.y = kNoPoint;
            } else {
                target = {f.x + 0.5, f.y + 0.6, f.z + 0.5};
                if (glm::length(target - m.pos) < 0.7 && --m.spellTicks <= 0) {
                    m.nectar = true;
                    m.volley = 0;
                    m.workTarget.y = kNoPoint;
                }
            }
        }
    }
    // Otherwise drift about (near home), a new spot every few seconds.
    if (target == m.goal && (++m.goalTicks > 80 || glm::length(m.goal - m.pos) < 0.8)) {
        m.goalTicks = 0;
        const glm::dvec3 around =
            hasHome ? glm::dvec3(home.x + 0.5, home.y + 0.5, home.z + 0.5) : m.pos;
        m.goal = around + glm::dvec3(ctx.rng.nextDouble() * 12 - 6, ctx.rng.nextDouble() * 4 - 1,
                                     ctx.rng.nextDouble() * 12 - 6);
        target = m.goal;
    }
    const glm::dvec3 d = target - m.pos;
    const double dl = glm::length(d);
    glm::dvec3 wish(0.0);
    if (dl > 0.2) {
        wish = d / dl * std::min(speed, dl);
        m.yaw = m.headYaw = yawTo(m.pos, target);
    }
    // Bees hover: the wing beat carries them up a little whatever they do.
    if (!m.onGround && wish.y < 0.0) wish.y *= 0.5;
    m.limbSwing += 1.5f; // (the wings beat all the time)
    physics(ctx.world, m, wish, false);
    return true;
}

void Mobs::tickHives(Context& ctx, Chunk& chunk) {
    // Bees inside wait out their time, then leave by day when it isn't raining; one that
    // brought pollen adds a level of honey (wiki: Beehive).
    const bool day = (ctx.dayTime % 24000) < 12000;
    const bool raining = ctx.weather && ctx.weather->raining;
    for (auto& h : chunk.beehives()) {
        BeehiveData& d = h.data;
        for (int i = 0; i < d.count;) {
            HiveBee& b = d.bees[size_t(i)];
            ++b.ticksInHive;
            if (b.ticksInHive < b.minTicks || !day || raining) {
                ++i;
                continue;
            }
            const BlockPos hp{chunk.pos().x * 16 + h.x, h.y, chunk.pos().z * 16 + h.z};
            const glm::dvec3 door = hiveDoor(ctx.world, hp);
            if (blockRegistry().collides(ctx.world.getBlock(
                    {int(std::floor(door.x)), int(std::floor(door.y)), int(std::floor(door.z))}))) {
                ++i; // (the way out is blocked)
                continue;
            }
            MobData bee = beeFromHive(b, door, hp);
            bee.nectar = false;
            if (b.nectar) { // honey
                const BlockStateId s = chunk.get(h.x, h.y, h.z);
                const int level = blockRegistry().get(s, properties::honeyLevel);
                if (level < 5)
                    ctx.world.updateBlock(
                        hp, blockRegistry().set(s, properties::honeyLevel, level + 1));
            }
            chunk.mobs().push_back(bee);
            d.bees[size_t(i)] = d.bees[size_t(d.count - 1)];
            --d.count;
            chunk.markDirty();
        }
    }
}

} // namespace mc
