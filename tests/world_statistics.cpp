// Statistics (M28.1d; wiki: Statistics): counters, vanilla's JSON file, formatting.
#include "world/Blocks.h"
#include "world/Statistics.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc::world;

TEST_CASE("statistics: vanilla ids, JSON round trip, unknown ids skipped") {
    Statistics s;
    s.add(Stat::Jump, 12);
    s.add(Stat::WalkOneCm, 12345);
    s.addMined(blocks::Stone, 30);
    const ItemId stick = *itemRegistry().find("stick");
    s.addItem(ItemStat::Crafted, stick, 4);
    s.addItem(ItemStat::PickedUp, stick, 2);
    s.addKilled(MobType::Zombie);
    s.addKilledBy(MobType::Creeper);
    const std::string json = s.toJson();
    CHECK(json.find("\"minecraft:custom\"") != std::string::npos);
    CHECK(json.find("\"minecraft:jump\": 12") != std::string::npos);
    CHECK(json.find("\"minecraft:stone\": 30") != std::string::npos);
    CHECK(json.find("\"minecraft:killed_by\"") != std::string::npos);
    CHECK(json.find("\"DataVersion\": 4671") != std::string::npos);
    CHECK(json.find("\"minecraft:broken\"") == std::string::npos); // (empty groups left out)

    Statistics back;
    REQUIRE(back.fromJson(json));
    CHECK(back.get(Stat::Jump) == 12);
    CHECK(back.get(Stat::WalkOneCm) == 12345);
    CHECK(back.mined(blocks::Stone) == 30);
    CHECK(back.item(ItemStat::Crafted, stick) == 4);
    CHECK(back.item(ItemStat::PickedUp, stick) == 2);
    CHECK(back.killed(MobType::Zombie) == 1);
    CHECK(back.killedBy(MobType::Creeper) == 1);

    Statistics other;
    CHECK(other.fromJson(R"({"stats": {"minecraft:custom": {"minecraft:jump": 3, "minecraft:not_ours": 9},
        "minecraft:custom_thing": {"x": 1}}, "DataVersion": 4671})"));
    CHECK(other.get(Stat::Jump) == 3);
    CHECK_FALSE(other.fromJson("{\"stats\": {"));
}

TEST_CASE("statistics: saved as stats/<uuid>.json") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_stats_test";
    std::filesystem::remove_all(dir);
    const auto path = Statistics::file(dir, 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
    CHECK(path.filename().string() == "01234567-89ab-cdef-fedc-ba9876543210.json");
    Statistics s;
    s.add(Stat::Deaths, 2);
    REQUIRE(s.save(path));
    const auto back = Statistics::load(path);
    REQUIRE(back);
    CHECK(back->get(Stat::Deaths) == 2);
    std::filesystem::remove_all(dir);
}

TEST_CASE("statistics: values shown as vanilla formats them") {
    char out[32];
    Statistics::formatValue(Stat::PlayTime, 20 * 60 * 90, out, sizeof(out));
    CHECK(std::string(out) == "1.50 h");
    Statistics::formatValue(Stat::PlayTime, 200, out, sizeof(out));
    CHECK(std::string(out) == "10.00 s");
    Statistics::formatValue(Stat::WalkOneCm, 123456, out, sizeof(out));
    CHECK(std::string(out) == "1.23 km");
    Statistics::formatValue(Stat::WalkOneCm, 40, out, sizeof(out));
    CHECK(std::string(out) == "40 cm");
    Statistics::formatValue(Stat::DamageTaken, 35, out, sizeof(out));
    CHECK(std::string(out) == "3.5");
    Statistics::formatValue(Stat::Jump, 7, out, sizeof(out));
    CHECK(std::string(out) == "7");
}
