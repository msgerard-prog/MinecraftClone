// Daylight cycle (wiki: Daylight cycle).
#include "world/Biome.h"
#include "world/DayTime.h"

#include <doctest/doctest.h>

#include <cmath>

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

TEST_CASE("internal sky light follows the wiki's table") {
    CHECK(internalSkyLight(6000) == 15);
    CHECK(internalSkyLight(12040) == 15);
    CHECK(internalSkyLight(12041) == 14);
    CHECK(internalSkyLight(13670) == 4);
    CHECK(internalSkyLight(18000) == 4);
    CHECK(internalSkyLight(22330) == 4);
    CHECK(internalSkyLight(23961) == 15);
}

TEST_CASE("the sun rises in the east, is overhead at noon and sets in the west") {
    const auto rise = sunDirection(celestialAngle(0));
    CHECK(rise.x > 0.9);
    CHECK(rise.y > 0.0); // just above the horizon (it rose around tick 23000)
    CHECK(rise.y < 0.3);
    CHECK(sunDirection(celestialAngle(22800)).y < 0.0); // still below before sunrise
    const auto noon = sunDirection(celestialAngle(6000));
    CHECK(noon.y == doctest::Approx(1.0));
    const auto set = sunDirection(celestialAngle(12000));
    CHECK(set.x < -0.9);
}

TEST_CASE("sky colour follows the biome temperature (plains #78A7FF)") {
    using mc::world::skyColorFor;
    CHECK(skyColorFor(0.8f) == 0x78A7FFu);               // plains
    CHECK(skyColorFor(mc::world::biomeInfo(mc::world::Biome::Plains).temperature) == 0x78A7FFu);
    // Warm skies shift toward cyan, cold ones toward violet (wiki biome sky colours).
    CHECK(skyColorFor(2.0f) == 0x6EB1FFu); // desert
    CHECK(skyColorFor(0.0f) == 0x7FA1FFu); // snowy plains
}

TEST_CASE("the sunrise glow shows only while the sun is near the horizon") {
    using namespace mc::world;
    CHECK(sunriseColor(celestialAngle(6000)).a == 0.0f);  // noon
    CHECK(sunriseColor(celestialAngle(18000)).a == 0.0f); // midnight
    const SunriseColor dawn = sunriseColor(celestialAngle(0));
    CHECK(dawn.a > 0.3f);
    CHECK(dawn.r > dawn.g); // orange-red
    CHECK(dawn.g > dawn.b);
    CHECK(sunriseColor(celestialAngle(12000)).a > 0.3f); // and at sunset
}
