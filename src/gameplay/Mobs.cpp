#include "gameplay/Mobs.h"

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
    const double s = m.isBaby() ? 0.5 : 1.0; // babies are half size (wiki: Breeding)
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
    if (fluid.fire) { // wiki: Fire - 1 a tick (hurt cooldown), 8 s alight
        if (m.hurtTime == 0 && m.deathTime == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        if (m.fireTicks < 160) m.fireTicks = 160;
    }
    if (inWater) m.fireTicks = 0; // water puts out burning mobs (wiki: Fire)
    if (fluid.lava) { // wiki: Lava - 4 damage (with the hurt cooldown), on fire 15 s
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
    if (inWater) {
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
        if (m.type != MobType::Chicken) m.fallDistance -= static_cast<float>(moved.y); // (chickens: no falls)
    }
    if (m.onGround) {
        const float damage = std::ceil(m.fallDistance - 3.0f);
        if (damage > 0.0f) {
            m.health -= damage;
            m.hurtTime = 10;
        }
        m.fallDistance = 0.0f;
    }
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
    const MobInfo& info = mobInfo(m.type);
    if (!info.hostile) animalUpkeep(ctx, m);
    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 toPlayer = playerPos - m.pos;
    const double playerDist2 = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
    double speed = info.speed * 0.5; // blocks per tick at speed modifier 1 (our estimate)
    bool chase = false;

    if (!info.hostile || !ctx.survival || ctx.playerDead || playerDist2 >= 35.0 * 35.0) {
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

    // Path to the goal (M16.2): chasing mobs repath every 4-10 ticks (vanilla
    // recomputes a moving target's path every few ticks), others when the goal changes.
    const glm::ivec3 feet{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 0.01)), int(std::floor(m.pos.z))};
    const glm::ivec3 goalCell{int(std::floor(m.goal.x)), int(std::floor(m.goal.y + 0.01)), int(std::floor(m.goal.z))};
    if (m.repathTicks > 0) --m.repathTicks;
    if (goalCell != feet && ((chase && m.repathTicks == 0) || (!chase && goalCell != m.pathGoal))) {
        const int heightCells = int(std::ceil(info.height));
        // Search budget: vanilla visits up to follow range x 16 nodes (zombie 35).
        m.pathLength = uint8_t(m_pathfinder.find(ctx.world, feet, goalCell, heightCells, chase ? 560 : 200,
                                                   m.path.data(), MobData::kMaxPath));
        m.pathIndex = 0;
        m.pathGoal = goalCell;
        m.repathTicks = int16_t(4 + ctx.rng.nextInt(7));
        if (!chase && m.pathLength > 0) { // a wander target it can't reach: stop where the path ends
            const glm::ivec3 end = m.path[size_t(m.pathLength - 1)];
            m.goal = {end.x + 0.5, double(end.y), end.z + 0.5};
            m.pathGoal = end;
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
    if (chase && m.attackCooldown == 0) {
        const double reach = info.width * 2.0 + 0.6;
        if (playerDist2 < reach * reach && box(m).intersects(Aabb{ctx.player.box().min - glm::dvec3(0.8, 0, 0.8),
                                                                  ctx.player.box().max + glm::dvec3(0.8, 0, 0.8)})) {
            if (ctx.vitals.damage(info.attackDamage)) ctx.player.knockback(toPlayer.x, toPlayer.z);
            m.attackCooldown = 20;
        }
    }

    // Undead burn in daylight under open sky (wiki: Zombie): 1 damage a second.
    if (m.type == MobType::Zombie) {
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

void Mobs::attack(MobData& m, float damage, const glm::dvec3& from) {
    if (m.hurtTime > 0 || m.deathTime > 0) return; // 10 ticks of invulnerability
    m.health -= damage;
    m.hurtTime = 10;
    m.noPlayerTicks = 0;                                 // damage resets the despawn clock
    if (!mobInfo(m.type).hostile) m.panicTicks = 100;    // passive mobs flee (wiki: Cow)
    const glm::dvec2 d(m.pos.x - from.x, m.pos.z - from.z);
    const double l = glm::length(d);
    if (l > 1e-6) { // wiki: Knockback - 0.4 away; lifted only when on the ground
        m.vel.x = m.vel.x / 2.0 + d.x / l * 0.4;
        m.vel.z = m.vel.z / 2.0 + d.y / l * 0.4;
        if (m.onGround) m.vel.y = std::min(0.4, m.vel.y / 2.0 + 0.4);
    }
}

void Mobs::die(Context& ctx, MobData& m) {
    // Loot (wiki: Cow - raw beef 1-3, leather 0-2; Zombie - rotten flesh 0-2).
    const auto& items = itemRegistry();
    auto drop = [&](const char* id, int lo, int hi) {
        const int n = lo + static_cast<int>(ctx.rng.nextInt(uint32_t(hi - lo + 1)));
        if (n > 0) ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {*items.find(id), uint8_t(n)}, ctx.rng);
    };
    if (m.isBaby()) return; // babies drop nothing (wiki: Breeding)
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
    const glm::dvec3 playerPos = ctx.player.position();
    const ChunkPos playerChunk{blockToChunk(int(std::floor(playerPos.x))), blockToChunk(int(std::floor(playerPos.z)))};
    ctx.world.forEachTickingChunk([&](Chunk& chunk) {
        // Only chunks within the simulation distance tick their mobs (vanilla: entity-
        // ticking chunks); farther mobs keep their state.
        if (std::abs(chunk.pos().x - playerChunk.x) > m_simulationDistance ||
            std::abs(chunk.pos().z - playerChunk.z) > m_simulationDistance)
            return;
        auto& mobs = chunk.mobs();
        for (size_t i = 0; i < mobs.size();) {
            MobData& m = mobs[i];
            m.prevPos = m.pos;
            m.prevYaw = m.yaw;
            m.prevHeadYaw = m.headYaw;
            m.prevPitch = m.pitch;
            if (m.hurtTime > 0) --m.hurtTime;
            bool remove = false;
            if (m.health <= 0.0f) { // loot at the moment of death, then the death animation
                if (++m.deathTime == 1) die(ctx, m);
                if (m.deathTime >= 20) remove = true;
            } else {
                ai(ctx, m);
                if (mobInfo(m.type).hostile) ++m_hostiles;
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
    if (ctx.naturalSpawning) spawnHostiles(ctx);
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
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    if (c->blockLight(lx, y, lz) > 0) return;
    const int sky = c->skyLight(lx, y, lz) - static_cast<int>(ctx.skyDarken);
    if (sky > static_cast<int>(ctx.rng.nextInt(8))) return;
    // A small group (wiki: Zombie - groups of up to 4).
    const int group = 1 + static_cast<int>(ctx.rng.nextInt(4));
    for (int i = 0; i < group && m_hostiles < 70; ++i) {
        const int gx = x + static_cast<int>(ctx.rng.nextInt(5)) - 2, gz = z + static_cast<int>(ctx.rng.nextInt(5)) - 2;
        if (!canSpawnAt(ctx.world, gx, y, gz)) continue;
        if (add(ctx.world, make(MobType::Zombie, {gx + 0.5, double(y), gz + 0.5}, ctx.rng))) ++m_hostiles;
    }
}

std::optional<Mobs::MobHit> Mobs::raycast(World& world, const glm::dvec3& eye, const glm::dvec3& dir, double reach) {
    std::optional<MobHit> best;
    const ChunkPos centre{blockToChunk(int(std::floor(eye.x))), blockToChunk(int(std::floor(eye.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk* c = world.chunk({centre.x + dx, centre.z + dz});
            if (!c) continue;
            for (size_t i = 0; i < c->mobs().size(); ++i) {
                const MobData& m = c->mobs()[i];
                if (m.health <= 0.0f) continue;
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
