// Daylight cycle (wiki: Daylight cycle).
#include "world/DayTime.h"

#include <doctest/doctest.h>

using namespace mc::world;

TEST_CASE("celestial angle: noon at 6000 is the zenith, midnight at 18000 the nadir") {
    CHECK(celestialAngle(6000) == doctest::Approx(0.0));
    CHECK(celestialAngle(18000) == doctest::Approx(0.5));
    CHECK(celestialAngle(6000 + kTicksPerDay) == doctest::Approx(0.0)); // wraps by day
    // Angle grows through the day (sun moves one way).
    double prev = celestialAngle(6000);
    for (int t = 6100; t < 6000 + kTicksPerDay; t += 100) {
        const double a = celestialAngle(t);
        CHECK(a > prev);
        prev = a;
    }
}

TEST_CASE("day is bright, night is dark: sky light loses 11 levels at midnight") {
    CHECK(skyDarken(celestialAngle(6000)) == doctest::Approx(0.0));
    CHECK(skyDarken(celestialAngle(18000)) == doctest::Approx(11.0));
    CHECK(daylight(celestialAngle(6000)) == doctest::Approx(1.0));
    CHECK(daylight(celestialAngle(18000)) == doctest::Approx(0.0));
    // Sunrise (0) and sunset (12000) are in between; stars only at night.
    CHECK(skyDarken(celestialAngle(13000)) > 0.0);
    CHECK(starBrightness(celestialAngle(6000)) == 0.0);
    CHECK(starBrightness(celestialAngle(18000)) == doctest::Approx(0.5));
}

TEST_CASE("the moon changes phase every day, cycling through 8") {
    CHECK(moonPhase(0) == 0);
    CHECK(moonPhase(23999) == 0);
    CHECK(moonPhase(24000) == 1);
    CHECK(moonPhase(8 * 24000) == 0);
}
