#include "gameplay/Buckets.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

namespace mc {

using namespace world;

std::optional<BucketResult> useBucket(World& world, ItemId held, const glm::dvec3& eye,
                                      const glm::dvec3& look, double reach,
                                      std::vector<BlockPos>& changed) {
    const auto& items = itemRegistry();
    const auto& reg = blockRegistry();
    const std::string_view id = items.item(held).id;
    if (id == "minecraft:bucket") {
        // Fill from the first fluid source in view (vanilla: ClipContext SOURCE_ONLY).
        const auto hit = raycastBlocks(world, eye, look, reach, RayFluids::Sources);
        if (!hit) return std::nullopt;
        const BlockId b = reg.blockOf(world.getBlock(hit->block));
        if (b != blocks::Water && b != blocks::Lava && b != blocks::PowderSnow && b != blocks::BubbleColumn)
            return std::nullopt;
        world.updateBlock(hit->block, 0);
        changed.push_back(hit->block);
        return BucketResult{*items.find(b == blocks::Water || b == blocks::BubbleColumn ? "water_bucket"
                                        : b == blocks::PowderSnow ? "powder_snow_bucket" // (M29.4c)
                                                                  : "lava_bucket")};
    }
    if (id == "minecraft:powder_snow_bucket") { // (M29.4c) a block against the clicked face
        const auto hit = raycastBlocks(world, eye, look, reach);
        if (!hit) return std::nullopt;
        BlockPos at = hit->block;
        if (!BlockUpdates::breaksInFluid(reg.blockOf(world.getBlock(at)))) at = neighbour(hit->block, hit->face);
        if (!world.isInHeight(at.y) || !world.chunk(at.chunk())) return std::nullopt;
        const BlockStateId there = world.getBlock(at);
        if (there != 0 && !BlockUpdates::isFluid(reg.blockOf(there)) && !BlockUpdates::breaksInFluid(reg.blockOf(there)))
            return std::nullopt;
        world.updateBlock(at, reg.defaultState(blocks::PowderSnow));
        changed.push_back(at);
        BucketResult result{*items.find("bucket")};
        result.at = at;
        return result;
    }
    const MobType fish = bucketFish(held);
    const bool water = id == "minecraft:water_bucket" || fish != MobType::Count,
               lava = id == "minecraft:lava_bucket";
    if (!water && !lava) return std::nullopt;
    const auto hit = raycastBlocks(world, eye, look, reach);
    if (!hit) return std::nullopt;
    // Into the clicked block if fluid can replace it (a plant, other fluid), else next
    // to it across the clicked face.
    BlockPos at = hit->block;
    const BlockId clicked = reg.blockOf(world.getBlock(at));
    if (!BlockUpdates::breaksInFluid(clicked)) at = neighbour(hit->block, hit->face);
    const BlockStateId there = world.getBlock(at);
    if (!world.isInHeight(at.y) || !world.chunk(at.chunk())) return std::nullopt;
    const BlockId t = reg.blockOf(there);
    if (there != 0 && !BlockUpdates::isFluid(t) && !BlockUpdates::breaksInFluid(t))
        return std::nullopt;
    const ItemId empty = *items.find("bucket");
    if (water && world.isUltrawarm())
        return BucketResult{empty}; // evaporates (wiki: Water › Nether)
    BucketResult result{empty};
    // Water washes the block away with its drop; lava burns it (as flowing fluids do).
    if (water && BlockUpdates::breaksInFluid(t))
        if (const ItemId item = items.blockItem(t)) result.washed = {item, 1};
    world.updateBlock(at, reg.defaultState(water ? blocks::Water : blocks::Lava));
    changed.push_back(at);
    result.fish = fish; // (a fish bucket lets its fish out with the water)
    result.at = at;
    return result;
}

// Fish buckets (M25.2; wiki: Bucket of Fish): a water bucket used on a fish takes it
// with its water; emptied, the bucket places the water and lets the fish out.
static constexpr std::pair<MobType, const char*> kFishBuckets[] = {
    {MobType::Cod, "cod_bucket"},
    {MobType::Salmon, "salmon_bucket"},
    {MobType::TropicalFish, "tropical_fish_bucket"},
    {MobType::Pufferfish, "pufferfish_bucket"},
    {MobType::Axolotl, "axolotl_bucket"}, // (M26.3c)
    {MobType::Tadpole, "tadpole_bucket"}};

MobType bucketFish(ItemId item) {
    for (const auto& [fish, name] : kFishBuckets)
        if (itemRegistry().find(name) == std::optional<ItemId>(item)) return fish;
    return MobType::Count;
}

ItemId fishBucketFor(MobType fish) {
    for (const auto& [f, name] : kFishBuckets)
        if (f == fish) return itemRegistry().find(name).value_or(0);
    return 0;
}

ItemStack applyBucket(Inventory& inventory, ItemId filled, bool survival) {
    const ItemStack held = inventory.selectedStack();
    const ItemStack result{filled, 1};
    if (!survival) {
        if (itemRegistry().item(held.item).id != "minecraft:bucket") return {};
        for (int sl = 0; sl < Inventory::kSlots; ++sl)
            if (inventory.slot(sl).item == filled) return {};
        const int left = inventory.add(result);
        return left > 0 ? result : ItemStack{};
    }
    if (held.count == 1) {
        inventory.setSlot(inventory.selected(), result);
        return {};
    }
    inventory.consumeSelected(1); // a stack of buckets: one is used
    return inventory.add(result) > 0 ? result : ItemStack{};
}

} // namespace mc
