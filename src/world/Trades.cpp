// Villager trade tables (M24.2; wiki: Trading › Java Edition offers - authored by hand
// from the wiki's tables; maps, banners, potions, stews and tipped arrows left out).
#include "world/Trades.h"

#include "world/Enchantments.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

namespace mc::world {

namespace {

// A trade template: buy (one or two stacks) -> sell. kind 1: the sold item gets a
// random enchantment; kind 2: an enchanted book (its price set by the enchantment).
struct T {
    std::string_view buy;
    int buyCount;
    std::string_view buyB;
    int buyBCount;
    std::string_view sell;
    int sellCount;
    int maxUses;
    int xp;
    float mult; // price multiplier (0.05 most, 0.2 tools/armor)
    int kind = 0;
};
// Common shapes: N of an item for an emerald, or emeralds for a sold item.
constexpr T buy(std::string_view item, int count, int uses, int xp) {
    return {item, count, "", 0, "emerald", 1, uses, xp, 0.05f};
}
constexpr T sell(int price, std::string_view item, int count, int uses, int xp, float mult = 0.05f,
                 int kind = 0) {
    return {"emerald", price, "", 0, item, count, uses, xp, mult, kind};
}

// Pools [profession][level 1..5].
constexpr T kFarmer1[] = {buy("wheat", 20, 16, 2), buy("potato", 26, 16, 2),
                          buy("carrot", 22, 16, 2), buy("beetroot", 15, 16, 2),
                          sell(1, "bread", 6, 16, 1)};
constexpr T kFarmer2[] = {buy("pumpkin", 6, 12, 10), sell(1, "pumpkin_pie", 4, 12, 5),
                          sell(1, "apple", 4, 16, 5)};
constexpr T kFarmer3[] = {sell(3, "cookie", 18, 12, 10), buy("melon", 4, 12, 20)};
constexpr T kFarmer4[] = {sell(1, "cake", 1, 12, 15)};
constexpr T kFarmer5[] = {sell(3, "golden_carrot", 3, 12, 30),
                          sell(4, "glistering_melon_slice", 3, 12, 30)};

constexpr T kArmorer1[] = {buy("coal", 15, 16, 2), sell(7, "iron_leggings", 1, 12, 1, 0.2f),
                           sell(4, "iron_boots", 1, 12, 1, 0.2f),
                           sell(5, "iron_helmet", 1, 12, 1, 0.2f),
                           sell(9, "iron_chestplate", 1, 12, 1, 0.2f)};
constexpr T kArmorer2[] = {buy("iron_ingot", 4, 12, 10), sell(36, "bell", 1, 12, 5, 0.2f)};
constexpr T kArmorer3[] = {buy("lava_bucket", 1, 12, 20), buy("diamond", 1, 12, 20),
                           sell(5, "shield", 1, 12, 10, 0.2f)};
constexpr T kArmorer4[] = {sell(19, "diamond_leggings", 1, 3, 15, 0.2f, 1),
                           sell(13, "diamond_boots", 1, 3, 15, 0.2f, 1)};
constexpr T kArmorer5[] = {sell(13, "diamond_helmet", 1, 3, 30, 0.2f, 1),
                           sell(21, "diamond_chestplate", 1, 3, 30, 0.2f, 1)};

constexpr T kButcher1[] = {buy("chicken", 14, 16, 2), buy("porkchop", 7, 16, 2)};
constexpr T kButcher2[] = {buy("coal", 15, 16, 2), sell(1, "cooked_porkchop", 5, 16, 5),
                           sell(1, "cooked_chicken", 8, 16, 5)};
constexpr T kButcher3[] = {buy("mutton", 7, 16, 20), buy("beef", 10, 16, 20)};
constexpr T kButcher4[] = {buy("dried_kelp_block", 10, 12, 30)};
constexpr T kButcher5[] = {buy("sweet_berries", 10, 12, 30)};

constexpr T kCartographer1[] = {buy("paper", 24, 16, 2), sell(7, "map", 1, 12, 1)};
constexpr T kCartographer2[] = {buy("glass_pane", 11, 16, 10)};
constexpr T kCartographer3[] = {buy("compass", 1, 12, 20)};
constexpr T kCartographer4[] = {sell(7, "item_frame", 1, 12, 15)};
constexpr T kCartographer5[] = {sell(8, "globe_banner_pattern", 1, 12, 30)};

constexpr T kCleric1[] = {buy("rotten_flesh", 32, 16, 2), sell(1, "redstone", 2, 12, 1)};
constexpr T kCleric2[] = {buy("gold_ingot", 3, 12, 10), sell(1, "lapis_lazuli", 1, 12, 5)};
constexpr T kCleric3[] = {buy("rabbit_foot", 2, 12, 20), sell(4, "glowstone", 1, 12, 10)};
constexpr T kCleric4[] = {buy("glass_bottle", 9, 12, 30), sell(5, "ender_pearl", 1, 12, 15)};
constexpr T kCleric5[] = {buy("nether_wart", 22, 12, 30), sell(3, "experience_bottle", 1, 12, 30)};

constexpr T kFisherman1[] = {buy("string", 20, 16, 2), buy("coal", 10, 16, 2)};
constexpr T kFisherman2[] = {buy("cod", 15, 16, 10), sell(2, "campfire", 1, 12, 5)};
constexpr T kFisherman3[] = {buy("salmon", 13, 16, 20), sell(3, "fishing_rod", 1, 3, 10, 0.2f, 1)};
constexpr T kFisherman4[] = {buy("tropical_fish", 6, 12, 30)};
constexpr T kFisherman5[] = {buy("pufferfish", 4, 12, 30), buy("oak_boat", 1, 12, 30)};

constexpr T kFletcher1[] = {buy("stick", 32, 16, 2),
                            sell(1, "arrow", 16, 12, 1),
                            {"emerald", 1, "gravel", 10, "flint", 10, 12, 1, 0.05f}};
constexpr T kFletcher2[] = {buy("flint", 26, 12, 10), sell(2, "bow", 1, 12, 5, 0.2f)};
constexpr T kFletcher3[] = {buy("string", 14, 16, 20), sell(3, "crossbow", 1, 12, 10, 0.2f)};
constexpr T kFletcher4[] = {buy("feather", 24, 16, 30), sell(7, "bow", 1, 3, 15, 0.2f, 1)};
constexpr T kFletcher5[] = {buy("tripwire_hook", 8, 12, 30),
                            sell(8, "crossbow", 1, 3, 15, 0.2f, 1)};

constexpr T kLeatherworker1[] = {buy("leather", 6, 16, 2),
                                 sell(3, "leather_leggings", 1, 12, 1, 0.2f),
                                 sell(7, "leather_chestplate", 1, 12, 1, 0.2f)};
constexpr T kLeatherworker2[] = {buy("flint", 26, 12, 10),
                                 sell(5, "leather_helmet", 1, 12, 5, 0.2f),
                                 sell(4, "leather_boots", 1, 12, 5, 0.2f)};
constexpr T kLeatherworker3[] = {buy("rabbit_hide", 9, 12, 20),
                                 sell(7, "leather_chestplate", 1, 12, 10, 0.2f)};
constexpr T kLeatherworker4[] = {buy("turtle_scute", 4, 12, 30),
                                 sell(6, "leather_horse_armor", 1, 12, 15, 0.2f)};
constexpr T kLeatherworker5[] = {sell(6, "saddle", 1, 12, 30, 0.2f),
                                 sell(5, "leather_helmet", 1, 12, 30, 0.2f)};

constexpr T kLibrarian1[] = {buy("paper", 24, 16, 2),
                             sell(9, "bookshelf", 1, 12, 1),
                             {"emerald", 1, "book", 1, "enchanted_book", 1, 12, 1, 0.2f, 2}};
constexpr T kLibrarian2[] = {buy("book", 4, 12, 10),
                             sell(1, "lantern", 1, 12, 5),
                             {"emerald", 1, "book", 1, "enchanted_book", 1, 12, 5, 0.2f, 2}};
constexpr T kLibrarian3[] = {buy("ink_sac", 5, 12, 20),
                             sell(1, "glass", 4, 12, 10),
                             {"emerald", 1, "book", 1, "enchanted_book", 1, 12, 10, 0.2f, 2}};
constexpr T kLibrarian4[] = {buy("writable_book", 2, 12, 30),
                             sell(5, "clock", 1, 12, 15),
                             sell(4, "compass", 1, 12, 15),
                             {"emerald", 1, "book", 1, "enchanted_book", 1, 12, 15, 0.2f, 2}};
// (M33 review; 26.1: name tags became craftable and left the master librarian, who sells
// candles in their place)
constexpr T kLibrarian5[] = {sell(3, "red_candle", 1, 12, 30), sell(3, "yellow_candle", 1, 12, 30)};

constexpr T kMason1[] = {buy("clay_ball", 10, 16, 2), sell(1, "brick", 10, 16, 1)};
constexpr T kMason2[] = {buy("stone", 20, 16, 10), sell(1, "chiseled_stone_bricks", 4, 16, 5)};
constexpr T kMason3[] = {buy("granite", 16, 16, 20),
                         buy("andesite", 16, 16, 20),
                         buy("diorite", 16, 16, 20),
                         sell(1, "polished_andesite", 4, 16, 10),
                         sell(1, "polished_diorite", 4, 16, 10),
                         sell(1, "polished_granite", 4, 16, 10)};
constexpr T kMason4[] = {buy("quartz", 12, 12, 30), sell(1, "white_terracotta", 1, 12, 15),
                         sell(1, "white_glazed_terracotta", 1, 12, 15)};
constexpr T kMason5[] = {sell(1, "quartz_pillar", 1, 12, 30), sell(1, "quartz_block", 1, 12, 30)};

constexpr T kShepherd1[] = {buy("white_wool", 18, 16, 2), buy("brown_wool", 18, 16, 2),
                            buy("black_wool", 18, 16, 2), buy("gray_wool", 18, 16, 2),
                            sell(2, "shears", 1, 12, 1)};
constexpr T kShepherd2[] = {buy("black_dye", 12, 16, 10),     buy("gray_dye", 12, 16, 10),
                            buy("lime_dye", 12, 16, 10),      buy("light_blue_dye", 12, 16, 10),
                            buy("white_dye", 12, 16, 10),     sell(1, "white_wool", 1, 16, 5),
                            sell(1, "white_carpet", 4, 16, 5)};
constexpr T kShepherd3[] = {buy("yellow_dye", 12, 16, 20), buy("light_gray_dye", 12, 16, 20),
                            buy("orange_dye", 12, 16, 20), buy("red_dye", 12, 16, 20),
                            buy("pink_dye", 12, 16, 20),   sell(3, "red_bed", 1, 12, 10)};
constexpr T kShepherd4[] = {buy("brown_dye", 12, 16, 30),      buy("purple_dye", 12, 16, 30),
                            buy("blue_dye", 12, 16, 30),       buy("green_dye", 12, 16, 30),
                            buy("magenta_dye", 12, 16, 30),    buy("cyan_dye", 12, 16, 30),
                            sell(3, "white_banner", 1, 12, 15)};
constexpr T kShepherd5[] = {sell(2, "painting", 3, 12, 30)};

constexpr T kToolsmith1[] = {buy("coal", 15, 16, 2), sell(1, "stone_axe", 1, 12, 1, 0.2f),
                             sell(1, "stone_shovel", 1, 12, 1, 0.2f),
                             sell(1, "stone_pickaxe", 1, 12, 1, 0.2f),
                             sell(1, "stone_hoe", 1, 12, 1, 0.2f)};
constexpr T kToolsmith2[] = {buy("iron_ingot", 4, 12, 10), sell(36, "bell", 1, 12, 5, 0.2f)};
constexpr T kToolsmith3[] = {buy("flint", 30, 12, 20), sell(1, "iron_axe", 1, 3, 10, 0.2f, 1),
                             sell(2, "iron_shovel", 1, 3, 10, 0.2f, 1),
                             sell(3, "iron_pickaxe", 1, 3, 10, 0.2f, 1),
                             sell(4, "diamond_hoe", 1, 3, 10, 0.2f)};
constexpr T kToolsmith4[] = {buy("diamond", 1, 12, 30), sell(12, "diamond_axe", 1, 3, 15, 0.2f, 1),
                             sell(5, "diamond_shovel", 1, 3, 15, 0.2f, 1)};
constexpr T kToolsmith5[] = {sell(13, "diamond_pickaxe", 1, 3, 30, 0.2f, 1)};

constexpr T kWeaponsmith1[] = {buy("coal", 15, 16, 2), sell(3, "iron_axe", 1, 12, 1, 0.2f),
                               sell(2, "iron_sword", 1, 3, 1, 0.2f, 1)};
constexpr T kWeaponsmith2[] = {buy("iron_ingot", 4, 12, 10), sell(36, "bell", 1, 12, 5, 0.2f)};
constexpr T kWeaponsmith3[] = {buy("flint", 24, 12, 20)};
constexpr T kWeaponsmith4[] = {buy("diamond", 1, 12, 30),
                               sell(12, "diamond_axe", 1, 3, 15, 0.2f, 1)};
constexpr T kWeaponsmith5[] = {sell(8, "diamond_sword", 1, 3, 30, 0.2f, 1)};

struct Pools {
    std::span<const T> level[5];
};
const Pools& poolsFor(Profession p) {
    static const Pools kNone{};
    static const Pools kAll[] = {
        kNone,
        {{kArmorer1, kArmorer2, kArmorer3, kArmorer4, kArmorer5}},
        {{kButcher1, kButcher2, kButcher3, kButcher4, kButcher5}},
        {{kCartographer1, kCartographer2, kCartographer3, kCartographer4, kCartographer5}},
        {{kCleric1, kCleric2, kCleric3, kCleric4, kCleric5}},
        {{kFarmer1, kFarmer2, kFarmer3, kFarmer4, kFarmer5}},
        {{kFisherman1, kFisherman2, kFisherman3, kFisherman4, kFisherman5}},
        {{kFletcher1, kFletcher2, kFletcher3, kFletcher4, kFletcher5}},
        {{kLeatherworker1, kLeatherworker2, kLeatherworker3, kLeatherworker4, kLeatherworker5}},
        {{kLibrarian1, kLibrarian2, kLibrarian3, kLibrarian4, kLibrarian5}},
        {{kMason1, kMason2, kMason3, kMason4, kMason5}},
        {{kShepherd1, kShepherd2, kShepherd3, kShepherd4, kShepherd5}},
        {{kToolsmith1, kToolsmith2, kToolsmith3, kToolsmith4, kToolsmith5}},
        {{kWeaponsmith1, kWeaponsmith2, kWeaponsmith3, kWeaponsmith4, kWeaponsmith5}},
        kNone,
    };
    static_assert(std::size(kAll) == size_t(Profession::Count));
    return kAll[size_t(p)];
}

ItemId itemOf(std::string_view name) {
    if (name.empty()) return kNoItem;
    return itemRegistry().find(name).value_or(kNoItem);
}

// A random enchantment the item can take, at a random level (ours: uniform; vanilla
// rolls a table enchantment of level 5-19 for gear).
uint16_t randomEnchantment(ItemId item, Xoroshiro& rng, bool book) {
    Enchantment choices[int(Enchantment::Count)];
    int n = 0;
    for (int e = 1; e < int(Enchantment::Count); ++e) {
        const auto kind = static_cast<Enchantment>(e);
        // (wiki: Librarian - every enchantment but Swift Sneak, Soul Speed and Wind Burst)
        if (kind == Enchantment::SwiftSneak || kind == Enchantment::SoulSpeed || kind == Enchantment::WindBurst)
            continue;
        if (!book && enchantmentInfo(kind).weight == 0) continue; // (gear: table enchantments only)
        if (book || canEnchant(item, kind)) choices[n++] = kind;
    }
    if (n == 0) return 0;
    const Enchantment e = choices[rng.nextInt(uint32_t(n))];
    const int level = 1 + int(rng.nextInt(uint32_t(enchantmentInfo(e).maxLevel)));
    return uint16_t(int(e) << 8 | level);
}

// The wandering trader (wiki: Wandering Trader › Trades - authored from the wiki;
// items not in the game yet are skipped).
constexpr T kTraderCommon[] = {
    sell(1, "oak_sapling", 1, 8, 1),   sell(1, "birch_sapling", 1, 8, 1),  sell(1, "spruce_sapling", 1, 8, 1),
    sell(1, "jungle_sapling", 1, 8, 1), sell(1, "acacia_sapling", 1, 8, 1), sell(1, "dark_oak_sapling", 1, 8, 1),
    sell(1, "cherry_sapling", 1, 8, 1), sell(1, "fern", 1, 12, 1),         sell(1, "sugar_cane", 1, 8, 1),
    sell(1, "pumpkin", 1, 4, 1),        sell(1, "cactus", 1, 8, 1),        sell(1, "dandelion", 1, 12, 1),
    sell(1, "poppy", 1, 12, 1),         sell(1, "blue_orchid", 1, 8, 1),   sell(1, "allium", 1, 12, 1),
    sell(1, "azure_bluet", 1, 12, 1),   sell(1, "red_tulip", 1, 12, 1),    sell(1, "oxeye_daisy", 1, 12, 1),
    sell(1, "cornflower", 1, 12, 1),    sell(1, "wheat_seeds", 1, 12, 1),  sell(1, "beetroot_seeds", 1, 12, 1),
    sell(1, "red_dye", 3, 12, 1),       sell(1, "white_dye", 3, 12, 1),    sell(1, "blue_dye", 3, 12, 1),
    sell(1, "yellow_dye", 3, 12, 1),    sell(1, "sand", 8, 8, 1),          sell(1, "red_sand", 4, 6, 1),
    sell(3, "podzol", 3, 6, 1),         sell(1, "gunpowder", 1, 8, 1),     sell(2, "glowstone", 1, 5, 1),
    sell(1, "brown_mushroom", 1, 4, 1), sell(1, "red_mushroom", 1, 4, 1),  sell(1, "lily_pad", 2, 5, 1),
    // (M33 review) 26.1's name tag; 26.3's poplar saplings and shelf mushrooms
    sell(1, "name_tag", 1, 5, 1),       sell(1, "poplar_sapling", 5, 8, 1), sell(1, "shelf_mushroom", 3, 12, 1)};
constexpr T kTraderRare[] = {sell(5, "nautilus_shell", 1, 5, 1), sell(3, "packed_ice", 1, 6, 1),
                             sell(6, "blue_ice", 1, 6, 1),       sell(4, "slime_ball", 1, 5, 1),
                             sell(6, "mangrove_propagule", 1, 6, 1), sell(5, "pale_oak_sapling", 1, 6, 1),
                             sell(1, "poplar_log", 8, 4, 1)}; // (M33.3f; 26.3; ours)
constexpr T kTraderBuys[] = {buy("baked_potato", 4, 2, 1), buy("hay_block", 1, 2, 1), buy("fermented_spider_eye", 1, 2, 1),
                             buy("glass_bottle", 1, 2, 1)};

// The usable templates of a pool, without heap allocation (pools are small and fixed).
struct UsableList {
    std::array<const T*, 64> items{};
    int count = 0;
    void add(const T* t) {
        if (count < int(items.size())) items[size_t(count++)] = t;
    }
    const T& take(Xoroshiro& rng) { // a random one, removed (order kept, as erase did)
        const int i = int(rng.nextInt(uint32_t(count)));
        const T* t = items[size_t(i)];
        for (int j = i; j + 1 < count; ++j) items[size_t(j)] = items[size_t(j + 1)];
        --count;
        return *t;
    }
};

void pickInto(MobData& v, std::span<const T> pool, int count, Xoroshiro& rng) {
    UsableList usable; // (fixed size: this runs in the tick when a trader arrives)
    for (const T& t : pool)
        if (itemOf(t.buy) && itemOf(t.sell)) usable.add(&t);
    for (int k = 0; k < count && usable.count > 0 && v.offerCount < kMaxOffers; ++k) {
        const T& t = usable.take(rng);
        TradeOffer o;
        o.buyA = itemOf(t.buy);
        o.buyACount = uint8_t(t.buyCount);
        o.sell = itemOf(t.sell);
        o.sellCount = uint8_t(t.sellCount);
        o.maxUses = uint8_t(t.maxUses);
        o.xp = uint8_t(t.xp);
        o.priceMultiplier = t.mult;
        v.offers[v.offerCount++] = o;
    }
}

} // namespace

void wanderingTraderTrades(MobData& trader, Xoroshiro& rng) {
    trader.offerCount = 0;
    pickInto(trader, kTraderBuys, 2, rng);
    pickInto(trader, kTraderCommon, 5, rng);
    pickInto(trader, kTraderRare, 1, rng);
}

int villagerLevelFor(int xp) {
    return xp >= 250 ? 5 : xp >= 150 ? 4 : xp >= 70 ? 3 : xp >= 10 ? 2 : 1;
}

void addLevelTrades(MobData& v, Xoroshiro& rng) {
    const Pools& pools = poolsFor(static_cast<Profession>(v.profession));
    const int level = std::clamp<int>(v.villagerLevel, 1, 5);
    // The usable templates of this level (their items exist), then 2 of them at random.
    UsableList usable;
    for (const T& t : pools.level[level - 1])
        if (itemOf(t.buy) && itemOf(t.sell) && (t.buyB.empty() || itemOf(t.buyB))) usable.add(&t);
    const int want = 2; // (every level, master too, unlocks up to two - wiki: Trading)
    for (int k = 0; k < want && usable.count > 0 && v.offerCount < kMaxOffers; ++k) {
        const T& t = usable.take(rng);
        TradeOffer o;
        o.buyA = itemOf(t.buy);
        o.buyACount = uint8_t(t.buyCount);
        o.buyB = itemOf(t.buyB);
        o.buyBCount = uint8_t(t.buyBCount);
        o.sell = itemOf(t.sell);
        o.sellCount = uint8_t(t.sellCount);
        o.maxUses = uint8_t(t.maxUses);
        o.xp = uint8_t(t.xp);
        o.priceMultiplier = t.mult;
        if (t.kind == 1) {
            o.sellEnchant = randomEnchantment(o.sell, rng, false);
        } else if (t.kind == 2) {
            // An enchanted book (wiki: Librarian): 2 + random(5 + level x 10) + 3 x level
            // emeralds, at most 64 (ours: no treasure doubling).
            o.sellEnchant = randomEnchantment(o.sell, rng, true);
            const int lvl = o.sellEnchant & 0xFF;
            o.buyACount =
                uint8_t(std::min(64, 2 + int(rng.nextInt(uint32_t(5 + lvl * 10))) + 3 * lvl));
        }
        v.offers[v.offerCount++] = o;
    }
}

int offerPrice(const TradeOffer& o, int heroLevel, int reputation) {
    const int base = o.buyACount;
    const int gossip = int(std::floor(float(reputation) * o.priceMultiplier)); // (M32.5)
    const int demandBonus = std::max(0, int(float(base) * o.priceMultiplier * float(o.demand)));
    const int maxStack = std::max<int>(1, itemRegistry().item(o.buyA).maxStack);
    const int hero =
        heroLevel > 0 ? std::max(1, int(std::floor(float(base) * (0.3f + 0.0625f * float(heroLevel - 1))))) : 0;
    return std::clamp(base + demandBonus + o.specialPrice - hero - gossip, 1, maxStack);
}

ItemStack offerBuyA(const TradeOffer& o, int heroLevel, int reputation) {
    return {o.buyA, uint8_t(offerPrice(o, heroLevel, reputation))};
}
ItemStack offerBuyB(const TradeOffer& o) {
    return o.buyB ? ItemStack{o.buyB, o.buyBCount} : ItemStack{};
}
ItemStack offerSell(const TradeOffer& o) {
    ItemStack s{o.sell, o.sellCount};
    if (o.sellEnchant)
        setEnchantment(s, static_cast<Enchantment>(o.sellEnchant >> 8), o.sellEnchant & 0xFF);
    return s;
}

bool useOffer(MobData& v, int i, Xoroshiro& rng) {
    TradeOffer& o = v.offers[size_t(i)];
    if (o.uses < 255) ++o.uses;
    if (v.type != MobType::Villager) return false; // (wandering traders have no levels)
    addGossip(v, Gossip::Trading, 2);               // (M32.5: vanilla's trade gossip)
    v.villagerXp += o.xp;
    const int level = villagerLevelFor(v.villagerXp);
    if (level <= v.villagerLevel) return false;
    v.villagerLevel = uint8_t(level);
    addLevelTrades(v, rng);
    return true;
}

void restock(MobData& v) {
    for (int i = 0; i < v.offerCount; ++i) {
        TradeOffer& o = v.offers[size_t(i)];
        o.demand = int8_t(std::clamp(int(o.demand) + o.uses - (o.maxUses - o.uses), 0, 100));
        o.uses = 0;
    }
}

void addGossip(MobData& v, Gossip kind, int amount) {
    const GossipInfo& g = kGossips[size_t(kind)];
    int16_t& value = v.gossip[size_t(kind)];
    value = int16_t(std::clamp(int(value) + amount, 0, g.max));
}

int reputation(const MobData& v) {
    int r = 0;
    for (size_t k = 0; k < v.gossip.size(); ++k) r += int(v.gossip[k]) * kGossips[k].weight;
    return r;
}

void decayGossip(MobData& v) {
    for (size_t k = 0; k < v.gossip.size(); ++k)
        v.gossip[k] = int16_t(std::max(0, int(v.gossip[k]) - kGossips[k].decay));
}

} // namespace mc::world
