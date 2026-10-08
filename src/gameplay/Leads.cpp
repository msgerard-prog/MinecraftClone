// Leads (M28.3c; wiki: Lead, Leash Knot, Llama › Caravans): part of Mobs.
//
// A mob on a lead is held by the player or tied to a fence (a leash knot entity on the
// post). Farther than 6 blocks from its holder it is pulled in; past 10 the lead snaps and
// drops. Llamas form caravans: one on a lead leads, up to 9 more follow nose to tail.
#include "gameplay/Mobs.h"

#include "gameplay/ItemEntities.h"
#include "world/Blocks.h"
#include "world/World.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

bool isFencePost(BlockId b) {
    return blockRegistry().likeOf(b) == blocks::OakFence || b == blocks::NetherBrickFence;
}

glm::dvec3 knotCentre(const glm::ivec3& p) { return {p.x + 0.5, p.y + 0.5, p.z + 0.5}; }

template <typename F> void forEachNear(World& world, const glm::dvec3& at, int chunks, F&& f) {
    const ChunkPos c0{blockToChunk(int(std::floor(at.x))), blockToChunk(int(std::floor(at.z)))};
    for (int dz = -chunks; dz <= chunks; ++dz)
        for (int dx = -chunks; dx <= chunks; ++dx)
            if (Chunk* c = world.chunk({c0.x + dx, c0.z + dz}))
                for (MobData& m : c->mobs())
                    f(*c, m);
}

void dropLead(ItemEntities& items, const glm::dvec3& at, Xoroshiro& rng) {
    if (const auto lead = itemRegistry().find("lead")) items.spawn(at, {*lead, 1}, rng);
}

} // namespace

void Mobs::leashTick(Context& ctx, MobData& m) {
    glm::dvec3 holder;
    if (m.leash == 1) {
        if (ctx.playerDead) { // (the player's leads drop when they die)
            m.leash = 0;
            dropLead(ctx.items, m.pos, ctx.rng);
            return;
        }
        holder = ctx.player.position() + glm::dvec3(0.0, 1.0, 0.0);
    } else {
        const BlockPos p{m.leashPos.x, m.leashPos.y, m.leashPos.z};
        if (!ctx.world.chunk(p.chunk())) return; // (its fence isn't loaded: wait)
        if (!isFencePost(blockRegistry().blockOf(ctx.world.getBlock(p)))) { // the fence went
            m.leash = 0;
            dropLead(ctx.items, m.pos, ctx.rng);
            return;
        }
        holder = knotCentre(m.leashPos);
    }
    const glm::dvec3 to = holder - (m.pos + glm::dvec3(0.0, mobInfo(m.type).height * 0.5, 0.0));
    const double d = glm::length(to);
    if (d > 12.0) { // snapped (wiki: beyond 12 blocks since 1.21.6)
        m.leash = 0;
        dropLead(ctx.items, m.pos, ctx.rng);
        return;
    }
    if (d > 6.0) { // pulled in, harder the farther it is
        const glm::dvec3 dir = to / d;
        m.vel += dir * std::min(0.4, (d - 6.0) * 0.1);
    } else if (d > 4.0 && m.leash == 1) {
        m.goal = holder; // (walks after the player)
    }
}

void Mobs::caravanTick(Context& ctx, MobData& m) {
    // Following: keep about 2 blocks behind the llama in front (wiki: Llama › Caravans).
    if (m.caravanHead != 0) {
        const MobData* head = mobByUuid(ctx.world, m.pos, m.caravanHead);
        if (!head || head->health <= 0.0f || m.leash != 0 ||
            glm::length(head->pos - m.pos) > 16.0) {
            m.caravanHead = 0;
            return;
        }
        const glm::dvec3 to = head->pos - m.pos;
        const double d = glm::length(to);
        if (d > 2.5) m.vel += glm::dvec3(to.x, 0.0, to.z) / d * std::min(0.1, (d - 2.5) * 0.05);
        m.goal = head->pos;
        return;
    }
    // A free llama joins the tail of a caravan within 9 blocks, once in a while.
    if (m.leash != 0 || ctx.rng.nextInt(40) != 0) return;
    MobData* best = nullptr;
    double bestD = 9.0 * 9.0;
    forEachNear(ctx.world, m.pos, 1, [&](Chunk&, MobData& o) {
        if (&o == &m || !isLlama(o.type) || o.health <= 0.0f ||
            (o.leash == 0 && o.caravanHead == 0))
            return;
        const double d2 = glm::dot(o.pos - m.pos, o.pos - m.pos);
        if (d2 >= bestD) return;
        // Only a tail takes a follower, and caravans stay at 10 llamas.
        bool hasFollower = false;
        forEachNear(ctx.world, o.pos, 1, [&](Chunk&, MobData& f) {
            hasFollower = hasFollower || f.caravanHead == o.uuidHi;
        });
        int length = 1;
        for (const MobData* h = &o; h && h->caravanHead != 0 && length < 11; ++length)
            h = mobByUuid(ctx.world, h->pos, h->caravanHead);
        if (!hasFollower && length < 10) {
            best = &o;
            bestD = d2;
        }
    });
    if (best) m.caravanHead = best->uuidHi;
}

bool Mobs::leashToPlayer(MobData& m) {
    if (!isLeashable(m.type) || m.leash != 0 || m.health <= 0.0f) return false;
    m.leash = 1;
    m.caravanHead = 0;
    return true;
}

int Mobs::tieToFence(World& world, const BlockPos& fence, const glm::dvec3& player,
                     Xoroshiro& rng) {
    if (!isFencePost(blockRegistry().blockOf(world.getBlock(fence)))) return 0;
    int tied = 0;
    forEachNear(world, player, 1, [&](Chunk& c, MobData& m) {
        if (m.leash != 1 || glm::length(m.pos - player) > 7.0) return;
        m.leash = 2;
        m.leashPos = {fence.x, fence.y, fence.z};
        c.markDirty();
        ++tied;
    });
    if (tied == 0) return 0;
    bool knot = false; // one knot per post
    forEachNear(world, knotCentre({fence.x, fence.y, fence.z}), 0, [&](Chunk&, MobData& m) {
        knot = knot || (m.type == MobType::LeashKnot &&
                        m.home == glm::ivec3(fence.x, fence.y, fence.z) && m.health > 0.0f);
    });
    if (!knot) {
        MobData k =
            make(MobType::LeashKnot, {fence.x + 0.5, double(fence.y) + 0.25, fence.z + 0.5}, rng);
        k.home = {fence.x, fence.y, fence.z};
        k.persistent = true;
        add(world, k);
    }
    return tied;
}

int Mobs::takeFromKnot(World& world, const MobData& knot) {
    int taken = 0;
    forEachNear(world, knot.pos, 1, [&](Chunk& c, MobData& m) {
        if (m.leash == 2 && m.leashPos == knot.home) {
            m.leash = 1;
            c.markDirty();
            ++taken;
        }
    });
    return taken;
}

void Mobs::breakKnot(World& world, MobData& knot, ItemEntities& items, Xoroshiro& rng) {
    forEachNear(world, knot.pos, 1, [&](Chunk& c, MobData& m) {
        if (m.leash == 2 && m.leashPos == knot.home) {
            m.leash = 0;
            dropLead(items, m.pos, rng);
            c.markDirty();
        }
    });
    knot.health = 0.0f;
    knot.deathTime = 19;
}

void Mobs::knotTick(Context& ctx, MobData& k) {
    k.vel = glm::dvec3(0.0);
    k.prevPos = k.pos;
    if (++k.phaseTicks < 20) return;
    k.phaseTicks = 0;
    // Gone with its fence, or once nothing is tied to it any more.
    bool used = false;
    forEachNear(ctx.world, k.pos, 1,
                [&](Chunk&, MobData& m) { used = used || (m.leash == 2 && m.leashPos == k.home); });
    const BlockPos p{k.home.x, k.home.y, k.home.z};
    if (!used || !isFencePost(blockRegistry().blockOf(ctx.world.getBlock(p)))) {
        k.health = 0.0f;
        k.deathTime = 19;
    }
}

} // namespace mc
