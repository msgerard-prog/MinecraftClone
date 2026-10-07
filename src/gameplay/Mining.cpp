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

std::vector<ItemStack> blockDrops(BlockStateId state, const ItemStack& held, Xoroshiro& rng) {
    const auto& reg = blockRegistry();
    const auto& items = itemRegistry();
    const BlockId b = reg.blockOf(state);
    if (!canHarvest(state, held)) return {};
    auto one = [&](std::string_view id, int count = 1) {
        return ItemStack{*items.find(id), static_cast<uint8_t>(count)};
    };
    auto between = [&](int lo, int hi) { return lo + static_cast<int>(rng.nextInt(uint32_t(hi - lo + 1))); };
    switch (b) {
    case blocks::Stone: return {one("cobblestone")};
    case blocks::GrassBlock: return {one("dirt")};
    case blocks::Deepslate: return {one("cobblestone")}; // vanilla: cobbled deepslate (not added yet)
    case blocks::CoalOre:
    case blocks::DeepslateCoalOre: return {one("coal")};
    case blocks::IronOre:
    case blocks::DeepslateIronOre: return {one("raw_iron")};
    case blocks::GoldOre:
    case blocks::DeepslateGoldOre: return {one("raw_gold")};
    case blocks::CopperOre:
    case blocks::DeepslateCopperOre: return {one("raw_copper", between(2, 5))};
    case blocks::RedstoneOre:
    case blocks::DeepslateRedstoneOre: return {one("redstone", between(4, 5))};
    case blocks::LapisOre:
    case blocks::DeepslateLapisOre: return {one("lapis_lazuli", between(4, 9))};
    case blocks::DiamondOre:
    case blocks::DeepslateDiamondOre: return {one("diamond")};
    case blocks::EmeraldOre:
    case blocks::DeepslateEmeraldOre: return {one("emerald")};
    case blocks::Gravel: // wiki: Gravel - 10% flint
        return {rng.nextFloat() < 0.1f ? one("flint") : one("gravel")};
    case blocks::Clay: return {one("clay")}; // vanilla: 4 clay balls (item not added yet)
    case blocks::OakLeaves:
    case blocks::BirchLeaves:
    case blocks::SpruceLeaves:
    case blocks::AcaciaLeaves: {
        // wiki: Leaves - sticks 2% (1-2), oak leaves also apples 0.5%; saplings
        // (5%) don't exist yet.
        std::vector<ItemStack> out;
        if (rng.nextFloat() < 0.02f) out.push_back(one("stick", between(1, 2)));
        if (b == blocks::OakLeaves && rng.nextFloat() < 0.005f) out.push_back(one("apple"));
        return out;
    }
    case blocks::Glass:
    case blocks::Ice:
    case blocks::ShortGrass:
    case blocks::Fern:
    case blocks::DeadBush: return {}; // need silk touch / shears (or drop seeds/sticks)
    case blocks::PackedIce: return {};
    default:
        if (const ItemId item = items.blockItem(b)) return {ItemStack{item, 1}};
        return {};
    }
}

} // namespace mc
