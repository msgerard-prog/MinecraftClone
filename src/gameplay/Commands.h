#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace mc {

// What commands can read and change. Owned by main.cpp.
struct CommandContext {
    Player& player;
    Inventory& inventory;
    int64_t& dayTime;  // world day time (world/DayTime.h)
    int64_t gameTime;  // ticks since the world started
    uint64_t seed;
    bool* survival = nullptr; // game mode (null: /gamemode unavailable)
    Vitals* vitals = nullptr; // /kill
};

struct CommandResult {
    bool ok = false;
    std::string message; // feedback line for chat (errors in red, as vanilla)
};

// Runs one chat command (with or without the leading '/'), vanilla syntax
// (wiki: Commands). Supported: /tp, /teleport, /time, /give, /gamemode, /kill, /seed, /help.
// Selectors: only @s / @p (the player). Coordinates accept ~ (relative).
CommandResult runCommand(std::string_view line, CommandContext& ctx);

} // namespace mc
