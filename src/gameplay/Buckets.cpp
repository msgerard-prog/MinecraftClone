#include "gameplay/Buckets.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

namespace mc {

using namespace world;

std::optional<BucketResult> useBucket(World& world, ItemId held, const glm::dvec3& eye, const glm::dvec3& look,
                                      double reach, std::vector<BlockPos>& changed) {
    const auto& items = itemRegistry();
    const auto& reg = blockRegistry();
    const std::string_view id = items.item(held).id;
    if (id == "minecraft:bucket") {
        // Fill from the first fluid source in view (vanilla: ClipContext SOURCE_ONLY).
        const auto hit = raycastBlocks(world, eye, look, reach, RayFluids::Sources);
        if (!hit) return std::nullopt;
        const BlockId b = reg.blockOf(world.getBlock(hit->block));
        if (b != blocks::Water && b != blocks::Lava) return std::nullopt;
        world.updateBlock(hit->block, 0);
        changed.push_back(hit->block);
        return BucketResult{*items.find(b == blocks::Water ? "water_bucket" : "lava_bucket")};
    }
    const bool water = id == "minecraft:water_bucket", lava = id == "minecraft:lava_bucket";
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
    if (there != 0 && !BlockUpdates::isFluid(t) && !BlockUpdates::breaksInFluid(t)) return std::nullopt;
    const ItemId empty = *items.find("bucket");
    if (water && world.isUltrawarm()) return BucketResult{empty}; // evaporates (wiki: Water › Nether)
    BucketResult result{empty};
    // Water washes the block away with its drop; lava burns it (as flowing fluids do).
    if (water && BlockUpdates::breaksInFluid(t))
        if (const ItemId item = items.blockItem(t)) result.washed = {item, 1};
    world.updateBlock(at, reg.defaultState(water ? blocks::Water : blocks::Lava));
    changed.push_back(at);
    return result;
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
