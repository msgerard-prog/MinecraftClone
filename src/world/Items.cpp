#include "world/Items.h"

#include "world/Blocks.h"

namespace mc::world {

const TierInfo& tierInfo(ToolTier tier) {
    // wiki: Tiers (Java) - level, mining speed, durability.
    static constexpr TierInfo kTiers[] = {
        {-1, 1.0f, 0},   // None (hand)
        {0, 2.0f, 59},   // Wood
        {1, 4.0f, 131},  // Stone
        {2, 6.0f, 250},  // Iron
        {3, 8.0f, 1561}, // Diamond
        {0, 12.0f, 32},  // Gold
        {1, 5.0f, 190},  // Copper (1.21.9; wiki: Copper Pickaxe - stone's level, faster)
    };
    return kTiers[static_cast<int>(tier)];
}

std::optional<ItemId> ItemRegistry::find(std::string_view id) const {
    const bool bare = id.find(':') == std::string_view::npos;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const std::string_view name = m_items[i].id;
        if (name == id || (bare && name.size() > 10 && name.substr(10) == id)) return static_cast<ItemId>(i);
    }
    return std::nullopt;
}

ItemId ItemRegistry::add(ItemDef def) {
    m_items.push_back(std::move(def));
    return static_cast<ItemId>(m_items.size() - 1);
}

void ItemRegistry::mapBlock(BlockId block, ItemId item) {
    if (m_blockItems.size() <= block) m_blockItems.resize(size_t(block) + 1, 0);
    m_blockItems[block] = item;
}

namespace {

ItemRegistry buildItems() {
    ItemRegistry r;
    r.add({.id = "minecraft:air", .maxStack = 0}); // kNoItem
    // Block items: same id as the block (wiki: Item - "block items").
    const auto& blocks = blockRegistry();
    for (BlockId b = 1; b < blocks.blockCount(); ++b) {
        if (b == blocks::Water || b == blocks::Lava) continue; // buckets, not items
        // Placed by another item (redstone dust, torches on walls) or never an item.
        if (b == blocks::RedstoneWire || b == blocks::RedstoneWallTorch || b == blocks::PistonHead ||
            b == blocks::NetherPortal || b == blocks::EndPortal)
            continue;
        const std::string& id = blocks.block(b).id;
        r.mapBlock(b, r.add({.id = id, .block = b}));
    }
    // Tools (wiki: Pickaxe, Axe, Shovel, Hoe, Sword - attack damage per tier).
    struct ToolKind {
        const char* name;
        ToolType type;
        float damage[6]; // wood, stone, iron, diamond, gold, copper
    };
    static constexpr ToolKind kTools[] = {
        {"pickaxe", ToolType::Pickaxe, {2, 3, 4, 5, 2, 3}},
        {"axe", ToolType::Axe, {7, 9, 9, 9, 7, 9}},
        {"shovel", ToolType::Shovel, {2.5f, 3.5f, 4.5f, 5.5f, 2.5f, 3.5f}},
        {"hoe", ToolType::Hoe, {1, 1, 1, 1, 1, 1}},
        {"sword", ToolType::Sword, {4, 5, 6, 7, 4, 5}},
    };
    static constexpr struct {
        const char* name;
        ToolTier tier;
    } kMaterials[] = {{"wooden", ToolTier::Wood},
                      {"stone", ToolTier::Stone},
                      {"iron", ToolTier::Iron},
                      {"diamond", ToolTier::Diamond},
                      {"golden", ToolTier::Gold},
                      {"copper", ToolTier::Copper}};
    for (int m = 0; m < 6; ++m)
        for (const auto& t : kTools) {
            const std::string name = std::string(kMaterials[m].name) + "_" + t.name;
            r.add({.id = "minecraft:" + name,
                   .maxStack = 1,
                   .tool = t.type,
                   .tier = kMaterials[m].tier,
                   .durability = tierInfo(kMaterials[m].tier).durability,
                   .attackDamage = t.damage[m],
                   .texture = "item/" + name});
        }
    // Materials (wiki: each item's page).
    for (const char* name : {"stick", "coal", "charcoal", "raw_iron", "raw_gold", "raw_copper",
                             "iron_ingot", "gold_ingot", "copper_ingot", "diamond", "emerald",
                             "lapis_lazuli", "redstone", "flint", "leather"})
        r.add({.id = std::string("minecraft:") + name,
               .block = std::string_view(name) == "redstone" ? BlockId(blocks::RedstoneWire) : BlockId(0),
               .texture = std::string("item/") + name});
    // Dimensions (wiki: Flint and Steel - 64 uses; Eye of Ender; Nether Quartz).
    r.add({.id = "minecraft:flint_and_steel", .maxStack = 1, .durability = 64, .texture = "item/flint_and_steel"});
    r.add({.id = "minecraft:ender_eye", .texture = "item/ender_eye"});
    r.add({.id = "minecraft:quartz", .texture = "item/quartz"});
    // Buckets (M14; wiki: Bucket - empty ones stack to 16, full ones don't).
    r.add({.id = "minecraft:bucket", .maxStack = 16, .texture = "item/bucket"});
    r.add({.id = "minecraft:water_bucket", .maxStack = 1, .texture = "item/water_bucket"});
    r.add({.id = "minecraft:lava_bucket", .maxStack = 1, .texture = "item/lava_bucket"});
    r.add({.id = "minecraft:milk_bucket", .maxStack = 1, .texture = "item/milk_bucket"});
    // Redstone dust is placed as redstone_wire; wall torches drop the torch item.
    r.mapBlock(blocks::RedstoneWire, *r.find("redstone"));
    r.mapBlock(blocks::RedstoneWallTorch, *r.find("redstone_torch"));
    // Food (wiki: Food - apple restores 4 hunger, 2.4 saturation).
    r.add({.id = "minecraft:apple", .food = 4, .saturation = 2.4f, .texture = "item/apple"});
    // (wiki: Raw Beef 3 / 1.8, Steak 8 / 12.8, Rotten Flesh 4 / 0.8)
    r.add({.id = "minecraft:beef", .food = 3, .saturation = 1.8f, .texture = "item/beef"});
    r.add({.id = "minecraft:cooked_beef", .food = 8, .saturation = 12.8f, .texture = "item/cooked_beef"});
    r.add({.id = "minecraft:rotten_flesh", .food = 4, .saturation = 0.8f, .texture = "item/rotten_flesh"});
    return r;
}

} // namespace

const ItemRegistry& itemRegistry() {
    static const ItemRegistry registry = buildItems();
    return registry;
}

} // namespace mc::world
