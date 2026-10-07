#include "gameplay/Enchanting.h"

#include "world/Blocks.h"
#include "world/Random.h"

#include <cmath>

namespace mc {

using namespace world;

int countBookshelves(const World& world, const BlockPos& t) {
    // Bookshelves exactly 2 blocks out on one axis (up to 2 on the other), at the
    // table's height or one above, with air between them and the table (wiki).
    int n = 0;
    for (int dy = 0; dy <= 1; ++dy)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
                if (std::abs(dx) != 2 && std::abs(dz) != 2) continue;
                if (blockRegistry().blockOf(world.getBlock({t.x + dx, t.y + dy, t.z + dz})) != blocks::Bookshelf) continue;
                const BlockPos between{t.x + dx / 2, t.y + dy, t.z + dz / 2}; // (the block halfway in)
                // Air or a "power transmitter" (snow layers, grass...; wiki) in between.
                const BlockId g = blockRegistry().blockOf(world.getBlock(between));
                if (g == 0 || g == blocks::Snow || g == blocks::ShortGrass || g == blocks::Fern) ++n;
            }
    return std::min(n, 15);
}

std::array<EnchantOffer, 3> enchantOffers(const ItemStack& item, int bookshelves, uint64_t seed) {
    std::array<EnchantOffer, 3> out{};
    if (item.empty() || enchantability(item.item) == 0 || isEnchanted(item)) return out;
    const int b = std::min(15, bookshelves);
    Xoroshiro rng(seed);
    // wiki: base = 1 + rand(1..8)... = random 1..8 + floor(b/2) + random 0..b.
    for (int slot = 0; slot < 3; ++slot) {
        const int base = 1 + int(rng.nextInt(8)) + b / 2 + int(rng.nextInt(uint32_t(b + 1)));
        int cost = slot == 0 ? std::max(base / 3, 1) : slot == 1 ? base * 2 / 3 + 1 : std::max(base, b * 2);
        if (cost < slot + 1) cost = 0;
        out[size_t(slot)].cost = cost;
    }
    for (int slot = 0; slot < 3; ++slot) {
        if (!out[size_t(slot)].cost) continue;
        const EnchantPick pick = pickEnchantments(item, out[size_t(slot)].cost, seed, slot);
        if (pick.count == 0) {
            out[size_t(slot)].cost = 0;
            continue;
        }
        out[size_t(slot)].hint = pick.list[0].first;
        out[size_t(slot)].hintLevel = pick.list[0].second;
    }
    return out;
}

EnchantPick pickEnchantments(const ItemStack& item, int cost, uint64_t seed, int slot) {
    EnchantPick out;
    Xoroshiro rng(seed * 31 + uint64_t(slot) + 1);
    const int ench = enchantability(item.item);
    if (ench <= 0) return out;
    // Modified level: cost + 1 + 2 x rand(ench/4 + 1), then +-15% (wiki).
    int level = cost + 1 + int(rng.nextInt(uint32_t(ench / 4 + 1))) + int(rng.nextInt(uint32_t(ench / 4 + 1)));
    const float bonus = 1.0f + (rng.nextFloat() + rng.nextFloat() - 1.0f) * 0.15f;
    level = std::max(1, int(std::lround(float(level) * bonus)));
    // Candidates: each enchantment that fits, at its highest level whose range holds `level`.
    std::array<std::pair<Enchantment, int>, size_t(Enchantment::Count)> cands{};
    int nc = 0;
    auto build = [&] {
        nc = 0;
        for (int e = 1; e < int(Enchantment::Count); ++e) {
            const Enchantment en = Enchantment(e);
            if (!canEnchant(item.item, en)) continue;
            const EnchantmentInfo& info = enchantmentInfo(en);
            for (int l = info.maxLevel; l >= 1; --l) {
                const int lo = info.minBase + info.minPerLevel * (l - 1);
                const int hi = l == info.maxLevel ? info.topMax : lo + info.maxSpan;
                if (level >= lo && level <= hi) {
                    cands[size_t(nc++)] = {en, l};
                    break;
                }
            }
        }
    };
    auto pickOne = [&]() -> int {
        int total = 0;
        for (int i = 0; i < nc; ++i)
            total += enchantmentInfo(cands[size_t(i)].first).weight;
        if (total <= 0) return -1;
        int r = int(rng.nextInt(uint32_t(total)));
        for (int i = 0; i < nc; ++i) {
            r -= enchantmentInfo(cands[size_t(i)].first).weight;
            if (r < 0) return i;
        }
        return nc - 1;
    };
    build();
    int i = pickOne();
    if (i < 0) return out;
    out.list[size_t(out.count++)] = cands[size_t(i)];
    // More with chance (level + 1) / 50, halving the level each time (wiki).
    while (out.count < 4 && int(rng.nextInt(50)) <= level) {
        // Drop candidates incompatible with what was picked (same enchantment or group).
        int w = 0;
        for (int k = 0; k < nc; ++k) {
            bool ok = true;
            for (int j = 0; j < out.count; ++j) {
                const auto& got = out.list[size_t(j)].first;
                const uint8_t g = enchantmentInfo(got).group;
                if (cands[size_t(k)].first == got || (g && enchantmentInfo(cands[size_t(k)].first).group == g)) ok = false;
            }
            if (ok) cands[size_t(w++)] = cands[size_t(k)];
        }
        nc = w;
        if (nc == 0) break;
        i = pickOne();
        if (i < 0) break;
        out.list[size_t(out.count++)] = cands[size_t(i)];
        level /= 2;
    }
    // Books from the table lose one random enchantment when several were picked (wiki).
    if (itemRegistry().item(item.item).id == "minecraft:book" && out.count > 1) {
        const int drop = int(rng.nextInt(uint32_t(out.count)));
        for (int k = drop; k + 1 < out.count; ++k)
            out.list[size_t(k)] = out.list[size_t(k + 1)];
        --out.count;
    }
    return out;
}

ItemStack applyEnchantments(const ItemStack& item, const EnchantPick& pick) {
    ItemStack s = item;
    if (itemRegistry().item(item.item).id == "minecraft:book") s = {*itemRegistry().find("enchanted_book"), 1};
    for (int i = 0; i < pick.count; ++i)
        setEnchantment(s, pick.list[size_t(i)].first, pick.list[size_t(i)].second);
    return s;
}

} // namespace mc
