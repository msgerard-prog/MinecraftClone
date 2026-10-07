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
