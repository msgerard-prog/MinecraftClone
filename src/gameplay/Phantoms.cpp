// Phantoms (M26.4a; wiki: Phantom, Insomnia). Part of Mobs.
//
// A player who hasn't slept for 3 in-game days (72000 ticks) draws phantoms: at night
// (or in a thunderstorm), under the open sky in the Overworld, a few appear 20-34
// blocks above them every minute or two - the longer awake, the likelier. A phantom
// circles high above its target, then swoops down, bites, and climbs back up. Undead,
// it burns in daylight; it keeps away from cats.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/Raycast.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

bool solid(const World& w, const glm::dvec3& p) {
    return blockRegistry().collides(w.getBlock({int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))}));
}

} // namespace

bool Mobs::phantomAi(Context& ctx, MobData& m) {
    if (m.type != MobType::Phantom) return false;
    const glm::dvec3 player = ctx.player.position();
    const bool target = ctx.survival && !ctx.playerDead && glm::length(player - m.pos) < 64.0;
    // Burning by day under the open sky (as the other undead).
    const BlockPos head{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 0.3)), int(std::floor(m.pos.z))};
    if (const Chunk* c = ctx.world.chunk(head.chunk());
        c && c->lit() && ctx.skyDarken < 4.0f && ctx.world.isInHeight(head.y) &&
        c->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z)) >= 15 && m.fireTicks <= 140)
        m.fireTicks = 160;
    if (m.fireTicks > 0) {
        if (m.fireTicks % 20 == 0) {
            m.health -= 1.0f;
            m.hurtTime = 10;
        }
        --m.fireTicks;
    }
    // A cat within 16 blocks: it climbs away (wiki: Phantom - phantoms avoid cats).
    bool cat = false;
    if (ctx.rng.nextInt(10) == 0) {
        const ChunkPos c0{blockToChunk(head.x), blockToChunk(head.z)};
        for (int dz = -1; dz <= 1 && !cat; ++dz)
            for (int dx = -1; dx <= 1 && !cat; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                    for (const MobData& o : ch->mobs())
                        if ((o.type == MobType::Cat || o.type == MobType::Ocelot) && glm::length(o.pos - m.pos) < 16.0) {
                            cat = true;
                            break;
                        }
    }
    if (cat) {
        m.phase = 2;
        m.phaseTicks = 60;
    }
    glm::dvec3 goal;
    double speed = 0.35;
    if (!target) { // nobody to bother: drift in wide circles where it is
        m.phase = 0;
        goal = m.pos + glm::dvec3(std::cos(m.goalTicks * 0.05) * 3.0, 0.0, std::sin(m.goalTicks * 0.05) * 3.0);
        ++m.goalTicks;
    } else if (m.phase == 1) { // the swoop: straight at the player
        goal = player + glm::dvec3(0.0, 0.9, 0.0);
        speed = 0.6;
        const Aabb reach{ctx.player.box().min - glm::dvec3(0.2), ctx.player.box().max + glm::dvec3(0.2)};
        if (box(m).intersects(reach)) {
            if (ctx.vitals.attacked(mobInfo(m.type).attackDamage, &m.pos)) setPlayerAttacker(m.uuidHi);
            m.phase = 2;
            m.phaseTicks = 30 + int16_t(ctx.rng.nextInt(20));
        } else if (--m.phaseTicks <= 0 || solid(ctx.world, m.pos + glm::normalize(goal - m.pos + glm::dvec3(1e-6)) * 1.0)) {
            m.phase = 2; // (missed, or about to hit a wall)
            m.phaseTicks = 30;
        }
    } else if (m.phase == 2) { // back up
        goal = m.pos + glm::dvec3(0.0, 4.0, 0.0);
        speed = 0.3;
        if (--m.phaseTicks <= 0) {
            m.phase = 0;
            m.chargeTicks = int16_t(100 + ctx.rng.nextInt(100)); // (circles 5-10 s before the next swoop)
        }
    } else { // circling 20 or so blocks above the player
        ++m.goalTicks;
        const double a = m.goalTicks * 0.04 + double(m.uuidLo % 628) / 100.0;
        goal = player + glm::dvec3(std::cos(a) * 12.0, 20.0, std::sin(a) * 12.0);
        if (--m.chargeTicks <= 0) { // swoop if it can see the player
            const glm::dvec3 d = player + glm::dvec3(0.0, 1.0, 0.0) - m.pos;
            const double len = glm::length(d);
            if (len > 1e-6 && !raycastBlocks(ctx.world, m.pos, d / len, len)) {
                m.phase = 1;
                m.phaseTicks = 100;
            } else {
                m.chargeTicks = 40;
            }
        }
    }
    const glm::dvec3 d = goal - m.pos;
    const double len = glm::length(d);
    const glm::dvec3 wish = len > 0.2 ? d / len * speed : glm::dvec3(0.0);
    if (len > 0.2) {
        m.yaw = m.headYaw = float(std::atan2(-d.x, d.z) * 180.0 / 3.14159265358979);
        m.pitch = float(-std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z)) * 180.0 / 3.14159265358979);
    }
    m.limbSwing += 0.4f; // (wing beats)
    physics(ctx.world, m, wish, false);
    return true;
}

void Mobs::spawnPhantoms(Context& ctx) {
    // Every 60-120 s (wiki: Phantom › Spawning): at night or in a thunderstorm, for a
    // survival player under the open sky, awake 72000+ ticks - a chance growing with the
    // time awake - 1-3 phantoms (Normal) 20-34 blocks above.
    if (--m_phantomTicks > 0) return;
    m_phantomTicks = 1200;
    const bool night = ctx.skyDarken >= 4.0f || ctx.thundering;
    if (!night || !ctx.survival || ctx.playerDead || !ctx.spawnPhantoms || ctx.difficulty == 0 || ctx.timeSinceRest < 72000) return;
    m_phantomTicks += int(ctx.rng.nextInt(1200)); // (the game's random only when phantoms may come)
    if (int(ctx.rng.nextInt(uint32_t(std::max(1, ctx.timeSinceRest)))) < 72000) return;
    const glm::dvec3 p = ctx.player.position();
    if (p.y < 63.0) return; // (wiki: only above sea level)
    const BlockPos head{int(std::floor(p.x)), int(std::floor(p.y + 1.6)), int(std::floor(p.z))};
    const Chunk* c = ctx.world.chunk(head.chunk());
    if (!c || !c->lit() || !ctx.world.isInHeight(head.y) ||
        c->skyLight(blockToLocal(head.x), head.y, blockToLocal(head.z)) < 15)
        return;
    const int n = 1 + int(ctx.rng.nextInt(3));
    for (int i = 0; i < n; ++i) {
        const glm::dvec3 at = p + glm::dvec3(ctx.rng.nextDouble() * 20 - 10, 20.0 + ctx.rng.nextInt(15),
                                             ctx.rng.nextDouble() * 20 - 10);
        if (solid(ctx.world, at) || !ctx.world.isInHeight(int(std::floor(at.y)))) continue;
        MobData m = make(MobType::Phantom, at, ctx.rng);
        m.chargeTicks = int16_t(60 + ctx.rng.nextInt(100));
        if (add(ctx.world, m)) ++m_hostiles;
    }
}

} // namespace mc
