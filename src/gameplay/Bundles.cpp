#include "gameplay/Bundles.h"

#include <algorithm>

namespace mc {

using namespace world;

bool isBundle(ItemId item) {
    const std::string_view id = itemRegistry().item(item).id;
    return id == "minecraft:bundle" || (id.starts_with("minecraft:") && id.ends_with("_bundle"));
}

int bundleItemWeight(const ItemStack& stack) {
    if (stack.empty()) return 0;
    if (isBundle(stack.item)) return 4 + bundleWeight(stack);
    if (stack.contents != 0) return 0; // (a shulker box with items: never)
    const int max = std::max<int>(1, itemRegistry().item(stack.item).maxStack);
    return 64 / max;
}

int bundleWeight(const ItemStack& bundle) {
    if (bundle.contents == 0) return 0;
    int w = 0;
    for (const ItemStack& s : itemContents(bundle.contents))
        if (!s.empty()) w += bundleItemWeight(s) * s.count;
    return w;
}

int addToBundle(ItemStack& bundle, const ItemStack& in) {
    const int each = bundleItemWeight(in);
    if (each <= 0 || in.empty()) return 0;
    const int room = 64 - bundleWeight(bundle);
    const int n = std::min<int>(in.count, room / each);
    if (n <= 0) return 0;
    ItemContents slots = itemContents(bundle.contents);
    // Onto the same kind already last in, else a new stack at the end (the next free slot).
    int last = -1;
    for (int i = 0; i < int(slots.size()); ++i)
        if (!slots[size_t(i)].empty()) last = i;
    ItemStack add = in;
    add.count = uint8_t(n);
    if (last >= 0 && slots[size_t(last)].sameKind(add) &&
        slots[size_t(last)].count + n <= itemRegistry().item(add.item).maxStack) {
        slots[size_t(last)].count = uint8_t(slots[size_t(last)].count + n);
    } else {
        if (last + 1 >= int(slots.size())) return 0;
        slots[size_t(last + 1)] = add;
    }
    bundle.contents = addItemContents(slots);
    return n;
}

ItemStack takeFromBundle(ItemStack& bundle) {
    if (bundle.contents == 0) return {};
    ItemContents slots = itemContents(bundle.contents);
    for (int i = int(slots.size()) - 1; i >= 0; --i)
        if (!slots[size_t(i)].empty()) {
            const ItemStack out = slots[size_t(i)];
            slots[size_t(i)] = {};
            bundle.contents = addItemContents(slots);
            return out;
        }
    return {};
}

} // namespace mc
