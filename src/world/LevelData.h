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
// Hotbar block states use vanilla's item component "minecraft:block_state".
struct LevelData {
    std::string name = "New World";
    uint64_t seed = 0;
    bool flat = false;                 // the superflat test world
    std::string generator = "overworld"; // non-flat: "overworld" (M8) or "terrain" (M3 placeholder)
    int64_t dayTime = 0;
    int64_t gameTime = 0;
    int32_t spawn[3] = {0, 64, 0}; // world spawn (fixed at creation; SpawnX/Y/Z)
    double pos[3] = {0, 0, 0};     // player feet
    float yaw = 0, pitch = 0;
    bool flying = false;
    std::array<std::string, 9> hotbar{}; // block state strings ("" = empty)
    int selectedSlot = 0;

    // Writes level.dat_new, then keeps the previous level.dat as level.dat_old and
    // moves the new file into place (replacing it in one rename).
    bool save(const std::filesystem::path& worldDir) const;
    // level.dat, falling back to level.dat_old and level.dat_new (a crash between the
    // save steps never loses the world's settings).
    static std::optional<LevelData> load(const std::filesystem::path& worldDir);
};

} // namespace mc::world
