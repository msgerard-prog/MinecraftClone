// Composters and cauldrons (M23.5; wiki: Composter, Cauldron). Part of BlockUpdates.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"
#include "world/Items.h"
#include "world/Potions.h"

#include <span>
#include <string_view>
#include <vector>

namespace mc::world {

namespace {

namespace B = blocks;
const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
bool is(ItemId item, std::string_view name) { return itemRegistry().item(item).id.substr(10) == name; }

// The chance (percent) that an item adds a layer (wiki: Composter › Compostable items),
// worked out once per item from its name.
const std::vector<uint8_t>& compostTable() {
    static const std::vector<uint8_t> table = [] {
        static constexpr std::string_view k30[] = {"beetroot_seeds", "dried_kelp", "glow_berries", "short_grass",
                                                   "hanging_roots", "kelp", "melon_seeds", "moss_carpet",
                                                   "pink_petals", "pitcher_pod", "pumpkin_seeds", "seagrass",
                                                   "small_dripleaf", "sweet_berries", "torchflower_seeds",
                                                   "wheat_seeds", "mangrove_roots", "mangrove_propagule"};
        static constexpr std::string_view k50[] = {"cactus", "dried_kelp_block", "flowering_azalea_leaves",
                                                   "glow_lichen", "melon_slice", "nether_sprouts", "sugar_cane",
                                                   "tall_grass", "twisting_vines", "vine", "weeping_vines"};
        static constexpr std::string_view k65[] = {
            "apple", "azalea", "beetroot", "big_dripleaf", "carrot", "cocoa_beans", "fern", "dandelion", "poppy",
            "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
            "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose", "sunflower", "lilac", "rose_bush",
            "peony", "torchflower", "pitcher_plant", "large_fern", "lily_pad", "melon", "moss_block",
            "brown_mushroom", "red_mushroom", "mushroom_stem", "nether_wart", "potato", "pumpkin",
            "carved_pumpkin", "crimson_fungus", "warped_fungus", "crimson_roots", "warped_roots", "sea_pickle",
            "shroomlight", "spore_blossom", "wheat"};
        static constexpr std::string_view k85[] = {"baked_potato", "bread", "cookie", "flowering_azalea", "hay_block",
                                                   "brown_mushroom_block", "red_mushroom_block", "nether_wart_block",
                                                   "warped_wart_block"};
        static constexpr std::string_view k100[] = {"cake", "pumpkin_pie"};
        const auto& items = itemRegistry();
        std::vector<uint8_t> t(items.count(), 0);
        auto mark = [&](std::span<const std::string_view> names, uint8_t chance) {
            for (const std::string_view n : names)
                if (const auto id = items.find(n)) t[*id] = chance;
        };
        for (size_t i = 1; i < items.count(); ++i) { // leaves and saplings by name (30%)
            const std::string_view n = std::string_view(items.item(static_cast<ItemId>(i)).id).substr(10);
            if (n.ends_with("_leaves") || n.ends_with("_sapling")) t[i] = 30;
        }
        mark(k30, 30);
        mark(k50, 50);
        mark(k65, 65);
        mark(k85, 85);
        mark(k100, 100);
        return t;
    }();
    return table;
}

} // namespace

int BlockUpdates::compostChance(ItemId item) {
    const auto& t = compostTable();
    return item < t.size() ? t[item] : 0;
}

// Vanilla: an item on a composter below level 7 adds a layer with its chance (always on
// an empty one); reaching 7 schedules the change to "ready" (8) 20 ticks later.
bool BlockUpdates::compost(const BlockPos& p, ItemId item) {
    const BlockStateId s = at(p);
    if (blockOf(s) != B::Composter) return false;
    const int level = R().get(s, properties::composterLevel);
    const int chance = compostChance(item);
    if (level >= 7 || chance == 0) return false;
    if (level == 0 || int(m_random.nextInt(100)) < chance) {
        set(p, R().set(s, properties::composterLevel, level + 1));
        if (level + 1 == 7) schedule(p, B::Composter, 20, 0);
    }
    m_world.levelEvent(LevelEvent::Type::BlockHit, p.x + 0.5, p.y + 0.8, p.z + 0.5); // (vanilla: green sparkles)
    return true;
}

ItemStack BlockUpdates::takeCompost(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (blockOf(s) != B::Composter || R().get(s, properties::composterLevel) != 8) return {};
    set(p, R().set(s, properties::composterLevel, 0));
    return {*itemRegistry().find("bone_meal"), 1};
}

// Wiki: Cauldron › Usage. Buckets fill or empty it whole, bottles move one level of
// water; a lava or powder snow cauldron takes a water bucket too (the contents swap).
std::optional<ItemStack> BlockUpdates::useCauldron(const BlockPos& p, const ItemStack& held) {
    const BlockStateId s = at(p);
    const BlockId b = blockOf(s);
    if (b != B::Cauldron && b != B::WaterCauldron && b != B::LavaCauldron && b != B::PowderSnowCauldron)
        return std::nullopt;
    if (held.empty()) return std::nullopt;
    const auto item = [](std::string_view name) { return ItemStack{*itemRegistry().find(name), 1}; };
    const int level = b == B::WaterCauldron || b == B::PowderSnowCauldron ? R().get(s, properties::cauldronLevel) + 1
                      : b == B::LavaCauldron                               ? 3
                                                                           : 0;
    auto water = [&](int lvl) {
        set(p, lvl == 0 ? R().defaultState(B::Cauldron)
                        : R().set(R().defaultState(B::WaterCauldron), properties::cauldronLevel, lvl - 1));
    };
    if (is(held.item, "water_bucket")) {
        water(3);
        m_world.playSound(Sound::Splash, p.x + 0.5, p.y + 0.5, p.z + 0.5, 0.5f, 1.0f);
        return item("bucket");
    }
    if (is(held.item, "lava_bucket")) {
        set(p, R().defaultState(B::LavaCauldron));
        return item("bucket");
    }
    if (is(held.item, "bucket") && level == 3) {
        if (b == B::WaterCauldron) {
            water(0);
            return item("water_bucket");
        }
        if (b == B::LavaCauldron) {
            set(p, R().defaultState(B::Cauldron));
            return item("lava_bucket");
        }
        return std::nullopt; // (powder snow: no powder snow bucket yet)
    }
    if (is(held.item, "glass_bottle") && b == B::WaterCauldron) {
        water(level - 1);
        ItemStack bottle = item("potion");
        bottle.potion = static_cast<uint8_t>(Potion::Water);
        return bottle;
    }
    if (is(held.item, "potion") && held.potion == static_cast<uint8_t>(Potion::Water) &&
        (b == B::Cauldron || (b == B::WaterCauldron && level < 3))) {
        water(level + 1);
        return item("glass_bottle");
    }
    return std::nullopt;
}

int BlockUpdates::cauldronSignal(BlockStateId s) {
    switch (blockOf(s)) {
    case B::Composter: return R().get(s, properties::composterLevel); // (wiki: 0..8)
    case B::Cauldron: return 0;
    case B::WaterCauldron:
    case B::PowderSnowCauldron: return R().get(s, properties::cauldronLevel) + 1;
    case B::LavaCauldron: return 3;
    default: return -1;
    }
}

// Vanilla Block.handlePrecipitation: rain fills a cauldron one level with chance 0.05 a
// precipitation tick, snow with powder snow at 0.1.
void BlockUpdates::fillCauldronByWeather(const BlockPos& p, bool snow) {
    const BlockStateId s = at(p);
    const BlockId b = blockOf(s);
    if (!snow && (b == B::Cauldron || b == B::WaterCauldron)) {
        if (m_random.nextFloat() >= 0.05f) return;
        const int level = b == B::Cauldron ? 0 : R().get(s, properties::cauldronLevel) + 1;
        if (level < 3) set(p, R().set(R().defaultState(B::WaterCauldron), properties::cauldronLevel, level));
    } else if (snow && (b == B::Cauldron || b == B::PowderSnowCauldron)) {
        if (m_random.nextFloat() >= 0.1f) return;
        const int level = b == B::Cauldron ? 0 : R().get(s, properties::cauldronLevel) + 1;
        if (level < 3) set(p, R().set(R().defaultState(B::PowderSnowCauldron), properties::cauldronLevel, level));
    }
}

} // namespace mc::world
