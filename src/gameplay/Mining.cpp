#include "gameplay/Mining.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"

#include <cmath>
#include <string_view>

namespace mc {

using namespace world;

HarvestInfo harvestInfo(BlockId b) {
    using T = ToolType;
    // Metal blocks that behave like wooden ones (copper doors) still need a pickaxe.
    if (const BlockSettings& real = blockRegistry().block(b).settings;
        real.like != 0 && real.tool == HarvestTool::Pickaxe && real.tier > 0)
        return {T::Pickaxe, real.tier};
    switch (blockRegistry().likeOf(b)) { // (wood sets mine like the oak ones: M23.3)
    // Pickaxe blocks; ores need the tier listed on the wiki.
    case blocks::Stone:
    case blocks::Cobblestone:
    case blocks::Deepslate:
    case blocks::Granite:
    case blocks::Diorite:
    case blocks::Andesite:
    case blocks::Tuff:
    case blocks::Calcite:
    case blocks::Sandstone:
    case blocks::RedSandstone:
    case blocks::Terracotta:
    case blocks::NetherBricks:
    case blocks::NetherBrickFence:
    case blocks::PolishedBlackstoneBricks:
    case blocks::CrackedPolishedBlackstoneBricks:
    case blocks::ChiseledPolishedBlackstone:
    case blocks::GildedBlackstone:
    case blocks::PolishedBasalt:
    case blocks::CrimsonNylium: // (wiki: any pickaxe; drop netherrack without Silk Touch)
    case blocks::WarpedNylium:
    case blocks::Basalt:
    case blocks::Blackstone:
    case blocks::BoneBlock:
    case blocks::ChiseledSandstone:
    case blocks::CutSandstone:
    case blocks::SmoothSandstone:
    case blocks::OrangeTerracotta:
    case blocks::BlueTerracotta:
    case blocks::StoneBricks:
    case blocks::MossyStoneBricks:
    case blocks::CrackedStoneBricks:
    case blocks::ChiseledStoneBricks:
    case blocks::MossyCobblestone:
    case blocks::Furnace:
    case blocks::RedstoneBlock: // wiki: Block of Redstone - any pickaxe
    case blocks::Netherrack:
    case blocks::NetherQuartzOre:
    case blocks::NetherGoldOre:
    case blocks::MagmaBlock:
    case blocks::EndStone:
    case blocks::Observer: // (wiki: Observer, Hopper, Dispenser, Dropper - any pickaxe)
    case blocks::Hopper:
    case blocks::Dispenser:
    case blocks::Dropper:
    case blocks::IronDoor: // (wiki: Iron Door, Iron Trapdoor, Stone/Weighted Pressure Plates - any pickaxe)
    case blocks::IronTrapdoor:
    case blocks::StonePressurePlate:
    case blocks::LightWeightedPressurePlate:
    case blocks::HeavyWeightedPressurePlate:
    case blocks::EndStoneBricks: // (wiki: End Stone Bricks, Purpur Block, Iron Bars - any pickaxe)
    case blocks::PurpurBlock:
    case blocks::PurpurPillar:
    case blocks::IronBars:
    case blocks::Anvil: // wiki: Anvil, Enchanting Table - any pickaxe
    case blocks::ChippedAnvil:
    case blocks::DamagedAnvil:
    case blocks::EnchantingTable:
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre:
        return {T::Pickaxe, 0};
    case blocks::GoldBlock: // wiki: Block of Gold - iron pickaxe or better
        return {T::Pickaxe, 2};
    case blocks::IronBlock: // wiki: Block of Iron - stone pickaxe or better
    case blocks::IronOre:
    case blocks::DeepslateIronOre:
    case blocks::CopperOre:
    case blocks::DeepslateCopperOre:
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre:
        return {T::Pickaxe, 1};
    case blocks::GoldOre:
    case blocks::DeepslateGoldOre:
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre:
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre:
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre:
        return {T::Pickaxe, 2};
    case blocks::Obsidian:
        return {T::Pickaxe, 3}; // wiki: Obsidian - diamond pickaxe
    case blocks::Spawner:
        return {T::Pickaxe, 0}; // wiki: Monster Spawner - any pickaxe for its experience; never drops itself
    case blocks::Ice:
    case blocks::PackedIce:
    case blocks::Piston: // wiki: Piston - pickaxe is fastest, any tool drops it
    case blocks::StickyPiston:
    case blocks::PistonHead:
    case blocks::StoneButton:
    case blocks::Rail: // (wiki: rails - any tool, pickaxe fastest)
    case blocks::PoweredRail:
    case blocks::DetectorRail:
    case blocks::ActivatorRail:
    case blocks::BrewingStand: // wiki: Brewing Stand - any tool, a pickaxe is fastest
    case blocks::ShulkerBox:   // wiki: Shulker Box - any tool (M23.6)
        return {T::Pickaxe, -1};
    case blocks::OakButton:
        return {T::Axe, -1};
    // Shovel (faster, not required).
    case blocks::Dirt:
    case blocks::Podzol:
    case blocks::Mycelium:
    case blocks::DirtPath:
    case blocks::SoulSoil:
    case blocks::GrassBlock:
    case blocks::Sand:
    case blocks::RedSand:
    case blocks::Gravel:
    case blocks::Clay:
    case blocks::SoulSand:
    case blocks::CoarseDirt:
        return {T::Shovel, -1};
    case blocks::SnowBlock:
    case blocks::Snow:
        return {T::Shovel, 0}; // wiki: Snow Block - needs a shovel to drop
    // Axe.
    case blocks::OakLog:
    case blocks::BirchLog:
    case blocks::SpruceLog:
    case blocks::AcaciaLog:
    case blocks::JungleLog:
    case blocks::DarkOakLog:
    case blocks::CherryLog:
    case blocks::OakPlanks:
    case blocks::BirchPlanks:
    case blocks::SprucePlanks:
    case blocks::AcaciaPlanks:
    case blocks::JunglePlanks:
    case blocks::DarkOakPlanks:
    case blocks::CherryPlanks:
    case blocks::CraftingTable:
    case blocks::Chest:
    case blocks::Bookshelf:
    case blocks::Pumpkin:
    case blocks::CrimsonStem:
    case blocks::WarpedStem:
    case blocks::CrimsonPlanks:
    case blocks::WarpedPlanks:
    case blocks::BrownMushroomBlock:
    case blocks::RedMushroomBlock:
    case blocks::MushroomStem:
    case blocks::RedBed:
    case blocks::ChorusPlant: // (wiki: Chorus Plant, Chorus Flower)
    case blocks::ChorusFlower:
    case blocks::OakDoor: // (wiki: wooden doors, trapdoors, fences, gates, plates - axe fastest)
    case blocks::OakTrapdoor:
    case blocks::OakFence:
    case blocks::OakFenceGate:
    case blocks::OakPressurePlate:
        return {T::Axe, -1}; // (wiki: axe is faster; no tool needed)
    // Hoe (leaves, wart blocks, shroomlights).
    case blocks::NetherWartBlock:
    case blocks::WarpedWartBlock:
    case blocks::Shroomlight:
        return {T::Hoe, -1};
    case blocks::OakLeaves:
    case blocks::BirchLeaves:
    case blocks::SpruceLeaves:
    case blocks::AcaciaLeaves:
    case blocks::JungleLeaves:
    case blocks::DarkOakLeaves:
    case blocks::CherryLeaves:
    case blocks::MangroveLeaves:
    case blocks::PaleOakLeaves:
        return {T::Hoe, -1};
    default: {
        // Blocks from M23 on carry their tool in their settings; slabs, stairs and
        // walls mine like the block they're cut from.
        const BlockSettings& st = blockRegistry().block(b).settings;
        if (st.kind != BlockKind::Plain && st.base != 0) {
            HarvestInfo h = harvestInfo(st.base);
            if (h.tool == T::None) h.tool = st.tool == HarvestTool::Axe ? T::Axe : T::Pickaxe;
            return h;
        }
        switch (st.tool) {
        case HarvestTool::Pickaxe: return {T::Pickaxe, st.tier};
        case HarvestTool::Axe: return {T::Axe, -1};
        case HarvestTool::Shovel: return {T::Shovel, -1};
        case HarvestTool::Hoe: return {T::Hoe, -1};
        case HarvestTool::None: break;
        }
        return {};
    }
    }
}

bool canHarvest(BlockStateId state, const ItemStack& held) {
    const HarvestInfo h = harvestInfo(blockRegistry().blockOf(state));
    if (h.minLevel < 0) return true;
    const ItemDef& item = itemRegistry().item(held.item);
    return !held.empty() && item.tool == h.tool && tierInfo(item.tier).level >= h.minLevel;
}

int breakTicks(BlockStateId state, const ItemStack& held, bool onGround, bool eyesInWater) {
    const auto& reg = blockRegistry();
    const float hardness = reg.block(reg.blockOf(state)).settings.hardness;
    if (hardness < 0.0f) return -1; // unbreakable
    if (hardness == 0.0f) return 0; // instant (plants, torches)
    const HarvestInfo h = harvestInfo(reg.blockOf(state));
    const bool harvest = canHarvest(state, held);
    float speed = 1.0f;
    const ItemDef& item = itemRegistry().item(held.item);
    if (!held.empty() && h.tool != ToolType::None && item.tool == h.tool && harvest)
        speed = tierInfo(item.tier).speed;
    // Efficiency adds level^2 + 1 when the tool speeds this block up (wiki: Efficiency).
    if (const int eff = enchantLevel(held, Enchantment::Efficiency); eff > 0 && speed > 1.0f)
        speed += float(eff * eff + 1);
    // Swords cut bamboo at once (wiki: Bamboo), leaves and plants 1.5x faster (wiki: Sword).
    if (!held.empty() && item.tool == ToolType::Sword && reg.blockOf(state) == blocks::Bamboo) return 0;
    if (!held.empty() && item.tool == ToolType::Sword) {
        const std::string_view id = reg.block(reg.blockOf(state)).id;
        if (id.ends_with("_leaves") || reg.blockOf(state) == blocks::ShortGrass ||
            reg.blockOf(state) == blocks::Fern || reg.blockOf(state) == blocks::DeadBush)
            speed = std::max(speed, 1.5f);
    }
    if (eyesInWater) speed /= 5.0f;
    if (!onGround) speed /= 5.0f;
    const float damage = speed / hardness / (harvest ? 30.0f : 100.0f);
    if (damage >= 1.0f) return 0; // wiki: instant at damage >= 1
    return static_cast<int>(std::ceil(1.0f / damage));
}

namespace {

// Drop item ids resolved once (no name searches when blocks break).
struct DropIds {
    ItemId cobblestone, cobbledDeepslate, dirt, coal, rawIron, rawGold, rawCopper, redstone, lapis, diamond, emerald,
        flint, gravel, clay, stick, apple, quartz, seeds, wheat, carrot, potato, poisonous,
        beetroot, beetrootSeeds;
    DropIds() {
        const auto& i = itemRegistry();
        cobblestone = *i.find("cobblestone"), dirt = *i.find("dirt"), coal = *i.find("coal");
        cobbledDeepslate = *i.find("cobbled_deepslate");
        rawIron = *i.find("raw_iron"), rawGold = *i.find("raw_gold"),
        rawCopper = *i.find("raw_copper");
        redstone = *i.find("redstone"), lapis = *i.find("lapis_lazuli"),
        diamond = *i.find("diamond");
        emerald = *i.find("emerald"), flint = *i.find("flint"), gravel = *i.find("gravel");
        clay = *i.find("clay_ball"), stick = *i.find("stick"), apple = *i.find("apple");
        quartz = *i.find("quartz");
        seeds = *i.find("wheat_seeds");
        wheat = *i.find("wheat"), carrot = *i.find("carrot"), potato = *i.find("potato");
        poisonous = *i.find("poisonous_potato"), beetroot = *i.find("beetroot");
        beetrootSeeds = *i.find("beetroot_seeds");
    }
};
const DropIds& dropIds() {
    static const DropIds ids;
    return ids;
}

} // namespace

namespace {
void blockDropsPlain(BlockStateId state, Xoroshiro& rng, std::vector<ItemStack>& out);
} // namespace

ItemStack wearItem(ItemStack s, int amount, Xoroshiro& rng) {
    if (s.empty()) return s;
    const int durability = itemRegistry().item(s.item).durability;
    if (durability <= 0) return s;
    const int unbreaking = enchantLevel(s, Enchantment::Unbreaking);
    for (int i = 0; i < amount; ++i)
        if (unbreaking == 0 || rng.nextInt(uint32_t(unbreaking + 1)) == 0) ++s.damage;
    return s.damage >= durability ? ItemStack{} : s;
}

int blockExperience(BlockStateId state, Xoroshiro& rng) {
    auto between = [&](int lo, int hi) {
        return lo + static_cast<int>(rng.nextInt(uint32_t(hi - lo + 1)));
    };
    switch (blockRegistry().blockOf(state)) {
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre:
        return between(0, 2);
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre:
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre:
        return between(3, 7);
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre:
    case blocks::NetherQuartzOre:
        return between(2, 5);
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre:
        return between(1, 5);
    case blocks::Spawner:
        return between(15, 43); // wiki: Monster Spawner
    case blocks::NetherGoldOre:
        return between(0, 1);
    default:
        return 0;
    }
}

void blockDrops(BlockStateId state, const ItemStack& held, Xoroshiro& rng,
                std::vector<ItemStack>& out, bool anyTool) {
    if (!anyTool && !canHarvest(state, held)) return;
    // Silk Touch: the block itself, for blocks that otherwise drop something else
    // (wiki: Silk Touch).
    if (enchantLevel(held, Enchantment::SilkTouch) > 0) {
        const BlockId sb = blockRegistry().blockOf(state);
        if (blockRegistry().kind(sb) == BlockKind::Pane || blockRegistry().block(sb).id.ends_with("_stained_glass")) {
            out.push_back({itemRegistry().blockItem(sb), 1});
            return;
        }
        switch (blockRegistry().blockOf(state)) {
        case blocks::Stone:
        case blocks::GrassBlock:
        case blocks::Podzol:
        case blocks::Mycelium:
        case blocks::Glowstone:
        case blocks::CrimsonNylium:
        case blocks::WarpedNylium:
        case blocks::BrownMushroomBlock:
        case blocks::RedMushroomBlock:
        case blocks::MushroomStem:
        case blocks::Glass:
        case blocks::Ice:
        case blocks::PackedIce:
        case blocks::CoalOre:
        case blocks::DeepslateCoalOre:
        case blocks::DiamondOre:
        case blocks::DeepslateDiamondOre:
        case blocks::EmeraldOre:
        case blocks::DeepslateEmeraldOre:
        case blocks::LapisOre:
        case blocks::DeepslateLapisOre:
        case blocks::RedstoneOre:
        case blocks::DeepslateRedstoneOre:
        case blocks::IronOre:
        case blocks::DeepslateIronOre:
        case blocks::GoldOre:
        case blocks::DeepslateGoldOre:
        case blocks::CopperOre:
        case blocks::DeepslateCopperOre:
        case blocks::NetherQuartzOre:
        case blocks::NetherGoldOre:
        case blocks::Gravel:
        case blocks::Clay:
        case blocks::OakLeaves:
        case blocks::BirchLeaves:
        case blocks::SpruceLeaves:
        case blocks::AcaciaLeaves:
        case blocks::JungleLeaves:
        case blocks::DarkOakLeaves:
        case blocks::CherryLeaves:
        case blocks::MangroveLeaves:
        case blocks::PaleOakLeaves:
        case blocks::Deepslate:
        case blocks::Snow:
        case blocks::ShortGrass:
        case blocks::Fern:
        case blocks::EnderChest:
            if (const ItemId it = itemRegistry().blockItem(blockRegistry().blockOf(state))) {
                out.push_back({it, 1});
                return;
            }
            break;
        default:
            break;
        }
    }
    const size_t before = out.size();
    blockDropsPlain(state, rng, out);
    // Fortune: ore drops x (1 + max(0, rand(level + 2) - 1)) (wiki: Fortune).
    if (const int fortune = enchantLevel(held, Enchantment::Fortune); fortune > 0) {
        switch (blockRegistry().blockOf(state)) {
        case blocks::CoalOre:
        case blocks::DeepslateCoalOre:
        case blocks::DiamondOre:
        case blocks::DeepslateDiamondOre:
        case blocks::EmeraldOre:
        case blocks::DeepslateEmeraldOre:
        case blocks::LapisOre:
        case blocks::DeepslateLapisOre:
        case blocks::NetherQuartzOre:
        case blocks::IronOre:
        case blocks::DeepslateIronOre:
        case blocks::GoldOre:
        case blocks::DeepslateGoldOre:
        case blocks::CopperOre:
        case blocks::DeepslateCopperOre: {
            const int mult = 1 + std::max(0, int(rng.nextInt(uint32_t(fortune + 2))) - 1);
            for (size_t i = before; i < out.size(); ++i)
                out[i].count = uint8_t(std::min(64, out[i].count * mult));
            break;
        }
        case blocks::RedstoneOre: // redstone: up to `level` more (wiki: Fortune)
        case blocks::DeepslateRedstoneOre:
            for (size_t i = before; i < out.size(); ++i)
                out[i].count = uint8_t(out[i].count + rng.nextInt(uint32_t(fortune + 1)));
            break;
        default:
            break;
        }
    }
}

namespace {
void blockDropsPlain(BlockStateId state, Xoroshiro& rng, std::vector<ItemStack>& out) {
    const auto& reg = blockRegistry();
    const DropIds& d = dropIds();
    const BlockId b = reg.blockOf(state);
    auto add = [&](ItemId id, int count = 1) { out.push_back({id, static_cast<uint8_t>(count)}); };
    auto between = [&](int lo, int hi) {
        return lo + static_cast<int>(rng.nextInt(uint32_t(hi - lo + 1)));
    };
    // Glass panes and stained glass drop nothing without Silk Touch (wiki: Glass Pane).
    if (reg.kind(b) == BlockKind::Pane || reg.block(b).id.ends_with("_stained_glass")) return;
    // A double slab is two slabs (wiki: Slab).
    if (reg.kind(b) == BlockKind::Slab && reg.get(state, properties::slabType) == 2) {
        add(itemRegistry().blockItem(b), 2);
        return;
    }
    switch (reg.likeOf(b)) {
    case blocks::Stone:
        add(d.cobblestone);
        return;
    case blocks::Deepslate:
        add(d.cobbledDeepslate);
        return;
    case blocks::GrassBlock:
    case blocks::Podzol: // (wiki: Podzol, Mycelium - drop dirt without Silk Touch)
    case blocks::Mycelium:
    case blocks::DirtPath: // (wiki: Dirt Path - drops dirt)
        add(d.dirt);
        return;
    // Huge mushroom caps: 0-2 mushrooms (wiki: Mushroom Block - 77.8% none, 11.1% one, 11.1% two);
    // stems drop nothing without Silk Touch.
    case blocks::BrownMushroomBlock:
    case blocks::RedMushroomBlock:
        if (const int n = static_cast<int>(rng.nextInt(9)) - 6; n > 0) // 0/1/2 at 7/9, 1/9, 1/9
            add(itemRegistry().blockItem(b == blocks::BrownMushroomBlock ? blocks::BrownMushroom : blocks::RedMushroom), n);
        return;
    case blocks::MushroomStem:
    case blocks::Spawner:
    case blocks::NetherSprouts: // (only with shears, which don't collect yet)
        return;
    case blocks::CrimsonNylium:
    case blocks::WarpedNylium:
        add(itemRegistry().blockItem(blocks::Netherrack)); // (wiki: Nylium)
        return;
    case blocks::NetherWart: // wiki: Nether Wart - 2-4 when ripe (age 3), else 1
        add(itemRegistry().blockItem(blocks::NetherWart), blockRegistry().get(state, properties::age3) == 3 ? between(2, 4) : 1);
        return;
    case blocks::ChorusPlant: // wiki: Chorus Plant - 0-1 chorus fruit
        if (rng.nextInt(2) == 0) add(*itemRegistry().find("chorus_fruit"));
        return;
    case blocks::OakDoor:
    case blocks::IronDoor: // only the lower half drops the door (the upper goes with it)
        if (blockRegistry().get(state, properties::doorHalf) == 1) add(itemRegistry().blockItem(blockRegistry().blockOf(state)));
        return;
    case blocks::Glowstone: // wiki: Glowstone - 2-4 dust
        add(*itemRegistry().find("glowstone_dust"), between(2, 4));
        return;
    case blocks::GildedBlackstone: // wiki: 10% 2-5 gold nuggets, else itself
        if (rng.nextFloat() < 0.1f) add(*itemRegistry().find("gold_nugget"), between(2, 5));
        else add(itemRegistry().blockItem(blocks::GildedBlackstone));
        return;
    // Vines drop only with shears or Silk Touch, or 33% (wiki: Weeping Vines); the
    // plant parts drop the vine item.
    case blocks::WeepingVines:
    case blocks::WeepingVinesPlant:
    case blocks::TwistingVines:
    case blocks::TwistingVinesPlant:
        if (rng.nextFloat() < 0.33f)
            add(itemRegistry().blockItem(b == blocks::WeepingVines || b == blocks::WeepingVinesPlant ? blocks::WeepingVines
                                                                                                    : blocks::TwistingVines));
        return;
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre:
        add(d.coal);
        return;
    case blocks::IronOre:
    case blocks::DeepslateIronOre:
        add(d.rawIron);
        return;
    case blocks::GoldOre:
    case blocks::DeepslateGoldOre:
        add(d.rawGold);
        return;
    case blocks::CopperOre:
    case blocks::DeepslateCopperOre:
        add(d.rawCopper, between(2, 5));
        return;
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre:
        add(d.redstone, between(4, 5));
        return;
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre:
        add(d.lapis, between(4, 9));
        return;
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre:
        add(d.diamond);
        return;
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre:
        add(d.emerald);
        return;
    case blocks::Gravel:
        add(rng.nextFloat() < 0.1f ? d.flint : d.gravel);
        return; // wiki: 10% flint
    case blocks::NetherQuartzOre:
        add(d.quartz);
        return;
    case blocks::EnderChest: // wiki: Ender Chest - 8 obsidian without Silk Touch
        add(*itemRegistry().find("obsidian"), 8);
        return;
    case blocks::Campfire: // wiki: Campfire - 2 charcoal; soul campfires a soul soil
        add(*itemRegistry().find("charcoal"), 2);
        return;
    case blocks::SoulCampfire:
        add(*itemRegistry().find("soul_soil"));
        return;
    case blocks::NetherGoldOre: // wiki: Nether Gold Ore - 2-6 gold nuggets
        add(*itemRegistry().find("gold_nugget"), between(2, 6));
        return;
    case blocks::Clay:
        add(d.clay, 4); // wiki: Clay - 4 clay balls
        return;
    case blocks::OakLeaves:
    case blocks::BirchLeaves:
    case blocks::SpruceLeaves:
    case blocks::AcaciaLeaves:
    case blocks::JungleLeaves:
    case blocks::DarkOakLeaves:
    case blocks::CherryLeaves:
    case blocks::MangroveLeaves:
    case blocks::PaleOakLeaves:
        // wiki: Leaves - saplings 5% (jungle 2.5%; mangrove leaves drop none: propagules
        // grow under them), sticks 2% (1-2), oak and dark oak leaves also apples 0.5%.
        if (b != blocks::MangroveLeaves && rng.nextFloat() < (b == blocks::JungleLeaves ? 0.025f : 0.05f)) {
            const BlockId sapling = b == blocks::BirchLeaves     ? blocks::BirchSapling
                                    : b == blocks::SpruceLeaves  ? blocks::SpruceSapling
                                    : b == blocks::AcaciaLeaves  ? blocks::AcaciaSapling
                                    : b == blocks::JungleLeaves  ? blocks::JungleSapling
                                    : b == blocks::DarkOakLeaves ? blocks::DarkOakSapling
                                    : b == blocks::CherryLeaves  ? blocks::CherrySapling
                                    : b == blocks::PaleOakLeaves ? blocks::PaleOakSapling
                                                                 : blocks::OakSapling;
            add(itemRegistry().blockItem(sapling));
        }
        if (rng.nextFloat() < 0.02f) add(d.stick, between(1, 2));
        if ((b == blocks::OakLeaves || b == blocks::DarkOakLeaves) && rng.nextFloat() < 0.005f) add(d.apple);
        return;
    case blocks::Glass:
    case blocks::Ice:
    case blocks::PackedIce:
    case blocks::Snow:       // snowballs: not added yet
    case blocks::PistonHead: // the base drops the piston
    case blocks::Fire:
        return;
    // Crops (wiki: each crop): ripe wheat 1 wheat + 1-4 seeds; carrots and potatoes
    // 2-5 (potatoes: 2% a poisonous one); beetroots 1 + 1-4 seeds; unripe: the seed.
    case blocks::Wheat:
    case blocks::Carrots:
    case blocks::Potatoes:
    case blocks::Beetroots: {
        const bool ripe = world::BlockUpdates::cropAge(state) >= world::BlockUpdates::cropMaxAge(b);
        // Ripe crops roll 3 extra tries at 4/7 each (wiki: a binomial, 1 + B(3, 4/7)
        // seeds; carrots/potatoes 2 + B(3, 4/7)).
        auto binomial = [&] {
            int n = 0;
            for (int i = 0; i < 3; ++i)
                n += rng.nextFloat() < 4.0f / 7.0f;
            return n;
        };
        if (b == blocks::Wheat) {
            if (ripe) add(d.wheat);
            add(d.seeds, ripe ? 1 + binomial() : 1);
        } else if (b == blocks::Beetroots) {
            if (ripe) add(d.beetroot);
            add(d.beetrootSeeds, ripe ? 1 + binomial() : 1);
        } else {
            add(b == blocks::Carrots ? d.carrot : d.potato, ripe ? 2 + binomial() : 1);
            if (ripe && b == blocks::Potatoes && rng.nextFloat() < 0.02f) add(d.poisonous);
        }
        return;
    }
    case blocks::Farmland:
        add(d.dirt);
        return;
    case blocks::Bookshelf:
        add(*itemRegistry().find("book"), 3);
        return; // wiki: 3 books
    case blocks::ShortGrass:
    case blocks::Fern: // wiki: Wheat Seeds - grass and ferns drop seeds 1 in 8
        if (rng.nextInt(8) == 0) add(d.seeds);
        return;
    case blocks::DeadBush: // wiki: Dead Bush - 0-2 sticks without shears
        if (const int n = between(0, 2)) add(d.stick, n);
        return;
    default:
        if (const ItemId item = itemRegistry().blockItem(b)) add(item);
        return;
    }
}

} // namespace

} // namespace mc
