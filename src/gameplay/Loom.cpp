#include "gameplay/Loom.h"

#include "world/Blocks.h"
#include "world/ItemExtras.h"

namespace mc {

using namespace world;

int loomPatterns(const ItemStack& patternItem, std::array<int, 48>& out) {
    const std::string_view held = patternItem.empty()
                                      ? std::string_view()
                                      : std::string_view(itemRegistry().item(patternItem.item).id);
    int n = 0;
    for (size_t i = 0; i < kBannerPatterns.size() && n < int(out.size()); ++i) {
        const BannerPattern& p = kBannerPatterns[i];
        // With a pattern item: only its pattern (vanilla); without: the plain ones.
        if (held.empty() ? p.item.empty() : (!p.item.empty() && held.substr(10) == p.item))
            out[size_t(n++)] = int(i);
    }
    return n;
}

int dyeColour(ItemId item) {
    std::string_view id =
        itemRegistry().item(item).id; // "minecraft:<colour>_dye" (no strings built)
    if (!id.starts_with("minecraft:") || !id.ends_with("_dye")) return -1;
    id = id.substr(10, id.size() - 14);
    for (int c = 0; c < 16; ++c)
        if (id == kDyeColours[c]) return c;
    return -1;
}

ItemStack loomResult(const ItemStack& banner, const ItemStack& dye, int pattern) {
    if (banner.empty() || dye.empty() || pattern < 0 || pattern >= int(kBannerPatterns.size()))
        return {};
    if (blockRegistry().kind(itemRegistry().item(banner.item).block) != BlockKind::Banner)
        return {};
    const int colour = dyeColour(dye.item);
    if (colour < 0) return {};
    BannerLayers layers = bannerLayers(banner.extra).value_or(BannerLayers{});
    if (layers.count >= kMaxLoomLayers) return {}; // (wiki: up to 6 patterns)
    layers.pattern[layers.count] = uint8_t(pattern);
    layers.colour[layers.count] = uint8_t(colour);
    ++layers.count;
    ItemStack out = banner;
    out.count = 1;
    out.extra = addBannerLayers(layers);
    return out;
}

} // namespace mc
