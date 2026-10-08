// Beacons and conduits (M23.6; wiki: Beacon, Conduit).
#include "gameplay/Beacons.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace mc {

namespace {

using world::BlockId;
namespace B = world::blocks;
BlockId blockAt(const world::World& w, int x, int y, int z) { return world::blockRegistry().blockOf(w.getBlock({x, y, z})); }
bool pyramidBlock(BlockId b) {
    return b == B::IronBlock || b == B::GoldBlock || b == B::DiamondBlock || b == B::EmeraldBlock ||
           b == B::NetheriteBlock;
}
bool frameBlock(BlockId b) {
    const std::string_view id = world::blockRegistry().block(b).id;
    return id == "minecraft:prismarine" || id == "minecraft:prismarine_bricks" || id == "minecraft:dark_prismarine" ||
           id == "minecraft:sea_lantern";
}

} // namespace

int beaconTiers(const world::World& world, const world::BlockPos& p) {
    int tiers = 0;
    for (int k = 1; k <= 4; ++k) {
        const int y = p.y - k;
        if (!world.isInHeight(y)) break;
        for (int dz = -k; dz <= k; ++dz)
            for (int dx = -k; dx <= k; ++dx)
                if (!pyramidBlock(blockAt(world, p.x + dx, y, p.z + dz))) return tiers;
        tiers = k;
    }
    return tiers;
}

bool beaconSky(const world::World& world, const world::BlockPos& p) {
    const auto& r = world::blockRegistry();
    for (int y = p.y + 1; y <= world.height().maxY(); ++y) {
        const world::BlockStateId s = world.getBlock({p.x, y, p.z});
        if (r.opaqueCube(s) && r.blockOf(s) != B::Bedrock) return false; // (vanilla: bedrock lets the beam through)
    }
    return true;
}

bool beaconPrimaryAllowed(world::Effect e, int tiers) {
    using world::Effect;
    switch (e) {
    case Effect::Speed:
    case Effect::Haste: return tiers >= 1;
    case Effect::Resistance:
    case Effect::JumpBoost: return tiers >= 2;
    case Effect::Strength: return tiers >= 3;
    default: return false;
    }
}

int beaconGifts(const world::BeaconData& beacon, const world::BlockPos& p, const glm::dvec3& feet,
                std::array<BeaconGift, 2>& out) {
    if (beacon.conduit || beacon.levels <= 0 || !beacon.beam || beacon.primary == 0) return 0;
    const double range = 10.0 + 10.0 * beacon.levels;
    if (std::abs(feet.x - (p.x + 0.5)) > range + 0.5 || std::abs(feet.z - (p.z + 0.5)) > range + 0.5 ||
        feet.y < p.y - range)
        return 0;
    const int duration = (9 + 2 * beacon.levels) * 20;
    int n = 0;
    const bool upgraded = beacon.levels >= 4 && beacon.secondary == beacon.primary;
    out[size_t(n++)] = {static_cast<world::Effect>(beacon.primary), upgraded ? 1 : 0, duration};
    if (beacon.levels >= 4 && beacon.secondary == static_cast<uint8_t>(world::Effect::Regeneration))
        out[size_t(n++)] = {world::Effect::Regeneration, 0, duration};
    return n;
}

bool isBeaconPayment(world::ItemId item) {
    const std::string_view id = world::itemRegistry().item(item).id;
    return id == "minecraft:iron_ingot" || id == "minecraft:gold_ingot" || id == "minecraft:emerald" ||
           id == "minecraft:diamond" || id == "minecraft:netherite_ingot";
}

int conduitFrame(const world::World& world, const world::BlockPos& p) {
    // The three square rings (radius 2) around the conduit, in the XY, XZ and YZ planes:
    // 3 x 16 places, the 6 they share counted once (42).
    int count = 0;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
                const int ax = std::abs(dx), ay = std::abs(dy), az = std::abs(dz);
                const bool ring = (dz == 0 && std::max(ax, ay) == 2) || (dy == 0 && std::max(ax, az) == 2) ||
                                  (dx == 0 && std::max(ay, az) == 2);
                if (ring) count += frameBlock(blockAt(world, p.x + dx, p.y + dy, p.z + dz));
            }
    return count;
}

bool conduitWet(const world::World& world, const world::BlockPos& p) {
    for (int dy = -1; dy <= 1; ++dy)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dy || dz) && blockAt(world, p.x + dx, p.y + dy, p.z + dz) != B::Water) return false;
    return true;
}

} // namespace mc
