#pragma once

#include "world/BlockRegistry.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc::world {

using ItemId = uint16_t; // dense, runtime-only (saves store "minecraft:<id>")
inline constexpr ItemId kNoItem = 0; // empty slot ("minecraft:air")

enum class ToolType : uint8_t { None, Pickaxe, Axe, Shovel, Hoe, Sword };

// Tool material tiers (wiki: Tiers): mining level, speed, durability, enchantability
// left out until enchanting exists.
enum class ToolTier : uint8_t { None, Wood, Stone, Iron, Diamond, Gold, Copper };
struct TierInfo {
    int level;      // harvest level: wood/gold 0, stone 1, iron 2, diamond 3
    float speed;    // mining speed multiplier
    int durability; // uses
};
const TierInfo& tierInfo(ToolTier tier);

struct ItemDef {
    std::string id;          // "minecraft:stick"
    uint8_t maxStack = 64;   // tools 1
    BlockId block = 0;       // block items: the block placed (0 = not a block item)
    ToolType tool = ToolType::None;
    ToolTier tier = ToolTier::None;
    int durability = 0;      // tools: uses before breaking
    float attackDamage = 1;  // hearts x2 (wiki: each tool's page)
    int food = 0;            // hunger points restored (wiki: Food)
    float saturation = 0;
    std::string texture;     // item sprite ("item/<name>"), empty for block items with a model
    // Armor (M17.3; wiki: Armor): where it is worn (0 none, 1 head, 2 chest, 3 legs,
    // 4 feet), its armor points and toughness.
    uint8_t armorSlot = 0;
    int armor = 0;
    float toughness = 0.0f;
};

// Every item: a block item for each placeable block (same id), then tools,
// materials and food (registered in Items.cpp from the wiki's item pages).
class ItemRegistry {
public:
    const ItemDef& item(ItemId id) const { return m_items[id]; }
    size_t count() const { return m_items.size(); }
    std::optional<ItemId> find(std::string_view id) const; // with or without "minecraft:"
    // The block item of a block (0 if it has none: air, fluids).
    ItemId blockItem(BlockId block) const { return block < m_blockItems.size() ? m_blockItems[block] : 0; }

    ItemId add(ItemDef def);
    void mapBlock(BlockId block, ItemId item);

private:
    std::vector<ItemDef> m_items;
    std::vector<ItemId> m_blockItems;
};

const ItemRegistry& itemRegistry();

// A stack in an inventory slot. `count` 0 = empty. `damage` counts tool uses.
// `state` keeps an exact block state for block items that carry one (vanilla's
// minecraft:block_state component, e.g. creative-picked log axes); 0 = default.
struct ItemStack {
    ItemId item = kNoItem;
    uint8_t count = 0;
    uint16_t damage = 0;
    BlockStateId state = 0;
    // Enchantments (M17.5, world/Enchantments.h): id << 8 | level, 0 = none; and the
    // anvil's prior-work penalty (vanilla minecraft:repair_cost).
    std::array<uint16_t, 4> enchantments{};
    uint8_t repairCost = 0;

    bool empty() const { return item == kNoItem || count == 0; }
    bool sameKind(const ItemStack& o) const {
        return item == o.item && state == o.state && damage == o.damage && enchantments == o.enchantments &&
               repairCost == o.repairCost;
    }
};

} // namespace mc::world
