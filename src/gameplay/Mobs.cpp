#include "gameplay/Mobs.h"

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/FluidContact.h"

#include "world/Blocks.h"
#include "world/Coords.h"
#include "world/Items.h"
#include "world/Raycast.h"

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
    if (m.type == MobType::MagmaCube) s = m.size / 4.0; // (info is the large one)
    return Aabb::fromFeet(m.pos, info.width * s, info.height * s);
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
    if (type == MobType::MagmaCube) { // wiki: Magma Cube - sizes 1, 2, 4 at spawn; health size^2
        const uint32_t r = rng.nextInt(3);
        m.size = uint8_t(1u << r);
        m.health = float(m.size * m.size);
    }
    return m;
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
    // Walking: horizontal speed approaches `wish` (blocks/tick) with ground friction.
    const double friction = m.onGround ? kGroundFriction : kAirFriction;
    const double accel = m.onGround ? (1.0 - kGroundFriction) : 0.02 / 0.1 * (1.0 - kGroundFriction) * 0.25;
    m.vel.x = m.vel.x * friction + wish.x * accel / (1.0 - friction + 1e-9) * (1.0 - friction);
    m.vel.z = m.vel.z * friction + wish.z * accel / (1.0 - friction + 1e-9) * (1.0 - friction);
    if (m.type == MobType::Spider && m.climbing) {
        m.vel.y = 0.2; // spiders climb walls (wiki: Spider)
        m.fallDistance = 0.0f;
    }
    if (m.type == MobType::Strider && fluid.lava) {
        // Striders stand on lava (wiki): it holds them up like ground.
        m.vel.y = std::max(m.vel.y, 0.0) * 0.5 + 0.04;
        m.onGround = true;
    } else if (mobInfo(m.type).flies) {
        // Ghasts and blazes fly: velocity eases toward the wish (with its height), no
        // gravity (our motion model).
        m.vel = m.vel * 0.9 + wish * 0.1;
    } else if (inWater) {
        // Cows swim up to the surface; zombies sink (wiki: Zombie - they sink and later
        // become drowned).
        m.vel.y = m.vel.y * 0.8 + (m.type == MobType::Zombie ? -0.02 : 0.04);
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
    auto gather = [&](const Aabb& region) {
        m_boxes.clear();
        for (int x = int(std::floor(region.min.x)); x <= int(std::floor(region.max.x)); ++x)
            for (int y = int(std::floor(region.min.y)); y <= int(std::floor(region.max.y)); ++y)
                for (int z = int(std::floor(region.min.z)); z <= int(std::floor(region.max.z)); ++z)
                    if (solidAt(world, x, y, z))
                        m_boxes.push_back({{double(x), double(y), double(z)}, {x + 1.0, y + 1.0, z + 1.0}});
    };
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
    gather(start.expandedTowards(m.vel).expandedTowards({0, kStep, 0}));
    glm::dvec3 moved = slide(start, m.vel);
    const bool blockedH = moved.x != m.vel.x || moved.z != m.vel.z;
    if (blockedH && m.onGround) { // try stepping up (slabs, snow)
        const glm::dvec3 up = slide(start, {0, kStep, 0});
        const Aabb raised = start.moved(up);
        glm::dvec3 across = slide(raised, {m.vel.x, 0, m.vel.z});
        const Aabb over = raised.moved(across);
        const glm::dvec3 down = slide(over, {0, -kStep, 0});
        const glm::dvec3 stepped = up + across + down;
        if (stepped.x * stepped.x + stepped.z * stepped.z > moved.x * moved.x + moved.z * moved.z + 1e-9)
            moved = stepped;
    }
    m.onGround = m.vel.y < 0.0 && moved.y != m.vel.y;
    // Falls (wiki: Fall damage): 1 per block beyond 3 on landing; water breaks them.
    if (inWater) {
        m.fallDistance = 0.0f;
    } else if (moved.y < 0.0) {
        if (m.type != MobType::Chicken && m.type != MobType::MagmaCube && !mobInfo(m.type).flies)
            m.fallDistance -= static_cast<float>(moved.y); // (chickens, magma cubes, fliers: no fall damage)
    }
    if (m.onGround) {
        const float damage = std::ceil(m.fallDistance - 3.0f);
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
    const MobInfo& info = mobInfo(m.type);
    if (!info.hostile) animalUpkeep(ctx, m);
    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 toPlayer = playerPos - m.pos;
    const double playerDist2 = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
    double speed = info.speed * 0.5; // blocks per tick at speed modifier 1 (our estimate)
    bool chase = false;

    // Follow range (wiki): zombies notice the player within 35 blocks, skeletons,
    // creepers and spiders within 16; endermen only when angered (64).
    const double follow = m.type == MobType::Zombie ? 35.0 : m.type == MobType::Enderman ? 64.0 : 16.0;
    if (!info.hostile || !ctx.survival || ctx.playerDead || playerDist2 >= follow * follow || !mayTarget(ctx, m)) {
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
    physics(ctx.world, m, wish, jump);

    // Melee (wiki: Zombie - 3 damage on normal, once a second, reach ~ width*2).
    if (m.attackCooldown > 0) --m.attackCooldown;
    if (chase && m.attackCooldown == 0 && info.attackDamage > 0.0f) {
        const double reach = info.width * 2.0 + 0.6;
        if (playerDist2 < reach * reach && box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.8, 0, 0.8),
                                                                  ctx.player.box().max + glm::dvec3(0.8, 0, 0.8)})) {
            if (ctx.vitals.attacked(info.attackDamage, &m.pos)) ctx.player.knockback(toPlayer.x, toPlayer.z);
            m.attackCooldown = 20;
        }
    }

    if (info.hostile) monsterTick(ctx, m, chase, playerDist2);

    // Undead burn in daylight under open sky (wiki: Zombie, Skeleton): 1 damage a second.
    if (m.type == MobType::Zombie || m.type == MobType::Skeleton) {
        const BlockPos head{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 1.6)), int(std::floor(m.pos.z))};
        const Chunk* c = ctx.world.chunk(head.chunk());
        const bool day = ctx.skyDarken < 4.0f;
        const bool sky = c && c->lit() && c->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z)) >= 15;
        const bool wet = blockRegistry().blockOf(ctx.world.getBlock(head)) == blocks::Water;
        // Re-lit to 8 s while in the sun; refreshed once a second so the 1-per-second
        // damage clock below keeps running.
        if (day && sky && !wet && m.fireTicks <= 140) m.fireTicks = 160;
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
    m.health -= damage;
    m.hurtTime = 10;
    m.noPlayerTicks = 0;                                 // damage resets the despawn clock
    m.lastHurtByPlayer = true;                           // (Mobs::attack: the player's hits)
    if (!mobInfo(m.type).hostile) m.panicTicks = 100;    // passive mobs flee (wiki: Cow)
    if (m.type == MobType::Piglin) m.admireTicks = 0; // a hit takes the ingot back (wiki: Bartering)
    if (m.type == MobType::Spider || m.type == MobType::Enderman || m.type == MobType::Piglin) { // provoked (wiki)
        m.angry = true;
        m.targeting = true;
        m.angerTicks = 600;
    }
    if (m.type == MobType::ZombifiedPiglin) m.angerAlert = true; // (the herd joins in: Mobs::tick)
    const glm::dvec2 d(m.pos.x - from.x, m.pos.z - from.z);
    const double l = glm::length(d);
    if (l > 1e-6) { // wiki: Knockback - 0.4 away; lifted only when on the ground
        m.vel.x = m.vel.x / 2.0 + d.x / l * 0.4;
        m.vel.z = m.vel.z / 2.0 + d.y / l * 0.4;
        if (m.onGround) m.vel.y = std::min(0.4, m.vel.y / 2.0 + 0.4);
    }
}

void Mobs::die(Context& ctx, MobData& m) {
    if (m.type == MobType::EndCrystal) {
        // Any damage blows it up: power 6, no fire (wiki: End Crystal); nearby crystals
        // caught in the blast go off too.
        m.deathTime = 19; // gone next tick
        m_scratchEdits.clear();
        std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
        ExplosionTargets t;
        if (ctx.survival && !ctx.playerDead) {
            t.player = &ctx.player;
            t.vitals = &ctx.vitals;
        }
        m_explosion.explode(ctx.world, m.pos + glm::dvec3(0, 1.0, 0), 6.0f, ctx.rng, ctx.items, changed, t);
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
        static Cached cache[32] = {};
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
    // Experience when the player killed it (wiki: Experience): monsters 5, animals 1-3.
    // (wiki: blazes 10, magma cubes their size)
    if (ctx.orbs && m.lastHurtByPlayer)
        ctx.orbs->drop(m.pos + glm::dvec3(0, 0.5, 0),
                       m.type == MobType::Blaze       ? 10
                       : m.type == MobType::MagmaCube ? int(m.size)
                       : mobInfo(m.type).hostile      ? 5
                                                      : 1 + static_cast<int>(ctx.rng.nextInt(3)),
                       ctx.rng);
    const bool burning = m.fireTicks > 0; // meat drops cooked
    switch (m.type) {
    case MobType::Cow:
        drop(burning ? "cooked_beef" : "beef", 1, 3);
        drop("leather", 0, 2);
        break;
    case MobType::Zombie: drop("rotten_flesh", 0, 2); break;
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
    case MobType::Creeper: drop("gunpowder", 0, 2); break; // wiki: Creeper
    case MobType::Spider: // wiki: Spider - string 0-2, spider eye 1 in 3
        drop("string", 0, 2);
        if (m.lastHurtByPlayer && ctx.rng.nextInt(3) == 0) drop("spider_eye", 1, 1); // player kills only
        break;
    case MobType::Enderman: // wiki: Enderman - ender pearl 0-1, and the block it carried
        drop("ender_pearl", 0, 1);
        if (m.carried)
            if (const ItemId it = items.blockItem(blockRegistry().blockOf(m.carried)))
                ctx.items.spawn(m.pos + glm::dvec3(0, 1, 0), {it, 1}, ctx.rng);
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
    m_striders = 0;
    m_angerAlertCount = 0;
    const glm::dvec3 playerPos = ctx.player.position();
    const ChunkPos playerChunk{blockToChunk(int(std::floor(playerPos.x))), blockToChunk(int(std::floor(playerPos.z)))};
    ctx.world.forEachTickingChunk([&](Chunk& chunk) {
        // Only chunks within the simulation distance tick their mobs (vanilla: entity-
        // ticking chunks); farther mobs keep their state.
        if (std::abs(chunk.pos().x - playerChunk.x) > m_simulationDistance ||
            std::abs(chunk.pos().z - playerChunk.z) > m_simulationDistance)
            return;
        if (!chunk.spawners().empty()) tickSpawners(ctx, chunk);
        auto& mobs = chunk.mobs();
        for (size_t i = 0; i < mobs.size();) {
            MobData& m = mobs[i];
            m.prevPos = m.pos;
            m.prevYaw = m.yaw;
            m.prevHeadYaw = m.headYaw;
            m.prevPitch = m.pitch;
            if (m.hurtTime > 0) --m.hurtTime;
            bool remove = false;
            if (m.angerAlert) { // (also from one killed by the hit)
                m.angerAlert = false;
                if (m_angerAlertCount < int(m_angerAlerts.size())) m_angerAlerts[size_t(m_angerAlertCount++)] = m.pos;
            }
            if (m.health <= 0.0f) { // loot at the moment of death, then the death animation
                if (++m.deathTime == 1) die(ctx, m);
                if (m.deathTime >= 20) remove = true;
            } else {
                ai(ctx, m);
                if (mobInfo(m.type).hostile) ++m_hostiles;
                m_striders += m.type == MobType::Strider;
                // Despawning (wiki: Spawn › Despawning): hostiles beyond 128 blocks
                // vanish; beyond 32 they may after 30 s without a player near.
                const double d2 = glm::dot(m.pos - playerPos, m.pos - playerPos);
                if (mobInfo(m.type).hostile && !m.persistent) {
                    if (d2 > 128.0 * 128.0) remove = true;
                    else if (d2 > 32.0 * 32.0 && ++m.noPlayerTicks > 600 && ctx.rng.nextInt(800) == 0) remove = true;
                    else if (d2 <= 32.0 * 32.0) m.noPlayerTicks = 0;
                }
                if (m.pos.y < ctx.world.height().minY - 64) remove = true; // fell out of the world
            }
            const ChunkPos now{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
            if (!remove && !(now == chunk.pos())) {
                if (ctx.world.chunk(now)) {
                    m_moves.push_back({now, m});
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
                        }
    }
    if (ctx.naturalSpawning) {
        if (ctx.world.isUltrawarm()) spawnNether(ctx);
        else spawnHostiles(ctx);
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
    if (c->blockLight(lx, y, lz) > 0) return;
    const int sky = c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken);
    if (sky > static_cast<int>(ctx.rng.nextInt(8))) return;
    // Which monster: vanilla's Overworld weights (wiki: Spawn › Java Edition) - zombie 95,
    // skeleton 100, creeper 100, spider 100, enderman 10 - in a group of up to 4.
    const uint32_t roll = ctx.rng.nextInt(405);
    const MobType kind = roll < 95    ? MobType::Zombie
                         : roll < 195 ? MobType::Skeleton
                         : roll < 295 ? MobType::Creeper
                         : roll < 395 ? MobType::Spider
                                      : MobType::Enderman;
    const int group = 1 + static_cast<int>(ctx.rng.nextInt(4));
    for (int i = 0; i < group && m_hostiles < 70; ++i) {
        const int gx = x + static_cast<int>(ctx.rng.nextInt(5)) - 2, gz = z + static_cast<int>(ctx.rng.nextInt(5)) - 2;
        if (!canSpawnAt(ctx.world, gx, y, gz)) continue;
        if (kind == MobType::Enderman && solidAt(ctx.world, gx, y + 2, gz)) continue; // 3 tall
        if (add(ctx.world, make(kind, {gx + 0.5, double(y), gz + 0.5}, ctx.rng))) ++m_hostiles;
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
                const Aabb b = box(m);
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
    return best;
}

} // namespace mc
