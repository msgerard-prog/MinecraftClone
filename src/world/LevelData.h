#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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
    bool survival = false; // GameType / playerGameType 0 (survival) or 1 (creative)
    float health = 20.0f;
    int food = 20;
    float saturation = 5.0f, exhaustion = 0.0f;
    // Inventory slots 0..35 (0..8 hotbar). `id` is the item id; `state` the full block
    // state string for block items placed in a non-default state ("" otherwise).
    struct SavedItem {
        int slot = 0;
        std::string id;
        std::string state;
        int count = 1;
        int damage = 0;
    };
    std::vector<SavedItem> inventory;
    int selectedSlot = 0;

    // Writes level.dat_new, then keeps the previous level.dat as level.dat_old and
    // moves the new file into place (replacing it in one rename).
    bool save(const std::filesystem::path& worldDir) const;
    // level.dat, falling back to level.dat_old and level.dat_new (a crash between the
    // save steps never loses the world's settings).
    static std::optional<LevelData> load(const std::filesystem::path& worldDir);
};

} // namespace mc::world
