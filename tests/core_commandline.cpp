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
