#include "gameplay/FallingBlocks.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

void FallingBlocks::spawn(const BlockPos& p, BlockStateId state) {
    if (m_blocks.size() >= size_t(kMax)) return; // (a huge collapse: the rest just vanish)
    FallingBlock f;
    f.pos = f.prevPos = {p.x + 0.5, double(p.y), p.z + 0.5};
    f.state = state;
    f.startY = f.pos.y;
    m_blocks.push_back(f);
}

bool FallingBlocks::move(const World& world, FallingBlock& f) {
    const auto& reg = blockRegistry();
    const Aabb box = Aabb::fromFeet(f.pos, kSize, kSize);
    const Aabb region = box.expandedTowards(f.vel);
    m_boxes.clear();
    for (int x = int(std::floor(region.min.x)); x <= int(std::floor(region.max.x)); ++x)
        for (int y = int(std::floor(region.min.y)); y <= int(std::floor(region.max.y)); ++y)
            for (int z = int(std::floor(region.min.z)); z <= int(std::floor(region.max.z)); ++z)
                if (reg.collides(world.getBlock({x, y, z})))
                    m_boxes.push_back({{double(x), double(y), double(z)}, {x + 1.0, y + 1.0, z + 1.0}});
    glm::dvec3 d = f.vel;
    Aabb b = box;
    for (int axis : {1, 0, 2}) { // y first, like vanilla
        for (const Aabb& w : m_boxes)
            d[axis] = b.clip(w, axis, d[axis]);
        glm::dvec3 step(0.0);
        step[axis] = d[axis];
        b = b.moved(step);
    }
    const bool landed = f.vel.y < 0.0 && d.y != f.vel.y;
    for (int a = 0; a < 3; ++a)
        if (d[a] != f.vel[a]) f.vel[a] = 0.0;
    f.pos += d;
    return landed;
}

void FallingBlocks::tick(World& world, ItemEntities& items, Xoroshiro& rng, std::vector<BlockPos>& changed) {
    const auto& reg = blockRegistry();
    for (size_t i = 0; i < m_blocks.size();) {
        FallingBlock& f = m_blocks[i];
        f.prevPos = f.pos;
        const BlockPos at{int(std::floor(f.pos.x)), int(std::floor(f.pos.y)), int(std::floor(f.pos.z))};
        const Chunk* chunk = world.chunk(at.chunk());
        if (!chunk) { // unloaded below it: wait (vanilla doesn't tick it there)
            ++i;
            continue;
        }
        ++f.time;
        f.vel.y -= 0.04; // gravity, then drag (wiki: Falling Block / Entity)
        const bool landed = move(world, f);
        f.vel *= 0.98;
        if (world.isInHeight(at.y) && chunk->lit()) {
            f.skyLight = chunk->skyLight(blockToLocal(at.x), at.y, blockToLocal(at.z));
            f.blockLight = chunk->blockLight(blockToLocal(at.x), at.y, blockToLocal(at.z));
        }
        bool remove = false;
        const auto dropItem = [&] {
            if (const ItemId item = itemRegistry().blockItem(reg.blockOf(f.state)))
                items.spawn(f.pos + glm::dvec3(0, 0.25, 0), {item, 1}, rng);
        };
        if (landed && reg.blockOf(f.state) == blocks::PointedDripstone) {
            // A stalactite breaks where it lands, hurting what's there: 6 for each block it
            // fell after the first, at most 40 (wiki: Pointed Dripstone).
            const float dmg = float(std::min(40, 6 * std::max(0, int(std::ceil(f.startY - f.pos.y)) - 1)));
            if (dmg > 0.0f && m_impacts.size() < m_impacts.capacity()) m_impacts.push_back({f.pos, dmg});
            dropItem();
            remove = true;
        } else if (landed) {
            // Lands where its bottom centre is: placed if that cell is replaceable and
            // the block below isn't (it stands on something), else dropped as an item.
            const BlockPos cell{int(std::floor(f.pos.x)), int(std::floor(f.pos.y + 0.01)), int(std::floor(f.pos.z))};
            const BlockStateId here = world.getBlock(cell);
            const BlockStateId below = world.getBlock({cell.x, cell.y - 1, cell.z});
            if (world.isInHeight(cell.y) && BlockUpdates::replaceable(here) && !BlockUpdates::fallThrough(below)) {
                world.updateBlock(cell, f.state);
                changed.push_back(cell);
            } else {
                dropItem();
            }
            remove = true;
        } else if (f.time > 600 || (f.time > 100 && f.pos.y < world.height().minY - 64)) {
            if (f.time > 600) dropItem();
            remove = true;
        }
        if (remove) {
            m_blocks[i] = m_blocks.back();
            m_blocks.pop_back();
        } else {
            ++i;
        }
    }
}

} // namespace mc
