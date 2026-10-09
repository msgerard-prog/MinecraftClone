#include "gameplay/Mobs.h"

#include "world/Rotation.h"
#include "world/Trades.h"

#include "gameplay/BlockCollision.h"
#include "gameplay/ExperienceOrbs.h"
#include "gameplay/FluidContact.h"
#include "gameplay/Projectiles.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Coords.h"
#include "world/Items.h"
#include "world/Raycast.h"
#include "world/RegionalDifficulty.h"
#include "world/Weather.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace mc {

using namespace world;

namespace {

constexpr double kGravity = 0.08, kDrag = 0.98, kStep = 0.6;
constexpr double kGroundFriction = 0.6 * 0.91, kAirFriction = 0.91;

float yawTowards(const glm::dvec3& from, const glm::dvec3& to) {
    // Vanilla yaw: 0 = +Z (south), 90 = -X (west).
    return static_cast<float>(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 /
                              std::numbers::pi);
}

float approachAngle(float from, float to, float maxStep) {
    float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    d = std::clamp(d, -maxStep, maxStep);
    return from + d;
}

// UUIDs must differ between sessions (vanilla: random version-4 UUIDs), so they
// don't come from the seeded game RNG.
Xoroshiro& uuidRng() {
    static Xoroshiro rng = [] {
        std::random_device rd;
        return Xoroshiro((uint64_t(rd()) << 32) ^ rd());
    }();
    return rng;
}

bool solidAt(const World& w, int x, int y, int z) {
    return blockRegistry().collides(w.getBlock({x, y, z}));
}

// A spot a mob can spawn in (wiki: Mob spawning): a spawnable block below (not
// glass, leaves, bedrock or ice), and two cells without collision or fluid.
bool canSpawnAt(const World& w, int x, int y, int z) {
    const auto& reg = blockRegistry();
    const BlockId below = reg.blockOf(w.getBlock({x, y - 1, z}));
    if (!reg.collides(w.getBlock({x, y - 1, z})) || below == blocks::Bedrock ||
        below == blocks::Glass || below == blocks::Ice || below == blocks::PackedIce ||
        reg.block(below).id.ends_with("_leaves"))
        return false;
    for (int dy = 0; dy < 2; ++dy) {
        const BlockStateId s = w.getBlock({x, y + dy, z});
        const BlockId b = reg.blockOf(s);
        if (reg.collides(s) || b == blocks::Water || b == blocks::Lava) return false;
    }
    return true;
}

} // namespace

Aabb Mobs::box(const MobData& m) {
    if (isHanging(m.type)) return hangingBox(m); // (M28.3a: flat on their wall)
    if (m.type == MobType::LeashKnot)            // (M28.3c: around its fence post's middle)
        return {m.pos - glm::dvec3(0.1875, 0.0, 0.1875), m.pos + glm::dvec3(0.1875, 0.5, 0.1875)};
    const MobInfo& info = mobInfo(m.type);
    double s = m.isBaby() ? (m.type == MobType::HappyGhast ? 0.2375 : 0.5)
                          : 1.0; // babies are half size (ghastlings 0.95)
    if (m.type == MobType::MagmaCube || m.type == MobType::Slime)
        s = m.size / 4.0; // (info is the large one)
    // (a sitting camel is 0.945 tall - wiki: Camel)
    const double h = isCamel(m.type) && m.sitting ? 0.945 : info.height;
    return Aabb::fromFeet(m.pos, info.width * s, h * s);
}

uint8_t Mobs::naturalWoolColour(Xoroshiro& rng) {
    // wiki: Sheep › Spawning - white 81.836%, black, gray and light gray 5% each,
    // brown 3%, pink 0.164%.
    const double r = rng.nextDouble() * 100.0;
    if (r < 5.0) return 15;   // black
    if (r < 10.0) return 7;   // gray
    if (r < 15.0) return 8;   // light gray
    if (r < 18.0) return 12;  // brown
    if (r < 18.164) return 6; // pink
    return 0;                 // white
}

MobData Mobs::make(MobType type, const glm::dvec3& pos, Xoroshiro& rng) {
    MobData m;
    m.type = type;
    m.uuidHi = (uuidRng().nextLong() & ~0xF000ull) | 0x4000ull;       // version 4
    m.uuidLo = (uuidRng().nextLong() & ~(3ull << 62)) | (2ull << 62); // variant 2
    m.pos = m.prevPos = m.goal = pos;
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = rng.nextFloat() * 360.0f - 180.0f;
    m.health = mobInfo(type).maxHealth;
    if (type == MobType::Sheep) m.woolColour = naturalWoolColour(rng);
    if (type == MobType::Chicken)
        m.eggTicks = 6000 + static_cast<int>(rng.nextInt(6000)); // wiki: 5-10 min
    if (type == MobType::IronGolem) m.persistent = true;
    // (M29.1c) brutes carry golden axes, illusioners bows; endermites last 2 minutes;
    // mooshrooms are red (1 in 1000 brown when born... ours: brown only from lightning).
    if (type == MobType::PiglinBrute) m.heldItem = uint16_t(*itemRegistry().find("golden_axe"));
    if (type == MobType::Illusioner) m.heldItem = uint16_t(*itemRegistry().find("bow"));
    if (type == MobType::Endermite) m.despawnDelay = 2400;
    if (type ==
        MobType::TropicalFish) { // (wiki: Tropical Fish - 2 shapes x 6 patterns x 16 x 16 colours)
        m.size = uint8_t(rng.nextInt(12));
        m.woolColour = uint8_t(rng.nextInt(16));
        m.color2 = uint8_t(rng.nextInt(16));
        if (m.color2 == m.woolColour) m.color2 = uint8_t((m.color2 + 7) & 15);
    }
    if (type == MobType::Pufferfish) m.size = 0;               // (deflated)
    if (isPet(type) || type == MobType::Ocelot) m.color2 = 14; // (red collars - wiki)
    if (type == MobType::Cat)
        m.woolColour = uint8_t(rng.nextInt(10)); // (all black only from swamp huts)
    if (type == MobType::Parrot) m.woolColour = uint8_t(rng.nextInt(5));
    if (isMount(type)) initMount(m, rng);   // (M26.2: its own health, speed and jump; coat)
    initWildlife(m, rng);                   // (M26.3: panda genes, screaming goats, scute timers)
    if (type == MobType::WanderingTrader) { // (M24.4) its wares; it leaves after 40 minutes (wiki)
        wanderingTraderTrades(m, rng);
        m.despawnDelay = 48000;
        m.persistent = true;
        m.home = {int(std::floor(pos.x)), int(std::floor(pos.y)), int(std::floor(pos.z))};
    }
    if (isTechnical(type) || type == MobType::Giant || type == MobType::Mannequin)
        m.persistent = true; // (M29.7e: command entities stay)
    if (type == MobType::Villager) {
        m.persistent = true;                    // (villagers never despawn)
        m.poiSearch = int16_t(rng.nextInt(40)); // (look around soon after appearing)
    }
    if (type == MobType::MagmaCube ||
        type == MobType::Slime) { // wiki: sizes 1, 2, 4 at spawn; health size^2
        const uint32_t r = rng.nextInt(3);
        m.size = uint8_t(1u << r);
        m.health = float(m.size * m.size);
    }
    return m;
}

bool Mobs::strikeLightning(World& world, const glm::dvec3& at) {
    const Aabb zone{at - glm::dvec3(3.0, 3.0, 3.0), at + glm::dvec3(3.0, 9.0, 3.0)};
    bool hit = false;
    const ChunkPos c0 = BlockPos{int(std::floor(at.x)), 0, int(std::floor(at.z))}.chunk();
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk* c = world.chunk({c0.x + dx, c0.z + dz});
            if (!c) continue;
            for (MobData& m : c->mobs()) {
                if (m.health <= 0.0f || !box(m).intersects(zone) ||
                    m.type == MobType::EnderDragon || m.type == MobType::EndCrystal ||
                    m.type == MobType::Minecart)
                    continue;
                hit = true;
                if (m.type == MobType::Creeper) m.powered = true;
                if (m.type ==
                    MobType::Villager) { // (wiki: a villager struck by lightning becomes a witch)
                    m.type = MobType::Witch;
                    m.health = mobInfo(m.type).maxHealth;
                    m.age = 0;
                    m.sleeping = false; // (out of its bed, not trading any more)
                    m.tradingTicks = 0;
                    m.persistent = true;
                    continue;
                }
                if (m.type == MobType::Mooshroom) { // (M29.1c; wiki) red <-> brown
                    m.woolColour = m.woolColour == 1 ? 0 : 1;
                    continue;
                }
                if (m.type == MobType::Pig) { // (vanilla: a new entity in its place)
                    m.type = MobType::ZombifiedPiglin;
                    m.health = mobInfo(m.type).maxHealth;
                    m.age = 0;
                    m.persistent = true;
                    continue;
                }
                if (!mobInfo(m.type).fireImmune) {
                    m.health -= 5.0f;
                    m.hurtTime = 10;
                    m.fireTicks = std::max<decltype(m.fireTicks)>(m.fireTicks, 160);
                }
                c->markDirty();
            }
        }
    return hit;
}

bool Mobs::add(World& world, const MobData& mob) {
    const BlockPos at{int(std::floor(mob.pos.x)), int(std::floor(mob.pos.y)),
                      int(std::floor(mob.pos.z))};
    Chunk* c = world.chunk(at.chunk());
    if (!c) return false;
    c->mobs().push_back(mob);
    c->markDirty();
    world.markTicking(c->pos());
    return true;
}

// Opens or shuts a wooden door from its lower half, both halves together (M30.5).
static void setMobDoor(World& world, const BlockPos& lower, bool open) {
    const auto& reg = blockRegistry();
    const BlockStateId ls = world.getBlock(lower);
    if (reg.likeOf(reg.blockOf(ls)) != blocks::OakDoor || reg.get(ls, properties::doorHalf) != 1)
        return;
    const BlockStateId now = reg.set(ls, properties::open, open ? 0 : 1);
    if (now == ls) return;
    world.updateBlock(lower, now);
    const BlockPos up{lower.x, lower.y + 1, lower.z};
    const BlockStateId us = world.getBlock(up);
    if (reg.likeOf(reg.blockOf(us)) == blocks::OakDoor)
        world.updateBlock(up, reg.set(us, properties::open, open ? 0 : 1));
    world.playSound(open ? Sound::DoorOpen : Sound::DoorClose, lower.x + 0.5, lower.y + 0.5,
                    lower.z + 0.5);
}

void Mobs::physics(const World& world, MobData& m, const glm::dvec3& wish, bool jump) {
    const FluidContact fluid = fluidContact(world, box(m));
    const bool inWater = fluid.water;
    m.vel += fluid.flow * 0.014;     // carried by currents (vanilla pushes mobs too)
    applyBubbleColumn(fluid, m.vel); // (M29.5)
    const bool fireproof = mobInfo(m.type).fireImmune; // Nether mobs (wiki)
    if (fireproof) m.fireTicks = 0;
    if (fluid.fire && !fireproof) { // wiki: Fire - 1 a tick (hurt cooldown), 8 s alight
        if (m.hurtTime == 0 && m.deathTime == 0) {
            m.health -= fluid.soulFire ? 2.0f : 1.0f; // (M29.4c: soul fire 2)
            m.hurtTime = 10;
        }
        if (m.fireTicks < 160) m.fireTicks = 160;
    }
    if (inWater) m.fireTicks = 0;   // water puts out burning mobs (wiki: Fire)
    if (fluid.lava && !fireproof) { // wiki: Lava - 4 damage (with the hurt cooldown), on fire 15 s
        if (m.hurtTime == 0 && m.deathTime == 0) {
            m.health -= 4.0f;
            m.hurtTime = 10;
        }
        m.fireTicks = 300;
        m.vel *= 0.5;
    }
    // Suffocation (M30.3; wiki: Suffocation): a head inside an opaque block hurts 1, paced
    // by the hurt cooldown (living mobs only; flat things and vehicles don't breathe).
    if (m.hurtTime == 0 && m.deathTime == 0 && !isHanging(m.type) && !isTechnical(m.type) &&
        m.type != MobType::Boat && m.type != MobType::Minecart && m.type != MobType::EndCrystal &&
        m.type != MobType::LeashKnot && m.type != MobType::ArmorStand &&
        m.type != MobType::Shulker &&
        [&] { // (its own size: babies, small slimes, sitting camels - M30 review)
            const Aabb bb = box(m);
            return headInWall(world, {m.pos.x, bb.min.y + (bb.max.y - bb.min.y) * 0.85, m.pos.z},
                              bb.max.x - bb.min.x);
        }()) {
        m.health -= 1.0f;
        m.hurtTime = 10;
    }
    const bool vehicle =
        m.type == MobType::Boat; // (its velocity is set by boatTick: collision only)
    // Walking: horizontal speed approaches `wish` (blocks/tick) with ground friction.
    const double friction = vehicle ? 1.0 : m.onGround ? kGroundFriction : kAirFriction;
    const double accel =
        m.onGround ? (1.0 - kGroundFriction) : 0.02 / 0.1 * (1.0 - kGroundFriction) * 0.25;
    if (!vehicle) {
        m.vel.x = m.vel.x * friction + wish.x * accel / (1.0 - friction + 1e-9) * (1.0 - friction);
        m.vel.z = m.vel.z * friction + wish.z * accel / (1.0 - friction + 1e-9) * (1.0 - friction);
    }
    if (isSpider(m.type) && m.climbing) {
        m.vel.y = 0.2; // spiders climb walls (wiki: Spider)
        m.fallDistance = 0.0f;
    }
    if (vehicle) {
        // (set by boatTick)
    } else if (m.type == MobType::Strider && fluid.lava) {
        // Striders stand on lava (wiki): it holds them up like ground.
        m.vel.y = std::max(m.vel.y, 0.0) * 0.5 + 0.04;
        m.onGround = true;
    } else if ((m.type == MobType::Turtle || m.type == MobType::Frog) &&
               inWater) { // (M26.3c: frogs swim too)
        // Turtles swim (wiki: Turtle): toward the goal, its height too.
        m.vel = m.vel * 0.9 + wish * 0.15;
        m.vel.y += std::clamp((m.goal.y - m.pos.y) * 0.02, -0.02, 0.02);
        m.vel.y *= 0.9;
    } else if (m.type == MobType::Drowned && inWater) {
        // Drowned swim after their target (wiki: Drowned): like fish, rising or diving
        // toward the goal's height.
        m.vel = m.vel * 0.9 + wish * 0.1;
        m.vel.y += std::clamp((m.goal.y - m.pos.y) * 0.02, -0.03, 0.03);
        m.vel.y *= 0.9;
    } else if (mobInfo(m.type).swims && inWater) {
        // Fish and squid swim (M25.2): like fliers, velocity eases toward the 3D wish.
        m.vel = m.vel * 0.9 + wish * 0.1;
    } else if (mobInfo(m.type).flies) {
        // Ghasts and blazes fly: velocity eases toward the wish (with its height), no
        // gravity (our motion model).
        m.vel = m.vel * 0.9 + wish * 0.1;
    } else if (inWater) {
        // Cows swim up to the surface; zombies sink (wiki: Zombie - they sink and later
        // become drowned).
        m.vel.y = m.vel.y * 0.8 + (isZombie(m.type) ? -0.02 : 0.04);
        m.vel.x *= 0.8;
        m.vel.z *= 0.8;
    } else {
        if (jump && m.onGround) m.vel.y = 0.42;
        m.vel.y = (m.vel.y - kGravity) * kDrag;
        // Chickens flap and fall slowly (wiki: Chicken - no fall damage).
        if (m.type == MobType::Chicken && !m.onGround && m.vel.y < 0.0) m.vel.y *= 0.6;
    }

    // Collision, axis by axis (y first), with step-up onto 0.6-high ledges.
    const Aabb start = box(m);
    auto gather = [&](const Aabb& region) { gatherBlockBoxes(world, region, m_boxes); };
    auto slide = [&](Aabb b, glm::dvec3 d) {
        for (int axis : {1, 0, 2}) {
            for (const Aabb& w : m_boxes)
                d[axis] = b.clip(w, axis, d[axis]);
            glm::dvec3 step(0.0);
            step[axis] = d[axis];
            b = b.moved(step);
        }
        return d;
    };
    // Mounts step up whole blocks (wiki: Horse - step height 1; Camel 1.5).
    const double step = isCamel(m.type) ? 1.5 : isMount(m.type) ? 1.0 : kStep;
    gather(start.expandedTowards(m.vel).expandedTowards({0, step, 0}));
    glm::dvec3 moved = slide(start, m.vel);
    const bool blockedH = moved.x != m.vel.x || moved.z != m.vel.z;
    if (blockedH && m.onGround) { // try stepping up (slabs, snow)
        const glm::dvec3 up = slide(start, {0, step, 0});
        const Aabb raised = start.moved(up);
        glm::dvec3 across = slide(raised, {m.vel.x, 0, m.vel.z});
        const Aabb over = raised.moved(across);
        const glm::dvec3 down = slide(over, {0, -step, 0});
        const glm::dvec3 stepped = up + across + down;
        if (stepped.x * stepped.x + stepped.z * stepped.z >
            moved.x * moved.x + moved.z * moved.z + 1e-9)
            moved = stepped;
    }
    m.onGround = m.vel.y < 0.0 && moved.y != m.vel.y;
    // Falls (wiki: Fall damage): 1 per block beyond 3 on landing; water breaks them.
    if (inWater) {
        m.fallDistance = 0.0f;
    } else if (moved.y < 0.0) {
        if (m.type != MobType::Chicken && m.type != MobType::MagmaCube &&
            m.type != MobType::Slime && m.type != MobType::SulfurCube && m.type != MobType::Breeze &&
            !mobInfo(m.type).flies &&
            !vehicle) // (breezes: no fall damage - M26.4c)
            m.fallDistance -=
                static_cast<float>(moved.y); // (chickens, magma cubes, fliers: no fall damage)
    }
    if (m.onGround) {
        // (mounts: half the damage and 3 more safe blocks - wiki: Horse, Camel)
        const float damage = isMount(m.type) ? std::ceil(m.fallDistance * 0.5f - 3.0f)
                                             : std::ceil(m.fallDistance - 3.0f);
        if (damage > 0.0f) {
            m.health -= damage;
            m.hurtTime = 10;
        }
        m.fallDistance = 0.0f;
    }
    m.climbing = moved.x != m.vel.x || moved.z != m.vel.z; // against a wall
    if (moved.x != m.vel.x) m.vel.x = 0.0;
    if (moved.z != m.vel.z) m.vel.z = 0.0;
    if (moved.y != m.vel.y) m.vel.y = 0.0;
    m.pos += moved;

    // Walk animation (vanilla limb swing: distance moved, smoothed).
    const double dist = std::sqrt(moved.x * moved.x + moved.z * moved.z);
    m.limbSwingAmount += (std::min(1.0f, float(dist) * 4.0f) - m.limbSwingAmount) * 0.4f;
    m.limbSwing += m.limbSwingAmount;
}

void Mobs::ai(Context& ctx, MobData& m) {
    // Outside the Nether piglins, brutes and hoglins shake for 15 s and turn into
    // zombified piglins and zoglins (M29.1c; wiki: Piglin, Hoglin › Zombification).
    if (m.type == MobType::Piglin || m.type == MobType::PiglinBrute || m.type == MobType::Hoglin) {
        if (ctx.world.isUltrawarm()) {
            m.zombifyTicks = 0;
        } else if (++m.zombifyTicks > 300) {
            m.type = m.type == MobType::Hoglin ? MobType::Zoglin : MobType::ZombifiedPiglin;
            m.health = mobInfo(m.type).maxHealth;
            m.zombifyTicks = 0;
            m.admireTicks = 0;
        }
    }
    // An endermite crumbles away after its 2 minutes unless it was made to stay (wiki).
    if (m.type == MobType::Endermite && !m.persistent && m.despawnDelay > 0 &&
        --m.despawnDelay == 0) {
        m.health = 0.0f;
        m.deathTime = 19; // (gone, no drops)
        m.lastHurtByPlayer = false;
        return;
    }
    if (isHanging(m.type)) { // (M28.3a, Hanging.cpp)
        hangingTick(ctx, m);
        return;
    }
    if (m.type == MobType::Giant ||
        m.type == MobType::Mannequin) { // (M29.7e) no AI: they only fall
        physics(ctx.world, m, glm::dvec3(0.0), false);
        return;
    }
    if (isTechnical(m.type)) { // (M29.7e) where they were put
        m.vel = glm::dvec3(0.0);
        // The ominous item spawner (wiki: Ominous Item Spawner): its item comes out after a
        // while - an arrow shot down, anything else dropped.
        if (m.type == MobType::OminousItemSpawner && --m.eggTicks <= 0) {
            const ItemId item = ItemId(m.commandId);
            if (item == *itemRegistry().find("arrow") && ctx.projectiles)
                ctx.projectiles->shoot(ProjectileKind::Arrow, m.pos, {0.0, -1.0, 0.0}, 1.0, 0.0,
                                       false, false, ctx.rng);
            else if (item != 0)
                ctx.items.spawn(m.pos, {item, 1}, ctx.rng);
            m.vanish = true;
            m.health = 0.0f;
        }
        return;
    }
    if (m.type == MobType::ArmorStand) { // (M28.3b, ArmorStands.cpp)
        armorStandTick(ctx, m);
        return;
    }
    if (m.type == MobType::LeashKnot) { // (M28.3c, Leads.cpp)
        knotTick(ctx, m);
        return;
    }
    if (m.type == MobType::Minecart) {
        minecartTick(ctx, m);
        return;
    }
    if (m.type == MobType::Boat) { // (M25.2b, Boats.cpp)
        boatTick(ctx, m);
        return;
    }
    if (isMount(m.type) && mountTick(ctx, m)) return; // (ridden: Mounts.cpp, M26.2)
    if (isStickRidden(m.type) && m.ridden) {
        // (M29.3d; wiki: Carrot on a Stick, Warped Fungus on a Stick) ridden, it walks where
        // the rider looks while they hold its stick (paddleForward set by main); a boost
        // (spellTicks) speeds it up for a while.
        m.yaw = approachAngle(m.yaw, m.headYaw, 20.0f);
        const double boost =
            m.spellTicks > 0 ? 1.0 + 1.15 * std::sin(double(m.spellTicks) * 0.0224) : 1.0;
        if (m.spellTicks > 0) --m.spellTicks;
        const glm::dvec3 f(forwardFlat(m.yaw));
        const double pace = mobInfo(m.type).speed * (m.type == MobType::Pig ? 0.9 : 0.55) * boost;
        physics(ctx.world, m, m.paddleForward > 0 ? f * pace : glm::dvec3(0.0), false);
        m.paddleForward = 0;
        return;
    }
    if (m.type == MobType::EnderDragon) {
        dragonAi(ctx, m);
        if (m.phaseTicks > 30000) m.phaseTicks = 30000;
        return;
    }
    if (m.type == MobType::Shulker) {
        // Stays put on its block (wiki: Shulker): while the player is within 16 blocks
        // (and in survival) it opens and shoots a bullet every 1-5.5 s; otherwise it
        // peeks now and then and closes again.
        m.vel = glm::dvec3(0.0);
        const glm::dvec3 to = ctx.player.position() - m.pos;
        const bool target = ctx.survival && !ctx.playerDead && glm::dot(to, to) < 16.0 * 16.0;
        int want = target ? 100 : (m.goalTicks > 0 ? 30 : 0);
        if (!target && ctx.rng.nextInt(400) == 0)
            m.goalTicks = 20 + static_cast<int>(ctx.rng.nextInt(41)); // a 1-3 s peek
        if (m.goalTicks > 0) --m.goalTicks;
        if (m.peek < want)
            m.peek = static_cast<uint8_t>(std::min(want, m.peek + 5));
        else if (m.peek > want)
            m.peek = static_cast<uint8_t>(std::max(want, m.peek - 5));
        if (target && ctx.projectiles && --m.chargeTicks <= 0) {
            const glm::dvec3 from = m.pos + glm::dvec3(0.0, 0.5, 0.0);
            const glm::dvec3 dir =
                glm::length(to) > 1e-6 ? glm::normalize(to) : glm::dvec3(0, 1, 0);
            ctx.projectiles->shoot(ProjectileKind::ShulkerBullet, from + dir * 0.8, dir, 0.15, 0.0,
                                   false, false, ctx.rng, m.uuidHi);
            m.chargeTicks = static_cast<int16_t>(20 + ctx.rng.nextInt(90)); // (wiki: 1-5.5 s)
        }
        m.yaw = 0.0f; // (the shell never turns; vanilla's head inside does)
        m.headYaw = float(std::atan2(-to.x, to.z) * 180.0 / 3.14159265358979);
        return;
    }
    if (m.type == MobType::EndCrystal) {
        // Doesn't move; its glass cubes turn (the yaw only animates the model).
        m.vel = glm::dvec3(0.0);
        m.headYaw += 3.0f;
        m.pitch = 35.0f;
        if (m.headYaw >= 360.0f) {
            m.headYaw -= 360.0f;
            m.prevHeadYaw -= 360.0f;
        }
        return;
    }
    if (m.type == MobType::SulfurCube && sulfurCubeAi(ctx, m)) return; // (M33.2c)
    if (netherAi(ctx, m)) return;     // ghasts, blazes, magma cubes (NetherMobs.cpp)
    if (waterAi(ctx, m)) return;      // fish and squid (WaterMobs.cpp)
    if (beeAi(ctx, m)) return;        // (M26.3b, Bees.cpp)
    if (phantomAi(ctx, m)) return;    // (M26.4a, Phantoms.cpp)
    if (witherAi(ctx, m)) return;     // (M26.4b, Wither.cpp)
    if (allayAi(ctx, m)) return;      // (M26.5a, Allays.cpp)
    if (batAi(ctx, m)) return;        // (M29.1c, Bats.cpp)
    if (happyGhastAi(ctx, m)) return; // (M26.5b, HappyGhasts.cpp)
    if (creakingTick(ctx, m)) return; // (M27.1c, Creakings.cpp: frozen or crumbling)
    if (wardenTick(ctx, m)) return;   // (M27.3c, Wardens.cpp: emerging, digging, booming)
    if (snifferTick(ctx, m)) return;  // (M27.5c, Sniffers.cpp: digging)
    const MobInfo& info = mobInfo(m.type);
    if (m.type == MobType::SnowGolem) snowGolemTick(ctx, m); // (M29.1c; then it strolls)
    if (!info.hostile) animalUpkeep(ctx, m);
    if (m.type == MobType::ZombieVillager) {
        zombieVillagerTick(m);
        if (m.type == MobType::Villager) return; // (cured this tick)
    }
    if (m.type == MobType::Villager) {
        villagersCallGolem(ctx, m);
        villagerUpkeep(ctx, m);
        villagerFear(ctx, m);
        double unused = 0.0;
        if (m.sleeping && villagerGoal(ctx, m, unused) &&
            m.sleeping) { // asleep: nothing else turns or moves it
            m.vel = glm::dvec3(0.0);
            m.prevPos = m.pos; // (no physics this tick: nothing else updates it)
            m.headYaw = m.prevHeadYaw = m.yaw;
            m.prevYaw = m.yaw;
            m.pitch = 0.0f;
            return;
        }
    }
    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 toPlayer = playerPos - m.pos;
    const double playerDist2 =
        toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
    double speed = info.speed * 0.5; // blocks per tick at speed modifier 1 (our estimate)
    bool chase = false;
    // (M29.1b) a mount carrying a monster goes where its rider wants (set by ridePass).
    const bool jockey = m.jockeyChase;
    m.jockeyChase = false;
    // A skeleton trap springs when a player comes within 10 blocks: a bolt that doesn't
    // burn or hurt, then four skeleton horsemen - tamed horses, skeletons with bows.
    if (m.skeletonTrap && !ctx.playerDead && playerDist2 < 10.0 * 10.0) {
        m.skeletonTrap = false;
        if (m_trapBolts.size() < m_trapBolts.capacity()) m_trapBolts.push_back(m.pos);
        static const uint16_t bow = uint16_t(*itemRegistry().find("bow"));
        for (int k = 0; k < 4; ++k) {
            MobData h = k == 0 ? m : make(MobType::SkeletonHorse, m.pos, ctx.rng);
            if (k > 0) {
                h.pos += glm::dvec3(ctx.rng.nextDouble() * 2.0 - 1.0, 0.0,
                                    ctx.rng.nextDouble() * 2.0 - 1.0);
            }
            h.tamed = true;
            MobData rider = make(MobType::Skeleton, h.pos, ctx.rng);
            rider.vehicle = h.uuidHi;
            rider.heldItem = bow;
            rider.persistent = true;
            if (k == 0) {
                m.tamed = true;
            } else {
                ctx.world.queueMob(h);
            }
            ctx.world.queueMob(rider);
        }
    }
    if (m.mobRidden)
        speed = (m.moveSpeed > 0.0f ? m.moveSpeed : info.speed) * 0.5; // (its own pace)
    if (m.isBaby() && isZombie(m.type))
        speed *= 1.5; // (M32.2; wiki: Zombie - babies are 50% faster)
    // (M29.2c; wiki: Speed, Slowness) +20% / -15% a level, as the player's
    speed *= std::max(0.0, (1.0 + 0.2 * m.effectLevel(uint8_t(Effect::Speed))) *
                               (1.0 - 0.15 * m.effectLevel(uint8_t(Effect::Slowness))));
    m.mobRidden = false;

    // Follow range (wiki): zombies notice the player within 35 blocks, skeletons,
    // creepers and spiders within 16; endermen only when angered (64).
    const double follow = isZombie(m.type) || m.type == MobType::Vex ? 35.0
                          : m.type == MobType::Enderman              ? 64.0
                          : m.type == MobType::Pillager ? 32.0 // (wiki: Pillager - follow range 32)
                                                        : 16.0;
    // An angered iron golem goes for the player like a monster (wiki: Iron Golem), until
    // its anger runs out (counted here, also while it chases).
    if ((m.type == MobType::IronGolem || m.type == MobType::Wolf || m.type == MobType::PolarBear ||
         m.type == MobType::Panda) &&
        m.angry && --m.angerTicks <= 0)
        m.angry = false;
    // (M26.1) and a wild wolf that was hit, until it calms down; (M26.3) a polar bear or an
    // aggressive panda that was provoked.
    const bool hostileNow = info.hostile || (m.type == MobType::IronGolem && m.angry) ||
                            (m.type == MobType::Wolf && m.angry && !m.tamed) ||
                            ((m.type == MobType::PolarBear || m.type == MobType::Panda) && m.angry);
    if (!hostileNow || !ctx.survival || ctx.playerDead || playerDist2 >= follow * follow ||
        !mayTarget(ctx, m)) {
        m.targeting = false;
    } else if (!m.targeting && ++m.sightCheck >= 10) {
        // Targets are picked on sight (wiki: Zombie): a line of sight check twice a second.
        m.sightCheck = 0;
        const glm::dvec3 eye = m.pos + glm::dvec3(0.0, info.height * 0.85, 0.0);
        const glm::dvec3 target = playerPos + glm::dvec3(0.0, 1.62, 0.0);
        const double dist = glm::length(target - eye);
        const auto block = raycastBlocks(ctx.world, eye, (target - eye) / dist, dist);
        m.targeting = !block;
        m.unseenTicks = 0;
    } else if (m.targeting && ++m.sightCheck >= 10 && m.type != MobType::Vex &&
               m.type != MobType::Enderman && m.type != MobType::Warden) {
        // (M32.6; observed: about 3 s) a target out of sight for 3 s is
        // forgotten (checked twice a second); it has to be seen again.
        m.sightCheck = 0;
        if (seesPlayer(ctx.world, m, ctx.player)) {
            m.unseenTicks = 0;
        } else if ((m.unseenTicks = uint8_t(std::min(255, m.unseenTicks + 10))) >= 60) {
            m.targeting = false;
            m.unseenTicks = 0;
        }
    }
    if (jockey) {
        chase = true; // (the goal was set by its rider)
    } else if (m.targeting) {
        chase = true; // wiki: Zombie - follow range 35
        m.goal = playerPos;
        // Skeletons (M32.2; wiki: Skeleton › Behavior): walk up until within 15 blocks having
        // seen the player for a second, then stand and strafe (below).
        if (isSkeleton(m.type) || m.type == MobType::Illusioner) {
            const bool seen = seesPlayer(ctx.world, m, ctx.player);
            m.seeTime = seen ? int16_t(std::max<int>(0, m.seeTime) + 1)
                             : int16_t(std::min<int>(0, m.seeTime) - 1);
            if (playerDist2 <= 15.0 * 15.0 && m.seeTime >= 20) {
                m.goal = m.pos;
                ++m.strafeTime;
            } else {
                m.strafeTime = -1;
            }
        }
        if (m.type == MobType::Witch && playerDist2 < 7.0 * 7.0)
            m.goal = m.pos; // (throws from where it stands)
        if (m.type == MobType::Pillager && playerDist2 < 8.0 * 8.0)
            m.goal = m.pos; // (shoots from ~8 blocks)
        if (m.type == MobType::Evoker && playerDist2 < 10.0 * 10.0)
            m.goal = m.pos; // (casts from a distance)
    } else if (m.type == MobType::WanderingTrader) {
        // Stands still while traded with, else strolls near where it arrived; its time
        // up, it's gone (wiki: Wandering Trader › Despawning).
        if (m.despawnDelay > 0 && --m.despawnDelay == 0) {
            m.health = 0.0f;
            m.deathTime = 19; // (gone next tick, no drops: not a death)
            m.lastHurtByPlayer = false;
        }
        if (m.tradingTicks > 0) {
            --m.tradingTicks;
            m.goal = m.pos;
        } else if (++m.goalTicks > 200 ||
                   glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 0.7) {
            if (ctx.rng.nextInt(80) == 0) {
                const glm::dvec3 home(m.home.x + 0.5, m.home.y, m.home.z + 0.5);
                m.goal = home + glm::dvec3(ctx.rng.nextDouble() * 20 - 10, 0.0,
                                           ctx.rng.nextDouble() * 20 - 10);
                m.goalTicks = 0;
            }
            speed *= 0.6;
        }
    } else if (m.type == MobType::IronGolem && golemGoal(ctx, m, speed)) {
        chase = true; // (after a monster: Golems.cpp)
    } else if (m.type == MobType::IronGolem) {
        // (patrolling: the goal is set)
    } else if ((isZombie(m.type) || m.type == MobType::Pillager || m.type == MobType::Vindicator ||
                m.type == MobType::Ravager) &&
               villageHunt(ctx, m)) {
        chase = true; // (after a villager: Villagers.cpp)
    } else if (m.raidId != 0 && m.raidId == ctx.raidId && ctx.raidCentre &&
               glm::length(glm::dvec2(ctx.raidCentre->x + 0.5 - m.pos.x,
                                      ctx.raidCentre->z + 0.5 - m.pos.z)) > 6.0) {
        // Raiders with nobody to fight march on the village bell (wiki: Raid).
        m.goal = glm::dvec3(*ctx.raidCentre) + glm::dvec3(0.5, 0.0, 0.5);
    } else if (m.type == MobType::Villager && villagerGoal(ctx, m, speed)) {
        // (home, work, the bell, sleep: Villagers.cpp)
    } else if ((isPet(m.type) || m.type == MobType::Ocelot) && m.panicTicks == 0 &&
               petGoal(ctx, m, speed)) {
        // (sitting, following, fighting, dancing: Pets.cpp)
    } else if (isWildlife(m.type) && wildlifeGoal(ctx, m, speed)) {
        // (fleeing, sleeping foxes, hunts, crops and berries, rams, rolled armadillos:
        // Wildlife.cpp)
    } else if (m.type == MobType::Warden && wardenGoal(ctx, m, speed)) {
        // (going to look where it heard something: Wardens.cpp)
    } else if (m.type == MobType::CopperGolem && copperGolemGoal(ctx, m, speed)) {
        // (sorting copper chests' items: CopperGolems.cpp)
    } else if (isMount(m.type) && m.panicTicks == 0 && mountGoal(ctx, m, speed)) {
        // (camels resting, trader llamas with their trader: Mounts.cpp)
    } else if (m.panicTicks == 0 && !info.hostile && animalGoal(ctx, m, speed)) {
        // (breeding partner, food, parent)
    } else if (m.panicTicks > 0) {
        --m.panicTicks;
        speed *= 2.0; // wiki: Cow - panics when hurt (faster)
        if (m.goalTicks++ > 30 ||
            glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.0) {
            m.goal =
                m.pos + glm::dvec3(ctx.rng.nextDouble() * 10 - 5, 0, ctx.rng.nextDouble() * 10 - 5);
            m.goalTicks = 0;
        }
    } else {
        // Random strolls (wiki: Mob - wander about every 6 s on average), only with a
        // player within 32 blocks.
        if (playerDist2 > 32.0 * 32.0) {
            m.goal = m.pos;
        } else if (++m.goalTicks > 200 ||
                   glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 0.7) {
            if (ctx.rng.nextInt(120) == 0) {
                m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 20 - 10, 0,
                                            ctx.rng.nextDouble() * 20 - 10);
                m.goalTicks = 0;
            } else if (m.goalTicks > 200) {
                m.goal = m.pos;
            }
        }
    }

    // Path to the goal (M16.2): a new search only when the goal cell moved (or a chase
    // path ran out), at most every 4-10 ticks; longer waits after a partial path (+15)
    // and for far targets (+5 past 16 blocks, +10 past 32), as vanilla's melee goal.
    // (M30.5) wide mobs path with their footprint (ceil of the width, at most 3 cells) from
    // its corner; villagers, traders and piglins open wooden doors (vanilla).
    PathOptions pathOpts;
    pathOpts.height = int(std::ceil(info.height));
    pathOpts.footprint = std::clamp(int(std::ceil(info.width - 1e-6)), 1, 3);
    pathOpts.openDoors =
        m.type == MobType::Villager || m.type == MobType::WanderingTrader ||
        m.type == MobType::Piglin || m.type == MobType::PiglinBrute ||
        (m.type == MobType::Vindicator && m.raidId != 0); // (wiki: raid vindicators)
    // (M32.2; wiki: Zombie) on Hard, zombies born with the ability (not drowned) path through
    // wooden doors - and break them instead of opening them.
    const bool breaksDoors = m.canBreakDoors && ctx.difficulty >= 3 && ctx.mobGriefing &&
                             isZombie(m.type) && m.type != MobType::Drowned &&
                             !m.isBaby(); // (M32 review: mob_griefing off keeps doors)
    pathOpts.openDoors = pathOpts.openDoors || breaksDoors;
    const double cellCentre = double(pathOpts.footprint) * 0.5;
    const glm::ivec3 feet = Pathfinder::cellOf(m.pos, pathOpts.footprint);
    const glm::ivec3 goalCell = Pathfinder::cellOf(m.goal, pathOpts.footprint);
    if (m.repathTicks > 0) --m.repathTicks;
    const bool moved = goalCell != m.pathRequest, finished = m.pathIndex >= m.pathLength;
    const bool wantsPath = goalCell != feet && m.repathTicks == 0 && (moved || (chase && finished));
    if (wantsPath && m_searches >= kMaxSearches)
        m.repathTicks = 1; // (next tick; deterministic, though the
                           // shared RNG then runs in another order)
    if (wantsPath && m_searches < kMaxSearches) {
        ++m_searches;
        // Search budget: vanilla visits up to follow range x 16 nodes (zombie 35).
        m.pathLength =
            uint8_t(m_pathfinder.find(ctx.world, feet, goalCell, pathOpts, chase ? 560 : 200,
                                      m.path.data(), MobData::kMaxPath));
        m.pathIndex = 0;
        m.pathRequest = goalCell;
        const bool partial = m.pathLength == 0 || m.path[size_t(m.pathLength - 1)] != goalCell;
        const double far2 = glm::dot(m.goal - m.pos, m.goal - m.pos);
        m.repathTicks = int16_t(4 + ctx.rng.nextInt(7) + (partial ? 15 : 0) +
                                (far2 > 16.0 * 16.0 ? 5 : 0) + (far2 > 32.0 * 32.0 ? 5 : 0));
        if (!chase && partial && m.pathLength > 0 && m.panicTicks == 0) {
            // A stroll target it can't reach: stop where the path ends.
            const glm::ivec3 end = m.path[size_t(m.pathLength - 1)];
            m.goal = {end.x + cellCentre, double(end.y), end.z + cellCentre};
            m.pathRequest = end;
        }
    }
    // The next cell of the path (skipping the ones reached), else the goal itself.
    while (m.pathIndex < m.pathLength) {
        const glm::ivec3 c = m.path[m.pathIndex];
        const double dx = c.x + cellCentre - m.pos.x, dz = c.z + cellCentre - m.pos.z;
        if (dx * dx + dz * dz < 0.35 * 0.35 && std::abs(double(c.y) - m.pos.y) < 1.1)
            ++m.pathIndex;
        else
            break;
    }
    glm::dvec3 steer = m.goal;
    bool climb = false;
    if (m.pathIndex < m.pathLength) {
        const glm::ivec3 c = m.path[m.pathIndex];
        steer = {c.x + cellCentre, double(c.y), c.z + cellCentre};
        const double cdx = c.x + cellCentre - m.pos.x, cdz = c.z + cellCentre - m.pos.z;
        climb = c.y > feet.y && cdx * cdx + cdz * cdz < 1.5 * 1.5; // a step up just ahead: jump
        // (M30.5; vanilla InteractWithDoor) a closed wooden door in the next cell opens; it
        // is shut again about a second after the mob is through.
        if (pathOpts.openDoors && m.doorOpened.y == INT32_MIN && m.doorBreaking.y == INT32_MIN) {
            const auto& reg = blockRegistry();
            const BlockPos dp{c.x, c.y, c.z};
            const BlockStateId ds = ctx.world.getBlock(dp);
            // (a breaker starts within 1.5 blocks - ours)
            if (reg.likeOf(reg.blockOf(ds)) == blocks::OakDoor &&
                reg.get(ds, properties::open) == 1 &&
                cdx * cdx + cdz * cdz < (breaksDoors ? 1.5 * 1.5 : 2.0 * 2.0)) {
                const bool upper = reg.get(ds, properties::doorHalf) == 0;
                const BlockPos lower = upper ? BlockPos{dp.x, dp.y - 1, dp.z} : dp;
                if (breaksDoors) {
                    m.doorBreaking = {lower.x, lower.y, lower.z};
                    m.doorBreakTicks = 0;
                } else {
                    setMobDoor(ctx.world, lower, true);
                    m.doorOpened = {lower.x, lower.y, lower.z};
                    m.doorTicks = 0;
                }
            }
        }
    }
    // (M32.2; wiki: Zombie › Breaking doors) beating on the door, a bang 1 tick in 20, for 240
    // ticks (12 s), then it is gone - no drop. It gives up when the door opens or goes,
    // when it is more than 2 blocks away or the difficulty drops below Hard.
    if (m.doorBreaking.y != INT32_MIN) {
        const auto& reg = blockRegistry();
        const BlockPos lower{m.doorBreaking.x, m.doorBreaking.y, m.doorBreaking.z};
        const BlockStateId ds = ctx.world.getBlock(lower);
        const double ddx = lower.x + 0.5 - m.pos.x, ddz = lower.z + 0.5 - m.pos.z;
        if (!breaksDoors || reg.likeOf(reg.blockOf(ds)) != blocks::OakDoor ||
            reg.get(ds, properties::open) == 0 || ddx * ddx + ddz * ddz > 2.0 * 2.0) {
            m.doorBreaking.y = INT32_MIN;
        } else {
            if (ctx.rng.nextInt(20) == 0)
                ctx.world.levelEvent(LevelEvent::Type::BlockHit, lower.x, lower.y, lower.z, ds);
            if (++m.doorBreakTicks >= kDoorBreakTicks) {
                ctx.world.levelEvent(LevelEvent::Type::BlockBreak, lower.x, lower.y, lower.z, ds);
                ctx.world.updateBlock(lower, 0);
                const BlockPos upper{lower.x, lower.y + 1,
                                     lower.z}; // (block updates take it too; made sure)
                if (reg.likeOf(reg.blockOf(ctx.world.getBlock(upper))) == blocks::OakDoor)
                    ctx.world.updateBlock(upper, 0);
                if (ctx.edits) {
                    ctx.edits->push_back(lower);
                    ctx.edits->push_back({lower.x, lower.y + 1, lower.z});
                }
                m.doorBreaking.y = INT32_MIN;
                m.repathTicks = 0;
            }
        }
    }
    if (m.doorOpened.y != INT32_MIN) {
        const glm::dvec3 door(m.doorOpened.x + 0.5, m.doorOpened.y, m.doorOpened.z + 0.5);
        const double gone = glm::length(glm::dvec2(door.x - m.pos.x, door.z - m.pos.z));
        ++m.doorTicks; // (every tick: the 10 s fallback must fire even beside the door - M30
                       // review)
        if ((gone > 1.5 && m.doorTicks > 20) || m.doorTicks > 200) {
            // Not on top of someone in the doorway (vanilla waits for them).
            const Aabb cell{glm::dvec3(m.doorOpened.x, m.doorOpened.y, m.doorOpened.z),
                            glm::dvec3(m.doorOpened.x + 1, m.doorOpened.y + 2, m.doorOpened.z + 1)};
            bool blocked = false;
            if (const Chunk* dc =
                    ctx.world.chunk({blockToChunk(m.doorOpened.x), blockToChunk(m.doorOpened.z)}))
                for (const MobData& o : dc->mobs())
                    if (&o != &m && o.health > 0.0f && box(o).intersects(cell)) blocked = true;
            if (!blocked || m.doorTicks > 400) {
                setMobDoor(ctx.world, {m.doorOpened.x, m.doorOpened.y, m.doorOpened.z}, false);
                m.doorOpened.y = INT32_MIN;
            }
        }
    }

    // Steer towards it; jump when a block is in the way.
    glm::dvec3 wish(0.0);
    bool jump = false;
    const bool last = m.pathIndex + 1 >= m.pathLength;
    const glm::dvec2 d(steer.x - m.pos.x, steer.z - m.pos.z);
    const double dl = glm::length(d);
    // (stopping short of the target only where the path reaches it: at the end of a partial
    // path a chaser keeps pushing on - spiders up the wall - M30.5)
    const bool reaches = m.pathLength > 0 && m.path[size_t(m.pathLength - 1)] == m.pathRequest;
    const double stopAt = chase && last && reaches ? info.width * 0.5 + 0.5
                                                   : (m.pathIndex < m.pathLength ? 0.1 : 0.5);
    if (dl > stopAt) {
        wish = glm::dvec3(d.x / dl, 0, d.y / dl) * speed;
        m.yaw = approachAngle(m.yaw, yawTowards(m.pos, steer), 10.0f);
        const int ax = int(std::floor(m.pos.x + d.x / dl * (info.width * 0.5 + 0.4)));
        const int az = int(std::floor(m.pos.z + d.y / dl * (info.width * 0.5 + 0.4)));
        const int fy = int(std::floor(m.pos.y + 0.01));
        jump = climb ||
               (solidAt(ctx.world, ax, fy, az) && !solidAt(ctx.world, ax, fy + 1, az) &&
                !solidAt(ctx.world, int(std::floor(m.pos.x)), fy + 2, int(std::floor(m.pos.z))));
    }
    // Strafing (M32.2; wiki: Skeleton › Behavior): sideways at half speed, flipping either way
    // 3 times in 10 each second; backing off inside a quarter of the range (15), coming in again
    // beyond three quarters; facing the player throughout.
    if (chase && (isSkeleton(m.type) || m.type == MobType::Illusioner) && m.strafeTime >= 0) {
        if (m.strafeTime >= 20) {
            if (ctx.rng.nextFloat() < 0.3f) m.strafeClockwise = !m.strafeClockwise;
            if (ctx.rng.nextFloat() < 0.3f) m.strafeBack = !m.strafeBack;
            m.strafeTime = 0;
        }
        if (playerDist2 > 15.0 * 15.0 * 0.75)
            m.strafeBack = false;
        else if (playerDist2 < 15.0 * 15.0 * 0.25)
            m.strafeBack = true;
        const glm::dvec2 f = glm::length(glm::dvec2(toPlayer.x, toPlayer.z)) > 1e-6
                                 ? glm::normalize(glm::dvec2(toPlayer.x, toPlayer.z))
                                 : glm::dvec2(0.0, 1.0);
        const glm::dvec2 side(-f.y, f.x);
        const glm::dvec2 w =
            f * (m.strafeBack ? -0.5 : 0.5) + side * (m.strafeClockwise ? 0.5 : -0.5);
        // (only onto ground: no strafing off a ledge)
        const glm::dvec3 ahead = m.pos + glm::dvec3(w.x, 0.0, w.y) * 2.0;
        if (solidAt(ctx.world, int(std::floor(ahead.x)), int(std::floor(m.pos.y - 0.5)),
                    int(std::floor(ahead.z))))
            wish = glm::dvec3(w.x, 0.0, w.y) * speed;
        m.yaw = approachAngle(m.yaw, yawTowards(m.pos, playerPos), 30.0f);
    }
    // Head: look at a near player (wiki: look-at-player goal, 6-8 blocks).
    if (playerDist2 < 8.0 * 8.0) {
        m.headYaw = approachAngle(m.headYaw, yawTowards(m.pos, playerPos), 10.0f);
        const double horiz = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        m.pitch = static_cast<float>(-std::atan2(toPlayer.y + 1.62 - info.height * 0.85, horiz) *
                                     180.0 / std::numbers::pi);
    } else {
        m.headYaw = approachAngle(m.headYaw, m.yaw, 10.0f);
        m.pitch *= 0.9f;
    }
    // Rabbits get about by hopping (wiki: Rabbit).
    if (m.type == MobType::Rabbit && (wish.x != 0.0 || wish.z != 0.0) && m.onGround) jump = true;
    if (m.type == MobType::Armadillo && m.sitting) wish = glm::dvec3(0.0); // (rolled up)
    const glm::dvec3 before = m.pos;
    physics(ctx.world, m, wish, jump);
    if (isWildlife(m.type)) wildlifeTick(ctx, m, m.climbing && glm::length(m.pos - before) < 0.05);

    // Melee (wiki: Zombie - 3 damage on normal, once a second, reach ~ width*2).
    if (m.attackCooldown > 0) --m.attackCooldown;
    if (chase && m.attackCooldown == 0 && info.attackDamage > 0.0f) {
        const double reach = info.width * 2.0 + 0.6;
        if (playerDist2 < reach * reach &&
            box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.8, 0, 0.8),
                                   ctx.player.box().max + glm::dvec3(0.8, 0, 0.8)})) {
            // (golems: 7.5 + 0-14 and a throw upward)
            // (M29.2c; wiki: Strength +3, Weakness -4 a level)
            const float hit = std::max(
                0.0f, info.attackDamage + (isZombie(m.type) ? weaponBonus(ctx.world, m) : 0.0f) +
                          (m.type == MobType::IronGolem ? float(ctx.rng.nextInt(15)) : 0.0f) +
                          3.0f * float(m.effectLevel(uint8_t(Effect::Strength))) -
                          4.0f * float(m.effectLevel(uint8_t(Effect::Weakness))));
            if (ctx.vitals.attacked(hit, &m.pos)) {
                setPlayerAttacker(m.uuidHi); // (tamed wolves go for it - M26.1)
                // Thorns (M29.2b; wiki): 15% a level (added up over the armor, ours) to hit
                // back for 1-4.
                if (ctx.thorns > 0 && int(ctx.rng.nextInt(100)) < 15 * ctx.thorns)
                    attack(m, float(1 + ctx.rng.nextInt(4)), ctx.player.position());
                if (ctx.vitals.lastHitFresh()) ctx.player.knockback(toPlayer.x, toPlayer.z);
                if (m.type == MobType::IronGolem && ctx.vitals.lastHitFresh())
                    ctx.player.setVelocity(ctx.player.velocity() + glm::dvec3(0.0, 0.4, 0.0));
                // (M26.4a; wiki, Normal) a cave spider's bite poisons for 7 s, a wither
                // skeleton's hit withers for 10 s.
                // Poison 7 s on Normal, 15 s on Hard, none on Easy (wiki: Cave Spider).
                if (m.type == MobType::CaveSpider && ctx.difficulty >= 2)
                    ctx.vitals.addEffect(Effect::Poison, 0, ctx.difficulty == 3 ? 300 : 140);
                if (m.type == MobType::WitherSkeleton) ctx.vitals.addEffect(Effect::Wither, 0, 200);
                // (M29.1a; wiki: Husk) a husk's hit starves: Hunger for 7 s (Normal).
                if (m.type == MobType::Husk) ctx.vitals.addEffect(Effect::Hunger, 0, 140);
            }
            m.attackCooldown = 20;
        }
    }

    if (info.hostile) monsterTick(ctx, m, chase, playerDist2);
    if (m.callReinforcements) zombieReinforcements(ctx, m);
    if (m.canPickUpLoot) gearPickup(ctx, m);
    if (isLlama(m.type)) llamaTick(ctx, m);

    // Undead burn in daylight under open sky (wiki: Zombie, Skeleton): 1 damage a second.
    // Water or rain on it puts any burning mob out (wiki: Fire, Rain).
    // (M29.1b: armor shades a zombie horse or nautilus)
    const bool undead = burnsInDaylight(m.type) && m.horseArmor == 0;
    if (undead || m.fireTicks > 0 || m.type == MobType::Husk) {
        const BlockPos head{int(std::floor(m.pos.x)),
                            int(std::floor(m.pos.y + mobInfo(m.type).height * 0.85)),
                            int(std::floor(m.pos.z))};
        const Chunk* c = ctx.world.chunk(head.chunk());
        const bool day = ctx.skyDarken < 4.0f;
        const bool sky =
            c && c->lit() && c->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z)) >= 15;
        const BlockStateId hs =
            c ? c->get(blockToLocal(head.x), head.y, blockToLocal(head.z)) : BlockStateId{0};
        bool wet =
            c && (blockRegistry().blockOf(hs) == blocks::Water || blockRegistry().waterlogged(hs));
        // A zombie under water turns into a drowned: 30 s submerged, then 15 s of
        // shaking (wiki: Zombie › Drowned conversion; ours counts 45 s in one go).
        // A husk does the same and becomes a zombie (wiki: Husk).
        if (m.type == MobType::Zombie ||
            m.type == MobType::Husk) { // (babies too; it keeps the zombie's persistence - review)
            if (!wet)
                m.airTicks = 300;
            else if (--m.airTicks <= -600) {
                m.type = m.type == MobType::Husk ? MobType::Zombie : MobType::Drowned;
                m.airTicks = 300;
                m.health = mobInfo(m.type).maxHealth;
            }
        }
        if (!wet && ctx.weather && ctx.weather->raining &&
            ((undead && day && sky) || m.fireTicks > 0))
            // (sky light 15 at the head: open sky, so no column scan is needed)
            wet = sky ? precipitationAt(ctx.world, head) == Precipitation::Rain
                      : rainingAt(ctx.world, *ctx.weather, head);
        // Re-lit to 8 s while in the sun; refreshed once a second so the 1-per-second
        // damage clock below keeps running.
        if (undead && day && sky && !wet && m.fireTicks <= 140) m.fireTicks = 160;
        if (wet) m.fireTicks = 0;
    }
    if (m.fireTicks > 0) { // wiki: Fire - 1 damage a second
        if (m.fireTicks % 20 == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        --m.fireTicks;
    }
}

bool Mobs::placeEndCrystal(World& world, const BlockPos& on, Xoroshiro& rng) {
    const auto& r = blockRegistry();
    const BlockId b = r.blockOf(world.getBlock(on));
    if (b != blocks::Obsidian && b != blocks::Bedrock) return false;
    if (world.getBlock({on.x, on.y + 1, on.z}) != 0 || world.getBlock({on.x, on.y + 2, on.z}) != 0)
        return false;
    MobData m = make(MobType::EndCrystal, {on.x + 0.5, on.y + 1.0, on.z + 0.5}, rng);
    m.showBottom = false; // (placed crystals have no base)
    const Aabb room = box(m);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* c = world.chunk({blockToChunk(on.x) + dx, blockToChunk(on.z) + dz}))
                for (const MobData& o : c->mobs())
                    if (box(o).intersects(room)) return false;
    return add(world, m);
}

void Mobs::attack(MobData& m, float damage, const glm::dvec3& from) {
    // (M32.3; wiki: Damage › Immunity) for 10 ticks after a hit only a stronger one
    // counts, by the difference - without a new hurt flash or anger.
    if (m.deathTime > 0) return;
    // (M33.2c; wiki: Sulfur Cube) with a block inside a hit doesn't hurt it: it is launched
    // away from the hitter, the harder the hit the farther.
    if (m.type == MobType::SulfurCube && m.absorbed != 0) {
        const glm::dvec2 d(m.pos.x - from.x, m.pos.z - from.z);
        const double l = std::max(1e-6, glm::length(d));
        const double f = 0.3 + 0.08 * double(damage);
        m.vel += glm::dvec3(d.x / l * f, 0.25 + 0.04 * double(damage), d.y / l * f);
        m.lastHurtByPlayer = true;
        return;
    }
    const bool cooling = m.hurtTime > 0;
    if (cooling) {
        if (damage <= m.lastHurtAmount) return;
        const float full = damage;
        damage -= m.lastHurtAmount;
        m.lastHurtAmount = full;
    } else {
        m.lastHurtAmount = damage;
    }
    if (isTechnical(m.type)) return; // (M29.7e: nothing to hurt)
    if (m.type == MobType::Wither && m.spellTicks > 0)
        return; // (M26.4b: charging, it can't be hurt)
    if (m.type == MobType::Creaking && m.home.y != kNoPoint)
        damage = 0.0f;               // (M27.1c: only its heart can end it)
    if (m.type == MobType::Warden) { // (M27.3c) unhurt while emerging or digging; a hit angers it
        if (m.phase != 1) return;
        m.angerTicks = int16_t(std::min(150, m.angerTicks + 100));
        m.goal = from;
        m.goalTicks = 0;
    }
    if (m.type == MobType::Shulker && m.peek == 0) damage *= 0.2f; // (armour 20 while closed)
    if (m.type == MobType::Armadillo && m.sitting)
        damage = std::max(0.0f, damage - 1.0f) * 0.5f; // (M26.3: rolled up)
    // A mount's body armor (M29.3a; wiki: Armor - vanilla's formula: armor points cut
    // max(points / 5, points - damage / (2 + toughness / 4)) x 4%, up to 80%).
    if (m.horseArmor > 0 && isMount(m.type)) {
        const bool nautilus = isNautilus(m.type);
        const float points = float(nautilus ? kNautilusArmorPoints[m.horseArmor % 6]
                                            : kHorseArmorPoints[m.horseArmor % 7]);
        const float toughness = m.horseArmor == (nautilus ? 4 : 6) ? (nautilus ? 2.0f : 3.0f)
                                : nautilus && m.horseArmor == 5    ? 3.0f
                                                                   : 0.0f;
        const float cut = std::clamp(
            std::max(points / 5.0f, points - damage / (2.0f + toughness / 4.0f)), 0.0f, 20.0f);
        damage *= 1.0f - cut / 25.0f;
    }
    // (M32.2c) a monster's armor - natural and worn - by the same formula, then Protection
    // (4% a level, at most 80%).
    damage = armorReduced(m, damage);
    m.health -= damage;
    if (cooling) return; // (the rest happened with the first hit)
    if (m.type == MobType::Villager)
        addGossip(m, Gossip::MinorNegative, 25); // (M32.5: it remembers)
    m.hurtTime = 10;
    m.noPlayerTicks = 0;       // damage resets the despawn clock
    m.lastHurtByPlayer = true; // (Mobs::attack: the player's hits)
    if (isZombie(m.type) && m.reinforcements > 0.0f) m.callReinforcements = true; // (M32.2)
    m.lastHurtBySkeleton = false;
    if (m.type == MobType::Wolf && !m.tamed) { // a wild wolf turns on the player (wiki: Wolf)
        m.angry = true;
        m.angerTicks = 600;
        m.targeting = true;
        m.angerAlert = true;                   // (M32.6: and its pack with it)
    } else if (m.type == MobType::IronGolem) { // golems don't flee: they fight back (wiki), unless
                                               // player-built
        if (!m.playerCreated) {
            m.angry = true;
            m.angerTicks = 600;
        }
    } else if (m.type == MobType::PolarBear ||
               (m.type == MobType::Panda && pandaPersonality(m.woolColour, m.color2) == 6)) {
        m.angry = true; // (M26.3: they fight back - wiki: Polar Bear, Panda)
        m.angerTicks = 400;
        m.targeting = true;
    } else if (isNautilus(m.type) && !m.tamed) { // (M26.5a: neutral - it bites back)
        m.angry = true;
        m.angerTicks = 400;
    } else if (isLlama(m.type)) { // llamas spit back (wiki: Llama)
        m.angry = true;
        m.angerTicks = 200;
    } else if (!mobInfo(m.type).hostile && m.type != MobType::SkeletonHorse && m.type != MobType::ZombieHorse) {
        m.panicTicks = 100; // passive mobs flee (wiki: Cow; 26.1: not skeleton or zombie horses)
    }
    if (m.type == MobType::Piglin)
        m.admireTicks = 0; // a hit takes the ingot back (wiki: Bartering)
    if (isSpider(m.type) || m.type == MobType::Enderman ||
        m.type == MobType::Piglin) { // provoked (wiki)
        m.angry = true;
        m.targeting = true;
        m.angerTicks = 600;
    }
    if (m.type == MobType::ZombifiedPiglin) m.angerAlert = true; // (the herd joins in: Mobs::tick)
    if (m.type == MobType::Bee &&
        !m.stung) { // (M26.3b: it stings back, and its hive-mates join in)
        m.angry = true;
        m.angerTicks = 400;
        m.angerAlert = true;
    }
    const glm::dvec2 d(m.pos.x - from.x, m.pos.z - from.z);
    const double l = glm::length(d);
    // wiki: Knockback - 0.4 away, less by the mob's knockback resistance (M32.3); lifted
    // only when on the ground.
    const double strength = 0.4 * (1.0 - knockbackResistance(m));
    if (l > 1e-6 && strength > 0.0) {
        m.vel.x = m.vel.x / 2.0 + d.x / l * strength;
        m.vel.z = m.vel.z / 2.0 + d.y / l * strength;
        if (m.onGround) m.vel.y = std::min(0.4, m.vel.y / 2.0 + strength);
    }
}

float Mobs::armorReduced(const MobData& m, float damage) {
    if (isMount(m.type)) return damage; // (their body armor: Mobs::attack)
    if (const int points = armorPoints(m); points > 0) {
        const float p = float(points), toughness = armorToughness(m);
        const float cut =
            std::clamp(std::max(p / 5.0f, p - damage / (2.0f + toughness / 4.0f)), 0.0f, 20.0f);
        damage *= 1.0f - cut / 25.0f;
    }
    if (m.gearEpf > 0) damage *= 1.0f - float(std::min<int>(m.gearEpf, 20)) / 25.0f;
    return damage;
}

double Mobs::knockbackResistance(const MobData& m) {
    // (M32.3; wiki: each mob's Attributes, Netherite armor) the knockback_resistance base -
    // iron golems, wardens, shulkers 1, ravagers 0.75, hoglins and zoglins 0.6, zombies a
    // random 0-0.05 they were born with (ours from the UUID) - plus 0.1 a netherite piece.
    double r = 0.0;
    switch (m.type) {
    case MobType::IronGolem:
    case MobType::Warden:
    case MobType::Shulker:
        r = 1.0;
        break;
    case MobType::Ravager:
        r = 0.75;
        break;
    case MobType::Hoglin:
    case MobType::Zoglin:
        r = 0.6;
        break;
    default:
        if (isNautilus(m.type)) r = 0.3; // (wiki: Attribute - nautiluses)
        if (isZombie(m.type) || m.type == MobType::ZombifiedPiglin)
            r = double(m.uuidHi % 51) / 1000.0;
        break;
    }
    for (uint8_t w : m.worn)
        r += w == 6 && m.type != MobType::ArmorStand ? 0.1 : 0.0;
    return std::min(r, 1.0);
}

void Mobs::die(Context& ctx, MobData& m) {
    if (m.doorOpened.y != INT32_MIN) { // (M30 review) a door it opened is shut behind it
        setMobDoor(ctx.world, {m.doorOpened.x, m.doorOpened.y, m.doorOpened.z}, false);
        m.doorOpened.y = INT32_MIN;
    }
    if (m.leash != 0) { // (M28.3c) its lead drops
        if (const auto lead = itemRegistry().find("lead"))
            ctx.items.spawn(m.pos, {*lead, 1}, ctx.rng);
        m.leash = 0;
    }
    if (m.type == MobType::LeashKnot) { // (blown up, say: what was tied to it goes free)
        breakKnot(ctx.world, m, ctx.items, ctx.rng);
        return;
    }
    if (m.type == MobType::ArmorStand) { // (M28.3b) itself and what it wore, gone at once
        dropArmorStand(ctx, m);
        m.deathTime = 19;
        return;
    }
    if (isHanging(m.type)) { // (M28.3a) the frame or painting as an item, gone at once
        dropHanging(ctx, m);
        m.deathTime = 19;
        return;
    }
    if (m.type == MobType::Boat) { // broken: the boat item, gone at once (M25.2b)
        m.deathTime = 19;
        if (const auto boat =
                itemRegistry().find(m.hasChest ? chestBoatId(m.woolColour) : boatId(m.woolColour)))
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {*boat, 1}, ctx.rng);
        if (m.hasChest) dropMountGear(ctx, m); // (M26.2: its chest's stacks)
        return;
    }
    if (m.hasGear || isSkeleton(m.type)) dropGear(ctx, m);   // (M32.2c)
    if (m.type == MobType::SulfurCube && !m.ownBlast) { // (M33.2c; wiki: Sulfur Cube)
        if (m.absorbed != 0) { // its block falls out
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {m.absorbed, 1}, ctx.rng);
            m.absorbed = 0;
        }
        if (!m.isBaby()) // a large one leaves two small ones
            for (int k = 0; k < 2 && m_births.size() < m_births.capacity(); ++k) {
                MobData small = make(MobType::SulfurCube,
                                     m.pos + glm::dvec3(k == 0 ? -0.25 : 0.25, 0.1, ctx.rng.nextDouble() * 0.5 - 0.25),
                                     ctx.rng);
                small.age = -24000;
                small.health = 4.0f;
                m_births.push_back(small);
            }
    }
    if (m.type == MobType::Villager && m.lastHurtByPlayer) { // (M32.5) the village remembers
        const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))),
                          blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                    for (MobData& o : ch->mobs())
                        if (&o != &m && o.type == MobType::Villager && o.health > 0.0f &&
                            glm::dot(o.pos - m.pos, o.pos - m.pos) < 16.0 * 16.0)
                            addGossip(o, Gossip::MajorNegative, 25);
    }
    if (isMount(m.type)) dropMountGear(ctx, m);             // (M26.2)
    if (m.type == MobType::Fox && m.mouthItem != kNoItem) { // (M26.3) what it carried
        ctx.items.spawn(m.pos + glm::dvec3(0, 0.4, 0), {m.mouthItem, 1}, ctx.rng);
        m.mouthItem = kNoItem;
    }
    if (m.type == MobType::Wolf && m.horseArmor > 0) { // (M26.3) its wolf armor, worn as it was
        ItemStack armor{*itemRegistry().find("wolf_armor"), 1};
        armor.damage = uint16_t(std::clamp<int>(m.armorWear, 0, 63));
        ctx.items.spawn(m.pos + glm::dvec3(0, 0.4, 0), armor, ctx.rng);
        m.horseArmor = 0;
    }
    if (m.type == MobType::Minecart) { // broken: the cart item, gone at once (its kind's - M29.3e)
        m.deathTime = 19;
        if (const auto cart = itemRegistry().find(kCartKinds[m.decor == 6 ? 0 : m.decor % 7]))
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {*cart, 1}, ctx.rng);
        if (m.hasChest) dropMountGear(ctx, m); // (a chest or hopper cart's stacks)
        return;
    }
    if (m.type == MobType::EndCrystal) {
        // Any damage blows it up: power 6, no fire (wiki: End Crystal); nearby crystals
        // caught in the blast go off too.
        m.deathTime = 19; // gone next tick
        m_scratchEdits.clear();
        std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
        ExplosionTargets t;
        t.tnt = ctx.tnt;
        if (ctx.survival && !ctx.playerDead) {
            t.player = &ctx.player;
            t.vitals = &ctx.vitals;
        }
        // A dragon it was healing takes 10 damage (wiki: End Crystal); any dragon
        // around turns on the player (strafes with a fireball; wiki: Ender Dragon).
        const glm::dvec3 top = m.pos + glm::dvec3(0.0, 1.4, 0.0);
        const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))),
                          blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -8; dz <= 8; ++dz)
            for (int dx = -8; dx <= 8; ++dx)
                if (Chunk* c = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                    for (MobData& d : c->mobs())
                        if (d.type == MobType::EnderDragon && d.health > 0.0f) {
                            if (d.hasBeam && glm::length(d.beam - top) < 0.5) {
                                d.health -= 10.0f;
                                d.hurtTime = 10;
                                d.hasBeam = false;
                            }
                            if (d.phase != 9 && ctx.survival && !ctx.playerDead) {
                                d.phase = 1; // strafe
                                d.phaseTicks = 0;
                            }
                        }
        m_explosion.explode(ctx.world, m.pos, 6.0f, ctx.rng, ctx.items, changed,
                            t); // (from its bottom)
        return;
    }
    // Loot (wiki: Cow - raw beef 1-3, leather 0-2; Zombie - rotten flesh 0-2).
    const auto& items = itemRegistry();
    auto drop = [&](const char* id, int lo, int hi) {
        // Item ids by name, resolved once per name (no string search per death).
        struct Cached {
            const char* name;
            ItemId item;
        };
        static Cached cache[128] = {}; // (room for every name dropped here: ~55 in M25)
        ItemId item = 0;
        for (Cached& c : cache) {
            if (c.name == id) { // same literal
                item = c.item;
                break;
            }
            if (!c.name) {
                c = {id, *items.find(id)};
                item = c.item;
                break;
            }
        }
        // Looting: up to one more per level (wiki: Looting).
        const int extra =
            m.lastHurtByPlayer ? static_cast<int>(ctx.rng.nextInt(uint32_t(m.looting) + 1)) : 0;
        const int n = lo + static_cast<int>(ctx.rng.nextInt(uint32_t(hi - lo + 1))) + extra;
        if (n > 0) ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {item, uint8_t(n)}, ctx.rng);
    };
    if (m.lastHurtByPlayer && m_killCount < int(m_kills.size()))
        m_kills[size_t(m_killCount++)] = m.type; // (statistics)
    // Babies drop nothing (wiki: Breeding) - but baby zombies drop their loot (M32.2).
    if (m.isBaby() && !isZombie(m.type)) return;
    // Game rule mob_drops off: no loot and no experience (what it wore or carried still falls).
    if (!ctx.mobDrops) return;
    // Killed by a charged creeper's blast: its head (M26.4b; wiki: Head - zombies,
    // skeletons, creepers, piglins and wither skeletons).
    if (m.chargedBlast > 0) {
        const char* head = m.type == MobType::Zombie           ? "zombie_head"
                           : m.type == MobType::Skeleton       ? "skeleton_skull"
                           : m.type == MobType::Creeper        ? "creeper_head"
                           : m.type == MobType::Piglin         ? "piglin_head"
                           : m.type == MobType::WitherSkeleton ? "wither_skeleton_skull"
                                                               : nullptr;
        if (head)
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {*itemRegistry().find(head), 1},
                            ctx.rng); // (one, Looting or not)
    }
    // Experience when the player killed it (wiki: Experience): monsters 5, animals 1-3.
    // (wiki: blazes, evokers and breezes 10, ravagers 20, magma cubes their size; villagers,
    // wandering traders and iron golems none)
    const bool noXp = m.type == MobType::Villager || m.type == MobType::WanderingTrader ||
                      m.type == MobType::IronGolem;
    if (ctx.orbs && m.lastHurtByPlayer && !noXp) {
        const int xp =
            m.type == MobType::Wither    ? 50 // (M26.4b)
            : m.type == MobType::Ravager ? 20
            : m.type == MobType::Blaze || m.type == MobType::Evoker || m.type == MobType::Breeze
                ? 10
            : m.type == MobType::MagmaCube || m.type == MobType::Slime ? int(m.size)
            : m.isBaby() && isZombie(m.type)                           ? 12 // (M32.2: 5 x 2.5)
            : m.type == MobType::SulfurCube ? (m.isBaby() ? 0 : 1 + int(ctx.rng.nextInt(2))) // (M33.2c: large 1-2)
            : mobInfo(m.type).hostile                                  ? 5
                                      : 1 + static_cast<int>(ctx.rng.nextInt(3));
        // (M27.3) a sculk catalyst nearby takes it and blooms sculk instead
        if (!BlockUpdates::sculkBloom(ctx.world, m.pos, xp, ctx.rng))
            ctx.orbs->drop(m.pos + glm::dvec3(0, 0.5, 0), xp, ctx.rng);
    }
    const bool burning = m.fireTicks > 0; // meat drops cooked
    switch (m.type) {
    case MobType::Cow:
        drop(burning ? "cooked_beef" : "beef", 1, 3);
        drop("leather", 0, 2);
        break;
    case MobType::Zombie:
    case MobType::ZombieVillager:
    case MobType::Husk: // wiki: Husk - rotten flesh 0-2
        drop("rotten_flesh", 0, 2);
        // (M32.6; wiki: Zombie › Drops) killed by the player: 2.5% (+1% a Looting level) an
        // iron ingot, a carrot or a potato.
        if (m.lastHurtByPlayer && ctx.rng.nextFloat() < 0.025f + 0.01f * float(m.looting)) {
            static constexpr const char* kRare[3] = {"iron_ingot", "carrot", "potato"};
            if (const auto rare = items.find(kRare[ctx.rng.nextInt(3)]))
                ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {*rare, 1}, ctx.rng);
        }
        break;
    case MobType::Drowned: // wiki: Drowned - rotten flesh 0-2, a copper ingot 11%, its trident 8.5%
        drop("rotten_flesh", 0, 2);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(100) < 11) drop("copper_ingot", 1, 1);
        if (m.heldTrident && m.lastHurtByPlayer && ctx.rng.nextInt(1000) < 85)
            drop("trident", 1, 1);
        break;
    case MobType::Witch: { // wiki: Witch - 1-3 rolls of bottles, glowstone, gunpowder, redstone,
                           // spider eyes, sugar, sticks
        static constexpr const char* kLoot[7] = {"glass_bottle", "glowstone_dust", "gunpowder",
                                                 "redstone",     "spider_eye",     "sugar",
                                                 "stick"};
        for (int k = 0, n = 1 + int(ctx.rng.nextInt(3)); k < n; ++k)
            drop(kLoot[ctx.rng.nextInt(7)], 1, 2);
        break;
    }
    case MobType::Vindicator: // wiki: Vindicator - 0-1 emerald, sometimes its iron axe
        drop("emerald", 0, 1);
        if (ctx.rng.nextInt(1000) < 85) drop("iron_axe", 1, 1);
        if (m.captain && m.raidId == 0 && m.lastHurtByPlayer) drop("ominous_bottle", 1, 1);
        break;
    case MobType::Evoker: // wiki: Evoker - a totem of undying, 0-1 emerald
        drop("totem_of_undying", 1, 1);
        drop("emerald", 0, 1);
        break;
    case MobType::Pillager: // wiki: Pillager - 0-2 arrows, sometimes its crossbow (8.5%)
        drop("arrow", 0, 2);
        if (ctx.rng.nextInt(1000) < 85) drop("crossbow", 1, 1);
        if (m.captain && m.raidId == 0 && m.lastHurtByPlayer)
            drop("ominous_bottle", 1, 1); // (a captain's: M24.5 raids)
        break;
    // Water mobs (M25.2; wiki): each fish itself (cod cooked when burning, 5% bone meal
    // from cod, salmon and pufferfish), squid 1-3 ink sacs, glow squid 1-3 glow ink sacs.
    case MobType::Cod:
        drop(burning ? "cooked_cod" : "cod", 1, 1);
        if (ctx.rng.nextInt(20) == 0) drop("bone_meal", 1, 1);
        break;
    case MobType::Salmon:
        drop(burning ? "cooked_salmon" : "salmon", 1, 1);
        if (ctx.rng.nextInt(20) == 0) drop("bone_meal", 1, 1);
        break;
    case MobType::TropicalFish:
        drop("tropical_fish", 1, 1);
        if (ctx.rng.nextInt(20) == 0) drop("bone_meal", 1, 1);
        break;
    case MobType::Pufferfish:
        drop("pufferfish", 1, 1);
        if (ctx.rng.nextInt(20) == 0) drop("bone_meal", 1, 1);
        break;
    case MobType::Squid:
        drop("ink_sac", 1, 3);
        break;
    // (wiki: Guardian - 0-2 prismarine shards, and 40% a raw cod or 0-1 prismarine
    // crystals otherwise; Elder Guardian - also a wet sponge when killed by a player)
    case MobType::Guardian:
    case MobType::ElderGuardian:
        // (guardian: 2/5 cod, 2/5 a crystal, 1/5 nothing; elder: 3/6, 2/6, 1/6 - review)
        drop("prismarine_shard", 0, 2);
        {
            const bool elder = m.type == MobType::ElderGuardian;
            const uint32_t r = ctx.rng.nextInt(elder ? 6 : 5);
            if (r < (elder ? 3u : 2u))
                drop(burning ? "cooked_cod" : "cod", 1, 1);
            else if (r < (elder ? 5u : 4u))
                drop("prismarine_crystals", 1, 1);
        }
        if (m.type == MobType::ElderGuardian && m.lastHurtByPlayer) {
            drop("wet_sponge", 1, 1);
            if (ctx.rng.nextInt(5) == 0) drop("tide_armor_trim_smithing_template", 1, 1); // (20%)
        }
        break;
    case MobType::Dolphin:
        drop(burning ? "cooked_cod" : "cod", 0, 1);
        break; // (wiki: Dolphin)
    case MobType::Turtle:
        drop("seagrass", 0, 2);
        break;           // (wiki: Turtle - 0-2 seagrass)
    case MobType::Horse: // (M26.2; wiki: Horse, Donkey, Mule, Llama - 0-2 leather; camels nothing)
    case MobType::Donkey:
    case MobType::Mule:
    case MobType::Llama:
    case MobType::TraderLlama:
        drop("leather", 0, 2);
        break;
    // (M26.3; wiki) rabbit: hide 0-1, meat 0-1, a rabbit's foot 10% (+3% a Looting level)
    // for player kills; polar bear: cod 0-2 (3 in 4) or salmon 0-2; panda: bamboo 1 (Java); cat:
    // string 0-2; parrot: feathers 1-2.
    case MobType::Rabbit:
        drop("rabbit_hide", 0, 1);
        drop(burning ? "cooked_rabbit" : "rabbit", 0, 1);
        if (m.lastHurtByPlayer && int(ctx.rng.nextInt(100)) < 10 + 3 * m.looting)
            drop("rabbit_foot", 1, 1);
        break;
    case MobType::PolarBear:
        if (ctx.rng.nextInt(4) != 0)
            drop(burning ? "cooked_cod" : "cod", 0, 2);
        else
            drop(burning ? "cooked_salmon" : "salmon", 0, 2);
        break;
    case MobType::Panda:
        drop("bamboo", 1, 1);
        break;
    case MobType::Cat:
        drop("string", 0, 2);
        break;
    case MobType::Parrot:
        drop("feather", 1, 2);
        break;
    // (M26.4a; wiki) wither skeleton: coal 0-1 (1 in 3), bones 0-2; phantom: a membrane
    // 0-1 for player kills.
    case MobType::WitherSkeleton:
        if (ctx.rng.nextInt(3) == 0) drop("coal", 1, 1);
        drop("bone", 0, 2);
        // its skull: 2.5% (+1% a Looting level) for player kills (wiki: Wither Skeleton Skull)
        if (m.lastHurtByPlayer && ctx.rng.nextInt(1000) < 25u + 10u * m.looting &&
            m.chargedBlast == 0)
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0),
                            {*itemRegistry().find("wither_skeleton_skull"), 1}, ctx.rng);
        break;
    case MobType::Phantom:
        if (m.lastHurtByPlayer) drop("phantom_membrane", 0, 1);
        break;
    case MobType::CopperGolem: // (M26.5b; wiki: 1-3 copper ingots, and what it carried)
        drop("copper_ingot", 1, 3);
        if (m.mouthItem != kNoItem && m.allayCount > 0)
            ctx.items.spawn(m.pos, {m.mouthItem, m.allayCount}, ctx.rng);
        break;
    case MobType::Allay: // (M26.5a) what it held and carried
        if (m.mouthItem != kNoItem) {
            ctx.items.spawn(m.pos, {m.mouthItem, 1}, ctx.rng);
            if (m.allayCount > 0) ctx.items.spawn(m.pos, {m.mouthItem, m.allayCount}, ctx.rng);
        }
        break;
    case MobType::PiglinBrute: // (M29.1c; wiki) its golden axe 8.5% (killed by the player)
        if (m.lastHurtByPlayer && ctx.rng.nextInt(1000) < 85) drop("golden_axe", 1, 1);
        break;
    case MobType::Zoglin: // (wiki) rotten flesh 1-3
        drop("rotten_flesh", 1, 3);
        break;
    case MobType::SnowGolem: // (wiki) snowballs 0-15
        drop("snowball", 0, 15);
        break;
    case MobType::Mooshroom: // (as a cow)
        drop(burning ? "cooked_beef" : "beef", 1, 3);
        drop("leather", 0, 2);
        break;
    case MobType::SkeletonHorse: // (M29.1b; wiki) bones 0-2
        drop("bone", 0, 2);
        break;
    case MobType::ZombieHorse: // rotten flesh 2-3; camel husks 2-3; zombie nautiluses 0-3
    case MobType::CamelHusk:
        drop("rotten_flesh", 2, 3);
        break;
    case MobType::ZombieNautilus:
        drop("rotten_flesh", 0, 3);
        break;
    case MobType::Nautilus: // (M26.5a; wiki: a nautilus shell 5%, +1% a Looting level)
        if (ctx.rng.nextInt(100) < 5u + m.looting) drop("nautilus_shell", 1, 1);
        break;
    case MobType::Breeze: // (M26.4c; wiki: 1-2 breeze rods for player kills)
        if (m.lastHurtByPlayer) drop("breeze_rod", 1, 2);
        break;
    case MobType::Warden:
        drop("sculk_catalyst", 1, 1);
        break;            // (M27.3c; wiki: Warden)
    case MobType::Wither: // (M26.4b; wiki: the nether star, always)
        ctx.items.spawn(m.pos + glm::dvec3(0, 1.5, 0), {*itemRegistry().find("nether_star"), 1},
                        ctx.rng);
        break;
    case MobType::GlowSquid:
        drop("glow_ink_sac", 1, 3);
        break;
    case MobType::IronGolem: // wiki: Iron Golem - 3-5 iron ingots, 0-2 poppies
        drop("iron_ingot", 3, 5);
        drop("poppy", 0, 2);
        break;
    case MobType::Sheep: // wiki: Sheep - its wool unless sheared, 1-2 mutton
        if (!m.sheared)
            ctx.items.spawn(
                m.pos + glm::dvec3(0, 0.5, 0),
                {items.blockItem(static_cast<BlockId>(blocks::WhiteWool + m.woolColour)), 1},
                ctx.rng);
        drop(burning ? "cooked_mutton" : "mutton", 1, 2);
        break;
    case MobType::Pig:
        drop(burning ? "cooked_porkchop" : "porkchop", 1, 3);
        break;              // wiki: Pig
    case MobType::Skeleton: // wiki: Skeleton - bones 0-2, arrows 0-2
    case MobType::Stray:    // (M29.1a) and its variants, plus 0-1 of their tipped arrow
    case MobType::Bogged:   // (killed by the player)
    case MobType::Parched:
        drop("bone", 0, 2);
        drop("arrow", 0, 2);
        if (m.type != MobType::Skeleton && m.lastHurtByPlayer && ctx.rng.nextInt(2) == 0) {
            ItemStack tipped{*items.find("tipped_arrow"), 1};
            tipped.potion = uint8_t(m.type == MobType::Stray    ? Potion::Slowness
                                    : m.type == MobType::Bogged ? Potion::Poison
                                                                : Potion::Weakness);
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), tipped, ctx.rng);
        }
        break;
    case MobType::Creeper: // wiki: Creeper - gunpowder 0-2; killed by a skeleton's arrow, a music
                           // disc
        drop("gunpowder", 0, 2);
        if (m.lastHurtBySkeleton) {
            static constexpr const char* kDiscs[12] = {
                "music_disc_13",    "music_disc_cat",  "music_disc_blocks",  "music_disc_chirp",
                "music_disc_far",   "music_disc_mall", "music_disc_mellohi", "music_disc_stal",
                "music_disc_strad", "music_disc_ward", "music_disc_11",      "music_disc_wait"};
            drop(kDiscs[ctx.rng.nextInt(12)], 1, 1);
        }
        break;
    case MobType::Spider: // wiki: Spider - string 0-2, spider eye 1 in 3
    case MobType::CaveSpider:
        drop("string", 0, 2);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(3) == 0)
            drop("spider_eye", 1, 1); // player kills only
        break;
    case MobType::Enderman: // wiki: Enderman - ender pearl 0-1, and the block it carried
        drop("ender_pearl", 0, 1);
        if (m.carried)
            if (const ItemId it = items.blockItem(blockRegistry().blockOf(m.carried)))
                ctx.items.spawn(m.pos + glm::dvec3(0, 1, 0), {it, 1}, ctx.rng);
        break;
    case MobType::Shulker: // wiki: Shulker - a shell 50% + 6.25% per Looting level, never more than
                           // one
        if (ctx.rng.nextFloat() < 0.5f + 0.0625f * float(m.lastHurtByPlayer ? m.looting : 0)) {
            static const ItemId shell = *items.find("shulker_shell");
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {shell, 1}, ctx.rng);
        }
        break;
    // Nether mobs (M19.2; wiki): ghast - gunpowder 0-2, ghast tear 0-1; blaze - a blaze
    // rod 0-1 for player kills; magma cube - magma cream 25% (not the smallest), and
    // 2-4 cubes of half its size; zombified piglin - rotten flesh 0-1, gold nugget
    // 0-1, a gold ingot 2.5% for player kills.
    case MobType::Ghast:
        drop("gunpowder", 0, 2);
        drop("ghast_tear", 0, 1);
        break;
    case MobType::Blaze:
        if (m.lastHurtByPlayer) drop("blaze_rod", 0, 1);
        break;
    case MobType::MagmaCube:
        if (m.size > 1) {
            if (ctx.rng.nextInt(4) == 0) drop("magma_cream", 1, 1);
            const int n = 2 + static_cast<int>(ctx.rng.nextInt(3));
            for (int i = 0; i < n; ++i) {
                MobData child = make(
                    MobType::MagmaCube,
                    m.pos + glm::dvec3(ctx.rng.nextDouble() - 0.5, 0.2, ctx.rng.nextDouble() - 0.5),
                    ctx.rng);
                child.size = uint8_t(m.size / 2);
                child.health = float(child.size * child.size);
                m_births.push_back(child);
            }
        }
        break;
    case MobType::Slime: // wiki: Slime - the smallest drop 0-2 slime balls; bigger ones split
        if (m.size <= 1) {
            drop("slime_ball", 0, 2);
        } else {
            const int n = 2 + static_cast<int>(ctx.rng.nextInt(3));
            for (int i = 0; i < n; ++i) {
                MobData child = make(
                    MobType::Slime,
                    m.pos + glm::dvec3(ctx.rng.nextDouble() - 0.5, 0.2, ctx.rng.nextDouble() - 0.5),
                    ctx.rng);
                child.size = uint8_t(m.size / 2);
                child.health = float(child.size * child.size);
                m_births.push_back(child);
            }
        }
        break;
    case MobType::Piglin:
        break;            // (drops only what it carries: nothing we model)
    case MobType::Hoglin: // wiki: Hoglin - porkchop 2-4 (cooked when burning), leather 0-1
        drop(burning ? "cooked_porkchop" : "porkchop", 2, 4);
        drop("leather", 0, 1);
        break;
    case MobType::Strider:
        drop("string", 2, 5);
        break; // wiki: Strider
    case MobType::ZombifiedPiglin:
        drop("rotten_flesh", 0, 1);
        drop("gold_nugget", 0, 1);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(40) == 0) drop("gold_ingot", 1, 1);
        break;
    case MobType::Chicken: // wiki: Chicken - feathers 0-2, 1 raw chicken
        drop("feather", 0, 2);
        drop(burning ? "cooked_chicken" : "chicken", 1, 1);
        break;
    default:
        break;
    }
}

void Mobs::tick(Context& ctx) {
    ++m_tickCount; // (staggered checks; never the game clock)
    m_searches = 0;
    m_moves.clear();
    m_births.clear();
    if (m_playerAttacker != 0 && ++m_playerAttackerTicks > 100) m_playerAttacker = 0;
    m_hostiles = 0;
    m_bats = 0;
    m_fish = m_squid = m_glowSquid = m_axolotls = 0;
    m_felinesLastTick = m_felines;
    m_felines = 0;
    m_creatures = m_cats = 0;
    m_striders = 0;
    m_angerAlertCount = 0;
    m_bossHealth = -1.0f;
    m_dragonDeaths.clear();
    m_trapBolts.clear();
    const glm::dvec3 playerPos = ctx.player.position();
    const ChunkPos playerChunk{blockToChunk(int(std::floor(playerPos.x))),
                               blockToChunk(int(std::floor(playerPos.z)))};
    int riders = 0; // (M29 review: the jockey pass only runs when something rides)
    ctx.world.forEachTickingChunk([&](Chunk& chunk) {
        // Only chunks within the simulation distance tick their mobs (vanilla: entity-
        // ticking chunks); farther mobs keep their state.
        if (std::abs(chunk.pos().x - playerChunk.x) > m_simulationDistance ||
            std::abs(chunk.pos().z - playerChunk.z) > m_simulationDistance)
            return;
        if (!chunk.spawners().empty()) tickSpawners(ctx, chunk);
        if (!chunk.beehives().empty()) tickHives(ctx, chunk); // (M26.3b: bees leaving their hives)
        auto& mobs = chunk.mobs();
        for (size_t i = 0; i < mobs.size();) {
            MobData& m = mobs[i];
            riders += m.vehicle != 0;
            m.prevPos = m.pos;
            m.prevYaw = m.yaw;
            m.prevHeadYaw = m.headYaw;
            m.prevPitch = m.pitch;
            if (m.chargedBlast > 0 && m.health > 0.0f) --m.chargedBlast; // (M26.4b)
            // Wolf armor (M26.3; wiki: Wolf Armor) takes the damage the wolf took since the
            // last tick, wearing down until it breaks at 64.
            if (m.type == MobType::Wolf) {
                if (m.horseArmor > 0 && m.health < m.lastHealth && m.lastHealth > 0.0f) {
                    m.armorWear = int16_t(m.armorWear + int(std::ceil(m.lastHealth - m.health)));
                    m.health = m.lastHealth;
                    m.deathTime = 0;
                    if (m.armorWear >= 64) {
                        m.horseArmor = 0;
                        m.armorWear = 0;
                        ctx.world.playSound(Sound::ToolBreak, m.pos.x, m.pos.y + 0.5, m.pos.z);
                    }
                }
                m.lastHealth = m.health;
            }
            // (M27 review) whatever hurt them - arrows, explosions, fire, golems - a creaking
            // whose heart stands and a warden still emerging or digging take no damage.
            if (m.type == MobType::Creaking || m.type == MobType::Warden) {
                const bool shielded =
                    m.type == MobType::Warden
                        ? m.phase != 1
                        : m.home.y != kNoPoint &&
                              blockRegistry().blockOf(ctx.world.getBlock(
                                  {m.home.x, m.home.y, m.home.z})) == blocks::CreakingHeart;
                if (shielded && m.deathTime < 19 && m.lastHealth > 0.0f &&
                    m.health < m.lastHealth) {
                    m.health = m.lastHealth;
                    m.deathTime = 0;
                }
                m.lastHealth = m.health;
            }
            // Sounds (M22.4): hurt since last tick (hurtTime was set to 10), and now and
            // then its ambient call - vanilla: 1/1000 chance growing each tick, then 80
            // ticks of quiet. Babies squeak half an octave higher.
            if (m.hurtTime == 10 && m.health > 0.0f)
                ctx.world.playSound(mobSound(m.type, MobSound::Hurt), m.pos.x,
                                    m.pos.y + mobInfo(m.type).height * 0.5, m.pos.z, 1.0f,
                                    m.isBaby() ? 1.5f : 1.0f);
            // (decorations - frames, paintings, stands, knots - never call out: no roll, no event)
            const bool silent =
                isHanging(m.type) || m.type == MobType::ArmorStand || m.type == MobType::LeashKnot;
            if (!silent && m.health > 0.0f &&
                int(m_soundRng.nextInt(1000)) < m.ambientTime++) { // (own RNG: gameplay unchanged)
                m.ambientTime = -80;
                ctx.world.playSound(mobSound(m.type, MobSound::Ambient), m.pos.x,
                                    m.pos.y + mobInfo(m.type).height * 0.85, m.pos.z, 1.0f,
                                    m.isBaby() ? 1.5f : 1.0f);
            }
            if (m.hurtTime > 0 && --m.hurtTime == 0)
                m.lastHurtAmount = 0.0f; // (M32 review: the window is over)
            bool remove = false;
            if (m.angerAlert) { // (also from one killed by the hit)
                m.angerAlert = false;
                if (m_angerAlertCount < int(m_angerAlerts.size()))
                    m_angerAlerts[size_t(m_angerAlertCount++)] = {m.pos, m.type};
            }
            if (m.type == MobType::EnderDragon ||
                m.type == MobType::Wither) { // (M26.4b: the Wither's bar too)
                m_bossHealth = std::max(0.0f, m.health);
                m_bossType = m.type;
            }
            if (m.health <= 0.0f && m.type == MobType::EnderDragon) {
                // The dragon rises slowly for 10 s, then is gone (wiki: Ender Dragon -
                // its death animation; experience, portal and egg: main).
                ++m.deathTime;
                m.phase = 9;
                m.vel = glm::dvec3(0.0);
                m.pos.y += 0.1;
                // It dies over the exit portal (vanilla flies there first; ours drifts
                // there while rising), so its experience lands on the island.
                const glm::dvec2 toMiddle(0.5 - m.pos.x, 0.5 - m.pos.z);
                const double dm = glm::length(toMiddle);
                if (dm > 0.5 && dm < 150.0) {
                    const glm::dvec2 step = toMiddle / dm * std::min(dm, 1.0);
                    m.pos.x += step.x;
                    m.pos.z += step.y;
                }
                if (m.deathTime >= 200) {
                    remove = true;
                    if (m_dragonDeaths.size() < m_dragonDeaths.capacity())
                        m_dragonDeaths.push_back(m.pos);
                    if (m_killCount < int(m_kills.size()))
                        m_kills[size_t(m_killCount++)] = m.type; // (M28.5c)
                }
            } else if (m.health <= 0.0f) { // loot at the moment of death, then the death animation
                if (++m.deathTime == 1) {
                    die(ctx, m);
                    ctx.world.playSound(mobSound(m.type, MobSound::Death), m.pos.x,
                                        m.pos.y + mobInfo(m.type).height * 0.5, m.pos.z, 1.0f,
                                        m.isBaby() ? 1.5f : 1.0f);
                }
                if (m.deathTime >= 20) {
                    remove = true;
                    // The poof of smoke when the body vanishes (vanilla: 20 particles).
                    if (m.type != MobType::Minecart && m.type != MobType::EndCrystal &&
                        m.type != MobType::Boat && !m.vanish && !isHanging(m.type) &&
                        m.type != MobType::ArmorStand && m.type != MobType::LeashKnot)
                        ctx.world.levelEvent(LevelEvent::Type::MobDeath, m.pos.x, m.pos.y, m.pos.z,
                                             uint32_t(mobInfo(m.type).width * 100.0f) |
                                                 uint32_t(mobInfo(m.type).height * 100.0f) << 16);
                }
            } else {
                tickMobEffects(ctx, m); // (M29.2c)
                ai(ctx, m);
                if (m.leash != 0) leashTick(ctx, m);               // (M28.3c)
                if (m.type == MobType::Llama) caravanTick(ctx, m); // (M28.3c)
                if (mobInfo(m.type).hostile) ++m_hostiles;
                m_bats += m.type == MobType::Bat;
                m_fish += isFish(m.type);
                m_squid += m.type == MobType::Squid || m.type == MobType::Dolphin;
                m_creatures += (m.type == MobType::Wolf || m.type == MobType::Ocelot ||
                                m.type == MobType::Parrot ||
                                (isMount(m.type) && m.type != MobType::TraderLlama) ||
                                isWildlife(m.type) || m.type == MobType::Mooshroom ||
                                ((m.type == MobType::Cow || m.type == MobType::Sheep ||
                                  m.type == MobType::Pig || m.type == MobType::Chicken) &&
                                 !m.persistent)) && // (M32 review: the farm animals too)
                               !m.tamed;
                m_cats += m.type == MobType::Cat;
                m_glowSquid += m.type == MobType::GlowSquid;
                m_axolotls += m.type == MobType::Axolotl;
                m_felines += m.type == MobType::Cat || m.type == MobType::Ocelot;
                m_striders += m.type == MobType::Strider;
                // Despawning (wiki: Spawn › Despawning): hostiles beyond 128 blocks
                // vanish; beyond 32 they may after 30 s without a player near.
                const double d2 = glm::dot(m.pos - playerPos, m.pos - playerPos);
                // Water mobs too (wiki): squid as monsters, fish beyond 64 blocks; never
                // fish from a bucket.
                if ((mobInfo(m.type).hostile || mobInfo(m.type).swims) && !m.persistent &&
                    !m.fromBucket) {
                    if (d2 > (isFish(m.type) ? 64.0 * 64.0 : 128.0 * 128.0))
                        remove = true;
                    else if (d2 > 32.0 * 32.0 && ++m.noPlayerTicks > 600 &&
                             ctx.rng.nextInt(800) == 0)
                        remove = true;
                    else if (d2 <= 32.0 * 32.0)
                        m.noPlayerTicks = 0;
                }
                if (m.pos.y < ctx.world.height().minY - 64) remove = true; // fell out of the world
                if (ctx.difficulty == 0 && despawnsInPeaceful(m.type)) remove = true; // (M28.1b)
            }
            const ChunkPos now{blockToChunk(int(std::floor(m.pos.x))),
                               blockToChunk(int(std::floor(m.pos.z)))};
            if (!remove && !(now == chunk.pos())) {
                if (ctx.world.chunk(now)) {
                    m_moves.push_back({now, chunk.pos(), m});
                    remove = true;
                } else { // the next chunk isn't loaded: stay at the border
                    m.pos = m.prevPos;
                    m.vel = glm::dvec3(0.0);
                }
            }
            // Saved state changed: the chunk needs saving (idle mobs don't dirty it).
            if (remove || m.pos != m.prevPos || m.yaw != m.prevYaw || m.hurtTime > 0 ||
                m.fireTicks > 0 || m.deathTime > 0)
                chunk.markDirty();
            if (remove) {
                if (m.hasGear) chunk.removeMobStore(m.uuidHi); // (M32.2c: despawned with its gear)
                mobs[i] = mobs.back();
                mobs.pop_back();
            } else {
                ++i;
            }
        }
    });
    for (const Move& mv : m_moves)
        if (Chunk* c = ctx.world.chunk(mv.to)) {
            c->mobs().push_back(mv.mob);
            // A mount's or chest boat's chest goes with it (M26.2).
            if (mv.mob.hasChest || mv.mob.hasGear) // (M32.2c: a monster's gear too)
                if (Chunk* from = ctx.world.chunk(mv.from))
                    if (ItemContents* slots = from->mobStore(mv.mob.uuidHi)) {
                        // (M32 review: merged - gear picked up this tick may already sit in the new
                        // chunk)
                        ItemContents& to = c->addMobStore(mv.mob.uuidHi);
                        for (size_t k = 0; k < to.size(); ++k)
                            if (to[k].empty()) to[k] = (*slots)[k];
                        from->removeMobStore(mv.mob.uuidHi);
                        from->markDirty();
                    }
            c->markDirty();
            ctx.world.markTicking(mv.to);
        }
    for (const MobData& baby : m_births)
        add(ctx.world, baby);
    // Mobs that came out of blocks during the pass (bees from broken hives - M26.5 review).
    for (const MobData& q : ctx.world.queuedMobs())
        add(ctx.world, q);
    ctx.world.queuedMobs().clear();
    for (const MobData& b : m_births)
        riders += b.vehicle != 0; // (jockeys spawned this tick)
    if (riders > 0) ridePass(ctx);
    ctx.world.vibrations().clear(); // (M27.3c: heard by the wardens this pass)
    // A hit zombified piglin angers the others around it (wiki: Zombified Piglin -
    // within about 33 blocks across and 11 up/down; 20-55 s of anger).
    for (int a = 0; a < m_angerAlertCount; ++a) {
        const glm::dvec3 hit = m_angerAlerts[size_t(a)].pos;
        const MobType kind = m_angerAlerts[size_t(a)].type;
        const ChunkPos hc{blockToChunk(int(std::floor(hit.x))),
                          blockToChunk(int(std::floor(hit.z)))};
        for (int dz = -3; dz <= 3; ++dz)
            for (int dx = -3; dx <= 3; ++dx)
                if (Chunk* c = ctx.world.chunk({hc.x + dx, hc.z + dz}))
                    for (MobData& o : c->mobs())
                        if (o.type == MobType::ZombifiedPiglin &&
                            kind == MobType::ZombifiedPiglin && std::abs(o.pos.x - hit.x) < 33.5 &&
                            std::abs(o.pos.z - hit.z) < 33.5 && std::abs(o.pos.y - hit.y) < 11.0) {
                            o.angry = true;
                            o.angerTicks = static_cast<int16_t>(400 + ctx.rng.nextInt(701));
                        } else if (o.type == MobType::Wolf && kind == MobType::Wolf && !o.tamed &&
                                   std::abs(o.pos.x - hit.x) < 16.5 &&
                                   std::abs(o.pos.z - hit.z) < 16.5 &&
                                   std::abs(o.pos.y - hit.y) < 10.0) {
                            // (M32.6; wiki: Wolf › Behavior) the pack joins in
                            o.angry = true;
                            o.angerTicks = 600;
                            o.targeting = true;
                        } else if (o.type == MobType::Bee && kind == MobType::Bee && !o.stung &&
                                   glm::length(o.pos - hit) < 20.0) {
                            o.angry = true; // (bees within 20 blocks - our reading of the wiki's
                                            // "nearby")
                            o.angerTicks = static_cast<int16_t>(400 + ctx.rng.nextInt(400));
                        }
    }
    if (ctx.naturalSpawning) {
        if (ctx.world.isUltrawarm())
            spawnNether(ctx);
        else if (ctx.difficulty > 0)
            spawnHostiles(ctx);                       // (Peaceful: no monsters)
        if (ctx.world.hasSkyLight()) spawnWater(ctx); // (M25.2: the Overworld's water)
        if (ctx.world.hasSkyLight() && !ctx.world.isUltrawarm()) spawnBats(ctx);      // (M29.1c)
        if (ctx.world.hasSkyLight() && !ctx.world.isUltrawarm()) spawnCreatures(ctx); // (M26.1)
        if (ctx.world.hasSkyLight() && !ctx.world.isUltrawarm()) spawnPhantoms(ctx);  // (M26.4a)
    }
}

void Mobs::tickSpawners(Context& ctx, Chunk& chunk) {
    // Monster spawners (wiki: Monster Spawner): active while the player is within 16
    // blocks of the block's centre; when the delay runs out, 4 tries at random spots
    // within 4 blocks across and 1 up/down (9x3x9) - in the air or not, but with room
    // for the mob and block light 11 or less (our reading for 1.21's monsters); at
    // most 6 of its kind in the 9x9x9 around it. After a spawn: 200-799 ticks; with no
    // spot found it tries again next tick. Mobs join after this tick's loop (m_births).
    const glm::dvec3 playerPos = ctx.player.position();
    for (auto& e : chunk.spawners()) {
        const BlockPos p{chunk.pos().x * 16 + e.x, e.y, chunk.pos().z * 16 + e.z};
        const glm::dvec3 centre(p.x + 0.5, p.y + 0.5, p.z + 0.5);
        if (e.data.trial) {
            tickTrialSpawner(ctx, chunk, p, e.data);
            continue;
        }
        if (ctx.playerDead || glm::dot(playerPos - centre, playerPos - centre) > 16.0 * 16.0)
            continue;
        SpawnerData& s = e.data;
        chunk.markDirty(); // its delay is saved
        if (s.delay > 0) {
            --s.delay;
            continue;
        }
        const Aabb area{centre - glm::dvec3(4.5), centre + glm::dvec3(4.5)};
        int nearby = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (const Chunk* c = ctx.world.chunk({chunk.pos().x + dx, chunk.pos().z + dz}))
                    for (const MobData& m : c->mobs())
                        nearby += m.type == s.mob && m.health > 0.0f && box(m).intersects(area);
        for (const MobData& m : m_births)
            nearby += m.type == s.mob && box(m).intersects(area);
        if (nearby >= 6) {
            s.delay = static_cast<int16_t>(200 + ctx.rng.nextInt(600));
            continue;
        }
        bool spawned = false;
        for (int i = 0; i < 4; ++i) {
            const double x = p.x + (ctx.rng.nextDouble() - ctx.rng.nextDouble()) * 4.0 + 0.5;
            const int y = p.y + static_cast<int>(ctx.rng.nextInt(3)) - 1;
            const double z = p.z + (ctx.rng.nextDouble() - ctx.rng.nextDouble()) * 4.0 + 0.5;
            const int bx = int(std::floor(x)), bz = int(std::floor(z));
            if (!ctx.world.isInHeight(y) || !ctx.world.isInHeight(y + 2)) continue;
            const int tall = s.mob == MobType::Enderman ? 3 : 2;
            bool room = true;
            for (int dy = 0; dy < tall && room; ++dy) {
                const BlockStateId b = ctx.world.getBlock({bx, y + dy, bz});
                const BlockId id = blockRegistry().blockOf(b);
                room = !blockRegistry().collides(b) && id != blocks::Water && id != blocks::Lava;
            }
            const Chunk* c = ctx.world.chunk({blockToChunk(bx), blockToChunk(bz)});
            if (!room || !c || !c->lit() ||
                c->blockLight(blockToLocal(bx), y, blockToLocal(bz)) > 11)
                continue;
            MobData m = make(s.mob, {x, double(y), z}, ctx.rng);
            m_births.push_back(m);
            spawned = true;
        }
        if (spawned) s.delay = static_cast<int16_t>(200 + ctx.rng.nextInt(600));
    }
}

void Mobs::spawnHostiles(Context& ctx) {
    // Vanilla's spawn cycle (M32.1; wiki: Mob spawning › Java Edition): every tick
    // each chunk within 8 chunks of the player (loaded and lit) gets one try at a random spot
    // between the bottom of the world and just above its surface; the monster cap 70 is scaled
    // by those chunks (70 x chunks / 289), so caves and the surface fill alike.
    const glm::dvec3 p = ctx.player.position();
    const ChunkPos pc{blockToChunk(int(std::floor(p.x))), blockToChunk(int(std::floor(p.z)))};
    int eligible = 0;
    for (int dz = -8; dz <= 8; ++dz)
        for (int dx = -8; dx <= 8; ++dx)
            if (const Chunk* c = ctx.world.chunk({pc.x + dx, pc.z + dz}); c && c->lit()) ++eligible;
    m_monsterCap = 70 * eligible / 289;
    if (m_hostiles >= m_monsterCap) return;
    const int minY = ctx.world.height().minY;
    const int start = int(ctx.rng.nextInt(289)); // (no chunk always first)
    for (int k = 0; k < 289 && m_hostiles < m_monsterCap; ++k) {
        const int i = (start + k) % 289;
        const ChunkPos cp{pc.x + i % 17 - 8, pc.z + i / 17 - 8};
        const Chunk* c = ctx.world.chunk(cp);
        if (!c || !c->lit()) continue;
        const int x = cp.x * 16 + int(ctx.rng.nextInt(16)),
                  z = cp.z * 16 + int(ctx.rng.nextInt(16));
        const int top = rainHeight(ctx.world, x, z) + 1;
        const int y = minY + int(ctx.rng.nextInt(uint32_t(std::max(1, top - minY + 1))));
        spawnMonsterPacks(ctx, x, y, z);
    }
}

void Mobs::spawnMonsterPacks(Context& ctx, int x0, int y, int z0) {
    // Up to 3 packs from the spot (vanilla spawnCategoryForPosition): each wanders its own way
    // (x and z move by random(6) - random(6) per member), up to 4 members, the kind chosen at
    // the first spot that works; members need 24+ blocks to the player (and within 128).
    if (!ctx.world.isInHeight(y) || !ctx.world.isInHeight(y + 2)) return;
    if (blockRegistry().collides(ctx.world.getBlock({x0, y, z0})))
        return; // (vanilla: a solid start spot)
    const glm::dvec3 p = ctx.player.position();
    for (int pack = 0; pack < 3; ++pack) {
        int x = x0, z = z0;
        MobType kind = MobType::Count;
        int spawned = 0, packMax = 4;
        // (M32 review; wiki: Mob spawning - "up to four" spots a pack)
        const int tries = 1 + int(ctx.rng.nextInt(4));
        for (int member = 0; member < tries && spawned < packMax && m_hostiles < m_monsterCap;
             ++member) {
            x += int(ctx.rng.nextInt(6)) - int(ctx.rng.nextInt(6));
            z += int(ctx.rng.nextInt(6)) - int(ctx.rng.nextInt(6));
            const double dx = x + 0.5 - p.x, dy = y - p.y, dz = z + 0.5 - p.z;
            const double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < 24.0 * 24.0 || d2 > 128.0 * 128.0) continue;
            if (spawnMonsterAt(ctx, x, y, z, kind, member == 0 || spawned == 0)) ++spawned;
            if (kind == MobType::Witch || kind == MobType::ZombieVillager)
                packMax = 1; // (slimes come in fours)
        }
    }
}

bool Mobs::spawnMonsterAt(Context& ctx, int x, int y, int z, MobType& kind, bool first) {
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit()) return false;
    if (!canSpawnAt(ctx.world, x, y, z)) return false;
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    const Biome biome =
        c->biomes() ? c->biomes()->at(lx, y, lz, ctx.world.height()) : Biome::Plains;
    // No monsters spawn in mushroom fields (wiki: Mushroom Fields); spawners still work.
    // Nor in the deep dark (M27.3b; wiki: Deep Dark).
    if (biome == Biome::MushroomFields || biome == Biome::DeepDark) return false;
    const bool end = !ctx.world.hasSkyLight();
    // The kind, chosen once per pack (vanilla: the biome's monster list) - zombie 95,
    // zombie villager 5, skeleton 100, creeper 100, spider 100, slime 100, enderman 10,
    // witch 5 (wiki: Spawn); the End: endermen only.
    if (kind == MobType::Count) {
        const uint32_t roll = ctx.rng.nextInt(515);
        kind = end          ? MobType::Enderman
               : roll < 95  ? MobType::Zombie
               : roll < 100 ? MobType::ZombieVillager
               : roll < 200 ? MobType::Skeleton
               : roll < 300 ? MobType::Creeper
               : roll < 400 ? MobType::Spider
               : roll < 500 ? MobType::Slime
               : roll < 510 ? MobType::Enderman
                            : MobType::Witch;
    }
    if (kind == MobType::Slime) {
        // Slimes (wiki: Slime › Spawning): in 1 chunk of 10 ("slime chunks", ours by seed)
        // below Y 40 at any light level; in swamps between Y 51 and 69 where the light is at
        // most a random 0..7 (night).
        const ChunkPos cp{blockToChunk(x), blockToChunk(z)};
        Xoroshiro sc(mixSeed(mixSeed(ctx.worldSeed ^ 0x3AD8025Full, static_cast<uint32_t>(cp.x)),
                             static_cast<uint32_t>(cp.z)));
        const bool slimeChunk = sc.nextInt(10) == 0 && y < 40;
        const int light = std::max<int>(c->blockLight(lx, y, lz),
                                        c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken));
        const bool swamp = y >= 51 && y <= 69 && light <= static_cast<int>(ctx.rng.nextInt(8)) &&
                           (biome == Biome::Swamp || biome == Biome::MangroveSwamp);
        if (!slimeChunk || !ctx.world.hasSkyLight()) {
            if (!swamp) return false;
        }
        return add(ctx.world, make(MobType::Slime, {x + 0.5, double(y), z + 0.5}, ctx.rng)) &&
               (++m_hostiles, true);
    }
    // Monsters need darkness: block light 0, sky light (dimmed by the time of day - in a
    // thunderstorm by at least 10, wiki: Weather) at most a random 0..7.
    if (c->blockLight(lx, y, lz) > 0) return false;
    const int darken = ctx.thundering ? std::max(10, static_cast<int>(ctx.skyDarken))
                                      : static_cast<int>(ctx.skyDarken);
    if (c->skyLight(lx, y, lz) - darken > static_cast<int>(ctx.rng.nextInt(8))) return false;
    if (kind == MobType::Enderman && solidAt(ctx.world, x, y + 2, z)) return false; // (3 tall)
    MobData mob = make(kind, {x + 0.5, double(y), z + 0.5}, ctx.rng);
    // Biome variants (M29.1a; wiki: Husk, Stray, Bogged, Parched): under the open sky 80% of
    // desert zombies are husks and 80% of snowy skeletons strays; half the desert skeletons
    // are parched and half the swamp skeletons bogged.
    if (kind == MobType::Zombie || kind == MobType::Skeleton) {
        const bool open = c->skyLight(lx, y, lz) >= 15;
        const bool snowy = biome == Biome::SnowyPlains || biome == Biome::IceSpikes ||
                           biome == Biome::FrozenRiver || biome == Biome::FrozenOcean ||
                           biome == Biome::DeepFrozenOcean;
        const uint32_t v = ctx.rng.nextInt(10);
        if (kind == MobType::Zombie && biome == Biome::Desert && open && v < 8)
            mob.type = MobType::Husk;
        if (kind == MobType::Skeleton && snowy && open && v < 8) mob.type = MobType::Stray;
        if (kind == MobType::Skeleton && biome == Biome::Desert && v < 5)
            mob.type = MobType::Parched;
        if (kind == MobType::Skeleton && (biome == Biome::Swamp || biome == Biome::MangroveSwamp) &&
            v < 5)
            mob.type = MobType::Bogged;
        if (mob.type != kind) mob.health = mobInfo(mob.type).maxHealth;
        // Jockeys (M29.1b; wiki: Zombie Horse, Camel Husk): zombie horsemen with iron spears on
        // plains, savannas and snowy plains (about 1 in 100 zombies); 1 in 10 husks rides a camel
        // husk with a parched behind.
        static const uint16_t ironSpear = uint16_t(*itemRegistry().find("iron_spear"));
        const bool horsemen = biome == Biome::Plains || biome == Biome::SunflowerPlains ||
                              biome == Biome::Savanna || biome == Biome::SavannaPlateau ||
                              biome == Biome::SnowyPlains;
        if (mob.type == MobType::Zombie && horsemen && open && first && ctx.rng.nextInt(100) == 0) {
            MobData horse = make(MobType::ZombieHorse, mob.pos, ctx.rng);
            mob.vehicle = horse.uuidHi;
            mob.heldItem = ironSpear;
            add(ctx.world, horse);
        }
        if (mob.type == MobType::Husk && first && ctx.rng.nextInt(10) == 0) {
            MobData camel = make(MobType::CamelHusk, mob.pos, ctx.rng);
            MobData back = make(MobType::Parched, mob.pos, ctx.rng);
            mob.vehicle = back.vehicle = camel.uuidHi;
            mob.heldItem = ironSpear;
            add(ctx.world, camel);
            if (add(ctx.world, back)) ++m_hostiles;
        }
    }
    if (isZombie(mob.type))
        zombieSpawnRolls(ctx, mob, clampedDifficultyAt(ctx, mob.pos.x, mob.pos.z));
    if (isSkeleton(mob.type))
        rollSpawnGear(ctx, mob, clampedDifficultyAt(ctx, mob.pos.x, mob.pos.z));
    // (M32.2c; wiki: Spider › Spawning) on Hard, 10% x the clamped regional difficulty
    // of spiders carry an endless Speed, Strength, Regeneration or Invisibility.
    if (isSpider(kind) && ctx.difficulty >= 3 &&
        ctx.rng.nextFloat() < float(0.1 * clampedDifficultyAt(ctx, mob.pos.x, mob.pos.z))) {
        // (wiki: Spider - Speed 2 in 5, Strength, Regeneration, Invisibility 1 in 5 each)
        static constexpr Effect kSpiderEffects[5] = {Effect::Speed, Effect::Speed, Effect::Strength,
                                                     Effect::Regeneration, Effect::Invisibility};
        addEffect(mob, kSpiderEffects[ctx.rng.nextInt(5)], 0, kInfiniteEffect);
    }
    // Spider jockeys (wiki: Spider Jockey): 1 in 100 spiders carries a skeleton.
    if (kind == MobType::Spider && ctx.rng.nextInt(100) == 0) {
        MobData rider = make(MobType::Skeleton, mob.pos, ctx.rng);
        rider.vehicle = mob.uuidHi;
        if (add(ctx.world, rider)) ++m_hostiles;
    }
    if (!add(ctx.world, mob)) return false;
    ++m_hostiles;
    return true;
}

double Mobs::clampedDifficultyAt(const Context& ctx, double x, double z) {
    const Chunk* c =
        ctx.world.chunk({blockToChunk(int(std::floor(x))), blockToChunk(int(std::floor(z)))});
    return clampedRegionalDifficulty(regionalDifficulty(
        ctx.difficulty, ctx.dayTime, c ? c->inhabitedTicks : 0, moonBrightness(ctx.dayTime)));
}

void Mobs::zombieSpawnRolls(Context& ctx, MobData& mob, double crd) {
    // (wiki: Zombie › Spawning) 5% are babies (they never grow up); 5% of those ride a
    // chicken (a chicken jockey). Drowned can be babies but don't ride.
    if (ctx.rng.nextFloat() < 0.05f) {
        mob.age = -24000;
        if (mob.type != MobType::Drowned && mob.vehicle == 0 && ctx.rng.nextFloat() < 0.05f) {
            MobData chicken = make(MobType::Chicken, mob.pos, ctx.rng);
            mob.vehicle = chicken.uuidHi;
            ctx.world.queueMob(
                chicken); // (M32 review: may run inside the mob pass - added after it)
        }
    }
    rollSpawnGear(ctx, mob, crd); // (M32.2c: CanPickUpLoot, armor, weapons, enchantments)
    // Door breakers: 10% at the highest clamped regional difficulty (they act on Hard only).
    mob.canBreakDoors = mob.type != MobType::Drowned && ctx.rng.nextFloat() < float(crd * 0.1);
    // The reinforcement chance: a random 0..0.1; leaders (5% at the highest difficulty) get
    // 0.5..0.75 more, 2-5x the health and always break doors.
    mob.reinforcements = float(ctx.rng.nextDouble() * 0.1);
    if (ctx.rng.nextFloat() < float(crd * 0.05)) {
        mob.reinforcements += float(ctx.rng.nextDouble() * 0.25 + 0.5);
        mob.maxHealth = mobInfo(mob.type).maxHealth * float(2.0 + ctx.rng.nextDouble() * 3.0);
        mob.health = mob.maxHealth;
        mob.canBreakDoors = mob.type != MobType::Drowned;
    }
}

void Mobs::zombieReinforcements(Context& ctx, MobData& m) {
    // (wiki: Zombie › Reinforcements) on Hard, a zombie hurt by the player calls another zombie
    // with its reinforcement chance - 50 tries at 7-40 blocks out each way, a dark spot a zombie
    // can stand in with no player within 7. Both then have 0.05 less chance from then on.
    m.callReinforcements = false;
    if (ctx.difficulty < 3 || !ctx.naturalSpawning || m.health <= 0.0f || ctx.playerDead) return;
    if (ctx.rng.nextFloat() >= m.reinforcements) return;
    const glm::dvec3 player = ctx.player.position();
    const int bx = int(std::floor(m.pos.x)), by = int(std::floor(m.pos.y)),
              bz = int(std::floor(m.pos.z));
    auto offset = [&] { // (vanilla: nextInt(7, 40) x nextInt(-1, 1))
        return (7 + int(ctx.rng.nextInt(34))) * (int(ctx.rng.nextInt(3)) - 1);
    };
    for (int tries = 0; tries < 50; ++tries) {
        const int x = bx + offset(), y = by + offset(), z = bz + offset();
        if (!ctx.world.isInHeight(y) || !canSpawnAt(ctx.world, x, y, z)) continue;
        const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
        if (!c || !c->lit()) continue;
        const int lx = blockToLocal(x), lz = blockToLocal(z);
        if (c->blockLight(lx, y, lz) > 0 ||
            c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken) >
                static_cast<int>(ctx.rng.nextInt(8)))
            continue;
        const glm::dvec3 at{x + 0.5, double(y), z + 0.5};
        if (glm::dot(at - player, at - player) < 7.0 * 7.0) continue;
        MobData z2 =
            make(MobType::Zombie, at, ctx.rng); // (vanilla: a plain zombie, whoever called)
        zombieSpawnRolls(ctx, z2, clampedDifficultyAt(ctx, at.x, at.z));
        z2.targeting = true; // (it comes for the player)
        z2.goal = player;
        z2.reinforcements = std::max(0.0f, z2.reinforcements - 0.05f);
        m.reinforcements = std::max(0.0f, m.reinforcements - 0.05f);
        ctx.world.queueMob(z2); // (added after the mob pass)
        ++m_hostiles;
        return;
    }
}

bool Mobs::spawnSkeletonTrap(World& world, const glm::dvec3& at, int difficulty, Xoroshiro& rng) {
    const uint32_t odds = difficulty <= 0   ? 0u
                          : difficulty == 1 ? 10u
                          : difficulty == 2 ? 25u
                                            : 45u; // per 1000
    if (rng.nextInt(1000) >= odds) return false;
    MobData h = make(MobType::SkeletonHorse, at, rng);
    h.skeletonTrap = true;
    h.persistent = true;
    return add(world, h);
}

void Mobs::addEffect(MobData& mob, Effect effect, int amplifier, int ticks) {
    if (mob.type == MobType::EnderDragon || mob.type == MobType::EndCrystal ||
        isHanging(mob.type) || mob.type == MobType::ArmorStand || mob.type == MobType::LeashKnot ||
        mob.type == MobType::Boat || mob.type == MobType::Minecart || effect == Effect::None ||
        effectInfo(effect).instant)
        return;
    if (isUndead(mob.type) && (effect == Effect::Poison || effect == Effect::Regeneration)) return;
    if ((mob.type == MobType::WitherSkeleton || mob.type == MobType::Wither) &&
        effect == Effect::Wither)
        return;
    MobData::ActiveEffect* free = nullptr;
    for (MobData::ActiveEffect& e : mob.effects) {
        if (e.type == uint8_t(effect) && e.ticks > 0) {
            if (amplifier > e.amplifier || (amplifier == e.amplifier && ticks > e.ticks))
                e = {uint8_t(effect), uint8_t(amplifier), int16_t(std::min(ticks, 32767))};
            return;
        }
        if (!free && e.ticks <= 0) free = &e;
    }
    if (free) *free = {uint8_t(effect), uint8_t(amplifier), int16_t(std::min(ticks, 32767))};
}

void Mobs::tickMobEffects(Context& ctx, MobData& m) {
    // (M29.4a) a wither rose withers what stands in it (not on Peaceful; addEffect spares
    // wither skeletons and the Wither - wiki: Wither (effect), M29 review)
    if (ctx.difficulty != 0 && blockRegistry().blockOf(ctx.world.getBlock(
                                   {int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 0.01)),
                                    int(std::floor(m.pos.z))})) == blocks::WitherRose)
        addEffect(m, Effect::Wither, 0, 40);
    // Lasting effects on mobs (M29.2c; wiki: each effect): poison down to 1, regeneration,
    // wither (may kill), levitation rising; speed/slowness and strength/weakness act where
    // the mob moves and hits.
    for (MobData::ActiveEffect& e : m.effects) {
        if (e.ticks <= 0) continue;
        const auto type = Effect(e.type);
        if (type == Effect::Poison && e.ticks % std::max(1, 25 >> e.amplifier) == 0 &&
            m.health > 1.0f) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        } else if (type == Effect::Wither && e.ticks % std::max(1, 40 >> e.amplifier) == 0 &&
                   m.hurtTime <= 0) { // (the hurt cooldown paces a rose's every-tick dose)
            m.health -= 1.0f;
            m.hurtTime = 10;
        } else if (type == Effect::Regeneration && e.ticks % std::max(1, 50 >> e.amplifier) == 0) {
            m.health = std::min(maxHealthOf(m), m.health + 1.0f);
        } else if (type == Effect::Levitation) {
            m.vel.y += (0.05 * double(e.amplifier + 1) - m.vel.y) * 0.2;
        } else if (type == Effect::FireResistance) {
            m.fireTicks = 0;
        }
        if (e.ticks < kInfiniteEffect) --e.ticks; // (M32.2c: a Hard spider's lasts)
    }
    (void)ctx;
}

void Mobs::ridePass(Context& ctx) {
    // Jockeys (M29.1b; wiki: Jockey): a rider sits on its mount's seat (two on a camel: the
    // second behind), the mount carries it and walks to the rider's goal while the rider
    // chases; a dead or missing mount drops its rider.
    const glm::dvec3 p = ctx.player.position();
    const ChunkPos pc{blockToChunk(int(std::floor(p.x))), blockToChunk(int(std::floor(p.z)))};
    ctx.world.forEachTickingChunk([&](Chunk& chunk) {
        if (std::abs(chunk.pos().x - pc.x) > m_simulationDistance ||
            std::abs(chunk.pos().z - pc.z) > m_simulationDistance)
            return;
        for (MobData& r : chunk.mobs()) {
            if (r.vehicle == 0 || r.health <= 0.0f) continue;
            MobData* v = mobByUuid(ctx.world, r.pos, r.vehicle);
            if (!v || v->health <= 0.0f || v->ridden) {
                r.vehicle = 0;
                continue;
            }
            v->mobRidden = true;
            if (r.targeting) {
                v->jockeyChase = true;
                v->goal = r.goal;
            }
            const double back = isCamel(v->type) && r.type == MobType::Parched ? -0.5 : 0.0;
            const glm::dvec3 f(forwardFlat(v->yaw));
            const glm::dvec3 seat(f.x * back, seatHeight(*v), f.z * back);
            const glm::dvec3 prevF(forwardFlat(v->prevYaw));
            r.pos = v->pos + seat;
            r.prevPos = v->prevPos + glm::dvec3(prevF.x * back, seat.y, prevF.z * back);
            r.vel = glm::dvec3(0.0);
            r.onGround = true;
            r.yaw = v->yaw;
            r.prevYaw = v->prevYaw;
        }
    });
}

std::optional<Mobs::MobHit> Mobs::raycast(World& world, const glm::dvec3& eye,
                                          const glm::dvec3& dir, double reach,
                                          uint64_t skipUuidHi) {
    std::optional<MobHit> best;
    const ChunkPos centre{blockToChunk(int(std::floor(eye.x))),
                          blockToChunk(int(std::floor(eye.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk* c = world.chunk({centre.x + dx, centre.z + dz});
            if (!c) continue;
            for (size_t i = 0; i < c->mobs().size(); ++i) {
                const MobData& m = c->mobs()[i];
                if (m.health <= 0.0f || (skipUuidHi && m.uuidHi == skipUuidHi)) continue;
                if (isTechnical(m.type) && m.type != MobType::Interaction)
                    continue; // (M29.7e: no box)
                // The dragon's head reaches out of its body box: rayed as its own box.
                for (int part = 0; part < (m.type == MobType::EnderDragon ? 2 : 1); ++part) {
                    const Aabb b = part == 0 ? box(m)
                                             : Aabb{dragonHead(m) - glm::dvec3(1.0),
                                                    dragonHead(m) + glm::dvec3(1.0)};
                    // Slab test along the ray.
                    double t0 = 0.0, t1 = reach;
                    bool hit = true;
                    for (int a = 0; a < 3 && hit; ++a) {
                        if (std::abs(dir[a]) < 1e-12) {
                            hit = eye[a] >= b.min[a] && eye[a] <= b.max[a];
                            continue;
                        }
                        double ta = (b.min[a] - eye[a]) / dir[a], tb = (b.max[a] - eye[a]) / dir[a];
                        if (ta > tb) std::swap(ta, tb);
                        t0 = std::max(t0, ta);
                        t1 = std::min(t1, tb);
                        hit = t0 <= t1;
                    }
                    if (hit && (!best || t0 < best->distance)) best = MobHit{c->pos(), int(i), t0};
                }
            }
        }
    return best;
}

} // namespace mc
