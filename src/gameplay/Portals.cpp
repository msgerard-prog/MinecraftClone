#include "gameplay/Portals.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Coords.h"

#include <algorithm>
#include <cmath>

namespace mc::portals {

using namespace world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockAt(const World& w, const BlockPos& p) { return R().blockOf(w.getBlock(p)); }
BlockPos add(const BlockPos& p, int dx, int dy, int dz) { return {p.x + dx, p.y + dy, p.z + dz}; }

void put(World& w, const BlockPos& p, BlockStateId s, std::vector<BlockPos>& changed) {
    if (w.getBlock(p) == s) return;
    w.updateBlock(p, s);
    changed.push_back(p);
}

// Places every block first, then sends the updates (a portal checks its whole frame:
// filled one by one, the first blocks would see an incomplete one and break).
struct Batch {
    World& world;
    std::vector<BlockPos>& changed;
    std::vector<std::pair<BlockPos, BlockStateId>> old;
    void put(const BlockPos& p, BlockStateId s) {
        const BlockStateId was = world.getBlock(p);
        if (was == s || !world.chunk(p.chunk())) return;
        world.setBlock(p, s);
        old.emplace_back(p, was);
        changed.push_back(p);
    }
    ~Batch() {
        for (const auto& [p, was] : old)
            world.notifyChanged(p, was, world.getBlock(p));
    }
};

int floorDiv(int a, int b) { return static_cast<int>(std::floor(double(a) / b)); }

} // namespace

std::optional<BlockPos> light(World& world, const BlockPos& p, std::vector<BlockPos>& changed) {
    if (world.getBlock(p) != 0) return std::nullopt;
    auto air = [&](const BlockPos& q) { return world.getBlock(q) == 0; };
    auto obsidian = [&](const BlockPos& q) { return blockAt(world, q) == blocks::Obsidian; };
    for (int axis = 0; axis < 2; ++axis) { // along x, then along z
        const int ax = axis == 0 ? 1 : 0, az = axis == 0 ? 0 : 1;
        // Down to the frame's bottom row.
        BlockPos b = p;
        for (int n = 0; n < 21 && air(add(b, 0, -1, 0)); ++n)
            b = add(b, 0, -1, 0);
        if (!obsidian(add(b, 0, -1, 0))) continue;
        // Back to the left side.
        for (int n = 0; n < 21 && air(add(b, -ax, 0, -az)) && obsidian(add(b, -ax, -1, -az)); ++n)
            b = add(b, -ax, 0, -az);
        if (!obsidian(add(b, -ax, 0, -az))) continue;
        int width = 0;
        while (width <= 21 && air(add(b, ax * width, 0, az * width)) && obsidian(add(b, ax * width, -1, az * width)))
            ++width;
        if (width < 2 || width > 21 || !obsidian(add(b, ax * width, 0, az * width))) continue;
        // Rows up to an all-obsidian top (sides obsidian on every row).
        int height = 0;
        bool closed = false;
        for (; height <= 21; ++height) {
            bool allObsidian = true, allAir = true;
            for (int i = 0; i < width; ++i) {
                const BlockPos q = add(b, ax * i, height, az * i);
                allObsidian = allObsidian && obsidian(q);
                allAir = allAir && air(q);
            }
            if (allObsidian) {
                closed = true;
                break;
            }
            if (!allAir || !obsidian(add(b, -ax, height, -az)) || !obsidian(add(b, ax * width, height, az * width))) break;
        }
        if (!closed || height < 3) continue;
        const BlockStateId portal = R().set(R().defaultState(blocks::NetherPortal), properties::haxis, axis);
        Batch batch{world, changed, {}};
        for (int h = 0; h < height; ++h)
            for (int i = 0; i < width; ++i)
                batch.put(add(b, ax * i, h, az * i), portal);
        return b;
    }
    return std::nullopt;
}

std::optional<BlockPos> find(const World& world, std::vector<Known>& known, Dimension dim, const BlockPos& target,
                             int radius) {
    std::optional<BlockPos> best;
    int64_t bestD = 0;
    for (size_t i = 0; i < known.size();) {
        const Known& k = known[i];
        if (k.dimension == dim && world.chunk(k.pos.chunk()) && blockAt(world, k.pos) != blocks::NetherPortal) {
            known.erase(known.begin() + std::ptrdiff_t(i)); // broken since
            continue;
        }
        const int dx = k.pos.x - target.x, dz = k.pos.z - target.z;
        if (k.dimension == dim && std::abs(dx) <= radius && std::abs(dz) <= radius) {
            const int64_t d = int64_t(dx) * dx + int64_t(dz) * dz + int64_t(k.pos.y - target.y) * (k.pos.y - target.y);
            if (!best || d < bestD) {
                best = k.pos;
                bestD = d;
            }
        }
        ++i;
    }
    return best;
}

BlockPos build(World& world, const BlockPos& target, int minY, int maxY, std::vector<BlockPos>& changed) {
    const auto& r = R();
    auto solid = [&](int x, int y, int z) { return r.collides(world.getBlock({x, y, z})); };
    auto empty = [&](int x, int y, int z) { return world.getBlock({x, y, z}) == 0; };
    // The inside's bottom corner c: frame x-1..x+2, y-1..y+3 (in z), on ground at y-2,
    // with room to step out on both sides.
    auto fits = [&](int x, int y, int z) {
        for (int i = -1; i <= 2; ++i) {
            if (!solid(x + i, y - 2, z)) return false;
            for (int h = -1; h <= 3; ++h)
                if (!empty(x + i, h + y, z)) return false;
            for (int h = 0; h <= 2; ++h)
                if (!empty(x + i, y + h, z - 1) || !empty(x + i, y + h, z + 1)) return false;
        }
        return true;
    };
    std::optional<BlockPos> spot;
    for (int radius = 0; radius <= 16 && !spot; ++radius)
        for (int dz = -radius; dz <= radius && !spot; ++dz)
            for (int dx = -radius; dx <= radius && !spot; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const int x = target.x + dx, z = target.z + dz;
                for (int y = maxY - 4; y >= minY + 2; --y)
                    if (fits(x, y, z)) {
                        spot = BlockPos{x, y, z};
                        break;
                    }
            }
    const BlockStateId obsidian = r.defaultState(blocks::Obsidian);
    Batch batch{world, changed, {}};
    if (!spot) { // no room: make some at the target height (vanilla: Y 70 up to 10
                 // below the top), on an obsidian floor
        const BlockPos c{target.x, std::clamp(target.y, std::max(minY + 2, 70), maxY - 4), target.z};
        for (int i = -1; i <= 2; ++i)
            for (int dz = -1; dz <= 1; ++dz) {
                batch.put({c.x + i, c.y - 2, c.z + dz}, obsidian);
                for (int h = -1; h <= 3; ++h)
                    batch.put({c.x + i, c.y + h, c.z + dz}, 0);
            }
        spot = c;
    }
    const BlockPos c = *spot;
    for (int i = -1; i <= 2; ++i) {
        batch.put({c.x + i, c.y - 1, c.z}, obsidian);
        batch.put({c.x + i, c.y + 3, c.z}, obsidian);
    }
    for (int h = 0; h <= 2; ++h) {
        batch.put({c.x - 1, c.y + h, c.z}, obsidian);
        batch.put({c.x + 2, c.y + h, c.z}, obsidian);
    }
    const BlockStateId portal = r.defaultState(blocks::NetherPortal); // axis x
    for (int h = 0; h <= 2; ++h)
        for (int i = 0; i <= 1; ++i)
            batch.put({c.x + i, c.y + h, c.z}, portal);
    return c;
}

BlockPos destination(Dimension from, Dimension to, const BlockPos& pos) {
    if (from == Dimension::Overworld && to == Dimension::Nether)
        return {floorDiv(pos.x, 8), pos.y, floorDiv(pos.z, 8)}; // Y unchanged (wiki)
    if (from == Dimension::Nether && to == Dimension::Overworld)
        return {pos.x * 8, pos.y, pos.z * 8};
    return pos;
}

bool touching(const World& world, const Aabb& box, BlockId block) {
    for (int x = int(std::floor(box.min.x)); x <= int(std::floor(box.max.x - 1e-7)); ++x)
        for (int y = int(std::floor(box.min.y)); y <= int(std::floor(box.max.y - 1e-7)); ++y)
            for (int z = int(std::floor(box.min.z)); z <= int(std::floor(box.max.z - 1e-7)); ++z)
                if (blockAt(world, {x, y, z}) == block) return true;
    return false;
}

glm::dvec3 endPlatform(World& world, std::vector<BlockPos>& changed) {
    const BlockStateId obsidian = R().defaultState(blocks::Obsidian);
    for (int z = -2; z <= 2; ++z)
        for (int x = 98; x <= 102; ++x) {
            put(world, {x, 48, z}, obsidian, changed);
            for (int y = 49; y <= 51; ++y)
                put(world, {x, y, z}, 0, changed);
        }
    return {100.5, 49.0, 0.5};
}

bool completeEndPortal(World& world, const BlockPos& frame, std::vector<BlockPos>& changed) {
    auto eyed = [&](int x, int z) {
        const BlockStateId s = world.getBlock({x, frame.y, z});
        return R().blockOf(s) == blocks::EndPortalFrame && R().get(s, properties::eye) == 0;
    };
    for (int cz = frame.z - 2; cz <= frame.z + 2; ++cz)
        for (int cx = frame.x - 2; cx <= frame.x + 2; ++cx) {
            bool ring = true;
            for (int i = -1; i <= 1 && ring; ++i)
                ring = eyed(cx + i, cz - 2) && eyed(cx + i, cz + 2) && eyed(cx - 2, cz + i) && eyed(cx + 2, cz + i);
            if (!ring) continue;
            const BlockStateId portal = R().defaultState(blocks::EndPortal);
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    put(world, {cx + dx, frame.y, cz + dz}, portal, changed);
            return true;
        }
    return false;
}

bool useItem(World& world, Dimension dimension, ItemId item, const BlockPos& block, Direction face,
             std::vector<BlockPos>& changed) {
    const std::string_view id = itemRegistry().item(item).id;
    if (id == "minecraft:flint_and_steel") {
        // Fire goes in front of the clicked face; inside an obsidian frame it becomes a
        // portal instead (wiki: Flint and Steel, Nether portal - not in the End).
        const glm::ivec3 n = normal(face);
        const BlockPos at{block.x + n.x, block.y + n.y, block.z + n.z};
        if (dimension != Dimension::End && light(world, at, changed)) return true;
        if (!world.isInHeight(at.y) || world.getBlock(at) != 0) return false;
        if (!BlockUpdates::fireCanStay(world, at)) return false; // nowhere for fire to stay: not used
        world.updateBlock(at, BlockUpdates::fireState(0));
        changed.push_back(at);
        return true;
    }
    if (id == "minecraft:ender_eye") {
        const BlockStateId s = world.getBlock(block);
        if (R().blockOf(s) != blocks::EndPortalFrame || R().get(s, properties::eye) == 0) return false;
        put(world, block, R().set(s, properties::eye, 0), changed);
        completeEndPortal(world, block, changed);
        return true;
    }
    return false;
}

} // namespace mc::portals
