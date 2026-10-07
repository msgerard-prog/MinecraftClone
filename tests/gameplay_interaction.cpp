#include "gameplay/BlockInteraction.h"
#include "gameplay/Hotbar.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

// Stone floor y 60..64 (deep enough to keep breaking); player stands on it at y 65.
struct Scene {
    World world;
    Player player;
    BlockInteraction interaction;
    std::vector<BlockPos> changed;
    Scene(float yaw, float pitch) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                auto& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        for (int y = 60; y <= 64; ++y)
                            c.set(x, y, z, S(blocks::Stone));
            }
        player.setPosition({0.5, 65.0, 0.5});
        for (int i = 0; i < 3; ++i)
            player.tick(world, {});
        player.setRotation(yaw, pitch);
    }
    void tick(bool attack, bool use, BlockStateId place) {
        InteractionInput in;
        in.attack = attack;
        in.use = use;
        interaction.tick(world, player, BlockInteraction::target(world, player), place, in,
                         changed);
    }
};

} // namespace

TEST_CASE("left click breaks the targeted block; holding repeats every 6 ticks (wiki: Creative)") {
    Scene s(0.0f, 60.0f); // looking down and south at the floor
    const auto t = BlockInteraction::target(s.world, s.player);
    REQUIRE(t.has_value());
    CHECK(t->block.y == 64);
    s.tick(true, false, 0);
    REQUIRE(s.changed.size() == 1);
    CHECK(s.world.getBlock(s.changed[0]) == 0);
    int breaks = 1;
    for (int i = 0; i < 10; ++i) {
        s.tick(true, false, 0);
        breaks += static_cast<int>(s.changed.size());
    }
    CHECK(breaks == 2); // ticks 0 and 6 within 11 ticks
}

TEST_CASE("right click places against the clicked face, never inside the player") {
    Scene s(0.0f, 60.0f);
    const auto t = BlockInteraction::target(s.world, s.player);
    REQUIRE(t.has_value());
    s.tick(false, true, S(blocks::Dirt));
    REQUIRE(s.changed.size() == 1);
    CHECK(s.changed[0] == neighbour(t->block, t->face));
    CHECK(s.world.getBlock(s.changed[0]) == S(blocks::Dirt));

    Scene down(0.0f, 90.0f); // straight down: the spot is where the player stands
    down.tick(false, true, S(blocks::Dirt));
    CHECK(down.changed.empty());
}

TEST_CASE("placed logs take the axis of the clicked face") {
    const auto log = S(blocks::OakLog);
    const auto& r = blockRegistry();
    CHECK(r.value(BlockInteraction::orientedState(log, Direction::East), "axis") == "x");
    CHECK(r.value(BlockInteraction::orientedState(log, Direction::North), "axis") == "z");
    CHECK(r.value(BlockInteraction::orientedState(log, Direction::Up), "axis") == "y");
    CHECK(BlockInteraction::orientedState(S(blocks::Stone), Direction::East) == S(blocks::Stone));
}

TEST_CASE("hotbar: number keys and the wheel (down = next slot) wrap around") {
    Hotbar h;
    CHECK(h.selectedBlock() == S(blocks::Stone));
    h.select(4);
    CHECK(h.selectedBlock() == S(blocks::OakPlanks));
    h.scroll(-1); // wheel down
    CHECK(h.selected() == 5);
    h.select(8);
    h.scroll(-1);
    CHECK(h.selected() == 0);
    h.scroll(1); // wheel up from slot 0 wraps to 8
    CHECK(h.selected() == 8);
}

TEST_CASE("placing into an unloaded chunk does nothing; a click acts despite cooldown") {
    Scene s(0.0f, 60.0f);
    const auto t = BlockInteraction::target(s.world, s.player);
    REQUIRE(t.has_value());
    // Pretend the target is at the edge of the loaded world: aim at a far fake hit.
    RayHit far = *t;
    far.block = {16 * 5, 64, 0}; // chunk (5, 0) is not loaded
    far.face = Direction::Up;
    InteractionInput use;
    use.useClick = true;
    s.interaction.tick(s.world, s.player, far, S(blocks::Dirt), use, s.changed);
    CHECK(s.changed.empty());
    // Two quick clicks in consecutive ticks both act (no 6-tick repeat delay).
    InteractionInput click;
    click.attackClick = true;
    s.interaction.tick(s.world, s.player, BlockInteraction::target(s.world, s.player), 0, click,
                       s.changed);
    CHECK(s.changed.size() == 1);
    s.interaction.tick(s.world, s.player, BlockInteraction::target(s.world, s.player), 0, click,
                       s.changed);
    CHECK(s.changed.size() == 1);
}

TEST_CASE("torches go on top of full blocks (glass too), never on sides or into water") {
    Scene s(0.0f, 60.0f); // looking down at the floor's top face
    s.tick(false, true, S(blocks::Torch));
    REQUIRE(s.changed.size() == 1);
    CHECK(s.world.getBlock(s.changed[0]) == S(blocks::Torch));
    const BlockPos torch = s.changed[0];

    Scene glass(0.0f, 60.0f);
    const auto t = BlockInteraction::target(glass.world, glass.player);
    REQUIRE(t.has_value());
    glass.world.setBlock(t->block, S(blocks::Glass)); // its top supports the centre
    glass.tick(false, true, S(blocks::Torch));
    CHECK(glass.changed.size() == 1);

    Scene water(0.0f, 60.0f);
    const auto wt = BlockInteraction::target(water.world, water.player);
    REQUIRE(wt.has_value());
    water.world.setBlock(neighbour(wt->block, wt->face), S(blocks::Water));
    water.tick(false, true, S(blocks::Torch));
    CHECK(water.changed.empty());

    Scene wall(0.0f, 0.0f); // looking south (+z) at a wall's side face
    wall.world.setBlock({0, 65, 3}, S(blocks::Stone));
    wall.world.setBlock({0, 66, 3}, S(blocks::Stone));
    wall.tick(false, true, S(blocks::Torch));
    CHECK(wall.changed.empty());
    CHECK(torch.y == 65);
}

TEST_CASE("an empty hotbar slot places nothing (and keeps water)") {
    // Regression: an empty slot used to "place" air, deleting water.
    Scene s(0.0f, 60.0f);
    const auto t = BlockInteraction::target(s.world, s.player);
    REQUIRE(t.has_value());
    const BlockPos front = neighbour(t->block, t->face);
    s.world.setBlock(front, S(blocks::Water));
    s.tick(false, true, 0);
    CHECK(s.changed.empty());
    CHECK(s.world.getBlock(front) == S(blocks::Water));
}
