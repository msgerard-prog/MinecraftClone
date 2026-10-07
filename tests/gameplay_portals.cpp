// Portals (wiki: Nether portal, End portal, End Portal Frame).
#include "gameplay/Portals.h"
#include "world/Blocks.h"
#include "world/Redstone.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

struct Scene {
    World world;
    Redstone updates{world}; // block updates (portals break with their frame)
    std::vector<BlockPos> changed;
    Scene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        for (int y = 60; y <= 63; ++y)
                            c.set(x, y, z, S(blocks::Stone));
            }
    }
    // A frame along x: inside x0..x0+w-1, y0..y0+h-1, at z.
    void frame(int x0, int y0, int z, int w, int h) {
        for (int x = x0 - 1; x <= x0 + w; ++x) {
            world.setBlock({x, y0 - 1, z}, S(blocks::Obsidian));
            world.setBlock({x, y0 + h, z}, S(blocks::Obsidian));
        }
        for (int y = y0; y < y0 + h; ++y) {
            world.setBlock({x0 - 1, y, z}, S(blocks::Obsidian));
            world.setBlock({x0 + w, y, z}, S(blocks::Obsidian));
        }
    }
    BlockId at(BlockPos p) const { return blockRegistry().blockOf(world.getBlock(p)); }
};

} // namespace

TEST_CASE("a lit obsidian frame fills with portal blocks; too small or open frames don't") {
    Scene s;
    s.frame(0, 64, 0, 2, 3);
    const auto corner = portals::light(s.world, {1, 65, 0}, s.changed);
    REQUIRE(corner);
    CHECK(*corner == BlockPos{0, 64, 0});
    for (int y = 64; y < 67; ++y)
        for (int x = 0; x < 2; ++x)
            CHECK(s.at({x, y, 0}) == blocks::NetherPortal);
    CHECK(s.changed.size() == 6);
    // Along z, larger (3 x 4).
    Scene t;
    for (int z = 4; z <= 8; ++z) {
        t.world.setBlock({5, 63, z}, S(blocks::Obsidian));
        t.world.setBlock({5, 68, z}, S(blocks::Obsidian));
    }
    for (int y = 64; y < 68; ++y) {
        t.world.setBlock({5, y, 4}, S(blocks::Obsidian));
        t.world.setBlock({5, y, 8}, S(blocks::Obsidian));
    }
    REQUIRE(portals::light(t.world, {5, 66, 6}, t.changed));
    CHECK(blockRegistry().value(t.world.getBlock({5, 64, 5}), "axis") == "z");
    // 1 wide: too small; a gap in the top: not closed.
    Scene u;
    u.frame(0, 64, 0, 1, 3);
    CHECK_FALSE(portals::light(u.world, {0, 64, 0}, u.changed));
    u.frame(10, 64, 0, 2, 3);
    u.world.setBlock({11, 67, 0}, 0);
    CHECK_FALSE(portals::light(u.world, {10, 64, 0}, u.changed));
}

TEST_CASE("breaking the frame breaks the portal") {
    Scene s;
    s.frame(0, 64, 0, 2, 3);
    REQUIRE(portals::light(s.world, {0, 64, 0}, s.changed));
    s.world.updateBlock({-1, 65, 0}, 0);
    for (int y = 64; y < 67; ++y)
        for (int x = 0; x < 2; ++x)
            CHECK(s.at({x, y, 0}) == 0);
}

TEST_CASE("portal travel: 8:1 coordinates, known portals are reused, else one is built") {
    CHECK(portals::destination(Dimension::Overworld, Dimension::Nether, {800, 70, -803}) == BlockPos{100, 70, -101});
    CHECK(portals::destination(Dimension::Nether, Dimension::Overworld, {100, 70, -101}) == BlockPos{800, 70, -808});
    Scene s;
    std::vector<portals::Known> known{{Dimension::Overworld, {3, 64, 3}}, {Dimension::Nether, {0, 64, 0}}};
    s.world.setBlock({3, 64, 3}, S(blocks::NetherPortal));
    const auto found = portals::find(s.world, known, Dimension::Overworld, {10, 64, 10}, 128);
    REQUIRE(found);
    CHECK(*found == BlockPos{3, 64, 3});
    s.world.setBlock({3, 64, 3}, 0); // broken since: forgotten
    CHECK_FALSE(portals::find(s.world, known, Dimension::Overworld, {10, 64, 10}, 128));
    CHECK(known.size() == 1);
    // Building: on the stone floor (top y 63) near the target.
    const BlockPos in = portals::build(s.world, {10, 70, 10}, 0, 120, s.changed);
    CHECK(in.y == 65); // frame bottom at 64, on the floor
    CHECK(s.at(in) == blocks::NetherPortal);
    CHECK(s.at({in.x + 1, in.y + 2, in.z}) == blocks::NetherPortal);
    CHECK(s.at({in.x - 1, in.y, in.z}) == blocks::Obsidian);
}

TEST_CASE("end portal frames with all 12 eyes open the portal; the arrival platform") {
    Scene s;
    const BlockStateId frame = S(blocks::EndPortalFrame);
    std::vector<BlockPos> ring;
    for (int i = -1; i <= 1; ++i)
        for (const BlockPos p : {BlockPos{i, 64, -2}, BlockPos{i, 64, 2}, BlockPos{-2, 64, i}, BlockPos{2, 64, i}}) {
            s.world.setBlock(p, frame);
            ring.push_back(p);
        }
    const ItemId eye = *itemRegistry().find("ender_eye");
    for (size_t i = 0; i < ring.size(); ++i) {
        CHECK(portals::useItem(s.world, Dimension::Overworld, eye, ring[i], Direction::Up, s.changed));
        CHECK(s.at({0, 64, 0}) == (i + 1 == ring.size() ? blocks::EndPortal : 0));
    }
    CHECK_FALSE(portals::useItem(s.world, Dimension::Overworld, eye, ring[0], Direction::Up, s.changed)); // already has one
    s.world.createChunk({6, 0});
    s.world.createChunk({6, -1});
    const glm::dvec3 arrive = portals::endPlatform(s.world, s.changed);
    CHECK(arrive.x == 100.5);
    CHECK(s.at({100, 48, 0}) == blocks::Obsidian);
    CHECK(s.at({100, 49, 0}) == 0);
}

TEST_CASE("flint and steel doesn't light portals in the End") {
    Scene s;
    s.frame(0, 64, 0, 2, 3);
    const ItemId flint = *itemRegistry().find("flint_and_steel");
    CHECK_FALSE(portals::useItem(s.world, Dimension::End, flint, {0, 63, 0}, Direction::Up, s.changed));
    CHECK(portals::useItem(s.world, Dimension::Overworld, flint, {0, 63, 0}, Direction::Up, s.changed));
    CHECK(s.at({0, 64, 0}) == blocks::NetherPortal);
}
