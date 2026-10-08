// Stonecutter recipes (M23.5; wiki: Stonecutter › Recipes), built once from the block
// families (slabs, stairs, walls cut from a base block) and the conversions below.
#include "gameplay/Stonecutter.h"

#include "world/Blocks.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace mc {

namespace {

struct Conversion {
    const char* from;
    const char* to;
    int count;
};
// Direct conversions (wiki: each block's Stonecutting recipe); the cut forms of each
// result follow from them.
constexpr Conversion kConversions[] = {
    {"stone", "stone_bricks", 1},
    {"stone", "chiseled_stone_bricks", 1},
    {"stone_bricks", "chiseled_stone_bricks", 1},
    {"granite", "polished_granite", 1},
    {"diorite", "polished_diorite", 1},
    {"andesite", "polished_andesite", 1},
    {"sandstone", "cut_sandstone", 1},
    {"sandstone", "chiseled_sandstone", 1},
    {"red_sandstone", "cut_red_sandstone", 1},
    {"red_sandstone", "chiseled_red_sandstone", 1},
    {"cobbled_deepslate", "polished_deepslate", 1},
    {"cobbled_deepslate", "chiseled_deepslate", 1},
    {"polished_deepslate", "deepslate_bricks", 1},
    {"deepslate_bricks", "deepslate_tiles", 1},
    {"blackstone", "polished_blackstone", 1},
    {"polished_blackstone", "polished_blackstone_bricks", 1},
    {"polished_blackstone", "chiseled_polished_blackstone", 1},
    {"tuff", "polished_tuff", 1},
    {"tuff", "chiseled_tuff", 1},
    {"polished_tuff", "tuff_bricks", 1},
    {"tuff_bricks", "chiseled_tuff_bricks", 1},
    {"end_stone", "end_stone_bricks", 1},
    {"purpur_block", "purpur_pillar", 1},
    {"quartz_block", "quartz_bricks", 1},
    {"quartz_block", "quartz_pillar", 1},
    {"quartz_block", "chiseled_quartz_block", 1},
    {"basalt", "polished_basalt", 1},
};

struct Table {
    std::vector<std::vector<world::ItemStack>> byItem;
    Table() {
        const auto& blocks = world::blockRegistry();
        const auto& items = world::itemRegistry();
        byItem.resize(items.count());
        // Families by base block, and the conversions, as item-to-item edges.
        std::vector<std::vector<std::pair<world::ItemId, int>>> edges(items.count());
        auto edge = [&](std::string_view from, std::string_view to, int count) {
            const auto a = items.find(from), b = items.find(to);
            if (a && b) edges[*a].push_back({*b, count});
        };
        for (world::BlockId b = 1; b < blocks.blockCount(); ++b) {
            const auto kind = blocks.kind(b);
            const world::BlockId base = blocks.block(b).settings.base;
            if (base == 0 || (kind != world::BlockKind::Slab && kind != world::BlockKind::Stairs &&
                              kind != world::BlockKind::Wall))
                continue;
            const std::string_view baseId = blocks.block(base).id;
            if (blocks.block(base).settings.tool == world::HarvestTool::Axe ||
                baseId.ends_with("_planks") || baseId.ends_with("bamboo_mosaic"))
                continue; // (wood isn't cut on a stonecutter)
            edge(std::string_view(blocks.block(base).id), std::string_view(blocks.block(b).id),
                 kind == world::BlockKind::Slab ? 2 : 1);
        }
        for (const Conversion& c : kConversions)
            edge(c.from, c.to, c.count);
        for (const char* stage : {"", "exposed_", "weathered_", "oxidized_"})
            for (const char* wax :
                 {"", "waxed_"}) { // (wiki: Cut Copper - 4 from a block of copper)
                const std::string p = std::string(wax) + stage;
                const std::string block = *stage ? p + "copper" : p + "copper_block";
                edge(block, p + "cut_copper", 4);
                edge(block, p + "chiseled_copper", 4);
                edge(block, p + "copper_grate", 4); // (wiki: Copper Grate)
                edge(p + "cut_copper", p + "chiseled_copper", 1);
            }
        // Everything reachable from each input, counts multiplied along the way.
        for (size_t i = 1; i < items.count(); ++i) {
            if (edges[i].empty()) continue;
            std::vector<world::ItemStack>& out = byItem[i];
            std::vector<std::pair<world::ItemId, int>> stack(edges[i].begin(), edges[i].end());
            while (!stack.empty()) {
                const auto [item, count] = stack.back();
                stack.pop_back();
                if (std::any_of(out.begin(), out.end(),
                                [&](const world::ItemStack& s) { return s.item == item; }))
                    continue;
                out.push_back({item, static_cast<uint8_t>(std::min(count, 64))});
                for (const auto& [next, n] : edges[item])
                    stack.push_back({next, count * n});
            }
            std::sort(out.begin(), out.end(),
                      [&](const world::ItemStack& a, const world::ItemStack& b) {
                          return items.item(a.item).id < items.item(b.item).id;
                      });
        }
    }
};

} // namespace

std::span<const world::ItemStack> stonecutterRecipes(world::ItemId input) {
    static const Table table;
    if (input >= table.byItem.size()) return {};
    return table.byItem[input];
}

} // namespace mc
