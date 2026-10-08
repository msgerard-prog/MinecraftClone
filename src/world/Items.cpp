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
            b == blocks::NetherPortal || b == blocks::EndPortal || b == blocks::EndGateway || b == blocks::Fire || b == blocks::Wheat ||
            b == blocks::Carrots || b == blocks::Potatoes || b == blocks::Beetroots) // crops: planted by seeds
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
                             "lapis_lazuli", "redstone", "flint", "leather", "brick", "nether_brick", "clay_ball"})
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
    // Farm animals (M16.3; wiki: each food's page - hunger / saturation).
    r.add({.id = "minecraft:porkchop", .food = 3, .saturation = 1.8f, .texture = "item/porkchop"});
    r.add({.id = "minecraft:cooked_porkchop", .food = 8, .saturation = 12.8f, .texture = "item/cooked_porkchop"});
    r.add({.id = "minecraft:mutton", .food = 2, .saturation = 1.2f, .texture = "item/mutton"});
    r.add({.id = "minecraft:cooked_mutton", .food = 6, .saturation = 9.6f, .texture = "item/cooked_mutton"});
    r.add({.id = "minecraft:chicken", .food = 2, .saturation = 1.2f, .texture = "item/chicken"});
    r.add({.id = "minecraft:cooked_chicken", .food = 6, .saturation = 7.2f, .texture = "item/cooked_chicken"});
    // Seeds and planted vegetables place their crop (M17.1).
    r.add({.id = "minecraft:carrot", .block = blocks::Carrots, .food = 3, .saturation = 3.6f, .texture = "item/carrot"});
    r.add({.id = "minecraft:wheat", .texture = "item/wheat"});
    r.add({.id = "minecraft:wheat_seeds", .block = blocks::Wheat, .texture = "item/wheat_seeds"});
    r.add({.id = "minecraft:feather", .texture = "item/feather"});
    r.add({.id = "minecraft:egg", .maxStack = 16, .texture = "item/egg"});
    r.add({.id = "minecraft:shears", .maxStack = 1, .durability = 238, .texture = "item/shears"});
    // Farming (M17.1; wiki: Potato 1 / 0.6, Baked Potato 5 / 6, Poisonous Potato 2 / 1.2,
    // Beetroot 1 / 1.2, Bread 5 / 6).
    r.add({.id = "minecraft:potato", .block = blocks::Potatoes, .food = 1, .saturation = 0.6f, .texture = "item/potato"});
    r.add({.id = "minecraft:baked_potato", .food = 5, .saturation = 6.0f, .texture = "item/baked_potato"});
    r.add({.id = "minecraft:poisonous_potato", .food = 2, .saturation = 1.2f, .texture = "item/poisonous_potato"});
    r.add({.id = "minecraft:beetroot", .food = 1, .saturation = 1.2f, .texture = "item/beetroot"});
    r.add({.id = "minecraft:beetroot_seeds", .block = blocks::Beetroots, .texture = "item/beetroot_seeds"});
    r.add({.id = "minecraft:bread", .food = 5, .saturation = 6.0f, .texture = "item/bread"});
    r.add({.id = "minecraft:bone_meal", .texture = "item/bone_meal"});
    // Armor (M17.3; wiki: Armor, Copper Armor): points and durability per piece
    // (helmet, chestplate, leggings, boots); diamond adds 2 toughness per piece.
    static constexpr struct {
        const char* material;
        int points[4];
        int durability[4];
        float toughness;
    } kArmor[] = {
        {"leather", {1, 3, 2, 1}, {55, 80, 75, 65}, 0.0f},
        {"copper", {2, 4, 3, 1}, {121, 176, 165, 143}, 0.0f},
        {"golden", {2, 5, 3, 1}, {77, 112, 105, 91}, 0.0f},
        {"iron", {2, 6, 5, 2}, {165, 240, 225, 195}, 0.0f},
        {"diamond", {3, 8, 6, 3}, {363, 528, 495, 429}, 2.0f},
    };
    static constexpr const char* kPieces[4] = {"helmet", "chestplate", "leggings", "boots"};
    for (const auto& a : kArmor)
        for (int p = 0; p < 4; ++p) {
            const std::string name = std::string(a.material) + "_" + kPieces[p];
            r.add({.id = "minecraft:" + name,
                   .maxStack = 1,
                   .durability = a.durability[p],
                   .texture = "item/" + name,
                   .armorSlot = static_cast<uint8_t>(p + 1),
                   .armor = a.points[p],
                   .toughness = a.toughness});
        }
    // (wiki: Shield - 336 uses)
    r.add({.id = "minecraft:shield", .maxStack = 1, .durability = 336, .texture = "item/shield"});
    // Books (M17.5; wiki: Paper, Book, Enchanted Book - stacks to 1).
    r.add({.id = "minecraft:paper", .texture = "item/paper"});
    r.add({.id = "minecraft:book", .texture = "item/book"});
    r.add({.id = "minecraft:enchanted_book", .maxStack = 1, .texture = "item/enchanted_book"});
    // Projectiles (M16.4; wiki: Bow - 384 uses; Arrow).
    r.add({.id = "minecraft:bow", .maxStack = 1, .durability = 384, .texture = "item/bow"});
    r.add({.id = "minecraft:arrow", .texture = "item/arrow"});
    // Hostile mob drops (M16.5; wiki: Bone, Gunpowder, String, Spider Eye 2 / 3.2,
    // Ender Pearl - stacks to 16).
    r.add({.id = "minecraft:bone", .texture = "item/bone"});
    r.add({.id = "minecraft:gunpowder", .texture = "item/gunpowder"});
    r.add({.id = "minecraft:string", .texture = "item/string"});
    r.add({.id = "minecraft:spider_eye", .food = 2, .saturation = 3.2f, .texture = "item/spider_eye"});
    r.add({.id = "minecraft:ender_pearl", .maxStack = 16, .texture = "item/ender_pearl"});
    // Nether mob drops (M19.2).
    r.add({.id = "minecraft:ghast_tear", .texture = "item/ghast_tear"});
    r.add({.id = "minecraft:blaze_rod", .texture = "item/blaze_rod"});
    r.add({.id = "minecraft:blaze_powder", .texture = "item/blaze_powder"});
    r.add({.id = "minecraft:magma_cream", .texture = "item/magma_cream"});
    r.add({.id = "minecraft:gold_nugget", .texture = "item/gold_nugget"});
    r.add({.id = "minecraft:fire_charge", .texture = "item/fire_charge"});
    // Brewing (M19.4; wiki: Potion, Glass Bottle, Sugar, Fermented Spider Eye, Golden
    // Carrot - 6 food, 14.4 saturation; Glowstone Dust).
    r.add({.id = "minecraft:glass_bottle", .texture = "item/glass_bottle"});
    r.add({.id = "minecraft:potion", .maxStack = 1, .texture = "item/potion"});
    r.add({.id = "minecraft:splash_potion", .maxStack = 1, .texture = "item/splash_potion"});
    r.add({.id = "minecraft:sugar", .texture = "item/sugar"});
    r.add({.id = "minecraft:fermented_spider_eye", .texture = "item/fermented_spider_eye"});
    r.add({.id = "minecraft:golden_carrot", .food = 6, .saturation = 14.4f, .texture = "item/golden_carrot"});
    // The End (M20.1; wiki: Chorus Fruit - 4 food, 2.4 saturation, edible when full,
    // teleports; Popped Chorus Fruit - smelted, for purpur).
    r.add({.id = "minecraft:chorus_fruit",
           .food = 4,
           .saturation = 2.4f,
           .alwaysEdible = true,
           .texture = "item/chorus_fruit"});
    r.add({.id = "minecraft:popped_chorus_fruit", .texture = "item/popped_chorus_fruit"});
    r.add({.id = "minecraft:end_crystal", .texture = "item/end_crystal"});
    // (wiki: Elytra - worn in the chest slot, 432 durability, no armor points; M20.4)
    r.add({.id = "minecraft:elytra", .maxStack = 1, .durability = 432, .texture = "item/elytra", .armorSlot = 2});
    r.add({.id = "minecraft:shulker_shell", .texture = "item/shulker_shell"});
    r.add({.id = "minecraft:minecart", .maxStack = 1, .texture = "item/minecart"}); // (M21.4)
    r.add({.id = "minecraft:slime_ball", .texture = "item/slime_ball"});             // (M21.5)
    r.add({.id = "minecraft:glowstone_dust", .texture = "item/glowstone_dust"});
    // Crops' items (pick block, drops of an immature crop).
    r.mapBlock(blocks::Wheat, *r.find("wheat_seeds"));
    r.mapBlock(blocks::Carrots, *r.find("carrot"));
    r.mapBlock(blocks::Potatoes, *r.find("potato"));
    r.mapBlock(blocks::Beetroots, *r.find("beetroot_seeds"));
    return r;
}

} // namespace

const ItemRegistry& itemRegistry() {
    static const ItemRegistry registry = buildItems();
    return registry;
}

} // namespace mc::world
