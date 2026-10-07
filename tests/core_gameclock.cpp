#include "core/GameClock.h"

#include <doctest/doctest.h>

TEST_CASE("GameClock runs 20 ticks per simulated second") {
    mc::GameClock clock;
    int ticks = 0;
    for (int frame = 0; frame < 60; ++frame) { // one second at 60 fps
        clock.advance(1.0 / 60.0);
        ticks += clock.ticksDue;
    }
    CHECK(ticks >= 19);
    CHECK(ticks <= 20);
}

TEST_CASE("GameClock caps catch-up after a long stall") {
    mc::GameClock clock;
    clock.advance(5.0);
    CHECK(clock.ticksDue == mc::GameClock::kMaxTicksPerFrame);
}

#include "core/FrameStats.h"

TEST_CASE("FrameStats average, 99th percentile and max") {
    mc::FrameStats stats;
    for (int i = 0; i < 99; ++i)
        stats.add(10.0);
    stats.add(50.0);
    const auto s = stats.summarize();
    CHECK(s.frames == 100);
    CHECK(s.avgMs == doctest::Approx(10.4));
    CHECK(s.p99Ms == doctest::Approx(50.0));
    CHECK(s.maxMs == doctest::Approx(50.0));
}
