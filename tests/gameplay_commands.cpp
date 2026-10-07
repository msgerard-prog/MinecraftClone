// Chat commands (wiki: Commands/teleport, Commands/time, Commands/give).
#include "gameplay/Commands.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;

namespace {

struct Ctx {
    Player player;
    Inventory hotbar;
    int64_t dayTime = 0;
    CommandContext ctx{player, hotbar, dayTime, 0, 42};
    Ctx() { player.setPosition({10.5, 70.0, -3.5}); }
};

} // namespace

TEST_CASE("/tp: absolute whole x/z are block centres, ~ is relative, rotation optional") {
    Ctx c;
    CHECK(runCommand("/tp 100 64 -20", c.ctx).ok);
    CHECK(c.player.position().x == doctest::Approx(100.5));
    CHECK(c.player.position().y == doctest::Approx(64.0));
    CHECK(c.player.position().z == doctest::Approx(-19.5));
    CHECK(runCommand("tp @s ~ ~10 ~-1.5", c.ctx).ok);
    CHECK(c.player.position().x == doctest::Approx(100.5));
    CHECK(c.player.position().y == doctest::Approx(74.0));
    CHECK(c.player.position().z == doctest::Approx(-21.0));
    CHECK(runCommand("/teleport 0.25 70 0.25 90 -30", c.ctx).ok);
    CHECK(c.player.position().x == doctest::Approx(0.25));
    CHECK(c.player.yaw() == doctest::Approx(90.0f));
    CHECK(c.player.pitch() == doctest::Approx(-30.0f));
    CHECK_FALSE(runCommand("/tp 1 2", c.ctx).ok);
    CHECK_FALSE(runCommand("/tp @e 1 2 3", c.ctx).ok);
    CHECK_FALSE(runCommand("/tp a b c", c.ctx).ok);
}

TEST_CASE("/time set sets the absolute time (the day count restarts); add and query") {
    Ctx c;
    c.dayTime = 3 * 24000 + 500;
    CHECK(runCommand("/time set night", c.ctx).ok);
    CHECK(c.dayTime == 13000); // wiki: Commands/time - the day value returns to 0
    CHECK(runCommand("/time query day", c.ctx).message == "The time is 0");
    CHECK(runCommand("/time set noon", c.ctx).ok);
    CHECK(c.dayTime == 6000);
    CHECK(runCommand("/time add 1d", c.ctx).ok);
    CHECK(c.dayTime == 24000 + 6000);
    CHECK(runCommand("/time add 10s", c.ctx).ok);
    CHECK(c.dayTime == 24000 + 6200);
    const auto q = runCommand("/time query day", c.ctx);
    CHECK(q.ok);
    CHECK(q.message == "The time is 1");
    CHECK_FALSE(runCommand("/time set dusk", c.ctx).ok);
    // Regression: huge values overflowed into negative day times.
    CHECK_FALSE(runCommand("/time add 100000d", c.ctx).ok);
    CHECK(c.dayTime == 24000 + 6200);
}

TEST_CASE("/tp rejects nan, infinity and positions outside the world limits") {
    Ctx c;
    for (const char* bad : {"/tp nan 0 0", "/tp 0 inf 0", "/tp 0 0 0 0 nan", "/tp 3e9 0 0",
                            "/tp 0 0 30000000", "/tp 0 20000000 0"})
        CHECK_FALSE(runCommand(bad, c.ctx).ok);
    CHECK(c.player.position().x == doctest::Approx(10.5)); // unchanged
    CHECK(runCommand("/tp 29999999 0 0", c.ctx).ok);
}

TEST_CASE("/give adds items to the inventory like pickups; unknown ids fail") {
    Ctx c;
    const auto& items = mc::world::itemRegistry();
    CHECK(runCommand("/give @s minecraft:glowstone 3", c.ctx).ok); // stacks onto slot 8
    CHECK(c.hotbar.slot(8).count == 4);
    const auto r = runCommand("/give @p oak_log[axis=x] 64", c.ctx);
    CHECK(r.ok);
    CHECK(r.message == "Gave 64 [oak_log[axis=x]] to Player");
    CHECK(c.hotbar.slot(9).count == 64); // first empty slot (main inventory)
    CHECK(c.hotbar.slot(9).state != 0);
    CHECK(runCommand("/give @s diamond_pickaxe 2", c.ctx).ok);
    CHECK(c.hotbar.slot(10).item == *items.find("diamond_pickaxe"));
    CHECK(c.hotbar.slot(11).item == *items.find("diamond_pickaxe"));
    CHECK_FALSE(runCommand("/give @s stone 0", c.ctx).ok);
    CHECK_FALSE(runCommand("/give @s stone lots", c.ctx).ok);
    CHECK_FALSE(runCommand("/give @s not_a_block", c.ctx).ok);
    CHECK_FALSE(runCommand("/frobnicate", c.ctx).ok);
    CHECK(runCommand("/seed", c.ctx).message == "Seed: [42]");
}

TEST_CASE("/gamemode switches survival/creative; /kill kills") {
    Ctx c;
    bool survival = false;
    mc::Vitals vitals;
    c.ctx.survival = &survival;
    c.ctx.vitals = &vitals;
    CHECK(runCommand("/gamemode survival", c.ctx).ok);
    CHECK(survival);
    CHECK(runCommand("/gamemode creative", c.ctx).message == "Set own game mode to Creative Mode");
    CHECK_FALSE(survival);
    CHECK_FALSE(runCommand("/gamemode 0", c.ctx).ok); // numeric ids are gone (wiki)
    CHECK(runCommand("/kill", c.ctx).ok);
    CHECK(vitals.dead());
}

TEST_CASE("/setblock places a block state and updates its neighbours") {
    Ctx c;
    mc::world::World w;
    for (int cz = -1; cz <= 0; ++cz)
        for (int cx = 0; cx <= 1; ++cx)
            w.createChunk({cx, cz});
    std::vector<mc::world::BlockPos> changed;
    c.ctx.world = &w;
    c.ctx.changed = &changed;
    CHECK(runCommand("/setblock 12 70 -4 repeater[facing=west,delay=3]", c.ctx).ok);
    const auto s = w.getBlock({12, 70, -4});
    CHECK(mc::world::blockRegistry().toString(s) == "minecraft:repeater[delay=3,facing=west,locked=false,powered=false]");
    CHECK(changed.size() == 1);
    CHECK(runCommand("/setblock ~ ~ ~ stone", c.ctx).ok); // relative: the player's block
    CHECK(w.getBlock({10, 70, -4}) == mc::world::blockRegistry().defaultState(mc::world::blocks::Stone));
    CHECK_FALSE(runCommand("/setblock 12 70 -4 repeater[facing=west,delay=3]", c.ctx).ok); // unchanged
    CHECK_FALSE(runCommand("/setblock 12 70 -4 nonsense", c.ctx).ok);
    CHECK_FALSE(runCommand("/setblock 500 70 0 stone", c.ctx).ok); // not loaded
}

TEST_CASE("/fill: places a box, then updates; limits and errors") {
    Ctx c;
    mc::world::World w;
    for (int cz = -1; cz <= 0; ++cz)
        for (int cx = 0; cx <= 1; ++cx)
            w.createChunk({cx, cz});
    std::vector<mc::world::BlockPos> changed;
    c.ctx.world = &w;
    c.ctx.changed = &changed;
    const auto r = runCommand("/fill 10 70 -5 12 71 -4 stone", c.ctx);
    CHECK(r.ok);
    CHECK(r.message == "Successfully filled 12 block(s)");
    CHECK(changed.size() == 12);
    CHECK_FALSE(runCommand("/fill 10 70 -5 12 71 -4 stone", c.ctx).ok); // nothing changed
    CHECK_FALSE(runCommand("/fill 0 0 0 100 100 100 stone", c.ctx).ok); // > 32768
    CHECK_FALSE(runCommand("/fill 0 70 0 40 70 0 stone", c.ctx).ok);    // not loaded
}

TEST_CASE("/summon takes a few of vanilla's data tags: Color, Sheared, Age, Health") {
    Ctx c;
    mc::world::World w;
    w.createChunk({0, -1});
    mc::world::Xoroshiro rng(1);
    c.ctx.world = &w;
    c.ctx.rng = &rng;
    CHECK(runCommand("/summon sheep 10 70 -4 {Color:14b,Age:-24000,Sheared:1b}", c.ctx).ok);
    const auto& m = w.chunk({0, -1})->mobs().at(0);
    CHECK(m.pos.x == doctest::Approx(10.5)); // the position is used with tags too
    CHECK(m.pos.z == doctest::Approx(-3.5));
    CHECK(m.woolColour == 14);
    CHECK(m.age == -24000);
    CHECK(m.sheared);
    CHECK_FALSE(runCommand("/summon pig 10 70 -4 {Saddle:1b}", c.ctx).ok); // unknown tag
    CHECK_FALSE(runCommand("/summon pig 10 70 -4 {Age:x}", c.ctx).ok);
}

TEST_CASE("/xp adds points or whole levels") {
    Ctx c;
    Vitals v;
    c.ctx.vitals = &v;
    CHECK(runCommand("/xp add @s 3 levels", c.ctx).ok);
    CHECK(v.xpLevel() == 3);
    CHECK(runCommand("/xp add @s 4", c.ctx).ok);
    CHECK(v.xpProgress() > 0.0f);
    CHECK_FALSE(runCommand("/xp add @s -1", c.ctx).ok);
}
