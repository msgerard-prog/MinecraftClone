#include "world/Enchantments.h"

#include <vector>

namespace mc::world {

namespace {

// Cost ranges reproduce the wiki's "Enchanting mechanics" table, e.g. Sharpness I
// 1-11, II 12-22 ...: min = minBase + minPerLevel (L-1), max = min + maxSpan; the top
// level has its own (wider) maximum, e.g. Unbreaking III 21-71.
constexpr EnchantmentInfo kInfo[] = {
    {"", "", 0, 0, 0, 0, 0, 0, EnchantTarget::Durable, 0},
    {"minecraft:protection", "Protection", 4, 10, 5, 8, 7, 37, EnchantTarget::Armor, 1},
    {"minecraft:fire_protection", "Fire Protection", 4, 5, 10, 8, 7, 42, EnchantTarget::Armor, 1},
    {"minecraft:feather_falling", "Feather Falling", 4, 5, 5, 6, 5, 29, EnchantTarget::Feet, 0},
    {"minecraft:blast_protection", "Blast Protection", 4, 2, 5, 8, 7, 37, EnchantTarget::Armor, 1},
    {"minecraft:projectile_protection", "Projectile Protection", 4, 5, 3, 6, 5, 27, EnchantTarget::Armor, 1},
    {"minecraft:respiration", "Respiration", 3, 2, 10, 10, 9, 60, EnchantTarget::Head, 0},
    {"minecraft:aqua_affinity", "Aqua Affinity", 1, 2, 1, 0, 40, 41, EnchantTarget::Head, 0},
    {"minecraft:thorns", "Thorns", 3, 1, 10, 20, 19, 100, EnchantTarget::Armor, 0},
    {"minecraft:sharpness", "Sharpness", 5, 10, 1, 11, 10, 65, EnchantTarget::Sword, 2},
    {"minecraft:smite", "Smite", 5, 5, 5, 8, 7, 57, EnchantTarget::Sword, 2},
    {"minecraft:bane_of_arthropods", "Bane of Arthropods", 5, 5, 5, 8, 7, 57, EnchantTarget::Sword, 2},
    {"minecraft:knockback", "Knockback", 2, 5, 5, 20, 19, 75, EnchantTarget::Sword, 0},
    {"minecraft:fire_aspect", "Fire Aspect", 2, 2, 10, 10, 9, 80, EnchantTarget::Sword, 0},
    {"minecraft:looting", "Looting", 3, 2, 15, 9, 8, 83, EnchantTarget::Sword, 0},
    {"minecraft:efficiency", "Efficiency", 5, 10, 1, 10, 9, 91, EnchantTarget::Digger, 0},
    {"minecraft:silk_touch", "Silk Touch", 1, 1, 15, 0, 50, 65, EnchantTarget::Digger, 3},
    {"minecraft:unbreaking", "Unbreaking", 3, 5, 5, 8, 7, 71, EnchantTarget::Durable, 0},
    {"minecraft:fortune", "Fortune", 3, 2, 15, 9, 8, 83, EnchantTarget::Digger, 3},
    {"minecraft:power", "Power", 5, 10, 1, 10, 9, 56, EnchantTarget::Bow, 0},
    {"minecraft:punch", "Punch", 2, 2, 12, 20, 19, 57, EnchantTarget::Bow, 0},
    {"minecraft:flame", "Flame", 1, 2, 20, 0, 30, 50, EnchantTarget::Bow, 0},
    {"minecraft:infinity", "Infinity", 1, 1, 20, 0, 30, 50, EnchantTarget::Bow, 4},
};
static_assert(std::size(kInfo) == size_t(Enchantment::Count));

} // namespace

const EnchantmentInfo& enchantmentInfo(Enchantment e) { return kInfo[size_t(e)]; }

std::optional<Enchantment> findEnchantment(std::string_view id) {
    if (!id.starts_with("minecraft:")) {
        for (size_t i = 1; i < std::size(kInfo); ++i)
            if (kInfo[i].id.substr(10) == id) return Enchantment(i);
        return std::nullopt;
    }
    for (size_t i = 1; i < std::size(kInfo); ++i)
        if (kInfo[i].id == id) return Enchantment(i);
    return std::nullopt;
}

namespace {
// Item ids resolved once (no string compares on hot paths).
struct Ids {
    ItemId book, enchantedBook, bow;
    std::vector<int> enchantability; // per item
    Ids() {
        const auto& r = itemRegistry();
        book = *r.find("book");
        enchantedBook = *r.find("enchanted_book");
        bow = *r.find("bow");
        enchantability.resize(r.count());
        for (size_t i = 0; i < r.count(); ++i)
            enchantability[i] = computeEnchantability(ItemId(i));
    }
    static int computeEnchantability(ItemId item);
};
const Ids& ids() {
    static const Ids instance;
    return instance;
}
} // namespace

bool canEnchant(ItemId item, Enchantment e) {
    const ItemDef& d = itemRegistry().item(item);
    if (e == Enchantment::Thorns) return false; // (no effect yet: not offered or applied)
    if (item == ids().book || item == ids().enchantedBook) return true;
    switch (enchantmentInfo(e).target) {
    case EnchantTarget::Armor: return d.armorSlot != 0;
    case EnchantTarget::Head: return d.armorSlot == 1;
    case EnchantTarget::Feet: return d.armorSlot == 4;
    case EnchantTarget::Sword: return d.tool == ToolType::Sword;
    case EnchantTarget::Digger:
        return d.tool == ToolType::Pickaxe || d.tool == ToolType::Axe || d.tool == ToolType::Shovel ||
               d.tool == ToolType::Hoe;
    case EnchantTarget::Durable: return d.durability > 0;
    case EnchantTarget::Bow: return item == ids().bow;
    }
    return false;
}

int enchantability(ItemId item) { return item < ids().enchantability.size() ? ids().enchantability[item] : 0; }

int Ids::computeEnchantability(ItemId item) {
    // wiki: Enchanting mechanics › Enchantability.
    const ItemDef& d = itemRegistry().item(item);
    const std::string_view id = d.id;
    if (id == "minecraft:book" || id == "minecraft:bow") return 1;
    if (d.armorSlot) {
        if (id.find("leather") != std::string_view::npos) return 15;
        if (id.find("golden") != std::string_view::npos) return 25;
        if (id.find("iron") != std::string_view::npos) return 9;
        if (id.find("copper") != std::string_view::npos) return 8;
        if (id.find("diamond") != std::string_view::npos) return 10;
        if (id.find("netherite") != std::string_view::npos) return 15;
        return 0;
    }
    switch (d.tier) {
    case ToolTier::Wood: return 15;
    case ToolTier::Stone: return 5;
    case ToolTier::Iron: return 14;
    case ToolTier::Gold: return 22;
    case ToolTier::Diamond: return 10;
    case ToolTier::Copper: return 13;
    case ToolTier::Netherite: return 15;
    default: return 0;
    }
}

int enchantLevel(const ItemStack& s, Enchantment e) {
    for (const uint16_t v : s.enchantments)
        if (v >> 8 == uint16_t(e)) return v & 0xFF;
    return 0;
}

bool setEnchantment(ItemStack& s, Enchantment e, int level) {
    for (uint16_t& v : s.enchantments)
        if (v >> 8 == uint16_t(e) || v == 0) {
            v = uint16_t(uint16_t(e) << 8 | uint16_t(level));
            return true;
        }
    return false;
}

} // namespace mc::world
