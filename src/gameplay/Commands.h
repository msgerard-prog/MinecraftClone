#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/GameRules.h"
#include "world/Random.h"
#include "world/Weather.h"
#include "world/World.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mc {

// What commands can read and change. Owned by main.cpp.
struct CommandContext {
    Player& player;
    Inventory& inventory;
    int64_t& dayTime; // world day time (world/DayTime.h)
    int64_t gameTime; // ticks since the world started
    uint64_t seed;
    bool* survival = nullptr;      // game mode (null: /gamemode unavailable)
    Vitals* vitals = nullptr;      // /kill
    world::World* world = nullptr; // /summon
    world::Xoroshiro* rng = nullptr;
    std::vector<world::BlockPos>* changed = nullptr;   // /setblock: edited positions (relight)
    world::Weather* weather = nullptr;                 // /weather
    std::vector<world::BlockPos>* lightning = nullptr; // /summon lightning_bolt: where to strike
    world::GameRules* rules = nullptr;                 // /gamerule (M28.1)
    int* difficulty = nullptr;                         // /difficulty: 0 peaceful .. 3 hard
    int* gameMode = nullptr; // /gamemode adventure|spectator (0 survival .. 3 spectator)
    // (M29.7) where the command runs from: a command block's centre; ~ coordinates (and
    // /summon without any) are relative to it. Null: the player.
    const glm::dvec3* origin = nullptr;
};

struct CommandResult {
    bool ok = false;
    std::string message; // feedback line for chat (errors in red, as vanilla)
};

// Runs one chat command (with or without the leading '/'), vanilla syntax
// (wiki: Commands). Supported: /tp, /teleport, /time, /give, /gamemode, /kill, /setblock,
// /summon, /seed, /weather, /help.
// Selectors: only @s / @p (the player). Coordinates accept ~ (relative).
CommandResult runCommand(std::string_view line, CommandContext& ctx);

} // namespace mc
