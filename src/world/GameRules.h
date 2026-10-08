#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mc::world {

// Game rules (M28.1; wiki: Game rule) - the ones our game follows, by their 1.21.11 ids
// ("minecraft:keep_inventory"; the older camelCase names are accepted too). Saved in
// level.dat GameRules as strings, as vanilla does; set with /gamerule.
struct GameRules {
    bool advanceTime = true;       // advance_time (doDaylightCycle)
    bool advanceWeather = true;    // advance_weather (doWeatherCycle)
    bool spawnMobs = true;         // spawn_mobs (doMobSpawning)
    bool keepInventory = false;    // keep_inventory
    bool fallDamage = true;        // fall_damage
    bool fireDamage = true;        // fire_damage (fire, lava, burning)
    bool drowningDamage = true;    // drowning_damage
    bool freezeDamage = true;      // freeze_damage (no powder snow yet: kept for vanilla)
    bool naturalRegeneration = true; // natural_health_regeneration
    bool mobDrops = true;          // mob_drops (doMobLoot: items and experience)
    bool blockDrops = true;        // block_drops (doTileDrops)
    bool mobGriefing = true;       // mob_griefing (creepers', ghasts' and the Wither's blasts...)
    bool immediateRespawn = false; // immediate_respawn
    bool showDeathMessages = true; // show_death_messages
    bool tntExplodes = true;       // tnt_explodes
    bool spawnPhantoms = true;     // spawn_phantoms (doInsomnia)
    bool spawnPatrols = true;      // spawn_patrols
    bool spawnWanderingTraders = true; // spawn_wandering_traders
    bool spawnWardens = true;      // spawn_wardens
    int randomTickSpeed = 3;       // random_tick_speed

    // The rule's value as a string ("true", "3"), nullopt for a rule we don't have.
    std::optional<std::string> get(std::string_view id) const;
    // Sets a rule from a string ("true"/"false" or a number); false if the id or value is wrong.
    bool set(std::string_view id, std::string_view value);
    // Every rule's 1.21.11 id ("minecraft:advance_time"), in our order (count: kCount).
    static constexpr int kCount = 20;
    static std::string_view id(int i);
};

} // namespace mc::world
