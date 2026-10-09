// Farming (M17.1; wiki: Farmland, Wheat Seeds, Carrot, Potato, Beetroot Seeds, Bone
// Meal, Tutorial:Crop farming). Part of BlockUpdates.
#include "world/BlockUpdates.h"

#include "world/Raycast.h"
#include "world/Weather.h"

#include "world/Blocks.h"
#include "world/Items.h"

#include <cmath>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }

const Property& ageOf(BlockId crop) { return crop == B::Beetroots ? age3 : age7; }

} // namespace

bool BlockUpdates::isCrop(BlockId b) {
    return b == B::Wheat || b == B::Carrots || b == B::Potatoes || b == B::Beetroots;
}

int BlockUpdates::cropMaxAge(BlockId b) { return b == B::Beetroots ? 3 : 7; }

int BlockUpdates::cropAge(BlockStateId s) { return R().get(s, ageOf(blockOf(s))); }

bool BlockUpdates::strip(World& world, const BlockPos& p) {
    const BlockStateId s = world.getBlock(p);
    const std::string_view id = R().block(blockOf(s)).id;
    if (scrapeCopper(world, p)) return true; // (M23.4b: copper - wax off or a stage scraped)
    if (id.find("stripped_") != std::string_view::npos) return false;
    const bool woody = id.ends_with("_log") || id.ends_with("_wood") || id.ends_with("_hyphae") ||
                       (id.ends_with("_stem") && id != "minecraft:mushroom_stem") || id == "minecraft:bamboo_block";
    if (!woody) return false;
    const auto stripped = R().findBlock("minecraft:stripped_" + std::string(id.substr(10)));
    if (!stripped) return false;
    BlockStateId now = R().defaultState(*stripped);
    if (const auto a = R().value(s, "axis")) now = R().with(now, "axis", *a).value_or(now);
    world.updateBlock(p, now);
    world.playSound(Sound::WoodClick, p.x + 0.5, p.y + 0.5, p.z + 0.5, 1.0f, 0.8f); // (vanilla: item.axe.strip)
    return true;
}

bool BlockUpdates::till(World& world, const BlockPos& p, Direction side) {
    // A hoe turns dirt or grass into farmland (coarse dirt into dirt), from any side
    // but below, if the block above is free (wiki: Hoe).
    if (side == Direction::Down) return false;
    const BlockId b = blockOf(world.getBlock(p));
    if (b != B::Dirt && b != B::GrassBlock && b != B::CoarseDirt) return false;
    if (world.getBlock({p.x, p.y + 1, p.z}) != 0) return false;
    world.updateBlock(p, R().defaultState(b == B::CoarseDirt ? B::Dirt : B::Farmland));
    return true;
}

bool BlockUpdates::nearWater(const BlockPos& p) const {
    // Water within 4 blocks horizontally, at the farmland's level or one above.
    for (int dy = 0; dy <= 1; ++dy)
        for (int dz = -4; dz <= 4; ++dz)
            for (int dx = -4; dx <= 4; ++dx)
                if (blockOf(at({p.x + dx, p.y + dy, p.z + dz})) == B::Water) return true;
    return false;
}

void BlockUpdates::tickFarmland(const BlockPos& p, BlockStateId s) {
    const int m = R().get(s, moisture);
    // Water nearby or rain falling on the block above keeps it moist (wiki: Farmland).
    if (nearWater(p) || (m_weather && rainingAt(m_world, *m_weather, {p.x, p.y + 1, p.z}))) {
        if (m != 7) setRaw(p, R().set(s, moisture, 7)); // hydrated at once
    } else if (m > 0) {
        setRaw(p, R().set(s, moisture, m - 1)); // dries a step per random tick
    } else if (const BlockId on = blockOf(at({p.x, p.y + 1, p.z}));
               !isCrop(on) && on != B::PumpkinStem && on != B::MelonStem && on != B::AttachedPumpkinStem &&
               on != B::AttachedMelonStem && on != B::TorchflowerCrop && on != B::PitcherCrop) {
        // dry and bare - vanilla's maintains_farmland tag: crops and stems keep it - back to dirt
        set(p, R().defaultState(B::Dirt));
    }
}

void BlockUpdates::trample(const BlockPos& farmland) {
    // Jumped on: dirt again; the crop on it pops off (its neighbour update).
    if (blockOf(at(farmland)) == B::Farmland) set(farmland, R().defaultState(B::Dirt));
}

float BlockUpdates::growthPoints(const BlockPos& p, BlockId crop) const {
    // The speed level (wiki: Tutorial:Crop farming): its own farmland 2 (4 if moist),
    // each of the 8 farmland around 0.25 (0.75 moist); halved when the same crop is
    // on a diagonal, or both north/south and east/west.
    float points = 0.0f;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            const BlockStateId f = at({p.x + dx, p.y - 1, p.z + dz});
            if (blockOf(f) != B::Farmland) continue;
            const bool wet = R().get(f, moisture) > 0;
            points += (dx == 0 && dz == 0) ? (wet ? 4.0f : 2.0f) : (wet ? 0.75f : 0.25f);
        }
    auto same = [&](int dx, int dz) { return blockOf(at({p.x + dx, p.y, p.z + dz})) == crop; };
    const bool diagonal = same(-1, -1) || same(1, -1) || same(1, 1) || same(-1, 1);
    const bool rows = (same(0, -1) || same(0, 1)) && (same(-1, 0) || same(1, 0));
    if (diagonal || rows) points /= 2.0f;
    return points;
}

void BlockUpdates::tickCrop(const BlockPos& p, BlockStateId s) {
    // Light 9+, then a 1 / (floor(25 / points) + 1) chance to grow a stage.
    if (rawBrightness(p) < 9) return;
    const BlockId crop = blockOf(s);
    const int a = cropAge(s);
    if (a >= cropMaxAge(crop)) return;
    const float points = growthPoints(p, crop);
    if (points <= 0.0f) return;
    if (m_random.nextInt(uint32_t(std::floor(25.0f / points)) + 1) != 0) return;
    if (crop == B::Beetroots && m_random.nextInt(3) == 0) return; // beetroots skip 1 in 3 (wiki)
    setRaw(p, R().set(s, ageOf(crop), a + 1));
}

void BlockUpdates::tickStem(const BlockPos& p, BlockStateId s) {
    // (M29.4b; wiki: Melon Seeds, Pumpkin Seeds) the crop's growth odds; at age 7 a tick
    // that would grow picks a random side and puts the fruit there if it is free and
    // stands on dirt, grass or farmland; the stem then bends toward it.
    if (rawBrightness(p) < 9) return;
    const BlockId stem = blockOf(s);
    const float points = growthPoints(p, stem);
    if (points <= 0.0f) return;
    if (m_random.nextInt(uint32_t(std::floor(25.0f / points)) + 1) != 0) return;
    const int a = R().get(s, age7);
    if (a < 7) {
        setRaw(p, R().set(s, age7, a + 1));
        return;
    }
    static constexpr Direction kSides[4] = {Direction::North, Direction::East, Direction::South, Direction::West};
    const Direction d = kSides[m_random.nextInt(4)];
    const BlockPos q = neighbour(p, d);
    const BlockId ground = blockOf(at({q.x, q.y - 1, q.z}));
    if (at(q) != 0 || !(ground == B::Farmland || ground == B::Dirt || ground == B::GrassBlock || ground == B::CoarseDirt ||
                        ground == B::Podzol || ground == B::Mud || ground == B::MossBlock || ground == B::RootedDirt))
        return;
    const bool melon = stem == B::MelonStem;
    set(q, R().defaultState(melon ? B::Melon : B::Pumpkin));
    const BlockStateId bent = R().defaultState(melon ? B::AttachedMelonStem : B::AttachedPumpkinStem);
    set(p, R().set(bent, facing, int(d) - 2)); // facing: north, south, west, east = Direction 2..5
}

bool BlockUpdates::jungleLog(BlockId b) {
    // vanilla's jungle_logs tag: the log, the wood and their stripped kinds
    static const BlockId kOthers[3] = {*R().findBlock("minecraft:jungle_wood"),
                                       *R().findBlock("minecraft:stripped_jungle_log"),
                                       *R().findBlock("minecraft:stripped_jungle_wood")};
    return b == B::JungleLog || b == kOthers[0] || b == kOthers[1] || b == kOthers[2];
}

int BlockUpdates::pickBerries(World& world, const BlockPos& p, Xoroshiro& rng) {
    const BlockStateId s = world.getBlock(p);
    if (const BlockId b = blockRegistry().blockOf(s); b == B::CaveVines || b == B::CaveVinesPlant) {
        // (M27.2; wiki: Glow Berries - one from a vine bearing them)
        if (blockRegistry().get(s, berries) != 0) return 0;
        world.updateBlock(p, blockRegistry().set(s, berries, 1));
        world.playSound(Sound::BerryPick, p.x + 0.5, p.y + 0.5, p.z + 0.5);
        return 1;
    }
    if (blockRegistry().blockOf(s) != B::SweetBerryBush) return 0;
    const int a = blockRegistry().get(s, age3);
    if (a < 2) return 0;
    const int n = (a == 3 ? 2 : 1) + static_cast<int>(rng.nextInt(2));
    world.updateBlock(p, blockRegistry().set(s, age3, 1));
    world.playSound(Sound::BerryPick, p.x + 0.5, p.y + 0.5, p.z + 0.5);
    return n;
}

bool BlockUpdates::boneMeal(const BlockPos& p) {
    // wiki: Bone Meal - crops grow 2-5 stages (beetroots 1); saplings advance a stage
    // 45% of the time; a grass block sprouts grass and flowers around it.
    if (lushBoneMeal(p)) return true; // (M27.2: vines, azaleas, dripleaves, moss)
    if (blockOf(at(p)) == B::TorchflowerCrop || blockOf(at(p)) == B::PitcherCrop) { // (M27.5c: a stage)
        growSniffCrop(p, at(p));
        return true;
    }
    const BlockStateId s = at(p);
    const BlockId b = blockOf(s);
    if (isCrop(b)) {
        const int a = cropAge(s), max = cropMaxAge(b);
        if (a >= max) return false;
        // Beetroots: 75% chance of +1 (the bone meal is used either way).
        const int add = b == B::Beetroots ? (m_random.nextFloat() < 0.75f ? 1 : 0) : 2 + static_cast<int>(m_random.nextInt(4));
        if (add > 0) set(p, R().set(s, ageOf(b), std::min(max, a + add)));
        return true;
    }
    if (b == B::PumpkinStem || b == B::MelonStem) { // (M29.4b) 2-5 stages, like crops
        const int a = R().get(s, age7);
        if (a >= 7) return false;
        set(p, R().set(s, age7, std::min(7, a + 2 + static_cast<int>(m_random.nextInt(4)))));
        return true;
    }
    if (b == B::BambooSapling) { // (M29.4c) it grows into bamboo
        const BlockPos up{p.x, p.y + 1, p.z};
        if (at(up) != 0) return false;
        set(p, R().defaultState(B::Bamboo));
        set(up, R().set(R().defaultState(B::Bamboo), bambooLeaves, 1));
        return true;
    }
    if (b == B::GlowLichen) { // (M29.4c; wiki: Glow Lichen) spreads to a free face nearby
        const int f = R().get(s, facing6);
        for (int i = 0; i < 8; ++i) {
            const BlockPos q{p.x + int(m_random.nextInt(3)) - 1, p.y + int(m_random.nextInt(3)) - 1,
                             p.z + int(m_random.nextInt(3)) - 1};
            if (at(q) != 0 && at(q) != R().defaultState(B::Water)) continue;
            if (!R().collides(at(neighbour(q, static_cast<Direction>(f))))) continue;
            set(q, R().set(R().set(s, facing6, f), waterlogged, at(q) == 0 ? 1 : 0));
            return true;
        }
        return false;
    }
    if (b == B::Cocoa) { // (M29.4b) a stage
        if (R().get(s, age2) >= 2) return false;
        set(p, R().set(s, age2, R().get(s, age2) + 1));
        return true;
    }
    if (b == B::SweetBerryBush) { // (M26.3) one stage
        if (R().get(s, age3) >= 3) return false;
        set(p, R().set(s, age3, R().get(s, age3) + 1));
        return true;
    }
    if (b == B::OakSapling || b == B::BirchSapling || b == B::SpruceSapling || b == B::AcaciaSapling ||
        b == B::JungleSapling || b == B::DarkOakSapling || b == B::CherrySapling || b == B::PaleOakSapling ||
        b == B::MangrovePropagule) {
        if (m_random.nextFloat() < 0.45f) {
            if (R().get(s, stage) == 0) setRaw(p, R().set(s, stage, 1));
            else growTree(p, s);
        }
        return true; // used up either way
    }
    // (M27.1; wiki: Bone Meal) a tall flower drops a copy of itself; short grass and ferns
    // grow into their two-block kinds when there is room above.
    if (b == B::Sunflower || b == B::Lilac || b == B::RoseBush || b == B::Peony) {
        m_drops.push_back({p, {itemRegistry().blockItem(b), 1}, 0});
        return true;
    }
    if (b == B::ShortGrass || b == B::Fern) {
        const BlockPos up{p.x, p.y + 1, p.z};
        if (at(up) != 0) return false;
        const BlockId tall = b == B::ShortGrass ? B::TallGrass : B::LargeFern;
        setRaw(p, R().set(R().defaultState(tall), doorHalf, 1));
        setRaw(up, R().set(R().defaultState(tall), doorHalf, 0));
        return true;
    }
    if (b == B::GrassBlock && at({p.x, p.y + 1, p.z}) == 0) {
        for (int i = 0; i < 32; ++i) {
            const BlockPos q{p.x + int(m_random.nextInt(7)) - 3, p.y + 1, p.z + int(m_random.nextInt(7)) - 3};
            if (at(q) != 0 || blockOf(at({q.x, q.y - 1, q.z})) != B::GrassBlock) continue;
            const BlockId plant = m_random.nextInt(8) == 0 ? (m_random.nextInt(2) ? B::Dandelion : B::Poppy) : B::ShortGrass;
            set(q, R().defaultState(plant));
        }
        return true;
    }
    return false;
}

} // namespace mc::world
