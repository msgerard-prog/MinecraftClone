#include "gameplay/Mining.h"

#include "world/Blocks.h"

#include <cmath>
#include <string_view>

namespace mc {

using namespace world;

HarvestInfo harvestInfo(BlockId b) {
    using T = ToolType;
    switch (b) {
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
    case blocks::MossyCobblestone:
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre: return {T::Pickaxe, 0};
    case blocks::IronOre:
    case blocks::DeepslateIronOre:
    case blocks::CopperOre:
    case blocks::DeepslateCopperOre:
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre: return {T::Pickaxe, 1};
    case blocks::GoldOre:
    case blocks::DeepslateGoldOre:
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre:
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre:
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre: return {T::Pickaxe, 2};
    case blocks::Ice:
    case blocks::PackedIce: return {T::Pickaxe, -1};
    // Shovel (faster, not required).
    case blocks::Dirt:
    case blocks::GrassBlock:
    case blocks::Sand:
    case blocks::RedSand:
    case blocks::Gravel:
    case blocks::Clay:
    case blocks::CoarseDirt:
    case blocks::SnowBlock: return {T::Shovel, -1};
    // Axe.
    case blocks::OakLog:
    case blocks::BirchLog:
    case blocks::SpruceLog:
    case blocks::AcaciaLog:
    case blocks::OakPlanks:
    case blocks::BirchPlanks:
    case blocks::SprucePlanks:
    case blocks::AcaciaPlanks: return {T::Axe, -1};
    // Hoe (leaves).
    case blocks::OakLeaves:
    case blocks::BirchLeaves:
    case blocks::SpruceLeaves:
    case blocks::AcaciaLeaves: return {T::Hoe, -1};
    default: return {};
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
    if (hardness < 0.0f) return -1;  // unbreakable
    if (hardness == 0.0f) return 0;  // instant (plants, torches)
    const HarvestInfo h = harvestInfo(reg.blockOf(state));
    const bool harvest = canHarvest(state, held);
    float speed = 1.0f;
    const ItemDef& item = itemRegistry().item(held.item);
    if (!held.empty() && h.tool != ToolType::None && item.tool == h.tool && harvest)
        speed = tierInfo(item.tier).speed;
    if (!held.empty() && item.tool == ToolType::Sword && reg.blockOf(state) != blocks::Air)
        speed = std::max(speed, 1.5f); // swords cut everything a little faster
    if (eyesInWater) speed /= 5.0f;
    if (!onGround) speed /= 5.0f;
    const float damage = speed / hardness / (harvest ? 30.0f : 100.0f);
    if (damage > 1.0f) return 0;
    return static_cast<int>(std::ceil(1.0f / damage));
}

namespace {

// Drop item ids resolved once (no name searches when blocks break).
struct DropIds {
    ItemId cobblestone, dirt, coal, rawIron, rawGold, rawCopper, redstone, lapis, diamond, emerald,
        flint, gravel, clay, stick, apple;
    DropIds() {
        const auto& i = itemRegistry();
        cobblestone = *i.find("cobblestone"), dirt = *i.find("dirt"), coal = *i.find("coal");
        rawIron = *i.find("raw_iron"), rawGold = *i.find("raw_gold"), rawCopper = *i.find("raw_copper");
        redstone = *i.find("redstone"), lapis = *i.find("lapis_lazuli"), diamond = *i.find("diamond");
        emerald = *i.find("emerald"), flint = *i.find("flint"), gravel = *i.find("gravel");
        clay = *i.find("clay"), stick = *i.find("stick"), apple = *i.find("apple");
    }
};
const DropIds& dropIds() {
    static const DropIds ids;
    return ids;
}

} // namespace

void blockDrops(BlockStateId state, const ItemStack& held, Xoroshiro& rng, std::vector<ItemStack>& out) {
    if (!canHarvest(state, held)) return;
    const auto& reg = blockRegistry();
    const DropIds& d = dropIds();
    const BlockId b = reg.blockOf(state);
    auto add = [&](ItemId id, int count = 1) { out.push_back({id, static_cast<uint8_t>(count)}); };
    auto between = [&](int lo, int hi) { return lo + static_cast<int>(rng.nextInt(uint32_t(hi - lo + 1))); };
    switch (b) {
    case blocks::Stone:
    case blocks::Deepslate: add(d.cobblestone); return; // vanilla deepslate: cobbled deepslate
    case blocks::GrassBlock: add(d.dirt); return;
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre: add(d.coal); return;
    case blocks::IronOre:
    case blocks::DeepslateIronOre: add(d.rawIron); return;
    case blocks::GoldOre:
    case blocks::DeepslateGoldOre: add(d.rawGold); return;
    case blocks::CopperOre:
    case blocks::DeepslateCopperOre: add(d.rawCopper, between(2, 5)); return;
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre: add(d.redstone, between(4, 5)); return;
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre: add(d.lapis, between(4, 9)); return;
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre: add(d.diamond); return;
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre: add(d.emerald); return;
    case blocks::Gravel: add(rng.nextFloat() < 0.1f ? d.flint : d.gravel); return; // wiki: 10% flint
    case blocks::Clay: add(d.clay); return; // vanilla: 4 clay balls (item not added yet)
    case blocks::OakLeaves:
    case blocks::BirchLeaves:
    case blocks::SpruceLeaves:
    case blocks::AcaciaLeaves:
        // wiki: Leaves - sticks 2% (1-2), oak leaves also apples 0.5%; saplings (5%)
        // don't exist yet.
        if (rng.nextFloat() < 0.02f) add(d.stick, between(1, 2));
        if (b == blocks::OakLeaves && rng.nextFloat() < 0.005f) add(d.apple);
        return;
    case blocks::Glass:
    case blocks::Ice:
    case blocks::PackedIce:
    case blocks::ShortGrass:
    case blocks::Fern:
    case blocks::DeadBush:
    case blocks::Snow: return; // silk touch / shears (or seeds, sticks, snowballs)
    default:
        if (const ItemId item = itemRegistry().blockItem(b)) add(item);
        return;
    }
}

} // namespace mc
