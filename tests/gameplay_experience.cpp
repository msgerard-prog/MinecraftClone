// Experience (wiki: Experience, Experience orb).
#include "gameplay/ExperienceOrbs.h"
#include "gameplay/Mining.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

TEST_CASE("levels: 7 points for level 1, 2L+7 / 5L-38 / 9L-158 per level") {
    CHECK(Vitals::pointsForLevel(0) == 7);
    CHECK(Vitals::pointsForLevel(15) == 37);
    CHECK(Vitals::pointsForLevel(16) == 42);
    CHECK(Vitals::pointsForLevel(30) == 112);
    CHECK(Vitals::pointsForLevel(31) == 121);
    Vitals v;
    v.addExperience(7);
    CHECK(v.xpLevel() == 1);
    CHECK(v.xpProgress() == doctest::Approx(0.0f));
    v.addExperience(4); // half of level 1's 9
    CHECK(v.xpProgress() == doctest::Approx(4.0f / 9.0f));
    v.addExperience(1395 - 11); // total 1395 = level 30 (wiki)
    CHECK(v.xpLevel() == 30);
    CHECK(v.spendLevels(3));
    CHECK(v.xpLevel() == 27);
    CHECK_FALSE(v.spendLevels(40));
    CHECK(v.deathExperience() == 100); // min(7 x level, 100)
}

TEST_CASE("orbs split into vanilla's sizes, fly to a near player and are collected") {
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
        }
    ExperienceOrbs orbs;
    Xoroshiro rng(2);
    orbs.drop({4.5, 64.5, 0.5}, 25, rng); // 17 + 7 + 1
    CHECK(orbs.orbs().size() == 3);
    const Aabb player = Aabb::fromFeet({0.5, 64.0, 0.5}, 0.6, 1.8);
    int got = 0;
    for (int i = 0; i < 200; ++i)
        got += orbs.tick(w, player, true);
    CHECK(got == 25);
    CHECK(orbs.orbs().empty());
}

TEST_CASE("ores give experience: diamond 3-7, coal 0-2, stone none") {
    Xoroshiro rng(4);
    for (int i = 0; i < 20; ++i) {
        const int d = blockExperience(blockRegistry().defaultState(blocks::DiamondOre), rng);
        CHECK(d >= 3);
        CHECK(d <= 7);
        CHECK(blockExperience(blockRegistry().defaultState(blocks::CoalOre), rng) <= 2);
    }
    CHECK(blockExperience(blockRegistry().defaultState(blocks::Stone), rng) == 0);
}
