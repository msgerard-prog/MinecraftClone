#pragma once

#include "world/BlockRegistry.h"
#include "world/Items.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// Villagers (M24; wiki: Villager). A villager's profession comes from the job site
// block it claims; its type (outfit) from the biome it was born in; nitwits never
// take a job; "none" is an unemployed villager that will.
enum class Profession : uint8_t {
    None,
    Armorer,
    Butcher,
    Cartographer,
    Cleric,
    Farmer,
    Fisherman,
    Fletcher,
    Leatherworker,
    Librarian,
    Mason,
    Shepherd,
    Toolsmith,
    Weaponsmith,
    Nitwit,
    Count
};
struct ProfessionInfo {
    std::string_view id;      // "minecraft:farmer"
    std::string_view jobSite; // "composter" ("" for none/nitwit)
    uint32_t colour;          // our apron tint (0xRRGGBB)
};
const ProfessionInfo& professionInfo(Profession p);
std::optional<Profession> findProfession(std::string_view id); // with or without "minecraft:"
// The profession a job site block gives (None: not a job site).
Profession professionForJobSite(BlockId block);
bool isJobSite(BlockId block);

// Villager types by biome (wiki: Villager › Villager types).
enum class VillagerType : uint8_t { Plains, Desert, Savanna, Taiga, Snow, Jungle, Swamp, Count };
std::string_view villagerTypeId(VillagerType t); // "minecraft:plains"
std::optional<VillagerType> findVillagerType(std::string_view id);

// A trade (M24.2; wiki: Trading): what the villager wants (one or two stacks) and
// gives, how often it can be used before restocking, and its price state. Compact (it
// lives in MobData): items as ids and counts; the sold item may carry one
// enchantment (id << 8 | level) - enchanted books and gear.
struct TradeOffer {
    ItemId buyA = 0, buyB = 0, sell = 0;
    uint8_t buyACount = 0, buyBCount = 0, sellCount = 0;
    uint16_t sellEnchant = 0;
    uint8_t uses = 0, maxUses = 0;
    uint8_t xp = 0;         // villager experience per trade
    int8_t demand = 0;      // raises the price of popular trades after restocking
    float priceMultiplier = 0.05f;
    int16_t specialPrice = 0; // discounts (reputation, Hero of the Village)
    bool empty() const { return sell == 0; }
};
inline constexpr int kMaxOffers = 10;

// A point a villager remembers (home bed, job site, meeting bell); y = kNoPoint: none.
inline constexpr int kNoPoint = -1000000;

} // namespace mc::world
