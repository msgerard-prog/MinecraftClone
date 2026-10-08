#pragma once

#include "world/Items.h"
#include "world/Mob.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc::world {

// The player's statistics (M28.1d; wiki: Statistics). Vanilla keeps them per player in
// saves/<world>/stats/<uuid>.json as {"stats": {"minecraft:custom": {"minecraft:jump":
// 12, ...}, "minecraft:mined": {"minecraft:stone": 30}, ...}, "DataVersion": 4671}.
// Counters are plain arrays sized once from the registries: counting is an index, never
// an allocation (hard rule 1).
enum class Stat : uint8_t {
    // Times (ticks).
    PlayTime,
    TotalWorldTime,
    TimeSinceDeath,
    TimeSinceRest,
    // Counts.
    LeaveGame,
    Deaths,
    MobKills,
    DamageDealt, // tenths of a health point, as vanilla
    DamageTaken,
    Jump,
    Drop,
    SleepInBed,
    AnimalsBred,
    FishCaught,
    TradedWithVillager,
    EnchantItem,
    OpenChest,
    InteractWithCraftingTable,
    InteractWithFurnace,
    // Distances (centimetres).
    WalkOneCm,
    SprintOneCm,
    CrouchOneCm,
    SwimOneCm,
    WalkOnWaterOneCm,
    WalkUnderWaterOneCm,
    FallOneCm,
    ClimbOneCm,
    FlyOneCm,
    AviateOneCm,
    BoatOneCm,
    MinecartOneCm,
    HorseOneCm,
    Count
};

// Per item (or per block for Mined) counters, vanilla's stat types.
enum class ItemStat : uint8_t { Mined, Crafted, Used, Broken, PickedUp, Dropped, Count };

class Statistics {
public:
    Statistics(); // sized from the block, item and mob registries

    void add(Stat s, int64_t n = 1) { m_custom[size_t(s)] += n; }
    void set(Stat s, int64_t v) { m_custom[size_t(s)] = v; }
    int64_t get(Stat s) const { return m_custom[size_t(s)]; }
    void addMined(BlockId b, int n = 1) {
        if (b < m_mined.size()) m_mined[b] += n;
    }
    void addItem(ItemStat k, ItemId item, int n = 1) {
        const size_t i = size_t(k) * m_items + item;
        if (k != ItemStat::Mined && item < m_items) m_perItem[i] += n;
    }
    int64_t mined(BlockId b) const { return b < m_mined.size() ? m_mined[b] : 0; }
    int64_t item(ItemStat k, ItemId item) const {
        return item < m_items ? m_perItem[size_t(k) * m_items + item] : 0;
    }
    void addKilled(MobType t) { ++m_killed[size_t(t)]; }
    void addKilledBy(MobType t) { ++m_killedBy[size_t(t)]; }
    int64_t killed(MobType t) const { return m_killed[size_t(t)]; }
    int64_t killedBy(MobType t) const { return m_killedBy[size_t(t)]; }

    // "minecraft:play_time"; the screen's label ("Time Played"); how to show the value.
    static std::string_view id(Stat s);
    static std::string_view label(Stat s);
    enum class Format : uint8_t { Count, Time, Distance, Damage };
    static Format format(Stat s);
    // A value as the Statistics screen shows it ("1.5 h", "12.3 km", "3.5 hearts"...).
    static void formatValue(Stat s, int64_t v, char* out, size_t size);

    std::string toJson() const;
    bool fromJson(std::string_view json); // unknown ids are skipped
    // saves/<world>/stats/<uuid>.json (uuid hyphenated, as vanilla).
    static std::filesystem::path file(const std::filesystem::path& worldDir, uint64_t uuidHi, uint64_t uuidLo);
    bool save(const std::filesystem::path& path) const;
    static std::optional<Statistics> load(const std::filesystem::path& path);

private:
    std::array<int64_t, size_t(Stat::Count)> m_custom{};
    std::vector<int64_t> m_mined;   // per block
    size_t m_items = 0;             // item count
    std::vector<int64_t> m_perItem; // ItemStat x item
    std::array<int64_t, size_t(MobType::Count)> m_killed{}, m_killedBy{};
};

} // namespace mc::world
