// The creaking (M27.1c; wiki: Creaking, Creaking Heart). Part of Mobs.
//
// A creaking heart set in a pale oak trunk wakes at night and calls a creaking. The
// creaking hunts players like any monster - but it can't move while a player looks at
// it. It can't be hurt while its heart stands: a hit only makes it twitch and grows resin
// clumps on the heart's tree. It crumbles away at daybreak, when its heart is broken, or
// when it strays more than 32 blocks from it. A creaking without a heart (/summon) is an
// ordinary mob with 1 health.
#include "gameplay/Mobs.h"

#include "gameplay/Player.h"
#include "world/Blocks.h"
#include "world/Raycast.h"
#include "world/Rotation.h"

#include <array>
#include <cmath>

namespace mc {

using namespace world;

namespace {

bool hasHome(const MobData& m) { return m.home.y != kNoPoint; }

bool night(int64_t dayTime) {
    const int64_t t = ((dayTime % 24000) + 24000) % 24000;
    return t >= 12600 && t < 23400; // (as the heart keeps it: BlockUpdates::nightTime)
}

// A hit grows 2-3 resin clumps on the pale oak logs around the heart (wiki: Resin Clump).
void growResin(World& world, const glm::ivec3& heart, Xoroshiro& rng) {
    // The open sides of the pale oak logs around it (within 2 across, 4 up or down), then
    // 2-3 of them at random.
    const auto& r = blockRegistry();
    std::array<std::pair<BlockPos, Direction>, 96> sides;
    int n = 0;
    static constexpr Direction kSides[4] = {Direction::North, Direction::South, Direction::West, Direction::East};
    for (int dy = -4; dy <= 4; ++dy)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
                const BlockPos log{heart.x + dx, heart.y + dy, heart.z + dz};
                if (r.blockOf(world.getBlock(log)) != blocks::PaleOakLog) continue;
                for (const Direction d : kSides) {
                    const glm::ivec3 nv = kDirectionNormals[int(d)];
                    const BlockPos at{log.x + nv.x, log.y + nv.y, log.z + nv.z};
                    if (world.getBlock(at) == 0 && n < int(sides.size())) sides[size_t(n++)] = {at, d};
                }
            }
    for (int want = 2 + int(rng.nextInt(2)); want > 0 && n > 0; --want) { // (wiki: 2-3)
        const int k = int(rng.nextInt(uint32_t(n)));
        const auto [at, d] = sides[size_t(k)];
        sides[size_t(k)] = sides[size_t(--n)];
        if (world.getBlock(at) != 0) continue;
        world.updateBlock(at, r.set(r.defaultState(blocks::ResinClump), properties::facing6, int(d) ^ 1)); // (toward the log)
    }
}

} // namespace

bool Mobs::watched(const World& world, const MobData& m, const glm::dvec3& eye, const glm::dvec3& look) {
    // Any of its feet, middle or head in front of the player's eyes (within ~30 degrees),
    // with nothing in between.
    for (const double h : {0.3, 1.35, 2.4}) {
        const glm::dvec3 p = m.pos + glm::dvec3(0.0, h, 0.0);
        const glm::dvec3 to = p - eye;
        const double dist = glm::length(to);
        if (dist > 48.0 || dist < 1e-6) continue;
        if (glm::dot(to / dist, look) < 0.86) continue;
        if (!raycastBlocks(world, eye, to / dist, dist)) return true;
    }
    return false;
}

bool Mobs::creakingTick(Context& ctx, MobData& m) {
    if (m.type != MobType::Creaking) return false;
    if (hasHome(m)) {
        const BlockPos heart{m.home.x, m.home.y, m.home.z};
        const bool heartThere = blockRegistry().blockOf(ctx.world.getBlock(heart)) == blocks::CreakingHeart;
        const glm::dvec3 h(heart.x + 0.5, heart.y, heart.z + 0.5);
        if (!heartThere || !night(ctx.dayTime) || glm::dot(m.pos - h, m.pos - h) > 32.0 * 32.0) {
            // Crumbles away (no drops, no experience).
            ctx.world.levelEvent(LevelEvent::Type::MobDeath, m.pos.x, m.pos.y, m.pos.z, 90 | 270 << 16);
            m.health = 0.0f;
            m.deathTime = 19;
            m.lastHurtByPlayer = false;
            return true;
        }
        // A hit (this tick) grows resin, at most once in 5 s (wiki).
        if (m.chargeTicks > 0) --m.chargeTicks;
        if (m.hurtTime == 9 && m.chargeTicks == 0) {
            growResin(ctx.world, m.home, ctx.rng);
            m.chargeTicks = 100;
        }
        m.health = maxHealthOf(m); // (its heart keeps it whole)
    }
    // Frozen while the player looks at it (any game mode but spectator).
    const glm::dvec3 eye = ctx.player.position() + glm::dvec3(0.0, ctx.player.eyeHeight(), 0.0);
    const glm::dvec3 look(lookVector(ctx.player.yaw(), ctx.player.pitch()));
    if (!ctx.playerDead && watched(ctx.world, m, eye, look)) {
        m.goal = m.pos;
        m.vel.x = m.vel.z = 0.0;
        physics(ctx.world, m, glm::dvec3(0.0), false); // (it still falls)
        m.peek = 1;
        return true;
    }
    m.peek = 0;
    return false; // (hunting: the monster goals)
}

bool Mobs::spawnCreaking(World& world, const BlockPos& heart, const glm::dvec3& player, Xoroshiro& rng) {
    const glm::dvec3 h(heart.x + 0.5, heart.y, heart.z + 0.5);
    if (glm::dot(player - h, player - h) > 32.0 * 32.0) return false;
    const ChunkPos c = heart.chunk();
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (const Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs())
                    if (o.type == MobType::Creaking && o.health > 0.0f && o.home == glm::ivec3(heart.x, heart.y, heart.z))
                        return false; // (its creaking is already out)
    const auto& r = blockRegistry();
    for (int tries = 0; tries < 24; ++tries) {
        const int x = heart.x + int(rng.nextInt(33)) - 16, z = heart.z + int(rng.nextInt(33)) - 16;
        for (int y = heart.y + 8; y >= heart.y - 12; --y) {
            if (!r.collides(world.getBlock({x, y - 1, z}))) continue;
            if (r.collides(world.getBlock({x, y, z})) || r.collides(world.getBlock({x, y + 1, z})) ||
                r.collides(world.getBlock({x, y + 2, z})) || world.getBlock({x, y, z}) == r.defaultState(blocks::Water))
                break;
            MobData m = make(MobType::Creaking, {x + 0.5, double(y), z + 0.5}, rng);
            m.home = {heart.x, heart.y, heart.z};
            return add(world, m);
        }
    }
    return false;
}

} // namespace mc
