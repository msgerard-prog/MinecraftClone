// Game rules (M28.1; wiki: Game rule): ids, values, level.dat, /gamerule, /difficulty.
#include "gameplay/Commands.h"
#include "gameplay/Vitals.h"
#include "world/GameRules.h"
#include "world/LevelData.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc;
using namespace mc::world;

TEST_CASE("game rules: 1.21.11 ids, short ids and the old camelCase names") {
    GameRules r;
    CHECK(*r.get("minecraft:keep_inventory") == "false");
    CHECK(r.set("keep_inventory", "true"));
    CHECK(r.keepInventory);
    CHECK(r.set("doDaylightCycle", "false")); // (pre-1.21.11 name)
    CHECK_FALSE(r.advanceTime);
    CHECK(r.set("minecraft:random_tick_speed", "10"));
    CHECK(r.randomTickSpeed == 10);
    CHECK_FALSE(r.set("random_tick_speed", "fast"));
    CHECK_FALSE(r.set("keep_inventory", "1"));
    CHECK_FALSE(r.get("no_such_rule"));
    for (int i = 0; i < GameRules::kCount; ++i)
        CHECK(r.get(GameRules::id(i)));
}

TEST_CASE("game rules, difficulty and game mode are saved in level.dat") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_gamerules_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    LevelData l;
    l.rules.keepInventory = true;
    l.rules.mobGriefing = false;
    l.rules.randomTickSpeed = 7;
    l.difficulty = 3;
    l.gameMode = 2;
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    CHECK(back->rules.keepInventory);
    CHECK_FALSE(back->rules.mobGriefing);
    CHECK(back->rules.randomTickSpeed == 7);
    CHECK(back->rules.advanceTime);
    CHECK(back->difficulty == 3);
    CHECK(back->gameMode == 2);
    CHECK(back->survival); // (adventure plays as survival)
    std::filesystem::remove_all(dir);
}

TEST_CASE("/gamerule queries and sets; /difficulty") {
    Player player;
    Inventory inv;
    int64_t dayTime = 0;
    GameRules rules;
    int difficulty = 2;
    CommandContext ctx{player, inv, dayTime, 0, 42};
    ctx.rules = &rules;
    ctx.difficulty = &difficulty;
    CHECK(runCommand("/gamerule keep_inventory", ctx).message == "Gamerule keep_inventory is currently set to: false");
    CHECK(runCommand("/gamerule keep_inventory true", ctx).ok);
    CHECK(rules.keepInventory);
    CHECK_FALSE(runCommand("/gamerule keep_inventory yes", ctx).ok);
    CHECK_FALSE(runCommand("/gamerule nope true", ctx).ok);
    CHECK(runCommand("/difficulty", ctx).message == "The difficulty is Normal");
    CHECK(runCommand("/difficulty hard", ctx).ok);
    CHECK(difficulty == 3);
    CHECK_FALSE(runCommand("/difficulty hard", ctx).ok); // (already)
    CHECK_FALSE(runCommand("/difficulty extreme", ctx).ok);
}

TEST_CASE("game rules: fall_damage and drowning_damage off spare the player") {
    Vitals v;
    v.setRules(false, true, false, true);
    v.tick(100.0, true, false, false);
    float hurt = 0.0f;
    for (double y = 100.0; y > 80.0; y -= 0.5)
        hurt += v.tick(y, false, false, false);
    hurt += v.tick(80.0, true, false, false);
    CHECK(hurt == 0.0f);
    for (int i = 0; i < 400; ++i) {
        v.tick(64.0, true, true, false);
        hurt += v.breathe(true);
    }
    CHECK(hurt == 0.0f);
    CHECK(v.health() == 20.0f);
}

TEST_CASE("difficulty: mob damage Easy min(d/2+1, d), Hard x1.5; lava doesn't scale") {
    CHECK(Vitals::scaledDamage(6.0f, 1) == 4.0f);
    CHECK(Vitals::scaledDamage(1.0f, 1) == 1.0f);
    CHECK(Vitals::scaledDamage(6.0f, 2) == 6.0f);
    CHECK(Vitals::scaledDamage(6.0f, 3) == 9.0f);
    CHECK(Vitals::scaledDamage(6.0f, 0) == 0.0f);
    Vitals hard;
    hard.setDifficulty(3);
    const glm::dvec3 from(1.0, 0.0, 0.0);
    hard.attacked(4.0f, &from);
    CHECK(hard.health() == 14.0f);
    Vitals lava;
    lava.setDifficulty(3);
    lava.attacked(4.0f, nullptr, Vitals::Hit::Fire);
    CHECK(lava.health() == 16.0f);
}

TEST_CASE("difficulty: starving stops at 10 on Easy and 1 on Normal; Hard starves to death") {
    for (int d = 1; d <= 3; ++d) {
        Vitals v;
        v.setDifficulty(d);
        v.setState(20.0f, 0, 0.0f, 0.0f);
        for (int t = 0; t < 80 * 25; ++t)
            v.tick(64.0, true, false, false);
        CHECK(v.health() == (d == 1 ? 10.0f : d == 2 ? 1.0f : 0.0f));
    }
}

TEST_CASE("Peaceful: health and food come back, food never drops") {
    Vitals v;
    v.setDifficulty(0);
    v.setState(10.0f, 4, 0.0f, 0.0f);
    for (int t = 0; t < 200; ++t)
        v.tick(64.0, true, false, false);
    CHECK(v.health() == 20.0f);
    CHECK(v.food() == 20);
    v.exhaust(40.0f);
    v.tick(64.0, true, false, false);
    CHECK(v.food() == 20);
}

TEST_CASE("keep_inventory: experience survives the respawn") {
    Vitals v;
    v.addExperience(100);
    const int level = v.xpLevel();
    REQUIRE(level > 0);
    v.reset(true);
    CHECK(v.xpLevel() == level);
    v.reset();
    CHECK(v.xpLevel() == 0);
}
