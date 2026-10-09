#include "world/Items.h"

#include "world/Mob.h"

#include "world/ArmorTrims.h"
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
        {4, 9.0f, 2031}, // Netherite (M23.6)
    };
    return kTiers[static_cast<int>(tier)];
}

std::optional<ItemId> ItemRegistry::find(std::string_view id) const {
    const bool bare = id.find(':') == std::string_view::npos;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const std::string_view name = m_items[i].id;
        if (name == id || (bare && name.size() > 10 && name.substr(10) == id))
            return static_cast<ItemId>(i);
    }
    return std::nullopt;
}

ItemId ItemRegistry::add(ItemDef def) {
    def.fireResistant =
        def.id.find("netherite") != std::string::npos || def.id == "minecraft:ancient_debris";
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
        if (b == blocks::KelpPlant || b == blocks::TallSeagrass)
            continue; // (picked as kelp / seagrass)
        // Placed by another item (redstone dust, torches on walls) or never an item.
        if (b == blocks::RedstoneWire || b == blocks::RedstoneWallTorch || b == blocks::WallTorch ||
            b == blocks::SoulWallTorch || b == blocks::PistonHead || b == blocks::NetherPortal ||
            b == blocks::EndPortal || b == blocks::EndGateway || b == blocks::Fire ||
            b == blocks::Wheat || b == blocks::Carrots || b == blocks::Potatoes ||
            b == blocks::Beetroots ||      // crops: planted by seeds
            b == blocks::SweetBerryBush || // (planted by sweet berries)
            b == blocks::CaveVines || b == blocks::CaveVinesPlant ||
            b == blocks::BigDripleafStem || // (M27.2)
            b == blocks::TorchflowerCrop ||
            b == blocks::PitcherCrop || // (M27.5c: planted by seeds and pods)
            b == blocks::FrostedIce)    // (M29.2b: only from Frost Walker)
            continue;
        const std::string& id = blocks.block(b).id;
        // Wall signs come from the sign items (M23.3c), like wall torches from torches.
        if (blocks.kind(b) == BlockKind::WallSign || blocks.kind(b) == BlockKind::WallHangingSign ||
            blocks.kind(b) == BlockKind::WallBanner)
            continue;
        if (blocks.likeOf(b) == blocks::CandleCake)
            continue;            // (M28.5a: made by putting a candle on a cake)
        if (b == blocks::Cake) { // (M28.5a) one at a time
            r.mapBlock(b, r.add({.id = id, .maxStack = 1, .block = b, .texture = "item/cake"}));
            continue;
        }
        if (blocks.kind(b) ==
            BlockKind::Banner) { // (M28.3d) stacks of 16, drawn from their own icon
            r.mapBlock(
                b,
                r.add({.id = id, .maxStack = 16, .block = b, .texture = "item/" + id.substr(10)}));
            continue;
        }
        if (isMobHead(b))
            continue; // (M26.4b: below - worn on the head, wall kinds from the standing ones)
        const bool sign =
            blocks.kind(b) == BlockKind::Sign || blocks.kind(b) == BlockKind::HangingSign;
        r.mapBlock(b, r.add({.id = id,
                             .maxStack = uint8_t(sign ? 16 : 64),
                             .block = b})); // (signs stack to 16)
    }
    r.mapBlock(blocks::KelpPlant, *r.find("kelp")); // (M25.1)
    // Mob heads (M26.4b; wiki: Head): placeable, and worn in the helmet slot.
    for (BlockId b = blocks::SkeletonSkull; b <= blocks::DragonHead; b += 2) {
        ItemDef head;
        head.id = blocks.block(b).id;
        head.block = b;
        head.armorSlot = 1;
        const ItemId item = r.add(head);
        r.mapBlock(b, item);
        r.mapBlock(static_cast<BlockId>(b + 1), item);
    }
    r.mapBlock(blocks::TallSeagrass, *r.find("seagrass"));
    // Wall signs pick and drop as their sign.
    for (BlockId b = 1; b < blocks.blockCount(); ++b)
        if (blocks.kind(b) == BlockKind::WallSign || blocks.kind(b) == BlockKind::WallHangingSign ||
            blocks.kind(b) == BlockKind::WallBanner) {
            std::string id = blocks.block(b).id;
            id.erase(id.find("_wall"),
                     5); // "oak_wall_sign" -> "oak_sign", "red_wall_banner" -> "red_banner"
            if (const auto item = r.find(id)) r.mapBlock(b, *item);
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
    } kMaterials[] = {{"wooden", ToolTier::Wood}, {"stone", ToolTier::Stone},
                      {"iron", ToolTier::Iron},   {"diamond", ToolTier::Diamond},
                      {"golden", ToolTier::Gold}, {"copper", ToolTier::Copper}};
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
    // Netherite tools (M23.6; wiki: Netherite Sword 8, Axe 10, Pickaxe 6, Shovel 6.5, Hoe 1).
    for (const auto& [kind, type, damage] :
         {std::tuple{"sword", ToolType::Sword, 8.0f},
          std::tuple{"pickaxe", ToolType::Pickaxe, 6.0f}, std::tuple{"axe", ToolType::Axe, 10.0f},
          std::tuple{"shovel", ToolType::Shovel, 6.5f}, std::tuple{"hoe", ToolType::Hoe, 1.0f}}) {
        const std::string name = std::string("netherite_") + kind;
        r.add({.id = "minecraft:" + name,
               .maxStack = 1,
               .tool = type,
               .tier = ToolTier::Netherite,
               .durability = tierInfo(ToolTier::Netherite).durability,
               .attackDamage = damage,
               .texture = "item/" + name});
    }
    // Spears (M28.4e; 1.21.11, wiki: Spear): a jab with a long reach, a charge that hits by
    // speed. Jab damage: wood/gold 1, stone/copper 2, iron 3, diamond 4, netherite 5;
    // durability by tier.
    for (const auto& [mat, tier, damage] :
         {std::tuple{"wooden", ToolTier::Wood, 1.0f}, std::tuple{"stone", ToolTier::Stone, 2.0f},
          std::tuple{"copper", ToolTier::Copper, 2.0f}, std::tuple{"iron", ToolTier::Iron, 3.0f},
          std::tuple{"golden", ToolTier::Gold, 1.0f},
          std::tuple{"diamond", ToolTier::Diamond, 4.0f},
          std::tuple{"netherite", ToolTier::Netherite, 5.0f}}) {
        const std::string name = std::string(mat) + "_spear";
        r.add({.id = "minecraft:" + name,
               .maxStack = 1,
               .tool = ToolType::Spear,
               .tier = tier,
               .durability = tierInfo(tier).durability,
               .attackDamage = damage,
               .texture = "item/" + name});
    }
    // Netherite materials and the smithing templates (M23.6; wiki: Netherite Ingot,
    // Smithing Template).
    // Beacons and conduits (M23.6): the nether star comes with the Wither (M26), the
    // heart and shells with oceans (M25); prismarine shards and crystals from guardians.
    for (const char* name : {"nether_star", "heart_of_the_sea", "nautilus_shell",
                             "prismarine_shard", "prismarine_crystals"})
        r.add({.id = std::string("minecraft:") + name, .texture = std::string("item/") + name});
    // Music discs (M23.6; wiki: Music Disc - stack to 1). Ours play tunes the game makes
    // from note-block sounds (gameplay/Jukebox), not recordings.
    for (const char* name : {"13", "cat", "blocks", "chirp", "far", "mall", "mellohi", "stal", "strad", "ward",
                             "11", "wait", "pigstep", "otherside", "5", "relic", "precipice", "creator",
                             "creator_music_box", "tears", "lava_chicken"}) // (M29.3a: the rest)
        r.add({.id = std::string("minecraft:music_disc_") + name,
               .maxStack = 1,
               .texture = std::string("item/music_disc_") + name});
    r.add({.id = "minecraft:disc_fragment_5",
           .texture = "item/disc_fragment_5"}); // (M28.5b: 9 make disc 5)
    for (const char* name :
         {"netherite_scrap", "netherite_ingot", "netherite_upgrade_smithing_template"})
        r.add({.id = std::string("minecraft:") + name, .texture = std::string("item/") + name});
    for (const std::string_view pattern : kTrimPatterns) { // (wiki: one template per armor trim)
        const std::string name = std::string(pattern) + "_armor_trim_smithing_template";
        r.add({.id = "minecraft:" + name, .texture = "item/" + name});
    }
    // Materials (wiki: each item's page).
    for (const char* name :
         {"stick", "coal", "charcoal", "raw_iron", "raw_gold", "raw_copper", "iron_ingot",
          "gold_ingot", "copper_ingot", "diamond", "emerald", "lapis_lazuli", "redstone", "flint",
          "leather", "brick", "nether_brick", "clay_ball"})
        r.add({.id = std::string("minecraft:") + name,
               .block = std::string_view(name) == "redstone" ? BlockId(blocks::RedstoneWire)
                                                             : BlockId(0),
               .texture = std::string("item/") + name});
    // Dimensions (wiki: Flint and Steel - 64 uses; Eye of Ender; Nether Quartz).
    r.add({.id = "minecraft:flint_and_steel",
           .maxStack = 1,
           .durability = 64,
           .texture = "item/flint_and_steel"});
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
    r.mapBlock(blocks::WallTorch, *r.find("torch")); // (M23.2: picked and dropped as the torch)
    r.mapBlock(blocks::SoulWallTorch, *r.find("soul_torch"));
    for (const BlockId c : {BlockId(blocks::WaterCauldron), BlockId(blocks::LavaCauldron),
                            BlockId(blocks::PowderSnowCauldron)})
        r.mapBlock(c, *r.find("cauldron")); // (filled cauldrons pick and drop as the cauldron)
    // Food (wiki: Food - apple restores 4 hunger, 2.4 saturation).
    r.add({.id = "minecraft:apple", .food = 4, .saturation = 2.4f, .texture = "item/apple"});
    // (M29.1c; wiki: Bowl, Mushroom Stew - 6 hunger, 7.2 saturation, stacks to 1, the bowl
    // comes back)
    r.add({.id = "minecraft:bowl", .texture = "item/bowl"});
    r.add({.id = "minecraft:mushroom_stew", .maxStack = 1, .food = 6, .saturation = 7.2f,
           .texture = "item/mushroom_stew"});
    // (M24.3; wiki: Golden Apple - 4 hunger, 9.6 saturation, always edible)
    r.add({.id = "minecraft:golden_apple",
           .food = 4,
           .saturation = 9.6f,
           .alwaysEdible = true,
           .texture = "item/golden_apple"});
    // (M24.4; wiki: Crossbow - 465 uses (shot by the player in M28); Ominous Bottle -
    // dropped by raid captains, drunk for Bad Omen in M24.5)
    r.add(
        {.id = "minecraft:crossbow", .maxStack = 1, .durability = 465, .texture = "item/crossbow"});
    r.add({.id = "minecraft:ominous_bottle", .texture = "item/ominous_bottle"});
    // (M24.5; wiki: Totem of Undying - held, it saves its holder from death once)
    r.add({.id = "minecraft:totem_of_undying", .maxStack = 1, .texture = "item/totem_of_undying"});
    // (wiki: Raw Beef 3 / 1.8, Steak 8 / 12.8, Rotten Flesh 4 / 0.8)
    r.add({.id = "minecraft:beef", .food = 3, .saturation = 1.8f, .texture = "item/beef"});
    r.add({.id = "minecraft:cooked_beef",
           .food = 8,
           .saturation = 12.8f,
           .texture = "item/cooked_beef"});
    // Fish (M25.2; wiki: Raw Cod 2/0.4, Cooked Cod 5/6, Raw Salmon 2/0.4, Cooked Salmon
    // 6/9.6, Tropical Fish 1/0.2, Pufferfish 1/0.2 - which poisons), ink sacs, fish
    // buckets (a fish and its water), the fishing rod (64 uses).
    r.add({.id = "minecraft:cod", .food = 2, .saturation = 0.4f, .texture = "item/cod"});
    r.add({.id = "minecraft:cooked_cod",
           .food = 5,
           .saturation = 6.0f,
           .texture = "item/cooked_cod"});
    r.add({.id = "minecraft:salmon", .food = 2, .saturation = 0.4f, .texture = "item/salmon"});
    r.add({.id = "minecraft:cooked_salmon",
           .food = 6,
           .saturation = 9.6f,
           .texture = "item/cooked_salmon"});
    r.add({.id = "minecraft:tropical_fish",
           .food = 1,
           .saturation = 0.2f,
           .texture = "item/tropical_fish"});
    r.add({.id = "minecraft:pufferfish",
           .food = 1,
           .saturation = 0.2f,
           .texture = "item/pufferfish"});
    r.add({.id = "minecraft:ink_sac", .texture = "item/ink_sac"});
    r.add({.id = "minecraft:glow_ink_sac", .texture = "item/glow_ink_sac"});
    for (const char* fish : {"cod", "salmon", "tropical_fish", "pufferfish"})
        r.add({.id = std::string("minecraft:") + fish + "_bucket",
               .maxStack = 1,
               .texture = std::string("item/") + fish + "_bucket"});
    r.add({.id = "minecraft:fishing_rod",
           .maxStack = 1,
           .durability = 64,
           .texture = "item/fishing_rod"});
    // (M25.3; wiki: Trident - 9 attack damage, 250 uses)
    r.add({.id = "minecraft:trident",
           .maxStack = 1,
           .durability = 250,
           .attackDamage = 9.0f,
           .texture = "item/trident"});
    for (int w = 0; w < 10; ++w) { // boats (M25.2b; wiki: Boat - stack to 1)
        const std::string id = boatId(w);
        r.add({.id = id, .maxStack = 1, .texture = "item/" + id.substr(10)});
    }
    for (int w = 0; w < 10; ++w) { // chest boats (M26.2)
        const std::string id = chestBoatId(w);
        r.add({.id = id, .maxStack = 1, .texture = "item/" + id.substr(10)});
    }
    // Wildlife (M26.3; wiki: Raw Rabbit 3 / 1.8, Cooked Rabbit 5 / 6, Rabbit Hide, Rabbit's
    // Foot, Goat Horn, Armadillo Scute, Wolf Armor - 64 durability).
    r.add({.id = "minecraft:rabbit", .food = 3, .saturation = 1.8f, .texture = "item/rabbit"});
    r.add({.id = "minecraft:cooked_rabbit",
           .food = 5,
           .saturation = 6.0f,
           .texture = "item/cooked_rabbit"});
    r.add({.id = "minecraft:rabbit_hide", .texture = "item/rabbit_hide"});
    r.add({.id = "minecraft:rabbit_foot", .texture = "item/rabbit_foot"});
    r.add({.id = "minecraft:goat_horn", .maxStack = 1, .texture = "item/goat_horn"});
    r.add({.id = "minecraft:armadillo_scute", .texture = "item/armadillo_scute"});
    r.add({.id = "minecraft:wolf_armor",
           .maxStack = 1,
           .durability = 64,
           .texture = "item/wolf_armor"});
    // (M26.3b; wiki: Honey Bottle - 6 hunger, 1.2 saturation, stacks to 16)
    r.add({.id = "minecraft:honey_bottle",
           .maxStack = 16,
           .food = 6,
           .saturation = 1.2f,
           .texture = "item/honey_bottle"});
    // (M27.5; wiki: Brush - 64 uses; Pottery Sherd - from suspicious blocks)
    r.add({.id = "minecraft:brush", .maxStack = 1, .durability = 64, .texture = "item/brush"});
    for (const char* s : kSherds)
        r.add({.id = std::string("minecraft:") + s + "_pottery_sherd",
               .texture = std::string("item/") + s + "_pottery_sherd"});
    // (M27.4d; wiki: Trial Key - opens a vault; from trial spawners)
    r.add({.id = "minecraft:trial_key", .texture = "item/trial_key"});
    r.add({.id = "minecraft:ominous_trial_key", .texture = "item/ominous_trial_key"}); // (M28.4d)
    // (M28.4d; wiki: Mace - 6 attack damage, 500 uses; falling hits smash)
    r.add({.id = "minecraft:mace",
           .maxStack = 1,
           .durability = 500,
           .attackDamage = 6.0f,
           .texture = "item/mace"});
    // (M27.3b; wiki: Echo Shard - found in ancient cities)
    r.add({.id = "minecraft:echo_shard", .texture = "item/echo_shard"});
    // (M27.1c; wiki: Resin Brick - smelted from resin clumps; 4 make resin bricks)
    r.add({.id = "minecraft:resin_brick", .texture = "item/resin_brick"});
    // (M26.5b; wiki: Snowball - stacks of 16; Harness - 16 colours, unstackable)
    r.add({.id = "minecraft:snowball", .maxStack = 16, .texture = "item/snowball"});
    for (const char* colour : kDyeColours)
        r.add({.id = std::string("minecraft:") + colour + "_harness",
               .maxStack = 1,
               .texture = std::string("item/") + colour + "_harness"});
    // (M26.5a; wiki: Amethyst Shard - an allay dancing to a jukebox duplicates with one;
    // the geodes it comes from arrive in M27)
    r.add({.id = "minecraft:amethyst_shard", .texture = "item/amethyst_shard"});
    // (M26.4c; wiki: Breeze Rod - 4 wind charges; Wind Charge - thrown, a burst of wind)
    r.add({.id = "minecraft:breeze_rod", .texture = "item/breeze_rod"});
    r.add({.id = "minecraft:wind_charge", .texture = "item/wind_charge"});
    // (M26.4a; wiki: Phantom Membrane - repairs elytra, brews Slow Falling)
    r.add({.id = "minecraft:phantom_membrane", .texture = "item/phantom_membrane"});
    // (M26.3c; wiki: Bucket of Axolotl, Bucket of Tadpole - unstackable)
    r.add({.id = "minecraft:axolotl_bucket", .maxStack = 1, .texture = "item/axolotl_bucket"});
    r.add({.id = "minecraft:tadpole_bucket", .maxStack = 1, .texture = "item/tadpole_bucket"});
    // Mount gear (M26.2; wiki: Saddle, Horse Armor - unstackable).
    r.add({.id = "minecraft:saddle", .maxStack = 1, .texture = "item/saddle"});
    for (int k = 1; k < 5; ++k) // (the first four; copper and netherite come last - M29.3a)
        r.add({.id = std::string("minecraft:") + kHorseArmorItems[k],
               .maxStack = 1,
               .texture = std::string("item/") + kHorseArmorItems[k]});
    r.add({.id = "minecraft:rotten_flesh",
           .food = 4,
           .saturation = 0.8f,
           .texture = "item/rotten_flesh"});
    // Farm animals (M16.3; wiki: each food's page - hunger / saturation).
    r.add({.id = "minecraft:porkchop", .food = 3, .saturation = 1.8f, .texture = "item/porkchop"});
    r.add({.id = "minecraft:cooked_porkchop",
           .food = 8,
           .saturation = 12.8f,
           .texture = "item/cooked_porkchop"});
    r.add({.id = "minecraft:mutton", .food = 2, .saturation = 1.2f, .texture = "item/mutton"});
    r.add({.id = "minecraft:cooked_mutton",
           .food = 6,
           .saturation = 9.6f,
           .texture = "item/cooked_mutton"});
    r.add({.id = "minecraft:chicken", .food = 2, .saturation = 1.2f, .texture = "item/chicken"});
    r.add({.id = "minecraft:cooked_chicken",
           .food = 6,
           .saturation = 7.2f,
           .texture = "item/cooked_chicken"});
    // Seeds and planted vegetables place their crop (M17.1).
    r.add({.id = "minecraft:carrot",
           .block = blocks::Carrots,
           .food = 3,
           .saturation = 3.6f,
           .texture = "item/carrot"});
    r.add({.id = "minecraft:wheat", .texture = "item/wheat"});
    r.add({.id = "minecraft:wheat_seeds", .block = blocks::Wheat, .texture = "item/wheat_seeds"});
    r.add({.id = "minecraft:feather", .texture = "item/feather"});
    r.add({.id = "minecraft:egg", .maxStack = 16, .texture = "item/egg"});
    // (M29.1d; 1.21.5) cold chickens lay blue eggs, warm ones brown; each hatches its kind
    r.add({.id = "minecraft:blue_egg", .maxStack = 16, .texture = "item/blue_egg"});
    r.add({.id = "minecraft:brown_egg", .maxStack = 16, .texture = "item/brown_egg"});
    r.add({.id = "minecraft:shears", .maxStack = 1, .durability = 238, .texture = "item/shears"});
    // Farming (M17.1; wiki: Potato 1 / 0.6, Baked Potato 5 / 6, Poisonous Potato 2 / 1.2,
    // Beetroot 1 / 1.2, Bread 5 / 6).
    r.add({.id = "minecraft:potato",
           .block = blocks::Potatoes,
           .food = 1,
           .saturation = 0.6f,
           .texture = "item/potato"});
    r.add({.id = "minecraft:baked_potato",
           .food = 5,
           .saturation = 6.0f,
           .texture = "item/baked_potato"});
    r.add({.id = "minecraft:poisonous_potato",
           .food = 2,
           .saturation = 1.2f,
           .texture = "item/poisonous_potato"});
    r.add({.id = "minecraft:beetroot", .food = 1, .saturation = 1.2f, .texture = "item/beetroot"});
    r.add({.id = "minecraft:beetroot_seeds",
           .block = blocks::Beetroots,
           .texture = "item/beetroot_seeds"});
    // (M26.3; wiki: Sweet Berries - 2 hunger, 0.4 saturation; they plant the bush)
    r.add({.id = "minecraft:sweet_berries",
           .block = blocks::SweetBerryBush,
           .food = 2,
           .saturation = 0.4f,
           .texture = "item/sweet_berries"});
    // (M27.5c; wiki: Torchflower Seeds, Pitcher Pod - planted on farmland; sniffers dig them up)
    r.add({.id = "minecraft:torchflower_seeds",
           .block = blocks::TorchflowerCrop,
           .texture = "item/torchflower_seeds"});
    r.add({.id = "minecraft:pitcher_pod",
           .block = blocks::PitcherCrop,
           .texture = "item/pitcher_pod"});
    // (M27.2; wiki: Glow Berries - food 2, plants cave vines under a block)
    r.add({.id = "minecraft:glow_berries",
           .block = blocks::CaveVines,
           .food = 2,
           .saturation = 0.4f,
           .texture = "item/glow_berries"});
    r.add({.id = "minecraft:bread", .food = 5, .saturation = 6.0f, .texture = "item/bread"});
    r.add({.id = "minecraft:bone_meal", .texture = "item/bone_meal"});
    // (M25.1; wiki: Dried Kelp - 1 food, 0.6 saturation, eaten in 0.8 s)
    r.add({.id = "minecraft:dried_kelp",
           .food = 1,
           .saturation = 0.6f,
           .texture = "item/dried_kelp"});
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
        {"netherite",
         {3, 8, 6, 3},
         {407, 592, 555, 481},
         3.0f}, // (M23.6; +0.1 knockback resistance, not modelled)
        // (M29.3a; wiki: Chainmail Armor - 1, 5, 4, 1; iron's durability; not craftable)
        {"chainmail", {1, 5, 4, 1}, {165, 240, 225, 195}, 0.0f},
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
    // Navigation (M28.2a; wiki: Compass, Recovery Compass, Clock): their icons turn
    // (gfx::ItemIcons picks the frame).
    r.add({.id = "minecraft:compass", .texture = "item/compass_00"});
    r.add({.id = "minecraft:recovery_compass", .texture = "item/recovery_compass_00"});
    r.add({.id = "minecraft:clock", .texture = "item/clock_00"});
    // Maps (M28.2b; wiki: Map): a filled map's id (vanilla minecraft:map_id) is kept in
    // ItemStack::damage.
    r.add({.id = "minecraft:map", .texture = "item/map"});
    r.add({.id = "minecraft:filled_map", .texture = "item/filled_map"});
    // Books (M28.2c; wiki: Book and Quill, Written Book): their pages are ItemStack::extra.
    r.add({.id = "minecraft:writable_book", .maxStack = 1, .texture = "item/writable_book"});
    // Decorations (M28.3; wiki: Item Frame, Painting).
    r.add({.id = "minecraft:item_frame", .texture = "item/item_frame"});
    r.add({.id = "minecraft:glow_item_frame", .texture = "item/glow_item_frame"});
    r.add({.id = "minecraft:painting", .texture = "item/painting"});
    r.add(
        {.id = "minecraft:armor_stand", .maxStack = 16, .texture = "item/armor_stand"}); // (M28.3b)
    r.add({.id = "minecraft:lead", .texture = "item/lead"});                             // (M28.3c)
    // Banner patterns (M28.3d; wiki: Banner Pattern): kept in the loom's third slot.
    for (const char* p : {"field_masoned", "bordure_indented", "creeper", "skull", "flower",
                          "mojang", "globe", "piglin", "flow", "guster"})
        r.add({.id = "minecraft:" + std::string(p) + "_banner_pattern",
               .maxStack = 1,
               .texture = "item/" + std::string(p) + "_banner_pattern"});
    r.add({.id = "minecraft:written_book", .maxStack = 16, .texture = "item/written_book"});
    r.add({.id = "minecraft:enchanted_book", .maxStack = 1, .texture = "item/enchanted_book"});
    // Projectiles (M16.4; wiki: Bow - 384 uses; Arrow).
    r.add({.id = "minecraft:bow", .maxStack = 1, .durability = 384, .texture = "item/bow"});
    r.add({.id = "minecraft:arrow", .texture = "item/arrow"});
    // Hostile mob drops (M16.5; wiki: Bone, Gunpowder, String, Spider Eye 2 / 3.2,
    // Ender Pearl - stacks to 16).
    r.add({.id = "minecraft:bone", .texture = "item/bone"});
    r.add({.id = "minecraft:gunpowder", .texture = "item/gunpowder"});
    r.add({.id = "minecraft:string", .texture = "item/string"});
    r.add({.id = "minecraft:spider_eye",
           .food = 2,
           .saturation = 3.2f,
           .texture = "item/spider_eye"});
    r.add({.id = "minecraft:ender_pearl", .maxStack = 16, .texture = "item/ender_pearl"});
    // Nether mob drops (M19.2).
    r.add({.id = "minecraft:ghast_tear", .texture = "item/ghast_tear"});
    r.add({.id = "minecraft:blaze_rod", .texture = "item/blaze_rod"});
    r.add({.id = "minecraft:blaze_powder", .texture = "item/blaze_powder"});
    r.add({.id = "minecraft:magma_cream", .texture = "item/magma_cream"});
    r.add({.id = "minecraft:gold_nugget", .texture = "item/gold_nugget"});
    // M23.2: iron nuggets, the 16 dyes (wiki: Iron Nugget, Dye).
    r.add({.id = "minecraft:iron_nugget", .texture = "item/iron_nugget"});
    r.add({.id = "minecraft:honeycomb", .texture = "item/honeycomb"}); // (M23.4b; bees come in M26)
    for (const char* colour : kDyeColours)
        r.add({.id = std::string("minecraft:") + colour + "_dye",
               .texture = std::string("item/") + colour + "_dye"});
    r.add({.id = "minecraft:fire_charge", .texture = "item/fire_charge"});
    // Brewing (M19.4; wiki: Potion, Glass Bottle, Sugar, Fermented Spider Eye, Golden
    // Carrot - 6 food, 14.4 saturation; Glowstone Dust).
    r.add({.id = "minecraft:glass_bottle", .texture = "item/glass_bottle"});
    r.add({.id = "minecraft:potion", .maxStack = 1, .texture = "item/potion"});
    r.add({.id = "minecraft:splash_potion", .maxStack = 1, .texture = "item/splash_potion"});
    // (M28.4b; wiki: Lingering Potion, Dragon's Breath, Tipped Arrow, Spectral Arrow)
    r.add({.id = "minecraft:lingering_potion", .maxStack = 1, .texture = "item/lingering_potion"});
    r.add({.id = "minecraft:dragon_breath", .texture = "item/dragon_breath"});
    r.add({.id = "minecraft:tipped_arrow", .texture = "item/tipped_arrow_base"});
    r.add({.id = "minecraft:spectral_arrow", .texture = "item/spectral_arrow"});
    // (M28.4c; wiki: Firework Rocket, Firework Star) - what they carry is ItemStack::extra
    r.add({.id = "minecraft:firework_rocket", .texture = "item/firework_rocket"});
    r.add({.id = "minecraft:firework_star", .texture = "item/firework_star"});
    r.add({.id = "minecraft:sugar", .texture = "item/sugar"});
    r.add({.id = "minecraft:fermented_spider_eye", .texture = "item/fermented_spider_eye"});
    r.add({.id = "minecraft:golden_carrot",
           .food = 6,
           .saturation = 14.4f,
           .texture = "item/golden_carrot"});
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
    r.add({.id = "minecraft:elytra",
           .maxStack = 1,
           .durability = 432,
           .texture = "item/elytra",
           .armorSlot = 2});
    // (M25.3b; wiki: Turtle Scute, Turtle Shell - 2 armor, 275 uses, Water Breathing above water)
    r.add({.id = "minecraft:turtle_scute", .texture = "item/turtle_scute"});
    r.add({.id = "minecraft:turtle_helmet",
           .maxStack = 1,
           .durability = 275,
           .texture = "item/turtle_helmet",
           .armorSlot = 1,
           .armor = 2});
    r.add({.id = "minecraft:shulker_shell", .texture = "item/shulker_shell"});
    r.add({.id = "minecraft:minecart", .maxStack = 1, .texture = "item/minecart"}); // (M21.4)
    r.add({.id = "minecraft:slime_ball", .texture = "item/slime_ball"});            // (M21.5)
    r.add({.id = "minecraft:glowstone_dust", .texture = "item/glowstone_dust"});
    // Crops' items (pick block, drops of an immature crop).
    r.mapBlock(blocks::Wheat, *r.find("wheat_seeds"));
    r.mapBlock(blocks::Carrots, *r.find("carrot"));
    r.mapBlock(blocks::Potatoes, *r.find("potato"));
    r.mapBlock(blocks::Beetroots, *r.find("beetroot_seeds"));
    r.mapBlock(blocks::SweetBerryBush, *r.find("sweet_berries"));
    r.mapBlock(blocks::CaveVines, *r.find("glow_berries"));            // (M27.2)
    r.mapBlock(blocks::TorchflowerCrop, *r.find("torchflower_seeds")); // (M27.5c)
    r.mapBlock(blocks::PitcherCrop, *r.find("pitcher_pod"));
    r.mapBlock(blocks::CaveVinesPlant, *r.find("glow_berries"));
    r.mapBlock(blocks::BigDripleafStem, *r.find("big_dripleaf"));
    // (M29.3a) copper nuggets (wiki: 9 make an ingot; copper gear smelts into one), the
    // copper and netherite horse armor and the nautilus armor (no durability: wiki).
    r.add({.id = "minecraft:copper_nugget", .texture = "item/copper_nugget"});
    // (M29.3b; wiki: Name Tag) renamed on an anvil, it names a mob (which then never despawns)
    r.add({.id = "minecraft:name_tag", .texture = "item/name_tag"});
    // Foods (M29.3c; wiki: each food - hunger, saturation): soups and stews stack to 1 and
    // leave the bowl; the enchanted golden apple is always edible.
    r.add({.id = "minecraft:beetroot_soup", .maxStack = 1, .food = 6, .saturation = 7.2f, .texture = "item/beetroot_soup"});
    r.add({.id = "minecraft:rabbit_stew", .maxStack = 1, .food = 10, .saturation = 12.0f, .texture = "item/rabbit_stew"});
    r.add({.id = "minecraft:suspicious_stew", .maxStack = 1, .food = 6, .saturation = 7.2f, .alwaysEdible = true,
           .texture = "item/suspicious_stew"});
    r.add({.id = "minecraft:cookie", .food = 2, .saturation = 0.4f, .texture = "item/cookie"});
    r.add({.id = "minecraft:pumpkin_pie", .food = 8, .saturation = 4.8f, .texture = "item/pumpkin_pie"});
    r.add({.id = "minecraft:melon_slice", .food = 2, .saturation = 1.2f, .texture = "item/melon_slice"});
    r.add({.id = "minecraft:glistering_melon_slice", .texture = "item/glistering_melon_slice"});
    r.add({.id = "minecraft:enchanted_golden_apple", .food = 4, .saturation = 9.6f, .alwaysEdible = true,
           .texture = "item/enchanted_golden_apple"});
    // (M29.3c; wiki: Spyglass, Bottle o' Enchanting, Knowledge Book)
    r.add({.id = "minecraft:spyglass", .maxStack = 1, .texture = "item/spyglass"});
    r.add({.id = "minecraft:experience_bottle", .texture = "item/experience_bottle"});
    r.add({.id = "minecraft:knowledge_book", .maxStack = 1, .texture = "item/knowledge_book"});
    for (int k = 5; k < 7; ++k)
        r.add({.id = std::string("minecraft:") + kHorseArmorItems[k], .maxStack = 1,
               .texture = std::string("item/") + kHorseArmorItems[k]});
    for (int k = 1; k < 6; ++k)
        r.add({.id = std::string("minecraft:") + kNautilusArmorItems[k], .maxStack = 1,
               .texture = std::string("item/") + kNautilusArmorItems[k]});
    // Spawn eggs (M29.1e; wiki: Spawn Egg): one per mob, except the entities that aren't
    // mobs and the illusioner (none in vanilla either). Registered last: older ids stay.
    for (int t = 0; t < int(MobType::Count); ++t) {
        const MobType type = MobType(t);
        if (type == MobType::EndCrystal || type == MobType::Minecart || type == MobType::Boat ||
            type == MobType::ItemFrame || type == MobType::GlowItemFrame || type == MobType::Painting ||
            type == MobType::ArmorStand || type == MobType::LeashKnot || type == MobType::Illusioner)
            continue;
        const std::string name = std::string(mobInfo(type).id.substr(10)) + "_spawn_egg";
        r.add({.id = "minecraft:" + name, .texture = "item/" + name, .spawnEgg = uint8_t(t + 1)});
    }
    return r;
}

} // namespace

const ItemRegistry& itemRegistry() {
    static const ItemRegistry registry = buildItems();
    return registry;
}

} // namespace mc::world
