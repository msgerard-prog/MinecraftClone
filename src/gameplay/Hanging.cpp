// Item frames and paintings (M28.3a; wiki: Item Frame, Glow Item Frame, Painting): part of
// Mobs. They hang on a block face; every 100 ticks they check that their wall is still
// there (and, for paintings, that nothing solid has grown into the canvas) and drop as
// items when it isn't. An item frame shows one item, turned in 45-degree steps by
// right-clicks; a hit takes the item out first, the next breaks the frame.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"
#include "world/Paintings.h"
#include "world/World.h"

#include <cmath>
#include <vector>

namespace mc {

using namespace world;

namespace {

bool solid(const World& world, const BlockPos& p) {
    const auto& r = blockRegistry();
    const BlockStateId s = world.getBlock(p);
    return r.collides(s) && r.opaqueCube(s);
}

// The wall's axes for a horizontal facing: `right` as seen by someone looking at it.
glm::ivec3 rightOf(Direction face) {
    const glm::ivec3 n = kDirectionNormals[int(face)];
    return {-n.z, 0, n.x}; // = cross(up, n)
}

struct Canvas {
    int left = 0, bottom = 0; // offsets of the first cell along right / up from the support
};
Canvas canvasOf(const PaintingVariant& v) { return {-(v.width - 1) / 2, -(v.height - 1) / 2}; }

// Every cell of a painting free and backed by a solid block.
bool paintingFits(const World& world, const BlockPos& support, Direction face, const PaintingVariant& v) {
    const glm::ivec3 n = kDirectionNormals[int(face)], r = rightOf(face);
    const Canvas c = canvasOf(v);
    for (int j = 0; j < v.height; ++j)
        for (int i = 0; i < v.width; ++i) {
            const glm::ivec3 off = r * (c.left + i) + glm::ivec3(0, c.bottom + j, 0);
            const BlockPos back{support.x + off.x, support.y + off.y, support.z + off.z};
            const BlockPos cell{back.x + n.x, back.y + n.y, back.z + n.z};
            if (!solid(world, back) || blockRegistry().collides(world.getBlock(cell))) return false;
        }
    return true;
}

glm::dvec3 centreOf(MobType type, const BlockPos& support, Direction face, int variant) {
    const glm::dvec3 n(kDirectionNormals[int(face)]);
    glm::dvec3 c = glm::dvec3(support.x + 0.5, support.y + 0.5, support.z + 0.5) + n * (0.5 + 1.0 / 32.0);
    if (type == MobType::Painting) {
        const PaintingVariant& v = kPaintings[size_t(variant)];
        const Canvas cv = canvasOf(v);
        c += glm::dvec3(rightOf(face)) * (cv.left + (v.width - 1) / 2.0) +
             glm::dvec3(0.0, cv.bottom + (v.height - 1) / 2.0, 0.0);
    }
    return c;
}

bool overlapsHanging(World& world, const Aabb& box) {
    const ChunkPos c0{blockToChunk(int(std::floor(box.min.x))), blockToChunk(int(std::floor(box.min.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* c = world.chunk({c0.x + dx, c0.z + dz}))
                for (const MobData& m : c->mobs())
                    if (isHanging(m.type) && m.health > 0.0f && Mobs::hangingBox(m).intersects(box)) return true;
    return false;
}

Chunk* chunkOf(World& world, const MobData& m) {
    return world.chunk({blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))});
}

} // namespace

Aabb Mobs::hangingBox(const MobData& m) {
    const glm::dvec3 n(kDirectionNormals[m.phase % 6]);
    glm::dvec3 half(0.375); // (item frames: 12 pixels square, 1 deep)
    if (m.type == MobType::Painting) {
        const PaintingVariant& v = kPaintings[m.woolColour % kPaintings.size()];
        const glm::dvec3 r(rightOf(Direction(m.phase % 6)));
        half = glm::abs(r) * (v.width / 2.0) + glm::dvec3(0.0, v.height / 2.0, 0.0);
    }
    half = half * (glm::dvec3(1.0) - glm::abs(n)) + glm::abs(n) * (1.0 / 32.0);
    return {m.pos - half, m.pos + half};
}

bool Mobs::placeHanging(World& world, MobType type, const BlockPos& support, Direction face, Xoroshiro& rng) {
    if (!solid(world, support)) return false;
    int variant = 0;
    if (type == MobType::Painting) {
        if (face == Direction::Up || face == Direction::Down) return false; // (walls only)
        // The biggest placeable canvases that fit; one of them at random (wiki: Painting).
        int bestArea = 0;
        std::vector<int> best;
        for (size_t i = 0; i < kPaintings.size(); ++i) {
            const PaintingVariant& v = kPaintings[i];
            if (!v.placeable || !paintingFits(world, support, face, v)) continue;
            const int area = v.width * v.height;
            if (area > bestArea) best.clear(), bestArea = area;
            if (area == bestArea) best.push_back(int(i));
        }
        if (best.empty()) return false;
        variant = best[rng.nextInt(uint32_t(best.size()))];
    } else {
        const glm::ivec3 n = kDirectionNormals[int(face)];
        if (blockRegistry().collides(world.getBlock({support.x + n.x, support.y + n.y, support.z + n.z}))) return false;
    }
    MobData m = make(type, centreOf(type, support, face, variant), rng);
    m.phase = uint8_t(face);
    m.woolColour = uint8_t(variant);
    m.home = {support.x, support.y, support.z};
    m.persistent = true;
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = 0.0f;
    if (overlapsHanging(world, hangingBox(m))) return false;
    return add(world, m);
}

bool Mobs::hangingSurvives(const World& world, const MobData& m) {
    const BlockPos support{m.home.x, m.home.y, m.home.z};
    if (m.type == MobType::Painting)
        return paintingFits(world, support, Direction(m.phase % 6), kPaintings[m.woolColour % kPaintings.size()]);
    return blockRegistry().collides(world.getBlock(support));
}

void Mobs::hangingTick(Context& ctx, MobData& m) {
    m.vel = glm::dvec3(0.0);
    m.prevPos = m.pos;
    if (++m.phaseTicks < 100) return; // (vanilla: checked every 100 ticks)
    m.phaseTicks = 0;
    if (!ctx.world.chunk({blockToChunk(m.home.x), blockToChunk(m.home.z)})) return; // (its wall isn't loaded)
    if (!hangingSurvives(ctx.world, m)) {
        m.health = 0.0f;
        m.lastHurtByPlayer = false;
    }
}

void Mobs::dropHanging(Context& ctx, MobData& m) {
    const char* item = m.type == MobType::Painting ? "painting" : m.type == MobType::GlowItemFrame ? "glow_item_frame" : "item_frame";
    if (Chunk* c = chunkOf(ctx.world, m)) {
        if (ItemContents* s = c->mobStore(m.uuidHi)) {
            if (!(*s)[0].empty()) ctx.items.spawn(m.pos, (*s)[0], ctx.rng);
            c->removeMobStore(m.uuidHi);
        }
    }
    if (const auto id = itemRegistry().find(item)) ctx.items.spawn(m.pos, {*id, 1}, ctx.rng);
}

ItemStack Mobs::frameItem(const World& world, const MobData& frame) {
    const Chunk* c = world.chunk({blockToChunk(int(std::floor(frame.pos.x))), blockToChunk(int(std::floor(frame.pos.z)))});
    const ItemContents* s = c ? c->mobStore(frame.uuidHi) : nullptr;
    return s ? (*s)[0] : ItemStack{};
}

bool Mobs::useItemFrame(World& world, MobData& frame, const ItemStack& held) {
    Chunk* c = chunkOf(world, frame);
    if (!c || frame.type == MobType::Painting) return false;
    ItemContents* s = c->mobStore(frame.uuidHi);
    if (s && !(*s)[0].empty()) { // turn it (wiki: 8 positions; comparators read them)
        frame.node = uint8_t((frame.node + 1) % 8);
        c->markDirty();
        return false;
    }
    if (held.empty()) return false;
    ItemContents& slots = c->addMobStore(frame.uuidHi);
    slots[0] = held;
    slots[0].count = 1;
    frame.node = 0;
    c->markDirty();
    return true;
}

bool Mobs::popFrameItem(World& world, MobData& frame, ItemEntities& items, Xoroshiro& rng) {
    Chunk* c = chunkOf(world, frame);
    ItemContents* s = c ? c->mobStore(frame.uuidHi) : nullptr;
    if (!s || (*s)[0].empty()) return false;
    items.spawn(frame.pos + glm::dvec3(kDirectionNormals[frame.phase % 6]) * 0.15, (*s)[0], rng);
    (*s)[0] = {};
    c->markDirty();
    return true;
}

} // namespace mc
