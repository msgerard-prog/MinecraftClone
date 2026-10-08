#include "world/GameRules.h"

#include <charconv>

namespace mc::world {

namespace {

struct Rule {
    std::string_view id, legacy; // "minecraft:advance_time", "doDaylightCycle"
    bool GameRules::*flag;
    int GameRules::*number;
};

constexpr Rule kRules[] = {
    {"minecraft:advance_time", "doDaylightCycle", &GameRules::advanceTime, nullptr},
    {"minecraft:advance_weather", "doWeatherCycle", &GameRules::advanceWeather, nullptr},
    {"minecraft:spawn_mobs", "doMobSpawning", &GameRules::spawnMobs, nullptr},
    {"minecraft:keep_inventory", "keepInventory", &GameRules::keepInventory, nullptr},
    {"minecraft:fall_damage", "fallDamage", &GameRules::fallDamage, nullptr},
    {"minecraft:fire_damage", "fireDamage", &GameRules::fireDamage, nullptr},
    {"minecraft:drowning_damage", "drowningDamage", &GameRules::drowningDamage, nullptr},
    {"minecraft:freeze_damage", "freezeDamage", &GameRules::freezeDamage, nullptr},
    {"minecraft:natural_health_regeneration", "naturalRegeneration", &GameRules::naturalRegeneration, nullptr},
    {"minecraft:mob_drops", "doMobLoot", &GameRules::mobDrops, nullptr},
    {"minecraft:block_drops", "doTileDrops", &GameRules::blockDrops, nullptr},
    {"minecraft:mob_griefing", "mobGriefing", &GameRules::mobGriefing, nullptr},
    {"minecraft:immediate_respawn", "doImmediateRespawn", &GameRules::immediateRespawn, nullptr},
    {"minecraft:show_death_messages", "showDeathMessages", &GameRules::showDeathMessages, nullptr},
    {"minecraft:tnt_explodes", "tntExplodes", &GameRules::tntExplodes, nullptr},
    {"minecraft:spawn_phantoms", "doInsomnia", &GameRules::spawnPhantoms, nullptr},
    {"minecraft:spawn_patrols", "doPatrolSpawning", &GameRules::spawnPatrols, nullptr},
    {"minecraft:spawn_wandering_traders", "doTraderSpawning", &GameRules::spawnWanderingTraders, nullptr},
    {"minecraft:spawn_wardens", "doWardenSpawning", &GameRules::spawnWardens, nullptr},
    {"minecraft:show_advancement_messages", "announceAdvancements", &GameRules::announceAdvancements, nullptr},
    {"minecraft:random_tick_speed", "randomTickSpeed", nullptr, &GameRules::randomTickSpeed},
};
static_assert(std::size(kRules) == size_t(GameRules::kCount));

const Rule* find(std::string_view id) {
    for (const Rule& r : kRules)
        if (r.id == id || r.id.substr(10) == id || r.legacy == id) return &r;
    return nullptr;
}

} // namespace

std::optional<std::string> GameRules::get(std::string_view id) const {
    const Rule* r = find(id);
    if (!r) return std::nullopt;
    if (r->flag) return std::string(this->*(r->flag) ? "true" : "false");
    return std::to_string(this->*(r->number));
}

bool GameRules::set(std::string_view id, std::string_view value) {
    const Rule* r = find(id);
    if (!r) return false;
    if (r->flag) {
        if (value != "true" && value != "false") return false;
        this->*(r->flag) = value == "true";
        return true;
    }
    int v = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
    if (ec != std::errc() || ptr != value.data() + value.size() || v < 0) return false;
    this->*(r->number) = v;
    return true;
}

std::string_view GameRules::id(int i) { return kRules[size_t(i)].id; }

} // namespace mc::world
