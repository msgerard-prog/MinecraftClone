#pragma once

#include "world/Mob.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mc::world {

// Advancements (M28.5c; wiki: Advancement). Vanilla's ids, titles, descriptions, tabs and
// frames; how each is earned is ours: an item in the inventory, a mob killed, a dimension
// entered or an event main reports (vanilla's triggers are richer - see game-design.md).
// Saved per player as vanilla's advancements/<uuid>.json.
enum class AdvTab : uint8_t { Story, Nether, End, Adventure, Husbandry, Count };
enum class AdvFrame : uint8_t { Task, Goal, Challenge };

// What earns an advancement besides items and kills.
enum class AdvEvent : uint8_t {
    None,
    EnterNether,
    EnterEnd,
    EnterGateway,
    KillDragon,
    RespawnDragon,
    Bred,
    Tamed,
    Traded,
    Enchanted,
    Slept,
    Fished,
    ShotArrow,
    ShotCrossbow,
    ThrewTrident,
    Ate,
    Planted,
    PlantedSnifferSeed,
    Waxed,
    UsedLodestone,
    CuredZombieVillager,
    SummonedIronGolem,
    SummonedWither,
    OpenedVault,
    OpenedOminousVault,
    Levitated,
    HeroOfTheVillage,
    MaceSmash50,
    TrimmedArmor,
    BrewedPotion,
    Count
};

struct Advancement {
    std::string_view id;    // "story/mine_stone" (saved as "minecraft:story/mine_stone")
    std::string_view title; // "Stone Age"
    std::string_view description;
    AdvTab tab;
    AdvFrame frame = AdvFrame::Task;
    // Earned by any of these items ("" = none), a kill of `kill` (Count: none; `anyKill`:
    // any mob) or `event`.
    std::array<std::string_view, 6> items{};
    MobType kill = MobType::Count;
    bool anyKill = false;
    AdvEvent event = AdvEvent::None;
};

std::span<const Advancement> advancements();
inline constexpr int kMaxAdvancements = 96;
std::optional<int> findAdvancement(std::string_view id); // with or without "minecraft:"

class Advancements {
public:
    bool done(int i) const { return m_done[size_t(i)]; }
    int doneCount() const;
    // Each returns the advancements this newly completed (up to 8, -1 terminated).
    using Granted = std::array<int, 8>;
    Granted onItem(std::string_view itemId);
    Granted onKill(MobType type);
    Granted onEvent(AdvEvent e);
    bool grant(int i); // true if it was new

    std::string toJson() const;
    bool fromJson(std::string_view json);
    static std::filesystem::path file(const std::filesystem::path& worldDir, uint64_t uuidHi, uint64_t uuidLo);
    bool save(const std::filesystem::path& path) const;
    static std::optional<Advancements> load(const std::filesystem::path& path);

private:
    template <typename F> Granted grantWhere(F&& match);
    std::array<bool, kMaxAdvancements> m_done{};
    std::array<std::string, kMaxAdvancements> m_when{}; // (when it was made: vanilla's criterion timestamp)
};

} // namespace mc::world
