// Minecarts (M21.4b; wiki: Minecart, Powered Rail). Part of Mobs. On a rail a cart
// follows its line (curves as the diagonal between their ends): slopes pull it down by
// 0.0078125 a tick, powered rails push it on by 0.06 (or kick a stopped cart away from
// a block at one end), unpowered ones brake it by half; drag 0.997 a tick with a rider
// (0.96 empty); at most 0.4 blocks a tick. Off rails it falls and slides (friction 0.5
// on the ground, 0.95 in the air).
#include "gameplay/Mobs.h"

#include "gameplay/BlockCollision.h"
#include "world/Blocks.h"
#include "world/Rails.h"

#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

namespace {

glm::dvec2 exitPoint(Direction d) { // from the cell centre to the middle of that edge
    const glm::ivec3 n = normal(d);
    return {n.x * 0.5, n.z * 0.5};
}

} // namespace

bool Mobs::placeMinecart(World& world, const BlockPos& rail, Xoroshiro& rng) {
    if (!isRail(blockRegistry().blockOf(world.getBlock(rail)))) return false;
    MobData m = make(MobType::Minecart, {rail.x + 0.5, rail.y + 0.0625, rail.z + 0.5}, rng);
    m.persistent = true;
    return add(world, m);
}

void Mobs::minecartTick(Context& ctx, MobData& m) {
    World& world = ctx.world;
    const auto& r = blockRegistry();
    // The rail it is on: in its cell, or the cell below (going down a slope).
    BlockPos cell{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)), int(std::floor(m.pos.z))};
    BlockStateId rs = world.getBlock(cell);
    if (!isRail(r.blockOf(rs))) {
        const BlockPos below{cell.x, cell.y - 1, cell.z};
        if (isRail(r.blockOf(world.getBlock(below)))) {
            cell = below;
            rs = world.getBlock(cell);
        }
    }
    const BlockId rail = r.blockOf(rs);
    if (isRail(rail)) {
        const int shape = railShapeOf(rs);
        const RailExits ex = railExits(shape);
        const glm::dvec2 a = exitPoint(ex.a), b = exitPoint(ex.b);
        const double len = glm::length(b - a);
        const glm::dvec2 along = (b - a) / len; // from end a to end b
        double speed = m.vel.x * along.x + m.vel.z * along.y;
        if (ex.aUp) speed += 0.0078125; // downhill is toward b (wiki: slopes)
        if (rail == BlockId(blocks::PoweredRail)) {
            if (r.get(rs, properties::powered) == 0) {
                if (std::abs(speed) > 0.01) {
                    speed += speed > 0 ? 0.06 : -0.06;
                } else { // a stopped cart is kicked away from a solid block at an end
                    const glm::ivec3 na = normal(ex.a), nb = normal(ex.b);
                    if (r.opaqueCube(world.getBlock({cell.x + na.x, cell.y, cell.z + na.z})))
                        speed = 0.02;
                    else if (r.opaqueCube(world.getBlock({cell.x + nb.x, cell.y, cell.z + nb.z})))
                        speed = -0.02;
                }
            } else {
                speed *= 0.5;
                if (std::abs(speed) < 0.03) speed = 0.0;
            }
        }
        speed *= m.ridden ? 0.997 : 0.96;
        speed = std::clamp(speed, -0.4, 0.4);
        // Onto the rail's line, then along it.
        const glm::dvec2 centre(cell.x + 0.5, cell.z + 0.5);
        const glm::dvec2 start = centre + a;
        double t = (m.pos.x - start.x) * along.x + (m.pos.z - start.y) * along.y;
        t = std::clamp(t, 0.0, len) + speed;
        const glm::dvec2 at = start + along * t;
        const double frac = std::clamp(t / len, 0.0, 1.0);
        m.pos = {at.x, cell.y + (ex.aUp ? 1.0 - frac : 0.0) + 0.0625, at.y};
        m.vel = {along.x * speed, 0.0, along.y * speed};
        m.onGround = true;
        if (std::abs(speed) < 1e-3) // standing still: lined up with the rail
            m.yaw = float(std::atan2(-along.x, along.y) * 180.0 / std::numbers::pi);
    } else {
        // Off the rails: fall and slide against blocks.
        m.vel.y -= 0.04;
        const Aabb box = Aabb::fromFeet(m.pos, 0.98, 0.7);
        gatherBlockBoxes(world, box.expandedTowards(m.vel), m_boxes);
        glm::dvec3 d = m.vel;
        Aabb bx = box;
        bool ground = false;
        for (int axis : {1, 0, 2}) {
            const double wanted = d[axis];
            for (const Aabb& w : m_boxes)
                d[axis] = bx.clip(w, axis, d[axis]);
            if (axis == 1 && wanted < 0.0 && d[1] > wanted) ground = true;
            if (d[axis] != wanted) m.vel[axis] = 0.0;
            glm::dvec3 step(0.0);
            step[axis] = d[axis];
            bx = bx.moved(step);
        }
        m.pos += d;
        m.onGround = ground;
        const double f = ground ? 0.5 : 0.95;
        m.vel.x *= f;
        m.vel.z *= f;
        m.vel.y *= 0.98;
    }
    if (m.vel.x * m.vel.x + m.vel.z * m.vel.z > 1e-6)
        m.yaw = float(std::atan2(-m.vel.x, m.vel.z) * 180.0 / std::numbers::pi);
    m.headYaw = m.yaw;
}

} // namespace mc
