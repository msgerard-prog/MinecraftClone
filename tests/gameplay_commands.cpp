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

TEST_CASE("/time set uses named times and keeps the day count; add and query") {
    Ctx c;
    c.dayTime = 3 * 24000 + 500;
    CHECK(runCommand("/time set night", c.ctx).ok);
    CHECK(c.dayTime == 3 * 24000 + 13000);
    CHECK(runCommand("/time set noon", c.ctx).ok);
    CHECK(c.dayTime % 24000 == 6000);
    CHECK(runCommand("/time add 1d", c.ctx).ok);
    CHECK(c.dayTime == 4 * 24000 + 6000);
    CHECK(runCommand("/time add 10s", c.ctx).ok);
    CHECK(c.dayTime == 4 * 24000 + 6200);
    const auto q = runCommand("/time query day", c.ctx);
    CHECK(q.ok);
    CHECK(q.message == "The time is 4");
    CHECK_FALSE(runCommand("/time set dusk", c.ctx).ok);
}

TEST_CASE("/give puts the block in the selected hotbar slot; unknown ids fail") {
    Ctx c;
    c.hotbar.select(2);
    CHECK(runCommand("/give @s minecraft:glowstone", c.ctx).ok);
    CHECK(c.hotbar.selectedBlock() ==
          mc::world::blockRegistry().defaultState(mc::world::blocks::Glowstone));
    CHECK(runCommand("/give @p oak_log[axis=x] 64", c.ctx).ok);
    CHECK_FALSE(runCommand("/give @s not_a_block", c.ctx).ok);
    CHECK_FALSE(runCommand("/frobnicate", c.ctx).ok);
    CHECK(runCommand("/seed", c.ctx).message == "Seed: [42]");
}
