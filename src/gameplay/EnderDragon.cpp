// The ender dragon (M20.2; wiki: Ender Dragon, End Crystal, Dragon Fireball). Part of
// Mobs. Its phases keep vanilla's DragonPhase numbers:
//   0 holding pattern - circles the pillars from node to node; after each node it may
//     land (chance 1 in 3 + the crystals left), or strafe the player;
//   1 strafe - flies at the player and spits a dragon fireball (a lingering cloud of
//     dragon's breath where it lands);
//   2 landing approach - down to the top of the exit portal's bedrock column;
//   4 takeoff - back up, then holding or charging;
//   5/6 sitting flaming / scanning - perched: turns to the player, breathes flames in
//     front of it; takes off after 50 damage or about 20 s;
//   8 charging player - dives at the player;
//   9 dying - rises for 10 s (Mobs::tick), then the fight ends (main).
// The nearest end crystal within 32 blocks heals it 1 health every 10 ticks; breaking
// that crystal hurts it by 10. Blocks in its way break, except the End's own
// (end stone, obsidian, bedrock, iron bars, portals). Hits on its head deal full
// damage, anywhere else a quarter + 1; arrows bounce off while it perches.
#include "gameplay/Mobs.h"

#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

namespace {

constexpr uint8_t kHolding = 0, kStrafe = 1, kLandingApproach = 2, kTakeoff = 4, kFlaming = 5,
                  kScanning = 6, kCharging = 8;
constexpr int kNodes = 12;

glm::dvec3 forwardOf(float yaw) {
    const double a = yaw * std::numbers::pi / 180.0;
    return {-std::sin(a), 0.0, std::cos(a)};
}

// The holding pattern's ring: 12 points 60 blocks out at heights 76-96 (vanilla: an
// outer ring of 12, an inner one of 8 and 4 near the middle; ours: one ring).
glm::dvec3 nodePos(int i) {
    const double a = 2.0 * std::numbers::pi * i / kNodes;
    return {60.0 * std::cos(a), 76.0 + 10.0 * (i % 3), 60.0 * std::sin(a)};
}

int nearestNode(const glm::dvec3& p) {
    int best = 0;
    double bd = 1e30;
    for (int i = 0; i < kNodes; ++i) {
        const glm::dvec3 d = nodePos(i) - p;
        if (glm::dot(d, d) < bd) {
            bd = glm::dot(d, d);
            best = i;
        }
    }
    return best;
}

bool dragonImmune(BlockId b) {
    // (vanilla #dragon_immune, the blocks we have)
    return b == blocks::Bedrock || b == blocks::Obsidian || b == blocks::EndStone ||
           b == blocks::IronBars || b == blocks::EndPortal || b == blocks::EndPortalFrame ||
           b == blocks::EndGateway || b == blocks::Water || b == blocks::Lava;
}

float approachAngle(float from, float to, float step) {
    float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    d = std::clamp(d, -step, step);
    return from + d;
}

} // namespace

glm::dvec3 Mobs::dragonHead(const MobData& m) {
    return m.pos + forwardOf(m.yaw) * 5.0 + glm::dvec3(0.0, 0.75, 0.0);
}

float Mobs::dragonDamage(const MobData& m, float damage, const glm::dvec3& at) {
    const glm::dvec3 h = dragonHead(m);
    const glm::dvec3 d = glm::abs(at - h);
    if (d.x < 1.5 && d.y < 1.5 && d.z < 1.5) return damage; // the head: full damage
    return damage / 4.0f + std::min(1.0f, damage);          // wiki: other parts
}

void Mobs::dragonAi(Context& ctx, MobData& m) {
    World& world = ctx.world;
    ++m.phaseTicks;
    const MobInfo& info = mobInfo(m.type);
    const bool perched = m.phase == kFlaming || m.phase == kScanning;
    const float taken = std::max(0.0f, m.lastHealth - m.health);
    m.lastHealth = m.health;
    if (perched) m.perchDamage += taken;
    m.limbSwing += perched ? 0.05f : 0.25f; // (wing beats)
    m.limbSwingAmount = 1.0f;

    // Healing from the nearest end crystal within 32 blocks.
    m.hasBeam = false;
    double best = 32.0 * 32.0;
    const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))),
                      blockToChunk(int(std::floor(m.pos.z)))};
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (const Chunk* c = world.chunk({c0.x + dx, c0.z + dz}))
                for (const MobData& o : c->mobs())
                    if (o.type == MobType::EndCrystal && o.health > 0.0f) {
                        const glm::dvec3 d = o.pos - m.pos;
                        if (glm::dot(d, d) < best) {
                            best = glm::dot(d, d);
                            m.beam = o.pos + glm::dvec3(0.0, 1.4, 0.0);
                            m.hasBeam = true;
                        }
                    }
    if (m.hasBeam && ++m.goalTicks % 10 == 0) {
        m.health = std::min(info.maxHealth, m.health + 1.0f);
        m.lastHealth = m.health;
    }

    const glm::dvec3 playerPos = ctx.player.position();
    const glm::dvec3 playerEye = playerPos + glm::dvec3(0.0, 1.62, 0.0);
    const bool canTarget = ctx.survival && !ctx.playerDead &&
                           playerPos.x * playerPos.x + playerPos.z * playerPos.z < 150.0 * 150.0;
    // The top of the exit portal's bedrock column, where it perches.
    auto perchPoint = [&]() {
        int y = world.height().maxY();
        while (y > world.height().minY && world.getBlock({0, y, 0}) == 0)
            --y;
        return glm::dvec3(0.5, y + 1.0, 0.5);
    };
    auto fly = [&](const glm::dvec3& goal, double speed) {
        const glm::dvec3 to = goal - m.pos;
        const double len = glm::length(to);
        const glm::dvec3 want = len > 1e-6 ? to / len * std::min(speed, len) : glm::dvec3(0.0);
        m.vel += (want - m.vel) * 0.1;
        m.pos += m.vel;
        if (m.vel.x * m.vel.x + m.vel.z * m.vel.z > 1e-4)
            m.yaw = approachAngle(
                m.yaw, float(std::atan2(-m.vel.x, m.vel.z) * 180.0 / std::numbers::pi), 6.0f);
        m.headYaw = m.yaw;
        return len;
    };
    auto setPhase = [&](uint8_t p) {
        m.phase = p;
        m.phaseTicks = 0;
    };

    switch (m.phase) {
    case kStrafe: {
        const double d = fly(playerEye + glm::dvec3(0.0, 6.0, 0.0), 0.7);
        const glm::dvec3 head = dragonHead(m);
        const glm::dvec3 to = playerEye - head;
        const double dist = glm::length(to);
        if (!canTarget || m.phaseTicks > 200) {
            m.node = static_cast<uint8_t>(nearestNode(m.pos));
            setPhase(kHolding);
        } else if (m.phaseTicks > 20 && dist < 64.0 &&
                   glm::dot(forwardOf(m.yaw), to / dist) > 0.8 &&
                   !raycastBlocks(world, head, to / dist, dist)) {
            if (ctx.projectiles)
                ctx.projectiles->shoot(ProjectileKind::DragonFireball, head, to / dist, 0.8, 0.0,
                                       false, false, ctx.rng, m.uuidHi);
            m.node = static_cast<uint8_t>(nearestNode(m.pos));
            setPhase(kHolding);
        } else if (d < 4.0) {
            setPhase(kHolding);
        }
        break;
    }
    case kLandingApproach: {
        if (m.phaseTicks == 1 || m.goal.y < 1.0)
            m.goal = perchPoint(); // (found once per landing; goals aren't saved)
        const glm::dvec3 perch = m.goal;
        if (fly(perch, 0.5) < 1.5) {
            m.pos = perch;
            m.vel = glm::dvec3(0.0);
            m.perchDamage = 0.0f;
            m.chargeTicks = 0; // (time perched)
            m.volley = 0;      // (breaths)
            setPhase(kScanning);
        }
        break;
    }
    case kScanning:
    case kFlaming: {
        m.vel = glm::dvec3(0.0);
        ++m.chargeTicks;
        m.yaw = approachAngle(m.yaw,
                              float(std::atan2(-(playerPos.x - m.pos.x), playerPos.z - m.pos.z) *
                                    180.0 / std::numbers::pi),
                              4.0f);
        m.headYaw = m.yaw;
        if (m.phase == kScanning && m.phaseTicks % 80 == 40 &&
            canTarget) { // (scans, then breathes)
            setPhase(kFlaming);
        } else if (m.phase == kFlaming) {
            if (m.phaseTicks == 10 && ctx.projectiles) { // flames on the ground in front of it
                glm::dvec3 at = dragonHead(m) + forwardOf(m.yaw) * 3.0;
                at.y = m.pos.y - 0.9;
                ctx.projectiles->addCloud(at, 2.5f, 60); // (wiki: 3 s)
                ++m.volley;                              // (breaths this perch)
            }
            if (m.phaseTicks > 60) setPhase(kScanning);
        }
        // Takes off after 50 damage, 4 breaths, or with no player within 150 (wiki).
        const bool alone =
            playerPos.x * playerPos.x + playerPos.z * playerPos.z > 150.0 * 150.0 || !canTarget;
        if (m.perchDamage >= 50.0f || m.volley >= 4 || (alone && m.chargeTicks > 100))
            setPhase(kTakeoff);
        break;
    }
    case kTakeoff:
        if (m.phaseTicks == 1 || m.goal.y < 1.0) m.goal = perchPoint() + glm::dvec3(0.0, 25.0, 0.0);
        if (fly(m.goal, 0.5) < 4.0 || m.phaseTicks > 200) {
            m.node = static_cast<uint8_t>(nearestNode(m.pos));
            setPhase(canTarget && ctx.rng.nextInt(2) == 0 ? kCharging : kHolding);
        }
        break;
    case kCharging:
        if (!canTarget || fly(playerPos, 0.9) < 4.0 || m.phaseTicks > 100) {
            m.node = static_cast<uint8_t>(nearestNode(m.pos));
            setPhase(kHolding);
        }
        break;
    default: { // holding pattern (and anything unknown from a save)
        m.phase = kHolding;
        if (fly(nodePos(m.node), 0.6) < 8.0) {
            int crystals = 0;
            for (int dz = -5; dz <= 5; ++dz)
                for (int dx = -5; dx <= 5; ++dx)
                    if (const Chunk* c = world.chunk({dx, dz}))
                        for (const MobData& o : c->mobs())
                            crystals += o.type == MobType::EndCrystal && o.health > 0.0f;
            if (ctx.rng.nextInt(uint32_t(crystals + 3)) == 0) {
                setPhase(kLandingApproach);
            } else if (canTarget && ctx.rng.nextInt(5) == 0) {
                setPhase(kStrafe);
            } else {
                if (ctx.rng.nextInt(8) == 0) m.nodeStep = static_cast<int8_t>(-m.nodeStep);
                m.node = static_cast<uint8_t>((m.node + m.nodeStep + kNodes) % kNodes);
            }
        }
        break;
    }
    }
    m.pitch = 0.0f;

    // Blocks in its way break (no drops), the End's own excepted.
    if (m.phase != kScanning && m.phase != kFlaming) {
        const Aabb b = box(m);
        for (int y = int(std::floor(b.min.y)); y <= int(std::floor(b.max.y)); ++y)
            for (int z = int(std::floor(b.min.z)); z <= int(std::floor(b.max.z)); ++z)
                for (int x = int(std::floor(b.min.x)); x <= int(std::floor(b.max.x)); ++x) {
                    if (!world.isInHeight(y)) continue;
                    const BlockStateId s = world.getBlock({x, y, z});
                    if (s == 0 || dragonImmune(blockRegistry().blockOf(s))) continue;
                    world.updateBlock({x, y, z}, 0);
                    if (ctx.edits) ctx.edits->push_back({x, y, z});
                }
    }

    // Contact: its head bites (10), its body and wings knock the player away (5).
    if (m.attackCooldown > 0) --m.attackCooldown;
    if (canTarget && m.attackCooldown == 0) {
        const glm::dvec3 head = dragonHead(m);
        const Aabb headBox{head - glm::dvec3(1.0), head + glm::dvec3(1.0)};
        const Aabb pb = ctx.player.box();
        const glm::dvec3 away = playerPos - m.pos;
        if (pb.intersects(headBox)) {
            if (ctx.vitals.attacked(info.attackDamage, &head))
                ctx.player.knockback(away.x, away.z, 1.0);
            m.attackCooldown = 10;
        } else if (!perched && pb.intersects(box(m).inflated(1.0))) {
            if (ctx.vitals.attacked(5.0f, &m.pos)) ctx.player.knockback(away.x, away.z, 2.0);
            m.attackCooldown = 10;
        }
    }
}

} // namespace mc
