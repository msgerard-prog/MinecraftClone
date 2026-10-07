#pragma once

#include "world/Items.h"

#include <optional>
#include <string_view>

namespace mc::world {

// Enchantments (M17.5; wiki: Enchanting, Enchanting mechanics). Stored on items as
// id << 8 | level in ItemStack::enchantments (up to 4).
enum class Enchantment : uint8_t {
    None,
    Protection,
    FireProtection,
    FeatherFalling,
    BlastProtection,
    ProjectileProtection,
    Respiration,
    AquaAffinity,
    Thorns,
    Sharpness,
    Smite,
    BaneOfArthropods,
    Knockback,
    FireAspect,
    Looting,
    Efficiency,
    SilkTouch,
    Unbreaking,
    Fortune,
    Power,
    Punch,
    Flame,
    Infinity,
    Count
};

// What an enchantment fits on.
enum class EnchantTarget : uint8_t { Armor, Head, Feet, Sword, Digger, Durable, Bow };

struct EnchantmentInfo {
    std::string_view id; // "minecraft:sharpness"
    std::string_view name; // "Sharpness"
    int maxLevel;
    int weight;        // chance weight when the table picks one (wiki)
    int minBase, minPerLevel; // cost range of level L: [minBase + minPerLevel (L-1), + span]
    int maxSpan;       // ... upper bound = lower bound + maxSpan (wiki's table)
    EnchantTarget target;
    uint8_t group;     // exclusive group (0 = none): protections 1, damage 2, fortune/silk 3, infinity 4
};
const EnchantmentInfo& enchantmentInfo(Enchantment e);
std::optional<Enchantment> findEnchantment(std::string_view id); // with or without "minecraft:"

// Whether `e` can go on `item` (books take all).
bool canEnchant(ItemId item, Enchantment e);
// The item's enchantability (wiki: material table); 0 = not enchantable.
int enchantability(ItemId item);

int enchantLevel(const ItemStack& s, Enchantment e);
// Adds or raises; false if the stack has no room (4 kinds max).
bool setEnchantment(ItemStack& s, Enchantment e, int level);
inline bool isEnchanted(const ItemStack& s) { return s.enchantments[0] != 0; }

} // namespace mc::world
