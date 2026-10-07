// Chat commands (wiki: Commands/teleport, Commands/time, Commands/give).
#include "gameplay/Commands.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;

namespace {

struct Ctx {
    Player player;
    Hotbar hotbar;
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

TEST_CASE("/give fills the first empty hotbar slot, else the selected one; unknown ids fail") {
    Ctx c;
    const auto glow = mc::world::blockRegistry().defaultState(mc::world::blocks::Glowstone);
    c.hotbar.select(2);
    CHECK(runCommand("/give @s minecraft:glowstone", c.ctx).ok); // hotbar full: selected
    CHECK(c.hotbar.selectedBlock() == glow);
    c.hotbar.setSlot(6, 0);
    const auto r = runCommand("/give @p oak_log[axis=x] 64", c.ctx);
    CHECK(r.ok);
    CHECK(r.message == "Gave 64 [oak_log[axis=x]] to Player");
    CHECK(c.hotbar.slot(6) != 0);
    CHECK_FALSE(runCommand("/give @s stone 0", c.ctx).ok);
    CHECK_FALSE(runCommand("/give @s stone lots", c.ctx).ok);
    CHECK_FALSE(runCommand("/give @s not_a_block", c.ctx).ok);
    CHECK_FALSE(runCommand("/frobnicate", c.ctx).ok);
    CHECK(runCommand("/seed", c.ctx).message == "Seed: [42]");
}
