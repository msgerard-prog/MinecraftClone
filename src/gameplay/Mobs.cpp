#include "gameplay/Mobs.h"

#include "world/Trades.h"

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/BlockCollision.h"
#include "gameplay/FluidContact.h"
#include "gameplay/Projectiles.h"

#include "world/Blocks.h"
#include "world/Coords.h"
#include "world/Items.h"
#include "world/Raycast.h"
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
    return static_cast<float>(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 / std::numbers::pi);
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

bool solidAt(const World& w, int x, int y, int z) { return blockRegistry().collides(w.getBlock({x, y, z})); }

// A spot a mob can spawn in (wiki: Mob spawning): a spawnable block below (not
// glass, leaves, bedrock or ice), and two cells without collision or fluid.
bool canSpawnAt(const World& w, int x, int y, int z) {
    const auto& reg = blockRegistry();
    const BlockId below = reg.blockOf(w.getBlock({x, y - 1, z}));
    if (!reg.collides(w.getBlock({x, y - 1, z})) || below == blocks::Bedrock || below == blocks::Glass ||
        below == blocks::Ice || below == blocks::PackedIce || reg.block(below).id.ends_with("_leaves"))
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
    const MobInfo& info = mobInfo(m.type);
    double s = m.isBaby() ? 0.5 : 1.0; // babies are half size (wiki: Breeding)
    if (m.type == MobType::MagmaCube || m.type == MobType::Slime) s = m.size / 4.0; // (info is the large one)
    // (a sitting camel is 0.945 tall - wiki: Camel)
    const double h = m.type == MobType::Camel && m.sitting ? 0.945 : info.height;
    return Aabb::fromFeet(m.pos, info.width * s, h * s);
}

uint8_t Mobs::naturalWoolColour(Xoroshiro& rng) {
    // wiki: Sheep › Spawning - white 81.836%, black, gray and light gray 5% each,
    // brown 3%, pink 0.164%.
    const double r = rng.nextDouble() * 100.0;
    if (r < 5.0) return 15;      // black
    if (r < 10.0) return 7;      // gray
    if (r < 15.0) return 8;      // light gray
    if (r < 18.0) return 12;     // brown
    if (r < 18.164) return 6;    // pink
    return 0;                    // white
}

MobData Mobs::make(MobType type, const glm::dvec3& pos, Xoroshiro& rng) {
    MobData m;
    m.type = type;
    m.uuidHi = (uuidRng().nextLong() & ~0xF000ull) | 0x4000ull; // version 4
    m.uuidLo = (uuidRng().nextLong() & ~(3ull << 62)) | (2ull << 62); // variant 2
    m.pos = m.prevPos = m.goal = pos;
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = rng.nextFloat() * 360.0f - 180.0f;
    m.health = mobInfo(type).maxHealth;
    if (type == MobType::Sheep) m.woolColour = naturalWoolColour(rng);
    if (type == MobType::Chicken) m.eggTicks = 6000 + static_cast<int>(rng.nextInt(6000)); // wiki: 5-10 min
    if (type == MobType::IronGolem) m.persistent = true;
    if (type == MobType::TropicalFish) { // (wiki: Tropical Fish - 2 shapes x 6 patterns x 16 x 16 colours)
        m.size = uint8_t(rng.nextInt(12));
        m.woolColour = uint8_t(rng.nextInt(16));
        m.color2 = uint8_t(rng.nextInt(16));
        if (m.color2 == m.woolColour) m.color2 = uint8_t((m.color2 + 7) & 15);
    }
    if (type == MobType::Pufferfish) m.size = 0; // (deflated)
    if (isPet(type) || type == MobType::Ocelot) m.color2 = 14; // (red collars - wiki)
    if (type == MobType::Cat) m.woolColour = uint8_t(rng.nextInt(10)); // (all black only from swamp huts)
    if (type == MobType::Parrot) m.woolColour = uint8_t(rng.nextInt(5));
    if (isMount(type)) initMount(m, rng); // (M26.2: its own health, speed and jump; coat)
    initWildlife(m, rng);                 // (M26.3: panda genes, screaming goats, scute timers)
    if (type == MobType::WanderingTrader) { // (M24.4) its wares; it leaves after 40 minutes (wiki)
        wanderingTraderTrades(m, rng);
        m.despawnDelay = 48000;
        m.persistent = true;
        m.home = {int(std::floor(pos.x)), int(std::floor(pos.y)), int(std::floor(pos.z))};
    }
    if (type == MobType::Villager) {
        m.persistent = true; // (villagers never despawn)
        m.poiSearch = int16_t(rng.nextInt(40)); // (look around soon after appearing)
    }
    if (type == MobType::MagmaCube || type == MobType::Slime) { // wiki: sizes 1, 2, 4 at spawn; health size^2
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
                if (m.health <= 0.0f || !box(m).intersects(zone) || m.type == MobType::EnderDragon ||
                    m.type == MobType::EndCrystal || m.type == MobType::Minecart)
                    continue;
                hit = true;
                if (m.type == MobType::Creeper) m.powered = true;
                if (m.type == MobType::Villager) { // (wiki: a villager struck by lightning becomes a witch)
                    m.type = MobType::Witch;
                    m.health = mobInfo(m.type).maxHealth;
                    m.age = 0;
                    m.sleeping = false; // (out of its bed, not trading any more)
                    m.tradingTicks = 0;
                    m.persistent = true;
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
    const BlockPos at{int(std::floor(mob.pos.x)), int(std::floor(mob.pos.y)), int(std::floor(mob.pos.z))};
    Chunk* c = world.chunk(at.chunk());
    if (!c) return false;
    c->mobs().push_back(mob);
    c->markDirty();
    world.markTicking(c->pos());
    return true;
}

void Mobs::physics(const World& world, MobData& m, const glm::dvec3& wish, bool jump) {
    const FluidContact fluid = fluidContact(world, box(m));
    const bool inWater = fluid.water;
    m.vel += fluid.flow * 0.014; // carried by currents (vanilla pushes mobs too)
    const bool fireproof = mobInfo(m.type).fireImmune; // Nether mobs (wiki)
    if (fireproof) m.fireTicks = 0;
    if (fluid.fire && !fireproof) { // wiki: Fire - 1 a tick (hurt cooldown), 8 s alight
        if (m.hurtTime == 0 && m.deathTime == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        if (m.fireTicks < 160) m.fireTicks = 160;
    }
    if (inWater) m.fireTicks = 0; // water puts out burning mobs (wiki: Fire)
    if (fluid.lava && !fireproof) { // wiki: Lava - 4 damage (with the hurt cooldown), on fire 15 s
        if (m.hurtTime == 0 && m.deathTime == 0) {
            m.health -= 4.0f;
            m.hurtTime = 10;
        }
        m.fireTicks = 300;
        m.vel *= 0.5;
    }
    const bool vehicle = m.type == MobType::Boat; // (its velocity is set by boatTick: collision only)
    // Walking: horizontal speed approaches `wish` (blocks/tick) with ground friction.
    const double friction = vehicle ? 1.0 : m.onGround ? kGroundFriction : kAirFriction;
    const double accel = m.onGround ? (1.0 - kGroundFriction) : 0.02 / 0.1 * (1.0 - kGroundFriction) * 0.25;
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
    } else if ((m.type == MobType::Turtle || m.type == MobType::Frog) && inWater) { // (M26.3c: frogs swim too)
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
    const double step = m.type == MobType::Camel ? 1.5 : isMount(m.type) ? 1.0 : kStep;
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
        if (stepped.x * stepped.x + stepped.z * stepped.z > moved.x * moved.x + moved.z * moved.z + 1e-9)
            moved = stepped;
    }
    m.onGround = m.vel.y < 0.0 && moved.y != m.vel.y;
    // Falls (wiki: Fall damage): 1 per block beyond 3 on landing; water breaks them.
    if (inWater) {
        m.fallDistance = 0.0f;
    } else if (moved.y < 0.0) {
        if (m.type != MobType::Chicken && m.type != MobType::MagmaCube && m.type != MobType::Slime &&
            !mobInfo(m.type).flies && !vehicle)
            m.fallDistance -= static_cast<float>(moved.y); // (chickens, magma cubes, fliers: no fall damage)
    }
    if (m.onGround) {
        // (mounts: half the damage and 3 more safe blocks - wiki: Horse, Camel)
        const float damage = isMount(m.type) ? std::ceil(m.fallDistance * 0.5f - 3.0f) : std::ceil(m.fallDistance - 3.0f);
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
    if (m.type == MobType::Minecart) {
        minecartTick(ctx, m);
        return;
    }
    if (m.type == MobType::Boat) { // (M25.2b, Boats.cpp)
        boatTick(ctx, m);
        return;
    }
    if (isMount(m.type) && mountTick(ctx, m)) return; // (ridden: Mounts.cpp, M26.2)
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
        if (!target && ctx.rng.nextInt(400) == 0) m.goalTicks = 20 + static_cast<int>(ctx.rng.nextInt(41)); // a 1-3 s peek
        if (m.goalTicks > 0) --m.goalTicks;
        if (m.peek < want) m.peek = static_cast<uint8_t>(std::min(want, m.peek + 5));
        else if (m.peek > want) m.peek = static_cast<uint8_t>(std::max(want, m.peek - 5));
        if (target && ctx.projectiles && --m.chargeTicks <= 0) {
            const glm::dvec3 from = m.pos + glm::dvec3(0.0, 0.5, 0.0);
            const glm::dvec3 dir = glm::length(to) > 1e-6 ? glm::normalize(to) : glm::dvec3(0, 1, 0);
            ctx.projectiles->shoot(ProjectileKind::ShulkerBullet, from + dir * 0.8, dir, 0.15, 0.0, false, false,
                                   ctx.rng, m.uuidHi);
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
    if (netherAi(ctx, m)) return; // ghasts, blazes, magma cubes (NetherMobs.cpp)
    if (waterAi(ctx, m)) return;  // fish and squid (WaterMobs.cpp)
    if (beeAi(ctx, m)) return;    // (M26.3b, Bees.cpp)
    if (phantomAi(ctx, m)) return; // (M26.4a, Phantoms.cpp)
    if (witherAi(ctx, m)) return;  // (M26.4b, Wither.cpp)
    const MobInfo& info = mobInfo(m.type);
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
        if (m.sleeping && villagerGoal(ctx, m, unused) && m.sleeping) { // asleep: nothing else turns or moves it
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
    const double playerDist2 = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
    double speed = info.speed * 0.5; // blocks per tick at speed modifier 1 (our estimate)
    bool chase = false;

    // Follow range (wiki): zombies notice the player within 35 blocks, skeletons,
    // creepers and spiders within 16; endermen only when angered (64).
    const double follow = isZombie(m.type) || m.type == MobType::Vex ? 35.0
                          : m.type == MobType::Enderman ? 64.0
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
    if (!hostileNow || !ctx.survival || ctx.playerDead || playerDist2 >= follow * follow || !mayTarget(ctx, m)) {
        m.targeting = false;
    } else if (!m.targeting && ++m.sightCheck >= 10) {
        // Targets are picked on sight (wiki: Zombie): a line of sight check twice a second.
        m.sightCheck = 0;
        const glm::dvec3 eye = m.pos + glm::dvec3(0.0, info.height * 0.85, 0.0);
        const glm::dvec3 target = playerPos + glm::dvec3(0.0, 1.62, 0.0);
        const double dist = glm::length(target - eye);
        const auto block = raycastBlocks(ctx.world, eye, (target - eye) / dist, dist);
        m.targeting = !block;
    }
    if (m.targeting) {
        chase = true; // wiki: Zombie - follow range 35
        m.goal = playerPos;
        // Skeletons hold their ground within 10 blocks to shoot (wiki: Skeleton).
        if (m.type == MobType::Skeleton && playerDist2 < 10.0 * 10.0) m.goal = m.pos;
        if (m.type == MobType::Witch && playerDist2 < 7.0 * 7.0) m.goal = m.pos; // (throws from where it stands)
        if (m.type == MobType::Pillager && playerDist2 < 8.0 * 8.0) m.goal = m.pos; // (shoots from ~8 blocks)
        if (m.type == MobType::Evoker && playerDist2 < 10.0 * 10.0) m.goal = m.pos; // (casts from a distance)
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
        } else if (++m.goalTicks > 200 || glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 0.7) {
            if (ctx.rng.nextInt(80) == 0) {
                const glm::dvec3 home(m.home.x + 0.5, m.home.y, m.home.z + 0.5);
                m.goal = home + glm::dvec3(ctx.rng.nextDouble() * 20 - 10, 0.0, ctx.rng.nextDouble() * 20 - 10);
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
               glm::length(glm::dvec2(ctx.raidCentre->x + 0.5 - m.pos.x, ctx.raidCentre->z + 0.5 - m.pos.z)) > 6.0) {
        // Raiders with nobody to fight march on the village bell (wiki: Raid).
        m.goal = glm::dvec3(*ctx.raidCentre) + glm::dvec3(0.5, 0.0, 0.5);
    } else if (m.type == MobType::Villager && villagerGoal(ctx, m, speed)) {
        // (home, work, the bell, sleep: Villagers.cpp)
    } else if ((isPet(m.type) || m.type == MobType::Ocelot) && m.panicTicks == 0 && petGoal(ctx, m, speed)) {
        // (sitting, following, fighting, dancing: Pets.cpp)
    } else if (isWildlife(m.type) && wildlifeGoal(ctx, m, speed)) {
        // (fleeing, sleeping foxes, hunts, crops and berries, rams, rolled armadillos: Wildlife.cpp)
    } else if (isMount(m.type) && m.panicTicks == 0 && mountGoal(ctx, m, speed)) {
        // (camels resting, trader llamas with their trader: Mounts.cpp)
    } else if (m.panicTicks == 0 && !info.hostile && animalGoal(ctx, m, speed)) {
        // (breeding partner, food, parent)
    } else if (m.panicTicks > 0) {
        --m.panicTicks;
        speed *= 2.0; // wiki: Cow - panics when hurt (faster)
        if (m.goalTicks++ > 30 || glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.0) {
            m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 10 - 5, 0, ctx.rng.nextDouble() * 10 - 5);
            m.goalTicks = 0;
        }
    } else {
        // Random strolls (wiki: Mob - wander about every 6 s on average), only with a
        // player within 32 blocks.
        if (playerDist2 > 32.0 * 32.0) {
            m.goal = m.pos;
        } else if (++m.goalTicks > 200 || glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 0.7) {
            if (ctx.rng.nextInt(120) == 0) {
                m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 20 - 10, 0, ctx.rng.nextDouble() * 20 - 10);
                m.goalTicks = 0;
            } else if (m.goalTicks > 200) {
                m.goal = m.pos;
            }
        }
    }

    // Path to the goal (M16.2): a new search only when the goal cell moved (or a chase
    // path ran out), at most every 4-10 ticks; longer waits after a partial path (+15)
    // and for far targets (+5 past 16 blocks, +10 past 32), as vanilla's melee goal.
    const glm::ivec3 feet{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 0.01)), int(std::floor(m.pos.z))};
    const glm::ivec3 goalCell{int(std::floor(m.goal.x)), int(std::floor(m.goal.y + 0.01)), int(std::floor(m.goal.z))};
    if (m.repathTicks > 0) --m.repathTicks;
    const bool moved = goalCell != m.pathRequest, finished = m.pathIndex >= m.pathLength;
    if (goalCell != feet && m.repathTicks == 0 && (moved || (chase && finished))) {
        const int heightCells = int(std::ceil(info.height));
        // Search budget: vanilla visits up to follow range x 16 nodes (zombie 35).
        m.pathLength = uint8_t(m_pathfinder.find(ctx.world, feet, goalCell, heightCells, chase ? 560 : 200,
                                                   m.path.data(), MobData::kMaxPath));
        m.pathIndex = 0;
        m.pathRequest = goalCell;
        const bool partial = m.pathLength == 0 || m.path[size_t(m.pathLength - 1)] != goalCell;
        const double far2 = glm::dot(m.goal - m.pos, m.goal - m.pos);
        m.repathTicks = int16_t(4 + ctx.rng.nextInt(7) + (partial ? 15 : 0) + (far2 > 16.0 * 16.0 ? 5 : 0) +
                                (far2 > 32.0 * 32.0 ? 5 : 0));
        if (!chase && partial && m.pathLength > 0 && m.panicTicks == 0) {
            // A stroll target it can't reach: stop where the path ends.
            const glm::ivec3 end = m.path[size_t(m.pathLength - 1)];
            m.goal = {end.x + 0.5, double(end.y), end.z + 0.5};
            m.pathRequest = end;
        }
    }
    // The next cell of the path (skipping the ones reached), else the goal itself.
    while (m.pathIndex < m.pathLength) {
        const glm::ivec3 c = m.path[m.pathIndex];
        const double dx = c.x + 0.5 - m.pos.x, dz = c.z + 0.5 - m.pos.z;
        if (dx * dx + dz * dz < 0.35 * 0.35 && std::abs(double(c.y) - m.pos.y) < 1.1) ++m.pathIndex;
        else break;
    }
    glm::dvec3 steer = m.goal;
    bool climb = false;
    if (m.pathIndex < m.pathLength) {
        const glm::ivec3 c = m.path[m.pathIndex];
        steer = {c.x + 0.5, double(c.y), c.z + 0.5};
        const double cdx = c.x + 0.5 - m.pos.x, cdz = c.z + 0.5 - m.pos.z;
        climb = c.y > feet.y && cdx * cdx + cdz * cdz < 1.5 * 1.5; // a step up just ahead: jump
    }

    // Steer towards it; jump when a block is in the way.
    glm::dvec3 wish(0.0);
    bool jump = false;
    const bool last = m.pathIndex + 1 >= m.pathLength;
    const glm::dvec2 d(steer.x - m.pos.x, steer.z - m.pos.z);
    const double dl = glm::length(d);
    const double stopAt = chase && last ? info.width * 0.5 + 0.5 : (m.pathIndex < m.pathLength ? 0.1 : 0.5);
    if (dl > stopAt) {
        wish = glm::dvec3(d.x / dl, 0, d.y / dl) * speed;
        m.yaw = approachAngle(m.yaw, yawTowards(m.pos, steer), 10.0f);
        const int ax = int(std::floor(m.pos.x + d.x / dl * (info.width * 0.5 + 0.4)));
        const int az = int(std::floor(m.pos.z + d.y / dl * (info.width * 0.5 + 0.4)));
        const int fy = int(std::floor(m.pos.y + 0.01));
        jump = climb || (solidAt(ctx.world, ax, fy, az) && !solidAt(ctx.world, ax, fy + 1, az) &&
                         !solidAt(ctx.world, int(std::floor(m.pos.x)), fy + 2, int(std::floor(m.pos.z))));
    }
    // Head: look at a near player (wiki: look-at-player goal, 6-8 blocks).
    if (playerDist2 < 8.0 * 8.0) {
        m.headYaw = approachAngle(m.headYaw, yawTowards(m.pos, playerPos), 10.0f);
        const double horiz = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        m.pitch = static_cast<float>(-std::atan2(toPlayer.y + 1.62 - info.height * 0.85, horiz) * 180.0 / std::numbers::pi);
    } else {
        m.headYaw = approachAngle(m.headYaw, m.yaw, 10.0f);
        m.pitch *= 0.9f;
    }
    // Rabbits get about by hopping (wiki: Rabbit).
    if (m.type == MobType::Rabbit && (wish.x != 0.0 || wish.z != 0.0) && m.onGround) jump = true;
    if (m.type == MobType::Armadillo && m.sitting) wish = glm::dvec3(0.0); // (rolled up)
    const glm::dvec3 before = m.pos;
    physics(ctx.world, m, wish, jump);
    if (isWildlife(m.type))
        wildlifeTick(ctx, m, m.climbing && glm::length(m.pos - before) < 0.05);

    // Melee (wiki: Zombie - 3 damage on normal, once a second, reach ~ width*2).
    if (m.attackCooldown > 0) --m.attackCooldown;
    if (chase && m.attackCooldown == 0 && info.attackDamage > 0.0f) {
        const double reach = info.width * 2.0 + 0.6;
        if (playerDist2 < reach * reach && box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.8, 0, 0.8),
                                                                  ctx.player.box().max + glm::dvec3(0.8, 0, 0.8)})) {
            // (golems: 7.5 + 0-14 and a throw upward)
            const float hit = info.attackDamage + (m.type == MobType::IronGolem ? float(ctx.rng.nextInt(15)) : 0.0f);
            if (ctx.vitals.attacked(hit, &m.pos)) {
                m_playerAttacker = m.uuidHi; // (tamed wolves go for it - M26.1)
                ctx.player.knockback(toPlayer.x, toPlayer.z);
                if (m.type == MobType::IronGolem) ctx.player.setVelocity(ctx.player.velocity() + glm::dvec3(0.0, 0.4, 0.0));
                // (M26.4a; wiki, Normal) a cave spider's bite poisons for 7 s, a wither
                // skeleton's hit withers for 10 s.
                if (m.type == MobType::CaveSpider) ctx.vitals.addEffect(Effect::Poison, 0, 140);
                if (m.type == MobType::WitherSkeleton) ctx.vitals.addEffect(Effect::Wither, 0, 200);
            }
            m.attackCooldown = 20;
        }
    }

    if (info.hostile) monsterTick(ctx, m, chase, playerDist2);
    if (isLlama(m.type)) llamaTick(ctx, m);

    // Undead burn in daylight under open sky (wiki: Zombie, Skeleton): 1 damage a second.
    // Water or rain on it puts any burning mob out (wiki: Fire, Rain).
    const bool undead = isZombie(m.type) || m.type == MobType::Skeleton;
    if (undead || m.fireTicks > 0) {
        const BlockPos head{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + mobInfo(m.type).height * 0.85)),
                            int(std::floor(m.pos.z))};
        const Chunk* c = ctx.world.chunk(head.chunk());
        const bool day = ctx.skyDarken < 4.0f;
        const bool sky = c && c->lit() && c->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z)) >= 15;
        const BlockStateId hs = c ? c->get(blockToLocal(head.x), head.y, blockToLocal(head.z)) : BlockStateId{0};
        bool wet = c && (blockRegistry().blockOf(hs) == blocks::Water || blockRegistry().waterlogged(hs));
        // A zombie under water turns into a drowned: 30 s submerged, then 15 s of
        // shaking (wiki: Zombie › Drowned conversion; ours counts 45 s in one go).
        if (m.type == MobType::Zombie) { // (babies too; it keeps the zombie's persistence - review)
            if (!wet) m.airTicks = 300;
            else if (--m.airTicks <= -600) {
                m.type = MobType::Drowned;
                m.airTicks = 300;
                m.health = mobInfo(MobType::Drowned).maxHealth;
            }
        }
        if (!wet && ctx.weather && ctx.weather->raining && ((undead && day && sky) || m.fireTicks > 0))
            // (sky light 15 at the head: open sky, so no column scan is needed)
            wet = sky ? precipitationAt(ctx.world, head) == Precipitation::Rain : rainingAt(ctx.world, *ctx.weather, head);
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
    if (world.getBlock({on.x, on.y + 1, on.z}) != 0 || world.getBlock({on.x, on.y + 2, on.z}) != 0) return false;
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
    if (m.hurtTime > 0 || m.deathTime > 0) return; // 10 ticks of invulnerability
    if (m.type == MobType::Wither && m.spellTicks > 0) return; // (M26.4b: charging, it can't be hurt)
    if (m.type == MobType::Shulker && m.peek == 0) damage *= 0.2f; // (armour 20 while closed)
    if (m.type == MobType::Armadillo && m.sitting) damage = std::max(0.0f, damage - 1.0f) * 0.5f; // (M26.3: rolled up)
    m.health -= damage;
    m.hurtTime = 10;
    m.noPlayerTicks = 0;                                 // damage resets the despawn clock
    m.lastHurtByPlayer = true;                           // (Mobs::attack: the player's hits)
    m.lastHurtBySkeleton = false;
    if (m.type == MobType::Wolf && !m.tamed) { // a wild wolf turns on the player (wiki: Wolf)
        m.angry = true;
        m.angerTicks = 600;
        m.targeting = true;
    } else if (m.type == MobType::IronGolem) { // golems don't flee: they fight back (wiki), unless player-built
        if (!m.playerCreated) {
            m.angry = true;
            m.angerTicks = 600;
        }
    } else if (m.type == MobType::PolarBear ||
               (m.type == MobType::Panda && pandaPersonality(m.woolColour, m.color2) == 6)) {
        m.angry = true; // (M26.3: they fight back - wiki: Polar Bear, Panda)
        m.angerTicks = 400;
        m.targeting = true;
    } else if (isLlama(m.type)) { // llamas spit back (wiki: Llama)
        m.angry = true;
        m.angerTicks = 200;
    } else if (!mobInfo(m.type).hostile) {
        m.panicTicks = 100; // passive mobs flee (wiki: Cow)
    }
    if (m.type == MobType::Piglin) m.admireTicks = 0; // a hit takes the ingot back (wiki: Bartering)
    if (isSpider(m.type) || m.type == MobType::Enderman || m.type == MobType::Piglin) { // provoked (wiki)
        m.angry = true;
        m.targeting = true;
        m.angerTicks = 600;
    }
    if (m.type == MobType::ZombifiedPiglin) m.angerAlert = true; // (the herd joins in: Mobs::tick)
    if (m.type == MobType::Bee && !m.stung) { // (M26.3b: it stings back, and its hive-mates join in)
        m.angry = true;
        m.angerTicks = 400;
        m.angerAlert = true;
    }
    const glm::dvec2 d(m.pos.x - from.x, m.pos.z - from.z);
    const double l = glm::length(d);
    if (l > 1e-6) { // wiki: Knockback - 0.4 away; lifted only when on the ground
        m.vel.x = m.vel.x / 2.0 + d.x / l * 0.4;
        m.vel.z = m.vel.z / 2.0 + d.y / l * 0.4;
        if (m.onGround) m.vel.y = std::min(0.4, m.vel.y / 2.0 + 0.4);
    }
}

void Mobs::die(Context& ctx, MobData& m) {
    if (m.type == MobType::Boat) { // broken: the boat item, gone at once (M25.2b)
        m.deathTime = 19;
        if (const auto boat = itemRegistry().find(m.hasChest ? chestBoatId(m.woolColour) : boatId(m.woolColour)))
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {*boat, 1}, ctx.rng);
        if (m.hasChest) dropMountGear(ctx, m); // (M26.2: its chest's stacks)
        return;
    }
    if (isMount(m.type)) dropMountGear(ctx, m); // (M26.2)
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
    if (m.type == MobType::Minecart) { // broken: the cart item, gone at once
        m.deathTime = 19;
        if (const auto cart = itemRegistry().find("minecart")) ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {*cart, 1}, ctx.rng);
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
        const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
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
        m_explosion.explode(ctx.world, m.pos, 6.0f, ctx.rng, ctx.items, changed, t); // (from its bottom)
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
        const int extra = m.lastHurtByPlayer ? static_cast<int>(ctx.rng.nextInt(uint32_t(m.looting) + 1)) : 0;
        const int n = lo + static_cast<int>(ctx.rng.nextInt(uint32_t(hi - lo + 1))) + extra;
        if (n > 0) ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {item, uint8_t(n)}, ctx.rng);
    };
    if (m.isBaby()) return; // babies drop nothing (wiki: Breeding)
    // Killed by a charged creeper's blast: its head (M26.4b; wiki: Head - zombies,
    // skeletons, creepers, piglins and wither skeletons).
    if (m.chargedBlast > 0) {
        const char* head = m.type == MobType::Zombie            ? "zombie_head"
                           : m.type == MobType::Skeleton        ? "skeleton_skull"
                           : m.type == MobType::Creeper         ? "creeper_head"
                           : m.type == MobType::Piglin          ? "piglin_head"
                           : m.type == MobType::WitherSkeleton  ? "wither_skeleton_skull"
                                                                 : nullptr;
        if (head) ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {*itemRegistry().find(head), 1}, ctx.rng); // (one, Looting or not)
    }
    // Experience when the player killed it (wiki: Experience): monsters 5, animals 1-3.
    // (wiki: blazes and evokers 10, ravagers 20, magma cubes their size; villagers,
    // wandering traders and iron golems none)
    const bool noXp = m.type == MobType::Villager || m.type == MobType::WanderingTrader || m.type == MobType::IronGolem;
    if (ctx.orbs && m.lastHurtByPlayer && !noXp)
        ctx.orbs->drop(m.pos + glm::dvec3(0, 0.5, 0),
                       m.type == MobType::Wither                              ? 50 // (M26.4b)
                       : m.type == MobType::Ravager                           ? 20
                       : m.type == MobType::Blaze || m.type == MobType::Evoker ? 10
                       : m.type == MobType::MagmaCube || m.type == MobType::Slime ? int(m.size)
                       : mobInfo(m.type).hostile      ? 5
                                                      : 1 + static_cast<int>(ctx.rng.nextInt(3)),
                       ctx.rng);
    const bool burning = m.fireTicks > 0; // meat drops cooked
    switch (m.type) {
    case MobType::Cow:
        drop(burning ? "cooked_beef" : "beef", 1, 3);
        drop("leather", 0, 2);
        break;
    case MobType::Zombie:
    case MobType::ZombieVillager: drop("rotten_flesh", 0, 2); break;
    case MobType::Drowned: // wiki: Drowned - rotten flesh 0-2, a copper ingot 11%, its trident 8.5%
        drop("rotten_flesh", 0, 2);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(100) < 11) drop("copper_ingot", 1, 1);
        if (m.heldTrident && m.lastHurtByPlayer && ctx.rng.nextInt(1000) < 85) drop("trident", 1, 1);
        break;
    case MobType::Witch: { // wiki: Witch - 1-3 rolls of bottles, glowstone, gunpowder, redstone, spider eyes, sugar, sticks
        static constexpr const char* kLoot[7] = {"glass_bottle", "glowstone_dust", "gunpowder", "redstone",
                                                 "spider_eye", "sugar", "stick"};
        for (int k = 0, n = 1 + int(ctx.rng.nextInt(3)); k < n; ++k) drop(kLoot[ctx.rng.nextInt(7)], 1, 2);
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
        if (m.captain && m.raidId == 0 && m.lastHurtByPlayer) drop("ominous_bottle", 1, 1); // (a captain's: M24.5 raids)
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
    case MobType::Squid: drop("ink_sac", 1, 3); break;
    // (wiki: Guardian - 0-2 prismarine shards, and 40% a raw cod or 0-1 prismarine
    // crystals otherwise; Elder Guardian - also a wet sponge when killed by a player)
    case MobType::Guardian:
    case MobType::ElderGuardian:
        // (guardian: 2/5 cod, 2/5 a crystal, 1/5 nothing; elder: 3/6, 2/6, 1/6 - review)
        drop("prismarine_shard", 0, 2);
        {
            const bool elder = m.type == MobType::ElderGuardian;
            const uint32_t r = ctx.rng.nextInt(elder ? 6 : 5);
            if (r < (elder ? 3u : 2u)) drop(burning ? "cooked_cod" : "cod", 1, 1);
            else if (r < (elder ? 5u : 4u)) drop("prismarine_crystals", 1, 1);
        }
        if (m.type == MobType::ElderGuardian && m.lastHurtByPlayer) {
            drop("wet_sponge", 1, 1);
            if (ctx.rng.nextInt(5) == 0) drop("tide_armor_trim_smithing_template", 1, 1); // (20%)
        }
        break;
    case MobType::Dolphin: drop(burning ? "cooked_cod" : "cod", 0, 1); break; // (wiki: Dolphin)
    case MobType::Turtle: drop("seagrass", 0, 2); break; // (wiki: Turtle - 0-2 seagrass)
    case MobType::Horse: // (M26.2; wiki: Horse, Donkey, Mule, Llama - 0-2 leather; camels nothing)
    case MobType::Donkey:
    case MobType::Mule:
    case MobType::Llama:
    case MobType::TraderLlama: drop("leather", 0, 2); break;
    // (M26.3; wiki) rabbit: hide 0-1, meat 0-1, a rabbit's foot 10% (+3% a Looting level)
    // for player kills; polar bear: cod 0-2 (3 in 4) or salmon 0-2; panda: bamboo 0-2.
    case MobType::Rabbit:
        drop("rabbit_hide", 0, 1);
        drop(burning ? "cooked_rabbit" : "rabbit", 0, 1);
        if (m.lastHurtByPlayer && int(ctx.rng.nextInt(100)) < 10 + 3 * m.looting) drop("rabbit_foot", 1, 1);
        break;
    case MobType::PolarBear:
        if (ctx.rng.nextInt(4) != 0) drop(burning ? "cooked_cod" : "cod", 0, 2);
        else drop(burning ? "cooked_salmon" : "salmon", 0, 2);
        break;
    case MobType::Panda: drop("bamboo", 0, 2); break;
    // (M26.4a; wiki) wither skeleton: coal 0-1 (1 in 3), bones 0-2; phantom: a membrane
    // 0-1 for player kills.
    case MobType::WitherSkeleton:
        if (ctx.rng.nextInt(3) == 0) drop("coal", 1, 1);
        drop("bone", 0, 2);
        // its skull: 2.5% (+1% a Looting level) for player kills (wiki: Wither Skeleton Skull)
        if (m.lastHurtByPlayer && ctx.rng.nextInt(1000) < 25u + 10u * m.looting && m.chargedBlast == 0)
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {*itemRegistry().find("wither_skeleton_skull"), 1}, ctx.rng);
        break;
    case MobType::Phantom:
        if (m.lastHurtByPlayer) drop("phantom_membrane", 0, 1);
        break;
    case MobType::Wither: // (M26.4b; wiki: the nether star, always)
        ctx.items.spawn(m.pos + glm::dvec3(0, 1.5, 0), {*itemRegistry().find("nether_star"), 1}, ctx.rng);
        break;
    case MobType::GlowSquid: drop("glow_ink_sac", 1, 3); break;
    case MobType::IronGolem: // wiki: Iron Golem - 3-5 iron ingots, 0-2 poppies
        drop("iron_ingot", 3, 5);
        drop("poppy", 0, 2);
        break;
    case MobType::Sheep: // wiki: Sheep - its wool unless sheared, 1-2 mutton
        if (!m.sheared)
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0),
                            {items.blockItem(static_cast<BlockId>(blocks::WhiteWool + m.woolColour)), 1}, ctx.rng);
        drop(burning ? "cooked_mutton" : "mutton", 1, 2);
        break;
    case MobType::Pig: drop(burning ? "cooked_porkchop" : "porkchop", 1, 3); break; // wiki: Pig
    case MobType::Skeleton: // wiki: Skeleton - bones 0-2, arrows 0-2
        drop("bone", 0, 2);
        drop("arrow", 0, 2);
        break;
    case MobType::Creeper: // wiki: Creeper - gunpowder 0-2; killed by a skeleton's arrow, a music disc
        drop("gunpowder", 0, 2);
        if (m.lastHurtBySkeleton) {
            static constexpr const char* kDiscs[12] = {"music_disc_13",   "music_disc_cat",     "music_disc_blocks",
                                                       "music_disc_chirp", "music_disc_far",     "music_disc_mall",
                                                       "music_disc_mellohi", "music_disc_stal",  "music_disc_strad",
                                                       "music_disc_ward",  "music_disc_11",      "music_disc_wait"};
            drop(kDiscs[ctx.rng.nextInt(12)], 1, 1);
        }
        break;
    case MobType::Spider: // wiki: Spider - string 0-2, spider eye 1 in 3
    case MobType::CaveSpider:
        drop("string", 0, 2);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(3) == 0) drop("spider_eye", 1, 1); // player kills only
        break;
    case MobType::Enderman: // wiki: Enderman - ender pearl 0-1, and the block it carried
        drop("ender_pearl", 0, 1);
        if (m.carried)
            if (const ItemId it = items.blockItem(blockRegistry().blockOf(m.carried)))
                ctx.items.spawn(m.pos + glm::dvec3(0, 1, 0), {it, 1}, ctx.rng);
        break;
    case MobType::Shulker: // wiki: Shulker - a shell 50% + 6.25% per Looting level, never more than one
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
                MobData child = make(MobType::MagmaCube, m.pos + glm::dvec3(ctx.rng.nextDouble() - 0.5, 0.2,
                                                                            ctx.rng.nextDouble() - 0.5), ctx.rng);
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
                MobData child = make(MobType::Slime, m.pos + glm::dvec3(ctx.rng.nextDouble() - 0.5, 0.2,
                                                                        ctx.rng.nextDouble() - 0.5), ctx.rng);
                child.size = uint8_t(m.size / 2);
                child.health = float(child.size * child.size);
                m_births.push_back(child);
            }
        }
        break;
    case MobType::Piglin: break; // (drops only what it carries: nothing we model)
    case MobType::Hoglin: // wiki: Hoglin - porkchop 2-4 (cooked when burning), leather 0-1
        drop(burning ? "cooked_porkchop" : "porkchop", 2, 4);
        drop("leather", 0, 1);
        break;
    case MobType::Strider: drop("string", 2, 5); break; // wiki: Strider
    case MobType::ZombifiedPiglin:
        drop("rotten_flesh", 0, 1);
        drop("gold_nugget", 0, 1);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(40) == 0) drop("gold_ingot", 1, 1);
        break;
    case MobType::Chicken: // wiki: Chicken - feathers 0-2, 1 raw chicken
        drop("feather", 0, 2);
        drop(burning ? "cooked_chicken" : "chicken", 1, 1);
        break;
    default: break;
    }
}

void Mobs::tick(Context& ctx) {
    m_moves.clear();
    m_births.clear();
    m_hostiles = 0;
    m_fish = m_squid = m_glowSquid = m_axolotls = 0;
    m_creatures = m_cats = 0;
    m_striders = 0;
    m_angerAlertCount = 0;
    m_bossHealth = -1.0f;
    m_dragonDeaths.clear();
    const glm::dvec3 playerPos = ctx.player.position();
    const ChunkPos playerChunk{blockToChunk(int(std::floor(playerPos.x))), blockToChunk(int(std::floor(playerPos.z)))};
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
            // Sounds (M22.4): hurt since last tick (hurtTime was set to 10), and now and
            // then its ambient call - vanilla: 1/1000 chance growing each tick, then 80
            // ticks of quiet. Babies squeak half an octave higher.
            if (m.hurtTime == 10 && m.health > 0.0f)
                ctx.world.playSound(mobSound(m.type, MobSound::Hurt), m.pos.x, m.pos.y + mobInfo(m.type).height * 0.5,
                                    m.pos.z, 1.0f, m.isBaby() ? 1.5f : 1.0f);
            if (m.health > 0.0f && int(m_soundRng.nextInt(1000)) < m.ambientTime++) { // (own RNG: gameplay unchanged)
                m.ambientTime = -80;
                ctx.world.playSound(mobSound(m.type, MobSound::Ambient), m.pos.x, m.pos.y + mobInfo(m.type).height * 0.85,
                                    m.pos.z, 1.0f, m.isBaby() ? 1.5f : 1.0f);
            }
            if (m.hurtTime > 0) --m.hurtTime;
            bool remove = false;
            if (m.angerAlert) { // (also from one killed by the hit)
                m.angerAlert = false;
                if (m_angerAlertCount < int(m_angerAlerts.size())) m_angerAlerts[size_t(m_angerAlertCount++)] = m.pos;
            }
            if (m.type == MobType::EnderDragon || m.type == MobType::Wither) { // (M26.4b: the Wither's bar too)
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
                    if (m_dragonDeaths.size() < m_dragonDeaths.capacity()) m_dragonDeaths.push_back(m.pos);
                }
            } else if (m.health <= 0.0f) { // loot at the moment of death, then the death animation
                if (++m.deathTime == 1) {
                    die(ctx, m);
                    ctx.world.playSound(mobSound(m.type, MobSound::Death), m.pos.x, m.pos.y + mobInfo(m.type).height * 0.5,
                                        m.pos.z, 1.0f, m.isBaby() ? 1.5f : 1.0f);
                }
                if (m.deathTime >= 20) {
                    remove = true;
                    // The poof of smoke when the body vanishes (vanilla: 20 particles).
                    if (m.type != MobType::Minecart && m.type != MobType::EndCrystal && m.type != MobType::Boat && !m.vanish)
                        ctx.world.levelEvent(LevelEvent::Type::MobDeath, m.pos.x, m.pos.y, m.pos.z,
                                             uint32_t(mobInfo(m.type).width * 100.0f) |
                                                 uint32_t(mobInfo(m.type).height * 100.0f) << 16);
                }
            } else {
                ai(ctx, m);
                if (mobInfo(m.type).hostile) ++m_hostiles;
                m_fish += isFish(m.type);
                m_squid += m.type == MobType::Squid || m.type == MobType::Dolphin;
                m_creatures += (m.type == MobType::Wolf || m.type == MobType::Ocelot || m.type == MobType::Parrot ||
                                (isMount(m.type) && m.type != MobType::TraderLlama) ||
                                isWildlife(m.type)) &&
                               !m.tamed;
                m_cats += m.type == MobType::Cat;
                m_glowSquid += m.type == MobType::GlowSquid;
                m_axolotls += m.type == MobType::Axolotl;
                m_striders += m.type == MobType::Strider;
                // Despawning (wiki: Spawn › Despawning): hostiles beyond 128 blocks
                // vanish; beyond 32 they may after 30 s without a player near.
                const double d2 = glm::dot(m.pos - playerPos, m.pos - playerPos);
                // Water mobs too (wiki): squid as monsters, fish beyond 64 blocks; never
                // fish from a bucket.
                if ((mobInfo(m.type).hostile || mobInfo(m.type).swims) && !m.persistent && !m.fromBucket) {
                    if (d2 > (isFish(m.type) ? 64.0 * 64.0 : 128.0 * 128.0)) remove = true;
                    else if (d2 > 32.0 * 32.0 && ++m.noPlayerTicks > 600 && ctx.rng.nextInt(800) == 0) remove = true;
                    else if (d2 <= 32.0 * 32.0) m.noPlayerTicks = 0;
                }
                if (m.pos.y < ctx.world.height().minY - 64) remove = true; // fell out of the world
            }
            const ChunkPos now{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
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
            if (remove || m.pos != m.prevPos || m.yaw != m.prevYaw || m.hurtTime > 0 || m.fireTicks > 0 ||
                m.deathTime > 0)
                chunk.markDirty();
            if (remove) {
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
            if (mv.mob.hasChest)
                if (Chunk* from = ctx.world.chunk(mv.from))
                    if (ItemContents* slots = from->mobStore(mv.mob.uuidHi)) {
                        c->addMobStore(mv.mob.uuidHi) = *slots;
                        from->removeMobStore(mv.mob.uuidHi);
                        from->markDirty();
                    }
            c->markDirty();
            ctx.world.markTicking(mv.to);
        }
    for (const MobData& baby : m_births)
        add(ctx.world, baby);
    // A hit zombified piglin angers the others around it (wiki: Zombified Piglin -
    // within about 33 blocks across and 11 up/down; 20-55 s of anger).
    for (int a = 0; a < m_angerAlertCount; ++a) {
        const glm::dvec3 hit = m_angerAlerts[size_t(a)];
        const ChunkPos hc{blockToChunk(int(std::floor(hit.x))), blockToChunk(int(std::floor(hit.z)))};
        for (int dz = -3; dz <= 3; ++dz)
            for (int dx = -3; dx <= 3; ++dx)
                if (Chunk* c = ctx.world.chunk({hc.x + dx, hc.z + dz}))
                    for (MobData& o : c->mobs())
                        if (o.type == MobType::ZombifiedPiglin && std::abs(o.pos.x - hit.x) < 33.5 &&
                            std::abs(o.pos.z - hit.z) < 33.5 && std::abs(o.pos.y - hit.y) < 11.0) {
                            o.angry = true;
                            o.angerTicks = static_cast<int16_t>(400 + ctx.rng.nextInt(701));
                        } else if (o.type == MobType::Bee && !o.stung && glm::length(o.pos - hit) < 20.0) {
                            o.angry = true; // (bees within 20 blocks - our reading of the wiki's "nearby")
                            o.angerTicks = static_cast<int16_t>(400 + ctx.rng.nextInt(400));
                        }
    }
    if (ctx.naturalSpawning) {
        if (ctx.world.isUltrawarm()) spawnNether(ctx);
        else spawnHostiles(ctx);
        if (ctx.world.hasSkyLight()) spawnWater(ctx); // (M25.2: the Overworld's water)
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
        if (ctx.playerDead || glm::dot(playerPos - centre, playerPos - centre) > 16.0 * 16.0) continue;
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
            if (!room || !c || !c->lit() || c->blockLight(blockToLocal(bx), y, blockToLocal(bz)) > 11) continue;
            MobData m = make(s.mob, {x, double(y), z}, ctx.rng);
            m_births.push_back(m);
            spawned = true;
        }
        if (spawned) s.delay = static_cast<int16_t>(200 + ctx.rng.nextInt(600));
    }
}

void Mobs::spawnHostiles(Context& ctx) {
    // One spawn attempt per tick near the player (wiki: Spawn): a dark spot (block
    // light 0, sky light after night darkening at most a random 0..7) on a solid
    // block with two free blocks above, 24+ blocks away, under the mob cap (70).
    if (m_hostiles >= 70) return;
    const glm::dvec3 p = ctx.player.position();
    const int x = int(std::floor(p.x)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int z = int(std::floor(p.z)) + static_cast<int>(ctx.rng.nextInt(129)) - 64;
    const int y = int(std::floor(p.y)) + static_cast<int>(ctx.rng.nextInt(65)) - 32;
    if (!ctx.world.isInHeight(y) || !ctx.world.isInHeight(y + 2)) return;
    const double dx = x + 0.5 - p.x, dz = z + 0.5 - p.z, dy = y - p.y;
    if (dx * dx + dy * dy + dz * dz < 24.0 * 24.0) return;
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit()) return;
    if (!canSpawnAt(ctx.world, x, y, z)) return;
    // No monsters spawn in mushroom fields (wiki: Mushroom Fields); spawners still work.
    if (c->biomes() && c->biomes()->at(blockToLocal(x), y, blockToLocal(z), ctx.world.height()) == Biome::MushroomFields)
        return;
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    // Slimes (wiki: Slime › Spawning): in 1 chunk of 10 ("slime chunks", ours by seed)
    // below Y 40 at any light level, in groups of up to 4; in swamps between Y 51 and
    // 69 where the light is at most a random 0..7 (night). The rest by the Overworld mix.
    if (ctx.world.hasSkyLight() && ctx.rng.nextInt(3) == 0) {
        const ChunkPos cp{blockToChunk(x), blockToChunk(z)};
        Xoroshiro sc(mixSeed(mixSeed(ctx.worldSeed ^ 0x3AD8025Full, static_cast<uint32_t>(cp.x)), static_cast<uint32_t>(cp.z)));
        const bool slimeChunk = sc.nextInt(10) == 0 && y < 40;
        const int light = std::max<int>(c->blockLight(lx, y, lz), c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken));
        const bool swamp = c->biomes() && y >= 51 && y <= 69 && light <= static_cast<int>(ctx.rng.nextInt(8)) &&
                           c->biomes()->at(lx, y, lz, ctx.world.height()) == Biome::Swamp;
        if (slimeChunk || swamp) {
            const int group = slimeChunk ? 1 + static_cast<int>(ctx.rng.nextInt(4)) : 1;
            for (int g = 0; g < group && m_hostiles < 70; ++g) {
                const int gx = x + (g == 0 ? 0 : static_cast<int>(ctx.rng.nextInt(5)) - 2);
                const int gz = z + (g == 0 ? 0 : static_cast<int>(ctx.rng.nextInt(5)) - 2);
                if (g > 0 && !canSpawnAt(ctx.world, gx, y, gz)) continue;
                if (add(ctx.world, make(MobType::Slime, {gx + 0.5, double(y), gz + 0.5}, ctx.rng))) ++m_hostiles;
            }
            return;
        }
    }
    if (c->blockLight(lx, y, lz) > 0) return;
    // In a thunderstorm the spawner darkens the sky by at least 10 (wiki: Weather ›
    // Thunderstorm): monsters spawn in the open at midday.
    const int darken = ctx.thundering ? std::max(10, static_cast<int>(ctx.skyDarken)) : static_cast<int>(ctx.skyDarken);
    const int sky = c->skyLight(lx, y, lz) - darken;
    if (sky > static_cast<int>(ctx.rng.nextInt(8))) return;
    // Which monster: vanilla's Overworld weights (wiki: Spawn › Java Edition) - zombie 95,
    // skeleton 100, creeper 100, spider 100, enderman 10 - in a group of up to 4.
    const uint32_t roll = ctx.rng.nextInt(410); // (+ witch 5, M24.4)
    // The End (no sky, not the Nether): endermen only, in groups of 4 (wiki: The End biomes).
    const bool end = !ctx.world.hasSkyLight();
    const MobType kind = end          ? MobType::Enderman
                         : roll < 95  ? MobType::Zombie
                         : roll < 195 ? MobType::Skeleton
                         : roll < 295 ? MobType::Creeper
                         : roll < 395 ? MobType::Spider
                         : roll < 405 ? MobType::Enderman
                                      : MobType::Witch;
    const int group = end ? 4 : 1 + static_cast<int>(ctx.rng.nextInt(4));
    for (int i = 0; i < group && m_hostiles < 70; ++i) {
        const int gx = x + static_cast<int>(ctx.rng.nextInt(5)) - 2, gz = z + static_cast<int>(ctx.rng.nextInt(5)) - 2;
        if (!canSpawnAt(ctx.world, gx, y, gz)) continue;
        if (kind == MobType::Enderman && solidAt(ctx.world, gx, y + 2, gz)) continue; // 3 tall
        MobData mob = make(kind, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
        // 5% of zombies come as zombie villagers (wiki: Zombie Villager › Spawning).
        if (kind == MobType::Zombie && ctx.rng.nextInt(20) == 0) mob.type = MobType::ZombieVillager;
        if (add(ctx.world, mob)) ++m_hostiles;
    }
}

std::optional<Mobs::MobHit> Mobs::raycast(World& world, const glm::dvec3& eye, const glm::dvec3& dir, double reach,
                                          uint64_t skipUuidHi) {
    std::optional<MobHit> best;
    const ChunkPos centre{blockToChunk(int(std::floor(eye.x))), blockToChunk(int(std::floor(eye.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk* c = world.chunk({centre.x + dx, centre.z + dz});
            if (!c) continue;
            for (size_t i = 0; i < c->mobs().size(); ++i) {
                const MobData& m = c->mobs()[i];
                if (m.health <= 0.0f || (skipUuidHi && m.uuidHi == skipUuidHi)) continue;
                // The dragon's head reaches out of its body box: rayed as its own box.
                for (int part = 0; part < (m.type == MobType::EnderDragon ? 2 : 1); ++part) {
                const Aabb b = part == 0 ? box(m) : Aabb{dragonHead(m) - glm::dvec3(1.0), dragonHead(m) + glm::dvec3(1.0)};
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
