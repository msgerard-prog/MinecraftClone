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
    std::string dimension = "minecraft:overworld"; // the player's dimension
    // Nether portals players lit or arrived through (vanilla keeps them in poi/ files;
    // ours: a list in our own level.dat tag), so travel links back to them.
    struct Portal {
        std::string dimension;
        int32_t x, y, z;
    };
    std::vector<Portal> portals;
    float yaw = 0, pitch = 0;
    bool flying = false;
    bool survival = false; // GameType / playerGameType 0 (survival) or 1 (creative)
    float health = 20.0f;
    int food = 20;
    float saturation = 5.0f, exhaustion = 0.0f;
    int foodTimer = 0; // regeneration / starvation clock (foodTickTimer)
    int air = 300;     // breath (Player.Air)
    int fire = -20;    // burning ticks left (Player.Fire; -20 = not burning)
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
