// The smithing table (M23.6; wiki: Smithing Table).
#include "gameplay/Smithing.h"

#include "world/ArmorTrims.h"

#include <string>

namespace mc {

world::ItemStack smith(const world::ItemStack& templ, const world::ItemStack& base, const world::ItemStack& addition) {
    if (templ.empty() || base.empty() || addition.empty()) return {};
    const auto& items = world::itemRegistry();
    const std::string_view templId = items.item(templ.item).id;
    const std::string_view baseId = items.item(base.item).id;
    if (templId == "minecraft:netherite_upgrade_smithing_template") {
        if (items.item(addition.item).id != "minecraft:netherite_ingot") return {};
        if (!baseId.starts_with("minecraft:diamond_")) return {};
        const auto upgraded = items.find("netherite_" + std::string(baseId.substr(18)));
        if (!upgraded) return {};
        world::ItemStack out = base; // (damage, enchantments, repair cost, trim carry over)
        out.item = *upgraded;
        out.count = 1;
        return out;
    }
    const int pattern = world::trimPatternOf(templ.item);
    const int material = world::trimMaterialOf(addition.item);
    if (pattern == 0 || material == 0 || items.item(base.item).armorSlot == 0) return {};
    if (baseId == "minecraft:elytra") return {}; // (not trimmable)
    const uint16_t trim = static_cast<uint16_t>(pattern << 8 | material);
    if (base.trim == trim) return {};
    world::ItemStack out = base;
    out.trim = trim;
    out.count = 1;
    return out;
}

} // namespace mc
