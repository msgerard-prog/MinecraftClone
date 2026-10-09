// Minecarts (M21.4b; wiki: Minecart, Powered Rail). Part of Mobs. On a rail a cart
// follows its line (curves as the diagonal between their ends): slopes pull it down by
// 0.0078125 a tick, powered rails push it on by 0.06 (or kick a stopped cart away from
// a block at one end), unpowered ones brake it by half; drag 0.997 a tick with a rider
// (0.96 empty); at most 0.4 blocks a tick. Off rails it falls and slides (friction 0.5
// on the ground, 0.95 in the air).
#include "gameplay/Mobs.h"

#include "gameplay/BlockCollision.h"
#include "gameplay/Hoppers.h"
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

bool Mobs::placeMinecart(World& world, const BlockPos& rail, Xoroshiro& rng, int kind) {
    if (!isRail(blockRegistry().blockOf(world.getBlock(rail)))) return false;
    MobData m = make(MobType::Minecart, {rail.x + 0.5, rail.y + 0.0625, rail.z + 0.5}, rng);
    m.persistent = true;
    m.decor = uint8_t(kind);
    m.strength = uint8_t(cartSlotsOf(kind));
    m.hasChest = m.strength > 0; // (its slots live in the chunk's mob store, as a chest boat's)
    if (m.hasChest) world.chunk(rail.chunk())->addMobStore(m.uuidHi);
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
    cartKindTick(ctx, m, isRail(rail) ? rs : BlockStateId{0}, cell);
}

void Mobs::cartKindTick(Context& ctx, MobData& m, BlockStateId rail, const BlockPos& cell) {
    const auto& r = blockRegistry();
    // A furnace cart (wiki: Minecart with Furnace): while it has fuel (`temper`, 3600 ticks
    // a coal) it pushes itself along its push direction (`home` x/z) at up to 0.2 a tick.
    if (m.decor == 2 && m.temper > 0) {
        --m.temper;
        const glm::dvec3 push(m.home.x, 0.0, m.home.z);
        if (glm::dot(push, push) > 0.0) {
            m.vel += glm::normalize(push) * 0.02;
            const double h = std::sqrt(m.vel.x * m.vel.x + m.vel.z * m.vel.z);
            if (h > 0.2) m.vel *= glm::dvec3(0.2 / h, 1.0, 0.2 / h);
        }
        if (ctx.rng.nextInt(4) == 0)
            ctx.world.levelEvent(LevelEvent::Type::Extinguish, m.pos.x, m.pos.y + 1.0, m.pos.z); // (its smoke)
    }
    // A hopper cart (wiki: Minecart with Hopper): every 4 ticks it picks up a dropped item
    // over it, else one from the container above its rail, and gives one to a hopper or
    // container below its rail. An activator rail powered under it switches it off.
    const bool activator = rail != 0 && r.blockOf(rail) == blocks::ActivatorRail && r.get(rail, properties::powered) == 0;
    if (m.decor == 3 && !activator && (m.age = m.age + 1) % 4 == 0) {
        Chunk* c = ctx.world.chunk(BlockPos{int(std::floor(m.pos.x)), 0, int(std::floor(m.pos.z))}.chunk());
        ItemContents* slots = c ? c->mobStore(m.uuidHi) : nullptr;
        if (!slots && c) slots = &c->addMobStore(m.uuidHi); // (a cart loaded empty)
        if (slots) {
            auto put = [&](const ItemStack& one) {
                for (int i = 0; i < 5; ++i) {
                    ItemStack& s = (*slots)[size_t(i)];
                    if (s.empty()) {
                        s = one;
                        return true;
                    }
                    if (s.sameKind(one) && s.count < itemRegistry().item(s.item).maxStack) {
                        ++s.count;
                        return true;
                    }
                }
                return false;
            };
            bool took = false;
            for (const ItemEntity& it : ctx.items.items())
                if (!took && std::abs(it.pos.x - m.pos.x) < 1.0 && std::abs(it.pos.z - m.pos.z) < 1.0 &&
                    it.pos.y >= m.pos.y - 0.5 && it.pos.y < m.pos.y + 1.5) {
                    ItemStack one = it.stack;
                    one.count = 1;
                    if (put(one)) {
                        ctx.items.takeOne(&it);
                        took = true;
                    }
                    break;
                }
            if (!took) {
                ItemStack one;
                const BlockPos above{cell.x, cell.y + 1, cell.z};
                ItemStack* from = nullptr;
                if (isContainer(ctx.world, above) && extractOne(ctx.world, above, Direction::Down, one, &from) && !put(one) && from)
                    ++from->count; // (no room: back where it came from)
            }
            const BlockPos below{cell.x, cell.y - 1, cell.z};
            if (isContainer(ctx.world, below))
                for (int i = 0; i < 5; ++i) {
                    ItemStack& s = (*slots)[size_t(i)];
                    if (s.empty()) continue;
                    ItemStack one = s;
                    one.count = 1;
                    if (insertOne(ctx.world, below, Direction::Up, one)) {
                        if (--s.count == 0) s = {};
                        break;
                    }
                }
            c->markDirty();
        }
    }
    // A command block cart (M29.7): on a powered activator rail it runs its command every 4 ticks.
    if (m.decor == 5 && rail != 0 && r.blockOf(rail) == blocks::ActivatorRail && r.get(rail, properties::powered) == 0 &&
        m.commandId != 0 && (m.age = m.age + 1) % 4 == 0 && m_cartCommands.size() < m_cartCommands.capacity())
        m_cartCommands.push_back(m.uuidHi);
    // A TNT cart (wiki: Minecart with TNT): a powered activator rail lights it; 4 s later
    // (`fuse` counts up to 80) it explodes with power 4.
    if (m.decor == 4) {
        if (rail != 0 && r.blockOf(rail) == blocks::ActivatorRail && r.get(rail, properties::powered) == 0 && m.fuse == 0)
            m.fuse = 1;
        if (m.fuse > 0 && ++m.fuse >= 80) {
            m_scratchEdits.clear();
            std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
            ExplosionTargets t;
            t.tnt = ctx.tnt;
            t.breakBlocks = ctx.mobGriefing;
            if (ctx.survival && !ctx.playerDead) {
                t.player = &ctx.player;
                t.vitals = &ctx.vitals;
            }
            m.health = 0.0f;
            m.deathTime = 19; // (gone, no item: it blew up)
            m_explosion.explode(ctx.world, m.pos + glm::dvec3(0.0, 0.5, 0.0), 4.0f, ctx.rng, ctx.items, changed, t);
        }
    }
}

} // namespace mc
