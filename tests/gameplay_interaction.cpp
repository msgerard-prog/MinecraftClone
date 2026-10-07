#include "gameplay/BlockInteraction.h"
#include "gameplay/Inventory.h"
#include "world/Blocks.h"
#include "world/Redstone.h"

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
    Inventory h;
    CHECK(h.placeState() == S(blocks::Stone));
    h.select(4);
    CHECK(h.placeState() == S(blocks::OakPlanks));
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

TEST_CASE("inventory: add stacks onto matching stacks first, then empty slots; max 64") {
    Inventory inv;
    for (int i = 0; i < Inventory::kSlots; ++i)
        inv.setSlot(i, {});
    const auto& items = itemRegistry();
    const ItemStack dirt = Inventory::blockStack(S(blocks::Dirt), 40);
    CHECK(inv.add(dirt) == 0);
    CHECK(inv.add(dirt) == 0); // 40 + 24 into slot 0, 16 into slot 1
    CHECK(inv.slot(0).count == 64);
    CHECK(inv.slot(1).count == 16);
    const ItemStack pick{*items.find("iron_pickaxe"), 1};
    CHECK(inv.add(pick) == 0);
    CHECK(inv.add(pick) == 0); // tools don't stack
    CHECK(inv.slot(2).item == pick.item);
    CHECK(inv.slot(3).item == pick.item);
    CHECK(inv.placeState() == S(blocks::Dirt)); // slot 0 selected
    inv.consumeSelected(64);
    CHECK(inv.slot(0).empty());
    CHECK(inv.placeState() == 0);
    // An exact state survives (log axis).
    const auto logX = *blockRegistry().with(S(blocks::OakLog), "axis", "x");
    inv.setSlot(0, Inventory::blockStack(logX));
    CHECK(inv.placeState() == logX);
}

namespace {

struct SurvivalScene : Scene {
    Inventory inv;
    Vitals vitals;
    Xoroshiro rng{7};
    std::vector<BlockInteraction::Drop> drops;
    SurvivalScene() : Scene(0.0f, 60.0f) {
        for (int i = 0; i < Inventory::kSlots; ++i)
            inv.setSlot(i, {});
    }
    // Holds attack until the block breaks; returns ticks taken (or -1 after 400).
    int breakTarget() {
        InteractionInput in;
        in.attack = true;
        for (int t = 1; t <= 400; ++t) {
            interaction.tickSurvival(world, player, BlockInteraction::target(world, player), inv,
                                     vitals, in, false, rng, changed, drops);
            if (!changed.empty()) return t;
        }
        return -1;
    }
};

} // namespace

TEST_CASE("survival: stone takes 7.5 s by hand and drops nothing; 1.15 s with a wooden pickaxe") {
    SurvivalScene hand;
    CHECK(hand.breakTarget() == 150);
    CHECK(hand.drops.empty());
    SurvivalScene pick;
    pick.inv.setSlot(0, {*itemRegistry().find("wooden_pickaxe"), 1});
    CHECK(pick.breakTarget() == 23);
    REQUIRE(pick.drops.size() == 1);
    CHECK(itemRegistry().item(pick.drops[0].stack.item).id == "minecraft:cobblestone");
    CHECK(pick.inv.slot(0).damage == 1); // tools wear
    CHECK(pick.vitals.exhaustion() == doctest::Approx(0.005f));
}

TEST_CASE("survival: switching target restarts progress; a pause follows each break") {
    SurvivalScene s;
    s.inv.setSlot(0, {*itemRegistry().find("diamond_pickaxe"), 1});
    CHECK(s.breakTarget() == 6);
    // Next block (the one under): 5-tick pause, then 6 ticks.
    CHECK(s.breakTarget() == 6 + 6); // 6-tick pause (wiki), then 6 ticks
}

TEST_CASE("survival: placing uses up the stack; worn-out tools break") {
    SurvivalScene s;
    s.inv.setSlot(0, Inventory::blockStack(S(blocks::Dirt), 2));
    InteractionInput use;
    use.useClick = true;
    s.interaction.tickSurvival(s.world, s.player, BlockInteraction::target(s.world, s.player), s.inv,
                               s.vitals, use, false, s.rng, s.changed, s.drops);
    CHECK(s.changed.size() == 1);
    CHECK(s.inv.slot(0).count == 1);
    const auto pickId = *itemRegistry().find("golden_pickaxe"); // 32 uses
    s.inv.setSlot(0, {pickId, 1, 31});
    CHECK(s.breakTarget() > 0);
    CHECK(s.inv.slot(0).empty());
}

TEST_CASE("survival: holding use with an apple eats it after 32 ticks when hungry") {
    SurvivalScene s;
    s.vitals.setState(20.0f, 10, 0.0f, 0.0f);
    s.inv.setSlot(0, {*itemRegistry().find("apple"), 2});
    InteractionInput use;
    use.use = true;
    for (int t = 0; t < 31; ++t)
        s.interaction.tickSurvival(s.world, s.player, std::nullopt, s.inv, s.vitals, use, false, s.rng,
                                   s.changed, s.drops);
    CHECK(s.vitals.food() == 10);
    s.interaction.tickSurvival(s.world, s.player, std::nullopt, s.inv, s.vitals, use, false, s.rng,
                               s.changed, s.drops);
    CHECK(s.vitals.food() == 14);
    CHECK(s.inv.slot(0).count == 1);
}

TEST_CASE("switching tools mid-break keeps the progress made (vanilla)") {
    // Regression: progress was recomputed from ticks held with the new tool.
    SurvivalScene s;
    InteractionInput in;
    in.attack = true;
    for (int t = 0; t < 100; ++t) // 100 of 150 ticks by hand: 2/3 done
        s.interaction.tickSurvival(s.world, s.player, BlockInteraction::target(s.world, s.player), s.inv,
                                   s.vitals, in, false, s.rng, s.changed, s.drops);
    CHECK(s.changed.empty());
    s.inv.setSlot(0, {*itemRegistry().find("wooden_pickaxe"), 1}); // 23 ticks for a whole block
    int more = 0;
    while (s.changed.empty() && more < 50) {
        s.interaction.tickSurvival(s.world, s.player, BlockInteraction::target(s.world, s.player), s.inv,
                                   s.vitals, in, false, s.rng, s.changed, s.drops);
        ++more;
    }
    CHECK(more == 8); // the remaining third: ceil(23 / 3)
}

TEST_CASE("breaking a furnace drops its contents, in creative too") {
    Scene s(0.0f, 60.0f);
    const auto t = BlockInteraction::target(s.world, s.player);
    REQUIRE(t.has_value());
    s.world.setBlock(t->block, S(blocks::Furnace));
    s.world.chunk(t->block.chunk())->furnace(blockToLocal(t->block.x), t->block.y, blockToLocal(t->block.z))->input =
        {*itemRegistry().find("raw_iron"), 7};
    std::vector<BlockInteraction::Drop> drops;
    InteractionInput in;
    in.attackClick = true;
    s.interaction.tick(s.world, s.player, t, 0, in, s.changed, &drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].stack.count == 7);
}

TEST_CASE("instant breaks have no pause; swords wear 2 and can't break in creative") {
    SurvivalScene s;
    s.world.setBlock(neighbour(BlockInteraction::target(s.world, s.player)->block, Direction::Up),
                     S(blocks::ShortGrass)); // a plant on the targeted block's top
    const auto grass = BlockInteraction::target(s.world, s.player);
    REQUIRE(grass.has_value());
    // (the raycast skips nothing solid: grass is targeted; it breaks on the first tick)
    Scene creative(0.0f, 60.0f);
    InteractionInput in;
    in.attackClick = true;
    std::vector<BlockInteraction::Drop> drops;
    creative.interaction.tick(creative.world, creative.player, BlockInteraction::target(creative.world, creative.player),
                              0, in, creative.changed, &drops, /*holdingSword=*/true);
    CHECK(creative.changed.empty());
    SurvivalScene sword;
    sword.inv.setSlot(0, {*itemRegistry().find("iron_sword"), 1});
    CHECK(sword.breakTarget() > 0);
    CHECK(sword.inv.slot(0).damage == 2);
}

TEST_CASE("right-clicking a lever uses it (sneaking places instead); dust can't go into water") {
    Scene s(0.0f, 60.0f);
    World& w = s.world;
    const auto t = BlockInteraction::target(w, s.player);
    REQUIRE(t);
    mc::world::Redstone redstone(w);
    s.interaction.setRedstone(&redstone);
    const auto lever = *blockRegistry().with(*blockRegistry().with(S(blocks::Lever), "face", "floor"), "facing", "north");
    const BlockPos above{t->block.x, t->block.y + 1, t->block.z};
    w.setBlock(above, lever); // the ray now hits the lever
    const auto onLever = BlockInteraction::target(w, s.player);
    REQUIRE(onLever);
    REQUIRE(onLever->block == above);
    s.tick(false, true, S(blocks::Stone));
    CHECK(blockRegistry().value(w.getBlock(above), "powered") == "true");
    CHECK(w.getBlock({above.x, above.y + 1, above.z}) == 0); // used, not placed on
    // Water: dust is not placed into it (vanilla: non-solid blocks can't be).
    w.setBlock(above, S(blocks::Water));
    const auto wire = S(blocks::RedstoneWire);
    for (int i = 0; i < 5; ++i)
        s.tick(false, true, wire);
    CHECK(blockRegistry().blockOf(w.getBlock(above)) == blocks::Water);
}
