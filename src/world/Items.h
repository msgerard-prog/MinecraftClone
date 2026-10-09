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

enum class ToolType : uint8_t { None, Pickaxe, Axe, Shovel, Hoe, Sword, Spear };

// Tool material tiers (wiki: Tiers): mining level, speed, durability, enchantability
// left out until enchanting exists.
enum class ToolTier : uint8_t { None, Wood, Stone, Iron, Diamond, Gold, Copper, Netherite };
struct TierInfo {
    int level;      // harvest level: wood/gold 0, stone 1, iron 2, diamond 3
    float speed;    // mining speed multiplier
    int durability; // uses
};
const TierInfo& tierInfo(ToolTier tier);

// Pottery sherds (M27.5; wiki: Pottery Sherd), vanilla's 23.
inline constexpr const char* kSherds[23] = {"angler", "archer", "arms_up", "blade", "brewer", "burn", "danger", "explorer",
                                            "flow", "friend", "guster", "heart", "heartbreak", "howl", "miner", "mourner",
                                            "plenty", "prize", "scrape", "sheaf", "shelter", "skull", "snort"};

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
    bool alwaysEdible = false; // eaten even when not hungry (chorus fruit, golden apples)
    std::string texture;     // item sprite ("item/<name>"), empty for block items with a model
    // Netherite things and ancient debris don't burn: dropped, they float on lava
    // (wiki: Netherite › Properties). Set from the name.
    bool fireResistant = false;
    // Armor (M17.3; wiki: Armor): where it is worn (0 none, 1 head, 2 chest, 3 legs,
    // 4 feet), its armor points and toughness.
    uint8_t armorSlot = 0;
    int armor = 0;
    float toughness = 0.0f;
    // (M29.1e; wiki: Spawn Egg) the mob a spawn egg makes: its MobType + 1 (0: not an egg).
    uint8_t spawnEgg = 0;
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
    // Enchantments (M17.5, world/Enchantments.h): id << 8 | level, 0 = none (8 kinds
    // at most; kinds we don't have yet are dropped on load - see game-design); and the
    // anvil's prior-work penalty (vanilla minecraft:repair_cost).
    std::array<uint16_t, 8> enchantments{};
    uint8_t repairCost = 0;
    // Potion items (M19.4): the potion inside (world/Potions.h; 0 = none), vanilla's
    // minecraft:potion_contents.
    uint8_t potion = 0;
    // Items carried inside (M23.6, vanilla minecraft:container: shulker boxes): an id in
    // world/ItemContainers.h, 0 = none.
    uint32_t contents = 0;
    // An armor trim (M23.6, world/ArmorTrims.h): pattern << 8 | material, 0 = none.
    uint16_t trim = 0;
    // Data in world/ItemExtras.h (M28.2): a lodestone compass's target, a book's pages;
    // 0 = none.
    uint32_t extra = 0;
    // (M29.3b) a custom name (world/ItemExtras.h nameText; vanilla minecraft:custom_name);
    // 0 = none.
    uint32_t name = 0;

    bool empty() const { return item == kNoItem || count == 0; }
    bool sameKind(const ItemStack& o) const {
        return item == o.item && state == o.state && damage == o.damage && enchantments == o.enchantments &&
               repairCost == o.repairCost && potion == o.potion && contents == o.contents &&
               trim == o.trim && extra == o.extra && name == o.name;
    }
};

} // namespace mc::world
