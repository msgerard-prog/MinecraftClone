#include "core/CommandLine.h"

#include <doctest/doctest.h>

#include <array>

TEST_CASE("command line: screenshot options") {
    std::array<const char*, 6> args = {"--screenshot", "out/shot.png", "--frames", "5",
                                       "--size",       "640x360"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->screenshotPath == "out/shot.png");
    CHECK(opts->screenshotFrames == 5);
    CHECK(opts->width == 640);
    CHECK(opts->height == 360);
}

TEST_CASE("command line: rejects bad input") {
    std::string error;
    std::array<const char*, 1> unknown = {"--bogus"};
    CHECK_FALSE(mc::parseCommandLine(unknown, error).has_value());
    std::array<const char*, 2> badSize = {"--size", "640"};
    CHECK_FALSE(mc::parseCommandLine(badSize, error).has_value());
    std::array<const char*, 1> missing = {"--seed"};
    CHECK_FALSE(mc::parseCommandLine(missing, error).has_value());
}

TEST_CASE("command line: --pos and --look") {
    std::array<const char*, 4> args = {"--pos", "-1.5,70,2.25", "--look", "-45,30.5"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->hasPos);
    CHECK(opts->pos.x == doctest::Approx(-1.5));
    CHECK(opts->pos.y == doctest::Approx(70));
    CHECK(opts->pos.z == doctest::Approx(2.25));
    CHECK(opts->hasLook);
    CHECK(opts->yaw == doctest::Approx(-45));
    CHECK(opts->pitch == doctest::Approx(30.5));

    std::array<const char*, 2> tooFew = {"--pos", "1,2"};
    CHECK_FALSE(mc::parseCommandLine(tooFew, error).has_value());
    std::array<const char*, 2> tooMany = {"--look", "1,2,3"};
    CHECK_FALSE(mc::parseCommandLine(tooMany, error).has_value());
    std::array<const char*, 2> notANumber = {"--look", "0,nan"};
    CHECK_FALSE(mc::parseCommandLine(notANumber, error).has_value());
    std::array<const char*, 2> infinite = {"--pos", "inf,0,0"};
    CHECK_FALSE(mc::parseCommandLine(infinite, error).has_value());
}

TEST_CASE("command line: M3 options") {
    std::string error;
    std::array<const char*, 7> ok = {"--render-distance", "32",     "--max-fps", "240",
                                     "--auto-fly",        "--flat", "--no-vsync"};
    const auto opts = mc::parseCommandLine(ok, error);
    REQUIRE(opts.has_value());
    CHECK(opts->renderDistance == 32);
    CHECK(opts->maxFps == 240);
    CHECK(opts->autoFly);
    CHECK(opts->flat);
    CHECK_FALSE(opts->vsync);
    std::array<const char*, 2> packs = {"--resourcepacks", "C:/packs"};
    CHECK(mc::parseCommandLine(packs, error)->resourcePacks == "C:/packs");
    for (const char* bad : {"0", "1", "33", "x"}) {
        std::array<const char*, 2> rd = {"--render-distance", bad};
        CHECK_FALSE(mc::parseCommandLine(rd, error).has_value());
    }
    std::array<const char*, 2> fps = {"--max-fps", "0"};
    CHECK_FALSE(mc::parseCommandLine(fps, error).has_value());
    std::array<const char*, 1> noDir = {"--resourcepacks"};
    CHECK_FALSE(mc::parseCommandLine(noDir, error).has_value());
}

TEST_CASE("command line: --time sets the day time in ticks") {
    std::array<const char*, 2> args = {"--time", "18000"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->time == 18000);
    std::array<const char*, 2> bad = {"--time", "-5"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --trade opens a villager's trades") {
    std::array<const char*, 1> args = {"--trade"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->trade);
}

TEST_CASE("command line: --mount rides the nearest mount") {
    std::array<const char*, 2> args = {"--mount", "--inventory"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->mount);
    CHECK(opts->inventory);
}

TEST_CASE("command line: --f3, --inventory, repeatable --command, signed --seed") {
    std::array<const char*, 8> args = {"--f3",    "--inventory", "--command", "/time set night",
                                       "--command", "/seed",     "--seed",    "-42"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->debugScreen);
    CHECK(opts->inventory);
    REQUIRE(opts->commands.size() == 2);
    CHECK(opts->commands[0] == "/time set night");
    CHECK(opts->commands[1] == "/seed");
    CHECK(static_cast<int64_t>(opts->seed) == -42);
    std::array<const char*, 1> missing = {"--command"};
    CHECK_FALSE(mc::parseCommandLine(missing, error).has_value());
}

TEST_CASE("command line: --world takes a folder name only; --no-save") {
    std::string error;
    std::array<const char*, 3> ok = {"--world", "My World", "--no-save"};
    const auto opts = mc::parseCommandLine(ok, error);
    REQUIRE(opts.has_value());
    CHECK(opts->world == "My World");
    CHECK(opts->noSave);
    for (const char* bad : {"../x", "a/b", "c:\\d", ".."}) {
        std::array<const char*, 2> args = {"--world", bad};
        CHECK_FALSE(mc::parseCommandLine(args, error).has_value());
    }
}

TEST_CASE("command line: --dimension overworld|nether|end") {
    std::array<const char*, 2> ok = {"--dimension", "nether"};
    std::string error;
    auto opts = mc::parseCommandLine(ok, error);
    REQUIRE(opts.has_value());
    CHECK(opts->dimension == "nether");
    std::array<const char*, 2> bad = {"--dimension", "aether"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --generator overworld6|overworld5|overworld4|overworld3|overworld2|overworld|terrain; the newest is the default") {
    std::string error;
    std::array<const char*, 2> ok = {"--generator", "terrain"};
    const auto opts = mc::parseCommandLine(ok, error);
    REQUIRE(opts.has_value());
    CHECK(opts->generator == "terrain");
    std::array<const char*, 0> none = {};
    CHECK(mc::parseCommandLine(none, error)->generator == "overworld7"); // the default
    std::array<const char*, 2> v2 = {"--generator", "overworld2"};
    CHECK(mc::parseCommandLine(v2, error)->generator == "overworld2");
    std::array<const char*, 2> old = {"--generator", "overworld"};
    CHECK(mc::parseCommandLine(old, error)->generator == "overworld");
    std::array<const char*, 2> bad = {"--generator", "flat"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --version") {
    std::string error;
    std::array<const char*, 1> v = {"--version"};
    const auto opts = mc::parseCommandLine(v, error);
    REQUIRE(opts.has_value());
    CHECK(opts->printVersion);
}

TEST_CASE("command line: --open-block takes whole block coordinates") {
    std::string error;
    std::array<const char*, 2> ok = {"--open-block", "1,-60,-3"};
    const auto o = mc::parseCommandLine(ok, error);
    REQUIRE(o);
    CHECK(o->hasOpenBlock);
    CHECK(o->openBlock[1] == -60);
    std::array<const char*, 2> bad = {"--open-block", "1.5,2,3"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --mute") {
    std::array<const char*, 2> args = {"--mute", "--sound"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->mute);
    CHECK(opts->sound);
    std::array<const char*, 2> menu = {"--menu", "worlds"};
    CHECK(mc::parseCommandLine(menu, error)->menu == "worlds");
    std::array<const char*, 2> stats = {"--menu", "statistics"};
    CHECK(mc::parseCommandLine(stats, error)->menu == "statistics");
    std::array<const char*, 2> advMenu = {"--menu", "advancements"};
    CHECK(mc::parseCommandLine(advMenu, error)->menu == "advancements");
    std::array<const char*, 2> cmdMenu = {"--menu", "commandblock"}; // (M29.7)
    CHECK(mc::parseCommandLine(cmdMenu, error)->menu == "commandblock");
    std::array<const char*, 2> bad = {"--menu", "nope"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --difficulty") {
    std::array<const char*, 2> args = {"--difficulty", "peaceful"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->difficulty == 0);
    std::array<const char*, 0> none{};
    CHECK(mc::parseCommandLine(none, error)->difficulty == 2);
    std::array<const char*, 2> bad = {"--difficulty", "extreme"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --book TEXT") {
    std::array<const char*, 2> args = {"--book", "Hello"};
    std::string error;
    const auto opts = mc::parseCommandLine(args, error);
    REQUIRE(opts.has_value());
    CHECK(opts->book);
    CHECK(opts->bookText == "Hello");
}

TEST_CASE("command line: --use N") {
    std::array<const char*, 2> args = {"--use", "3"};
    std::string error;
    CHECK(mc::parseCommandLine(args, error)->use == 3);
    std::array<const char*, 2> bad = {"--use", "0"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}

TEST_CASE("command line: --perspective 0|1|2 (M30.1)") {
    std::string error;
    std::array<const char*, 2> back = {"--perspective", "1"};
    const auto opts = mc::parseCommandLine(back, error);
    REQUIRE(opts.has_value());
    CHECK(opts->perspective == 1);
    std::array<const char*, 2> bad = {"--perspective", "3"};
    CHECK_FALSE(mc::parseCommandLine(bad, error).has_value());
}
