#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace mc::world {

// level.dat (wiki: Java Edition level format): gzip-compressed NBT, root { Data {...} }.
// We write vanilla's key fields (DataVersion, LevelName, DayTime, Time, GameType,
// WorldGenSettings.seed, Player {Pos, Rotation, abilities, Inventory}) plus our own
// "MinecraftClone" compound for settings vanilla stores differently (generator kind).
struct LevelData {
    std::string name = "New World";
    uint64_t seed = 0;
    bool flat = false; // our generators: "terrain" (placeholder until M8) or "flat"
    int64_t dayTime = 0;
    int64_t gameTime = 0;
    double pos[3] = {0, 0, 0}; // player feet
    float yaw = 0, pitch = 0;
    bool flying = false;
    std::array<std::string, 9> hotbar{}; // block state strings ("" = empty)
    int selectedSlot = 0;

    bool save(const std::filesystem::path& worldDir) const;
    static std::optional<LevelData> load(const std::filesystem::path& worldDir);
};

} // namespace mc::world
