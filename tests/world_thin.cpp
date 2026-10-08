// Thin and small blocks (M23.2; wiki: Glass Pane, Carpet, Ladder, Lantern, Torch, Dye).
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "gameplay/Recipes.h"
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <doctest/doctest.h>

#include <array>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(std::string_view text) { return *R().parse(text); }
ItemId I(std::string_view id) { return *itemRegistry().find(id); }

struct Scene {
    World world;
    BlockUpdates updates{world};
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, R().defaultState(blocks::Stone));
            }
    }
    BlockStateId at(BlockPos p) const { return world.getBlock(p); }
};

} // namespace

TEST_CASE("thin blocks: panes join panes, glass and full blocks") {
    Scene s;
    s.world.updateBlock({0, 64, 0}, S("glass_pane"));
    s.world.updateBlock({1, 64, 0}, S("red_stained_glass_pane"));
    s.world.updateBlock({0, 64, 1}, S("stone"));
    CHECK(R().value(s.at({0, 64, 0}), "east") == "true");
    CHECK(R().value(s.at({0, 64, 0}), "south") == "true");
    CHECK(R().value(s.at({0, 64, 0}), "north") == "false");
    CHECK(R().value(s.at({1, 64, 0}), "west") == "true");
    CHECK(collisionShape(s.at({0, 64, 0})).count == 3); // post + 2 arms
}

TEST_CASE("thin blocks: torches, wall torches, lanterns, ladders and carpets need support") {
    Scene s;
    s.world.updateBlock({0, 64, 0}, S("torch"));
    s.world.updateBlock({0, 64, 3}, S("white_carpet"));
    s.world.setBlock({5, 64, 5}, S("stone"));
    s.world.updateBlock({5, 64, 4}, S("wall_torch[facing=north]"));
    s.world.updateBlock({5, 65, 4}, S("ladder[facing=north]"));
    s.world.setBlock({8, 66, 8}, S("stone"));
    s.world.updateBlock({8, 65, 8}, S("lantern[hanging=true]"));
    s.world.updateBlock({5, 64, 5}, 0); // the wall goes: wall torch and ladder drop
    CHECK(s.at({5, 64, 4}) == 0);
    CHECK(s.at({5, 65, 4}) == 0);
    s.world.updateBlock({8, 66, 8}, 0); // the ceiling goes: the lantern drops
    CHECK(s.at({8, 65, 8}) == 0);
    s.world.updateBlock({0, 63, 0}, 0); // the floor under the torch goes
    CHECK(s.at({0, 64, 0}) == 0);
    CHECK(R().blockOf(s.at({0, 64, 3})) == *R().findBlock("white_carpet")); // (still on stone)
    // Placement: torches turn into wall torches on a side, lanterns hang under blocks.
    s.world.setBlock({10, 65, 10}, S("stone"));
    const auto wall = BlockUpdates::placement(s.world, S("torch"), {10, 65, 9}, Direction::North, 0, 0);
    REQUIRE(wall);
    CHECK(R().blockOf(*wall) == blocks::WallTorch);
    const auto hang = BlockUpdates::placement(s.world, S("lantern"), {10, 64, 10}, Direction::Down, 0, 0);
    REQUIRE(hang);
    CHECK(R().value(*hang, "hanging") == "true");
    CHECK_FALSE(BlockUpdates::placement(s.world, S("ladder"), {12, 64, 12}, Direction::Up, 0, 0)); // only on sides
    CHECK_FALSE(BlockUpdates::placement(s.world, S("white_carpet"), {12, 70, 12}, Direction::Up, 0, 0)); // nothing below
}

TEST_CASE("thin blocks: wall torches drop torches; panes need Silk Touch; nether gold ore drops nuggets") {
    Xoroshiro rng(3);
    std::vector<ItemStack> drops;
    blockDrops(S("wall_torch[facing=east]"), {}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == I("torch"));
    drops.clear();
    blockDrops(S("glass_pane"), {}, rng, drops);
    CHECK(drops.empty());
    drops.clear();
    blockDrops(R().defaultState(blocks::NetherGoldOre), {I("wooden_pickaxe"), 1}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == I("gold_nugget"));
    CHECK(drops[0].count >= 2);
    CHECK(drops[0].count <= 6);
}

TEST_CASE("dyes: stained glass, wool and sheep take the colour") {
    std::array<ItemStack, 9> grid{};
    for (auto& g : grid)
        g = {I("glass"), 1};
    grid[4] = {I("blue_dye"), 1};
    const auto glass = craft(grid, 3);
    REQUIRE(glass);
    CHECK(glass->item == I("blue_stained_glass"));
    CHECK(glass->count == 8);
    std::array<ItemStack, 4> two{};
    two[0] = {I("white_wool"), 1};
    two[1] = {I("lime_dye"), 1};
    const auto wool = craft(two, 2);
    REQUIRE(wool);
    CHECK(wool->item == I("lime_wool"));
    std::array<ItemStack, 4> flower{};
    flower[0] = {I("poppy"), 1};
    CHECK(craft(flower, 2)->item == I("red_dye"));
    // Right-clicking a sheep with a dye recolours it.
    Xoroshiro rng(5);
    ItemEntities items;
    MobData sheep = Mobs::make(MobType::Sheep, {0, 64, 0}, rng);
    sheep.woolColour = 0;
    CHECK(Mobs::interact(sheep, I("red_dye"), rng, items) == Mobs::Use::Fed);
    CHECK(sheep.woolColour == 14); // red
}
