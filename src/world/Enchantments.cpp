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
    // (M25.2; wiki: Luck of the Sea, Lure - weight 2, levels 1-3, cost 15 + 9 (L-1) .. +50)
    {"minecraft:luck_of_the_sea", "Luck of the Sea", 3, 2, 15, 9, 50, 65, EnchantTarget::FishingRod, 0},
    {"minecraft:lure", "Lure", 3, 2, 15, 9, 50, 65, EnchantTarget::FishingRod, 0},
    // (M25.3; wiki: Loyalty 5, 12 + 7 (L-1); Riptide 2, 17 + 7 (L-1); Impaling 2, 1 + 8 (L-1),
    // span 20; Channeling 1, 25 - all with a span of 50 unless noted)
    {"minecraft:loyalty", "Loyalty", 3, 5, 12, 7, 38, 50, EnchantTarget::Trident, 0},
    {"minecraft:riptide", "Riptide", 3, 2, 17, 7, 33, 50, EnchantTarget::Trident, 0},
    {"minecraft:impaling", "Impaling", 5, 2, 1, 8, 20, 53, EnchantTarget::Trident, 0},
    {"minecraft:channeling", "Channeling", 1, 1, 25, 0, 25, 50, EnchantTarget::Trident, 0},
    // (M27.3; wiki: Swift Sneak - III, a treasure: weight 0, never from the table)
    {"minecraft:swift_sneak", "Swift Sneak", 3, 0, 25, 25, 50, 50, EnchantTarget::Legs, 0},
    // (M28.4a; wiki: Quick Charge III weight 5, Multishot I weight 2, Piercing IV weight 10;
    // costs up to 50)
    {"minecraft:quick_charge", "Quick Charge", 3, 5, 12, 20, 38, 50, EnchantTarget::Crossbow, 0},
    {"minecraft:multishot", "Multishot", 1, 2, 20, 0, 30, 50, EnchantTarget::Crossbow, 0},
    {"minecraft:piercing", "Piercing", 4, 10, 1, 10, 49, 50, EnchantTarget::Crossbow, 0},
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
    ItemId book, enchantedBook, bow, rod, trident;
    std::vector<int> enchantability; // per item
    Ids() {
        const auto& r = itemRegistry();
        book = *r.find("book");
        enchantedBook = *r.find("enchanted_book");
        bow = *r.find("bow");
        rod = *r.find("fishing_rod");
        trident = *r.find("trident");
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
    case EnchantTarget::Legs: return d.armorSlot == 3;
    case EnchantTarget::Sword: return d.tool == ToolType::Sword;
    case EnchantTarget::Digger:
        return d.tool == ToolType::Pickaxe || d.tool == ToolType::Axe || d.tool == ToolType::Shovel ||
               d.tool == ToolType::Hoe;
    case EnchantTarget::Durable: return d.durability > 0;
    case EnchantTarget::Bow: return item == ids().bow;
    case EnchantTarget::FishingRod: return item == ids().rod;
    case EnchantTarget::Trident: return item == ids().trident;
    case EnchantTarget::Crossbow: return d.id == "minecraft:crossbow";
    }
    return false;
}

bool conflicts(Enchantment a, Enchantment b) {
    if (a == b) return false;
    const uint8_t g = enchantmentInfo(a).group;
    if (g && enchantmentInfo(b).group == g) return true;
    auto pair = [&](Enchantment x, Enchantment y) { return (a == x && b == y) || (a == y && b == x); };
    return pair(Enchantment::Riptide, Enchantment::Loyalty) || pair(Enchantment::Riptide, Enchantment::Channeling) ||
           pair(Enchantment::Multishot, Enchantment::Piercing); // (M28.4a)
}

int enchantability(ItemId item) { return item < ids().enchantability.size() ? ids().enchantability[item] : 0; }

int Ids::computeEnchantability(ItemId item) {
    // wiki: Enchanting mechanics › Enchantability.
    const ItemDef& d = itemRegistry().item(item);
    const std::string_view id = d.id;
    if (id == "minecraft:book" || id == "minecraft:bow" || id == "minecraft:fishing_rod" || id == "minecraft:trident")
        return 1;
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
