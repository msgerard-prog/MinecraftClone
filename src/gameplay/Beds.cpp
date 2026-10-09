#include "gameplay/Beds.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }

glm::ivec3 facingVec(BlockStateId s) {
    switch (R().get(s, properties::facing)) { // north, south, west, east
    case 0:
        return {0, 0, -1};
    case 1:
        return {0, 0, 1};
    case 2:
        return {-1, 0, 0};
    default:
        return {1, 0, 0};
    }
}

} // namespace

bool canSleepAt(int64_t dayTime, bool raining, bool thundering) {
    if (thundering) return true;
    const int64_t t = dayTime % 24000;
    return raining ? t >= 12002 && t <= 23998 : t >= 12523 && t <= 23477;
}

std::optional<BlockPos> bedHead(const World& world, const BlockPos& p) {
    const BlockStateId s = world.getBlock(p);
    if (R().likeOf(R().blockOf(s)) != blocks::RedBed) return std::nullopt;
    if (R().get(s, properties::bedPart) == 0) return p;
    const glm::ivec3 f = facingVec(s);
    const BlockPos head{p.x + f.x, p.y, p.z + f.z};
    if (R().likeOf(R().blockOf(world.getBlock(head))) != blocks::RedBed) return std::nullopt;
    return head;
}

BedUse useBed(const World& world, const BlockPos& p, int64_t dayTime, Dimension dimension,
              bool creative, const glm::dvec3* player, bool raining, bool thundering) {
    const auto head = bedHead(world, p);
    if (!head) return BedUse::NotABed;
    if (dimension != Dimension::Overworld) return BedUse::Explodes;
    if (player &&
        (std::abs(player->x - (head->x + 0.5)) > 3.5 ||
         std::abs(player->z - (head->z + 0.5)) > 3.5 || std::abs(player->y - head->y) > 3.0))
        return BedUse::TooFar;
    if (R().collides(world.getBlock({head->x, head->y + 1, head->z}))) return BedUse::Obstructed;
    // (One player: occupancy is never stored, so an "occupied" bed from another save
    // doesn't lock it - vanilla checks the sleeping entity.)
    if (!canSleepAt(dayTime, raining, thundering)) return BedUse::NotNight;
    // Monsters within 8 blocks horizontally and 5 vertically keep the player awake
    // (not in creative).
    if (creative) return BedUse::Sleep;
    const ChunkPos c = head->chunk();
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& m : ch->mobs()) {
                    if (!mobInfo(m.type).hostile || m.health <= 0.0f) continue;
                    if (std::abs(m.pos.x - (head->x + 0.5)) <= 8.0 &&
                        std::abs(m.pos.z - (head->z + 0.5)) <= 8.0 &&
                        std::abs(m.pos.y - head->y) <= 5.0)
                        return BedUse::Monsters;
                }
    return BedUse::Sleep;
}

std::optional<glm::dvec3> anchorStandSpot(const World& world, const BlockPos& anchor) {
    const BlockStateId s = world.getBlock(anchor);
    if (R().blockOf(s) != blocks::RespawnAnchor || R().get(s, properties::charges) == 0) return std::nullopt;
    auto fits = [&](int x, int y, int z) {
        return R().collides(world.getBlock({x, y - 1, z})) && !R().collides(world.getBlock({x, y, z})) &&
               !R().collides(world.getBlock({x, y + 1, z})) && R().blockOf(world.getBlock({x, y, z})) != blocks::Lava;
    };
    for (const int dy : {0, -1, 1})
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dz) && fits(anchor.x + dx, anchor.y + dy, anchor.z + dz))
                    return glm::dvec3(anchor.x + dx + 0.5, double(anchor.y + dy), anchor.z + dz + 0.5);
    if (fits(anchor.x, anchor.y + 1, anchor.z)) return glm::dvec3(anchor.x + 0.5, anchor.y + 1.0, anchor.z + 0.5);
    return std::nullopt;
}

std::optional<glm::dvec3> bedStandSpot(const World& world, const BlockPos& head) {
    const BlockStateId s = world.getBlock(head);
    if (R().likeOf(R().blockOf(s)) != blocks::RedBed) return std::nullopt;
    const glm::ivec3 f = facingVec(s);
    const BlockPos foot{head.x - f.x, head.y, head.z - f.z};
    auto fits = [&](int x, int y, int z) {
        return R().collides(world.getBlock({x, y - 1, z})) &&
               !R().collides(world.getBlock({x, y, z})) &&
               !R().collides(world.getBlock({x, y + 1, z})) &&
               R().blockOf(world.getBlock({x, y, z})) != blocks::Lava;
    };
    for (const BlockPos& half : {head, foot})
        for (int dy = 0; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = half.x + dx, y = half.y + dy, z = half.z + dz;
                    if (R().likeOf(R().blockOf(world.getBlock({x, y, z}))) == blocks::RedBed) continue;
                    if (fits(x, y, z)) return glm::dvec3(x + 0.5, double(y), z + 0.5);
                }
    // On the bed itself (vanilla also allows standing on top).
    if (!R().collides(world.getBlock({head.x, head.y + 1, head.z})) &&
        !R().collides(world.getBlock({head.x, head.y + 2, head.z})))
        return glm::dvec3(head.x + 0.5, head.y + 0.5625, head.z + 0.5);
    return std::nullopt;
}

} // namespace mc
