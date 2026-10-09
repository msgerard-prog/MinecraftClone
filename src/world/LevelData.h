#pragma once

#include "world/GameRules.h"

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
// Our save-format generation, kept in level.dat's MinecraftClone compound ("format")
// and on every chunk we write ("clone_format"). 0 = worlds from before v0.17.1, whose
// chunks may hold placed leaves saved as `distance=7, persistent=false` (placing
// didn't set persistent before v0.15.0): ChunkStorage upgrades those when a chunk
// without the chunk tag is loaded (see chunkFromNbt).
inline constexpr int32_t kCloneFormat = 1;

struct LevelData {
    std::string name = "New World";
    uint64_t seed = 0;
    bool flat = false;                 // the superflat test world
    // Non-flat: "overworld2" (M18, new worlds), "overworld" (M8) or "terrain" (M3
    // placeholder); level.dat files without one are M8 worlds.
    std::string generator = "overworld";
    // The Nether's generator: "nether2" (M19, new worlds) or "nether" (M12; level.dat
    // files without one).
    std::string netherGenerator = "nether";
    std::string endGenerator = "end"; // (missing in worlds before M20: the M12 End)
    // The format the world was created with (kept across saves; see kCloneFormat).
    // Vanilla worlds (no MinecraftClone compound) count as current: nothing to upgrade.
    int32_t cloneFormat = kCloneFormat;
    int64_t dayTime = 0;
    int64_t lastPlayed = 0; // LastPlayed (read only: saving writes the current time)
    // Weather (M22.1; vanilla raining, rainTime, thundering, thunderTime, clearWeatherTime).
    bool raining = false, thundering = false;
    int32_t rainTime = 0, thunderTime = 0, clearWeatherTime = 0;
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
    float absorption = 0.0f; // (M29.2a) AbsorptionAmount
    int food = 20;
    float saturation = 5.0f, exhaustion = 0.0f;
    int foodTimer = 0; // regeneration / starvation clock (foodTickTimer)
    int air = 300;     // breath (Player.Air)
    int fire = -20;    // burning ticks left (Player.Fire; -20 = not burning)
    int xpLevel = 0;       // XpLevel, XpP (bar 0..1), XpTotal
    float xpProgress = 0.0f;
    int xpTotal = 0;
    int32_t xpSeed = 0;    // XpSeed: the enchanting table's seed
    GameRules rules;      // (M28.1) level.dat GameRules
    int difficulty = 2;   // (M28.1) Difficulty: 0 peaceful, 1 easy, 2 normal, 3 hard
    int gameMode = 0;     // (M28.1) GameType: 0 survival, 1 creative, 2 adventure, 3 spectator
    int32_t timeSinceRest = 0; // (M26.4a) ticks awake (our tag TimeSinceRest; vanilla keeps it in stats/)
    int32_t wardenLevel = 0, wardenTicks = 0, wardenCooldown = 0; // (M27.3) vanilla warden_spawn_tracker
    // Status effects (M19.4): Player.active_effects [{id, amplifier, duration, ...}].
    struct SavedEffect {
        std::string id; // "minecraft:speed"
        int amplifier = 0, duration = 0;
    };
    std::vector<SavedEffect> effects;
    // The dragon fight (M20.2; vanilla Data.DragonFight): killed now, ever killed (the
    // egg and the 12,000 experience come only the first time), the living dragon's
    // UUID (0: none spawned), the gateways spawned so far (M20.3, angles 0..19).
    bool dragonKilled = false, dragonPreviouslyKilled = false;
    int traderSpawnDelay = 24000, traderSpawnChance = 25; // (M24.4) WanderingTraderSpawnDelay/Chance
    uint64_t playerUuidHi = 0, playerUuidLo = 0; // (M26.1) Player.UUID: pets' Owner (0: none yet)
    // The village raid in progress (M24.5; our tag "Raid" - vanilla keeps raids in
    // data/raids.dat): bell position, waves spawned/total, omen level, ticks running,
    // ticks to the next wave, the wave's starting health (the bar).
    bool raidActive = false;
    int32_t raidCentre[3] = {0, 0, 0};
    int raidWave = 0, raidWaves = 0, raidLevel = 1, raidTicks = 0, raidCooldown = 0;
    float raidWaveHealth = 0.0f;
    int32_t raidId = 0, raidNextId = 1;
    int raidPendingTicks = 0, raidPendingLevel = 1; // Raid Omen running (the raid starts at raidPendingCentre)
    int32_t raidPendingCentre[3] = {0, 0, 0};
    uint64_t dragonUuidHi = 0, dragonUuidLo = 0;
    std::vector<int32_t> gateways;
    bool hasGateways = false; // (the Gateways list exists: the fight has started)
    bool hasRespawn = false; // a bed's respawn point (Player.respawn, 1.21.5+; Overworld)
    int32_t respawn[3] = {0, 0, 0};
    // Where the player last died (Player.LastDeathLocation {dimension, pos}; recovery
    // compasses point there - M28.2a).
    bool hasLastDeath = false;
    int32_t lastDeath[3] = {0, 0, 0};
    int lastDeathDimension = 0; // world::Dimension
    // Inventory slots 0..35 (0..8 hotbar), the ender chest 200..226 (vanilla EnderItems,
    // M23.6), worn armor 100 (feet)..103 (head) and the
    // offhand 150 - saved as 1.21.5+'s `equipment` compound. `id` is the item id; `state` the full block
    // state string for block items placed in a non-default state ("" otherwise).
    struct SavedItem {
        int slot = 0;
        std::string id;
        std::string state;
        int count = 1;
        int damage = 0;
        std::vector<std::pair<std::string, int>> enchantments; // ("minecraft:sharpness", 5)
        int repairCost = 0;
        std::string potion; // potion id without "minecraft:" ("" = none)
        bool storedEnchantments = false; // an enchanted book's (minecraft:stored_enchantments)
        uint32_t contents = 0; // a shulker box's slots (world/ItemContainers.h, M23.6)
        uint16_t trim = 0;     // an armor trim (world/ArmorTrims.h, M23.6)
        uint32_t extra = 0;    // a lodestone target or book pages (world/ItemExtras.h, M28.2)
        uint16_t itemState = 0; // a non-block item's state: a map's post-processing, a loaded crossbow (M28.4a)
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
