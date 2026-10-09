// Bats (M29.1c; wiki: Bat). Part of Mobs.
//
// A bat flutters about at random, changing its course every second or so, and roosts
// hanging upside down under a solid block; a player coming within 4 blocks (or the block
// going) wakes it. Bats spawn in the dark below sea level (light at most a random 0..3),
// up to 15 near the player, in groups of 8.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {
bool solidBlock(const World& w, int x, int y, int z) { return blockRegistry().opaqueCube(w.getBlock({x, y, z})); }
} // namespace

bool Mobs::batAi(Context& ctx, MobData& m) {
    if (m.type != MobType::Bat) return false;
    const int bx = int(std::floor(m.pos.x)), bz = int(std::floor(m.pos.z));
    const int above = int(std::floor(m.pos.y + mobInfo(m.type).height + 0.05));
    if (m.sitting) { // roosting: still, until disturbed
        m.vel = glm::dvec3(0.0);
        const double d = glm::length(ctx.player.position() - m.pos);
        if (!solidBlock(ctx.world, bx, above, bz) || (!ctx.playerDead && d < 4.0) || m.hurtTime > 0) {
            m.sitting = false;
            m.vel.y = -0.1;
        }
        return true;
    }
    // A new heading now and then (wiki: about every 1-3 s), mostly level, a little up or down.
    if (--m.goalTicks <= 0 || glm::length(m.goal - m.pos) < 1.0) {
        m.goal = m.pos + glm::dvec3(ctx.rng.nextDouble() * 14.0 - 7.0, ctx.rng.nextDouble() * 6.0 - 2.0,
                                    ctx.rng.nextDouble() * 14.0 - 7.0);
        m.goalTicks = int(20 + ctx.rng.nextInt(40));
    }
    const glm::dvec3 to = m.goal - m.pos;
    const double l = glm::length(to);
    const glm::dvec3 wish = l > 1e-6 ? to / l * 0.12 : glm::dvec3(0.0);
    if (l > 1e-6) m.yaw = m.headYaw = float(std::atan2(-to.x, to.z) * 180.0 / 3.14159265358979);
    physics(ctx.world, m, wish, false);
    // Under a solid block it may settle to roost.
    if (solidBlock(ctx.world, bx, above, bz) && ctx.rng.nextInt(100) == 0) {
        m.sitting = true;
        m.pos.y = double(above) - mobInfo(m.type).height;
        m.vel = glm::dvec3(0.0);
    }
    return true;
}

void Mobs::spawnBats(Context& ctx) {
    if (m_bats >= 15 || ctx.rng.nextInt(40) != 0) return; // (a try every 2 s on average)
    const glm::dvec3 p = ctx.player.position();
    const int x = int(std::floor(p.x)) + int(ctx.rng.nextInt(49)) - 24;
    const int z = int(std::floor(p.z)) + int(ctx.rng.nextInt(49)) - 24;
    const int y = int(std::floor(p.y)) + int(ctx.rng.nextInt(33)) - 16;
    if (y >= 63 || !ctx.world.isInHeight(y)) return; // (below sea level)
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit()) return;
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    if (c->get(lx, y, lz) != 0 || c->get(lx, y + 1, lz) != 0) return;
    if (std::max(c->skyLight(lx, y, lz), c->blockLight(lx, y, lz)) > int(ctx.rng.nextInt(4))) return;
    const double dx = x + 0.5 - p.x, dz = z + 0.5 - p.z;
    if (dx * dx + dz * dz < 24.0 * 24.0) return;
    for (int i = 0; i < 8 && m_bats < 15; ++i) {
        const int gx = x + int(ctx.rng.nextInt(5)) - 2, gz = z + int(ctx.rng.nextInt(5)) - 2;
        if (ctx.world.getBlock({gx, y, gz}) != 0) continue;
        if (add(ctx.world, make(MobType::Bat, {gx + 0.5, double(y), gz + 0.5}, ctx.rng))) ++m_bats;
    }
}

} // namespace mc
