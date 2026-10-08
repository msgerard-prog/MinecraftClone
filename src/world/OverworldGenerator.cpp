#include "world/OverworldGenerator.h"

#include "world/Blocks.h"
#include "world/Loot.h"
#include "world/StructurePlacement.h"
#include "world/Random.h"
#include "world/TreeFeature.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <numbers>
#include <string>

namespace mc::world {

namespace {

// --- Small helpers ------------------------------------------------------------------

double smoothstep(double e0, double e1, double x) {
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Piecewise-linear spline through (x, y) points sorted by x.
template <size_t N> double spline(const std::array<std::array<double, 2>, N>& pts, double x) {
    if (x <= pts[0][0]) return pts[0][1];
    for (size_t i = 1; i < N; ++i)
        if (x <= pts[i][0])
            return lerp(pts[i - 1][1], pts[i][1], (x - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
    return pts[N - 1][1];
}

// Two octave noises averaged, the second sampled at a slightly different scale
// (any factor near 1.02 breaks up lattice alignment; ours).
constexpr double kDoubleScale = 1.0213;
double pair2(const OctaveNoise (&n)[2], double x, double z, double gain) {
    return (n[0].noise2d(x, z) + n[1].noise2d(x * kDoubleScale, z * kDoubleScale)) * 0.5 * gain;
}
double pair3(const OctaveNoise (&n)[2], double x, double y, double z, double gain) {
    return (n[0].noise(x, y, z) + n[1].noise(x * kDoubleScale, y * kDoubleScale, z * kDoubleScale)) *
           0.5 * gain;
}

// Per-position random in [0,1) (deterministic: seed, position, purpose).
double positional(uint64_t seed, int32_t x, int32_t y, int32_t z, uint64_t purpose) {
    uint64_t h = mixSeed(seed, purpose);
    h = mixSeed(h, static_cast<uint32_t>(x));
    h = mixSeed(h, static_cast<uint32_t>(y));
    h = mixSeed(h, static_cast<uint32_t>(z));
    return static_cast<double>(h >> 11) * 0x1.0p-53;
}

uint64_t chunkSeed(uint64_t seed, int32_t cx, int32_t cz, uint64_t purpose) {
    return mixSeed(mixSeed(mixSeed(seed, purpose), static_cast<uint32_t>(cx)), static_cast<uint32_t>(cz));
}

// Corner grid of the density (vanilla cell size: 4 x 8 x 4).
constexpr int kCellW = 4, kCellH = 8;
constexpr int kCornersXZ = 16 / kCellW + 1;                   // 5
constexpr int kCornersY = (kOverworldHeight.maxY() - kOverworldHeight.minY + 1) / kCellH + 1;   // 49

int floorDiv(int a, int b) { return (a >= 0 ? a : a - b + 1) / b; }

bool isOcean(Biome b) {
    switch (b) {
    case Biome::Ocean:
    case Biome::DeepOcean:
    case Biome::WarmOcean:
    case Biome::LukewarmOcean:
    case Biome::ColdOcean:
    case Biome::FrozenOcean:
    case Biome::DeepLukewarmOcean:
    case Biome::DeepColdOcean:
    case Biome::DeepFrozenOcean: return true;
    default: return false;
    }
}

struct Blocks {
    BlockStateId air, stone, deepslate, bedrock, water, lava, grass, snowyGrass, dirt, sand,
        sandstone, redSand, terracotta, gravel, clay, snow, ice, packedIce, calcite;
    BlockStateId granite, diorite, andesite, tuff;
    BlockStateId oakLog, oakLeaves, birchLog, birchLeaves, spruceLog, spruceLeaves, acaciaLog,
        acaciaLeaves;
    BlockStateId shortGrass, fern, deadBush, flowers[5], snowLayer;
    BlockStateId sugarCane, cactus, pumpkin, brownMushroom, redMushroom, coarseDirt;
    BlockStateId jungleLog, darkOakLog, cherryLog, podzol, mycelium;
    BlockStateId brownCap, redCap, stem; // huge mushroom blocks (cap: pale underside; stem: no ends)
    BlockStateId cobblestone, mossyCobblestone, chest, spawner, oakPlanks, bookshelf, frame[4], frameEye[4];
    BlockStateId dirtPath, glass, torch, farmland, crops[4], acaciaPlanks;
    BlockStateId chiseledSandstone, cutSandstone, smoothSandstone, orangeTerracotta, blueTerracotta, stoneBricks,
        mossyStoneBricks, crackedStoneBricks, chiseledStoneBricks, tnt, sprucePlanks, craftingTable, furnace,
        redstoneTorch, bedFoot, bedHead;
    // Leaves by wood (oak, birch, spruce, acacia, jungle, dark oak, cherry) and
    // distance 1..7 (index d-1).
    BlockStateId leaves[7][7];
    struct Ore {
        BlockStateId stone, deep;
    } coal, iron, copper, gold, redstone, lapis, diamond, emerald;
};

const Blocks& blockSet() {
    static const Blocks b = [] {
        const auto& r = blockRegistry();
        auto S = [&](BlockId id) { return r.defaultState(id); };
        Blocks x{};
        x.air = 0;
        x.stone = S(blocks::Stone);
        x.deepslate = S(blocks::Deepslate);
        x.bedrock = S(blocks::Bedrock);
        x.water = S(blocks::Water);
        x.lava = S(blocks::Lava);
        x.grass = S(blocks::GrassBlock);
        x.snowyGrass = r.with(x.grass, "snowy", "true").value_or(x.grass);
        x.dirt = S(blocks::Dirt);
        x.sand = S(blocks::Sand);
        x.sandstone = S(blocks::Sandstone);
        x.redSand = S(blocks::RedSand);
        x.terracotta = S(blocks::Terracotta);
        x.gravel = S(blocks::Gravel);
        x.clay = S(blocks::Clay);
        x.snow = S(blocks::SnowBlock);
        x.ice = S(blocks::Ice);
        x.packedIce = S(blocks::PackedIce);
        x.calcite = S(blocks::Calcite);
        x.granite = S(blocks::Granite);
        x.diorite = S(blocks::Diorite);
        x.andesite = S(blocks::Andesite);
        x.tuff = S(blocks::Tuff);
        x.oakLog = S(blocks::OakLog);
        x.oakLeaves = S(blocks::OakLeaves);
        x.birchLog = S(blocks::BirchLog);
        x.birchLeaves = S(blocks::BirchLeaves);
        x.spruceLog = S(blocks::SpruceLog);
        x.spruceLeaves = S(blocks::SpruceLeaves);
        x.acaciaLog = S(blocks::AcaciaLog);
        x.acaciaLeaves = S(blocks::AcaciaLeaves);
        x.shortGrass = S(blocks::ShortGrass);
        x.fern = S(blocks::Fern);
        x.deadBush = S(blocks::DeadBush);
        x.snowLayer = S(blocks::Snow);
        x.sugarCane = S(blocks::SugarCane);
        x.cactus = S(blocks::Cactus);
        x.pumpkin = S(blocks::Pumpkin);
        x.brownMushroom = S(blocks::BrownMushroom);
        x.redMushroom = S(blocks::RedMushroom);
        x.coarseDirt = S(blocks::CoarseDirt);
        x.chiseledSandstone = S(blocks::ChiseledSandstone);
        x.cutSandstone = S(blocks::CutSandstone);
        x.smoothSandstone = S(blocks::SmoothSandstone);
        x.orangeTerracotta = S(blocks::OrangeTerracotta);
        x.blueTerracotta = S(blocks::BlueTerracotta);
        x.stoneBricks = S(blocks::StoneBricks);
        x.mossyStoneBricks = S(blocks::MossyStoneBricks);
        x.crackedStoneBricks = S(blocks::CrackedStoneBricks);
        x.chiseledStoneBricks = S(blocks::ChiseledStoneBricks);
        x.tnt = S(blocks::Tnt);
        x.sprucePlanks = S(blocks::SprucePlanks);
        x.craftingTable = S(blocks::CraftingTable);
        x.furnace = S(blocks::Furnace);
        x.redstoneTorch = S(blocks::RedstoneTorch);
        x.bedFoot = r.with(S(blocks::RedBed), "facing", "east").value_or(0);
        x.bedHead = r.with(x.bedFoot, "part", "head").value_or(0);
        x.cobblestone = S(blocks::Cobblestone);
        x.oakPlanks = S(blocks::OakPlanks);
        x.bookshelf = S(blocks::Bookshelf);
        x.dirtPath = S(blocks::DirtPath);
        x.glass = S(blocks::Glass);
        x.torch = S(blocks::Torch);
        x.farmland = r.with(S(blocks::Farmland), "moisture", "7").value_or(0);
        x.crops[0] = S(blocks::Wheat);
        x.crops[1] = S(blocks::Carrots);
        x.crops[2] = S(blocks::Potatoes);
        x.crops[3] = S(blocks::Beetroots);
        x.acaciaPlanks = S(blocks::AcaciaPlanks);
        static constexpr const char* kFacings[4] = {"south", "west", "north", "east"}; // +z, -x, -z, +x
        for (int f = 0; f < 4; ++f) {
            x.frame[f] = r.with(S(blocks::EndPortalFrame), "facing", kFacings[f]).value_or(0);
            x.frameEye[f] = r.with(x.frame[f], "eye", "true").value_or(x.frame[f]);
        }
        x.mossyCobblestone = S(blocks::MossyCobblestone);
        x.chest = S(blocks::Chest);
        x.spawner = S(blocks::Spawner);
        x.jungleLog = S(blocks::JungleLog);
        x.darkOakLog = S(blocks::DarkOakLog);
        x.cherryLog = S(blocks::CherryLog);
        x.podzol = S(blocks::Podzol);
        x.mycelium = S(blocks::Mycelium);
        x.brownCap = r.with(S(blocks::BrownMushroomBlock), "down", "false").value_or(0);
        x.redCap = r.with(S(blocks::RedMushroomBlock), "down", "false").value_or(0);
        x.stem = r.with(r.with(S(blocks::MushroomStem), "down", "false").value_or(0), "up", "false").value_or(0);
        const BlockStateId leafDefaults[7] = {x.oakLeaves,         x.birchLeaves,           x.spruceLeaves,
                                              x.acaciaLeaves,      S(blocks::JungleLeaves), S(blocks::DarkOakLeaves),
                                              S(blocks::CherryLeaves)};
        for (int k = 0; k < 7; ++k)
            for (int d = 1; d <= 7; ++d)
                x.leaves[k][d - 1] = r.with(leafDefaults[k], "distance", std::to_string(d)).value_or(leafDefaults[k]);
        x.flowers[0] = S(blocks::Dandelion);
        x.flowers[1] = S(blocks::Poppy);
        x.flowers[2] = S(blocks::OxeyeDaisy);
        x.flowers[3] = S(blocks::AzureBluet);
        x.flowers[4] = S(blocks::Cornflower);
        x.coal = {S(blocks::CoalOre), S(blocks::DeepslateCoalOre)};
        x.iron = {S(blocks::IronOre), S(blocks::DeepslateIronOre)};
        x.copper = {S(blocks::CopperOre), S(blocks::DeepslateCopperOre)};
        x.gold = {S(blocks::GoldOre), S(blocks::DeepslateGoldOre)};
        x.redstone = {S(blocks::RedstoneOre), S(blocks::DeepslateRedstoneOre)};
        x.lapis = {S(blocks::LapisOre), S(blocks::DeepslateLapisOre)};
        x.diamond = {S(blocks::DiamondOre), S(blocks::DeepslateDiamondOre)};
        x.emerald = {S(blocks::EmeraldOre), S(blocks::DeepslateEmeraldOre)};
        return x;
    }();
    return b;
}

// The chunk being generated, as one flat array (sections encoded once at the end:
// writing through paletted sections block by block is slow).
struct Buf {
    BlockStateId* d;
    static int index(int x, int y, int z) {
        return kOverworldHeight.sectionIndex(y) * Section::kVolume + Section::index(x, blockToLocal(y), z);
    }
    BlockStateId get(int x, int y, int z) const { return kOverworldHeight.contains(y) ? d[index(x, y, z)] : 0; }
    void set(int x, int y, int z, BlockStateId s) {
        if (kOverworldHeight.contains(y)) d[index(x, y, z)] = s;
    }
};

} // namespace

// --- Noises ---------------------------------------------------------------------------

OverworldGenerator::OverworldGenerator(uint64_t seed, int version)
    : m_seed(seed), m_version(version),
      m_continentalness{OctaveNoise(mixSeed(seed, 101), -9, 6), OctaveNoise(mixSeed(seed, 102), -9, 6)},
      m_erosion{OctaveNoise(mixSeed(seed, 103), -9, 5), OctaveNoise(mixSeed(seed, 104), -9, 5)},
      m_weirdness{OctaveNoise(mixSeed(seed, 105), -7, 4), OctaveNoise(mixSeed(seed, 106), -7, 4)},
      m_temperature{OctaveNoise(mixSeed(seed, 107), -10, 4), OctaveNoise(mixSeed(seed, 108), -10, 4)},
      m_humidity{OctaveNoise(mixSeed(seed, 109), -8, 4), OctaveNoise(mixSeed(seed, 110), -8, 4)},
      m_terrain3d(mixSeed(seed, 111), -6, 4), m_jagged(mixSeed(seed, 112), -4, 3),
      m_cheese{OctaveNoise(mixSeed(seed, 113), -7, 4), OctaveNoise(mixSeed(seed, 114), -7, 4)},
      m_spaghettiA(mixSeed(seed, 115), -7, 3), m_spaghettiB(mixSeed(seed, 116), -7, 3),
      m_spaghettiWidth(mixSeed(seed, 117), -6, 1), m_noodleA(mixSeed(seed, 118), -6, 2),
      m_noodleB(mixSeed(seed, 119), -6, 2), m_surface(mixSeed(seed, 120), -4, 2) {
    if (m_version >= 2) {
        // Strongholds in 8 rings (wiki: Stronghold - counts and distance bands per
        // ring); evenly spread around each ring from a random angle, at a random
        // distance in the band (the angle jitter and biome-free placement are ours).
        struct Ring {
            int count, min, max;
        };
        static constexpr Ring kRings[] = {{3, 1280, 2816},     {6, 4352, 5888},     {10, 7424, 8960},
                                          {15, 10496, 12032},  {21, 13568, 15104},  {28, 16640, 18176},
                                          {36, 19712, 21248},  {9, 22784, 24320}};
        Xoroshiro r(mixSeed(seed, 0x5354524fu));
        for (const Ring& ring : kRings) {
            const double start = r.nextDouble() * 2.0 * std::numbers::pi;
            for (int i = 0; i < ring.count && m_strongholdCount < int(m_strongholds.size()); ++i) {
                const double angle = start + i * 2.0 * std::numbers::pi / ring.count + (r.nextDouble() - 0.5) * 0.3;
                const double dist = ring.min + r.nextDouble() * (ring.max - ring.min);
                m_strongholds[size_t(m_strongholdCount++)] = {
                    static_cast<int32_t>(std::floor(std::cos(angle) * dist / 16.0)),
                    static_cast<int32_t>(std::floor(std::sin(angle) * dist / 16.0))};
            }
        }
    }
}

// --- Climate and terrain shape ---------------------------------------------------------

OverworldGenerator::Column OverworldGenerator::column(int32_t x, int32_t z) const {
    Column c{};
    // Climate parameters, roughly in [-1, 1] like vanilla's (gains calibrate our
    // octave noise to a similar spread).
    c.continentalness = pair2(m_continentalness, x, z, 3.4) + 0.08; // a little more land
    c.erosion = pair2(m_erosion, x, z, 3.2);
    c.weirdness = pair2(m_weirdness, x, z, 3.2);
    c.temperature = pair2(m_temperature, x, z, 3.0);
    c.humidity = pair2(m_humidity, x, z, 3.0);
    // Peaks and valleys from weirdness (wiki: World generation): 1 - |3|W| - 2|.
    c.peaksValleys = 1.0 - std::abs(3.0 * std::abs(c.weirdness) - 2.0);

    // Base height by continentalness: deep ocean -> ocean -> coast -> inland.
    static constexpr std::array<std::array<double, 2>, 12> kBase = {{{-1.2, 22.0},
                                                                     {-0.6, 30.0},
                                                                     {-0.455, 38.0},
                                                                     {-0.3, 46.0},
                                                                     {-0.19, 55.0},
                                                                     {-0.15, 61.0},
                                                                     {-0.11, 63.5},
                                                                     {-0.05, 65.0},
                                                                     {0.1, 68.0},
                                                                     {0.3, 74.0},
                                                                     {0.6, 82.0},
                                                                     {1.2, 92.0}}};
    const double base = spline(kBase, c.continentalness);
    const double land = smoothstep(-0.2, -0.05, c.continentalness);
    // Low erosion inland = mountains; high erosion = flat.
    c.mountain = smoothstep(0.05, -0.55, c.erosion) * smoothstep(-0.05, 0.3, c.continentalness);
    const double flat = smoothstep(0.25, 0.65, c.erosion);
    const double hills = (4.0 + 26.0 * c.mountain) * (1.0 - 0.7 * flat) * land;
    double h = base + c.peaksValleys * hills;
    // Mountain ranges: broad massifs rising with peaks-and-valleys, a little
    // jaggedness on the highest parts.
    // |weirdness| rises steadily across a range (peaks-and-valleys would flip back
    // within ~60 blocks and make towers), so ranges are broad.
    const double ridge = smoothstep(0.1, 0.8, std::abs(c.weirdness));
    h += c.mountain * ridge * ridge * 85.0;
    h += c.mountain * smoothstep(0.6, 1.0, c.peaksValleys) * std::abs(m_jagged.noise2d(x, z)) * 12.0;
    // Rivers: the deepest valleys inland drop just below sea level.
    const double river = smoothstep(-0.7, -0.92, c.peaksValleys) * land * (1.0 - 0.8 * c.mountain);
    h = lerp(h, 58.5, river);
    // Overworld 2: mushroom islands rise out of the deepest oceans (vanilla places
    // mushroom fields at the lowest continentalness).
    if (m_version >= 2) h = lerp(h, 67.0, smoothstep(-0.8, -0.9, c.continentalness));
    c.height = h;
    c.scale = 7.0 + 26.0 * c.mountain;
    c.roughness = 0.12 + 0.75 * c.mountain;
    return c;
}

double OverworldGenerator::terrainDensity(int32_t x, int32_t y, int32_t z, const Column& c) const {
    double d = (c.height - y) / c.scale + c.roughness * m_terrain3d.noise(x, y * 1.4, z) * 1.1;
    if (y < kOverworldHeight.minY + 8) d += (kOverworldHeight.minY + 8 - y) * 0.3; // closed floor
    if (y > kOverworldHeight.maxY() - 20) d -= (y - (kOverworldHeight.maxY() - 20)) * 0.3; // open sky at the top
    return d;
}

double OverworldGenerator::caveDensity(int32_t x, int32_t y, int32_t z, const Column& c) const {
    // Cheese and noodle caves stay ~12 blocks under the terrain's target surface
    // (fading out over 8 blocks above that); spaghetti tunnels may break through on
    // land (cave entrances), but not under the sea before overworld4 (without its
    // flooded caves they would leave dry holes in the sea floor). All stay above the
    // bedrock floor.
    const double floor = std::clamp((y - (kOverworldHeight.minY + 5.0)) / 4.0, 0.0, 1.0);
    const double fade = std::clamp((c.height - 12.0 - y) / 8.0, 0.0, 1.0) * floor;
    const bool land = c.height > kSeaLevel + 3 || (m_version >= 4 && c.height < kSeaLevel + 2.0);
    const double entranceFade = land ? std::clamp((c.height + 6.0 - y) / 6.0, 0.0, 1.0) * floor : fade;
    if (fade <= 0.0 && entranceFade <= 0.0) return 1e9;
    // Cheese: large open chambers where a low-frequency noise is high.
    const double cheese = (0.42 - pair3(m_cheese, x, y * 1.6, z, 3.2)) * 4.0;
    // Spaghetti: long tunnels where two noises are both near zero (their zero
    // surfaces intersect along lines); stretched vertically so tunnels run sideways.
    const double width = 0.055 + 0.03 * m_spaghettiWidth.noise(x, y, z);
    const double a = m_spaghettiA.noise(x, y * 1.5, z) * 3.0;
    const double b = m_spaghettiB.noise(x, y * 1.5, z) * 3.0;
    const double spaghetti = lerp(1e3, (std::max(std::abs(a), std::abs(b)) - width) * 8.0, entranceFade);
    // Noodles: thin, winding, deeper down.
    double noodle = 1e9;
    if (y < 40) {
        const double na = m_noodleA.noise(x, y, z) * 3.0, nb = m_noodleB.noise(x, y, z) * 3.0;
        noodle = (std::max(std::abs(na), std::abs(nb)) - 0.03) * 12.0;
    }
    return std::min(lerp(1e3, std::min(cheese, noodle), fade), spaghetti);
}

int OverworldGenerator::surfaceY(int32_t x, int32_t z) const {
    // The same corners and interpolation as generate() (minus caves), so a tree placed
    // from a neighbouring chunk stands exactly on the ground.
    const int32_t x0 = floorDiv(x, kCellW) * kCellW, z0 = floorDiv(z, kCellW) * kCellW;
    const double fx = (x - x0) / double(kCellW), fz = (z - z0) / double(kCellW);
    const Column cols[4] = {column(x0, z0), column(x0 + kCellW, z0), column(x0, z0 + kCellW),
                            column(x0 + kCellW, z0 + kCellW)};
    double top = -1e9;
    for (const Column& c : cols)
        top = std::max(top, c.height + c.roughness * c.scale * 1.1 + 8.0);
    int j = std::clamp(static_cast<int>(std::ceil((top - kOverworldHeight.minY) / kCellH)), 1, kCornersY - 1);
    auto layer = [&](int jj) {
        const int32_t y = kOverworldHeight.minY + jj * kCellH;
        const double d00 = terrainDensity(x0, y, z0, cols[0]);
        const double d10 = terrainDensity(x0 + kCellW, y, z0, cols[1]);
        const double d01 = terrainDensity(x0, y, z0 + kCellW, cols[2]);
        const double d11 = terrainDensity(x0 + kCellW, y, z0 + kCellW, cols[3]);
        return lerp(lerp(d00, d10, fx), lerp(d01, d11, fx), fz);
    };
    double upper = layer(j);
    for (; j > 0; --j) {
        const double lower = layer(j - 1);
        for (int dy = kCellH - 1; dy >= 0; --dy) {
            if (lerp(lower, upper, dy / double(kCellH)) > 0.0) return kOverworldHeight.minY + (j - 1) * kCellH + dy;
        }
        upper = lower;
    }
    return kOverworldHeight.minY;
}

// --- Biomes ------------------------------------------------------------------------------

Biome OverworldGenerator::biomeAt(const Column& c) const {
    const Biome b = baseBiome(c);
    if (m_version < 2) return b;
    // Overworld 2 (M18.2): vanilla's biome table has more biomes in the same climate
    // slots (wiki: Biome › Generation); ours splits our M8 biomes by the same
    // humidity/temperature bands and weirdness (as vanilla's "variant" biomes).
    const double T = c.temperature, H = c.humidity, W = c.weirdness, E = c.erosion;
    const int temp = T < -0.45 ? 0 : T < -0.15 ? 1 : T < 0.2 ? 2 : T < 0.55 ? 3 : 4;
    const int hum = H < -0.35 ? 0 : H < -0.1 ? 1 : H < 0.1 ? 2 : H < 0.3 ? 3 : 4;
    if (c.continentalness < -0.82 && c.height > 61.0) return Biome::MushroomFields;
    // Overworld 4 (M25.1): every ocean temperature but warm has a deep variant (wiki:
    // Ocean › Deep variants) where the continentalness is lowest.
    if (m_version >= 4 && isOcean(b) && c.continentalness < -0.455) {
        static constexpr Biome kDeep[5] = {Biome::DeepFrozenOcean, Biome::DeepColdOcean, Biome::DeepOcean,
                                           Biome::DeepLukewarmOcean, Biome::WarmOcean};
        return kDeep[temp];
    }
    switch (b) {
    case Biome::SnowyPlains: return hum == 0 && W > 0.0 ? Biome::IceSpikes : b;
    case Biome::Taiga: return hum == 4 ? Biome::OldGrowthSpruceTaiga : b;
    case Biome::Plains: return temp == 2 && hum == 0 && W < 0.0 ? Biome::FlowerForest : b;
    case Biome::Forest:
        if (temp == 3) return hum == 4 || W < 0.0 ? Biome::Jungle : Biome::SparseJungle;
        if (temp == 2 && hum == 4) return Biome::DarkForest;
        return b;
    case Biome::Meadow: return W < 0.0 ? Biome::CherryGrove : b;
    case Biome::Badlands:
        if (c.height > 95.0) return Biome::WoodedBadlands;
        return E > 0.35 ? Biome::ErodedBadlands : b;
    default: return b;
    }
}

Biome OverworldGenerator::baseBiome(const Column& c) const {
    const double T = c.temperature, H = c.humidity, C = c.continentalness, E = c.erosion;
    const int temp = T < -0.45 ? 0 : T < -0.15 ? 1 : T < 0.2 ? 2 : T < 0.55 ? 3 : 4;
    const int hum = H < -0.35 ? 0 : H < -0.1 ? 1 : H < 0.1 ? 2 : H < 0.3 ? 3 : 4;
    if (C < -0.19) { // oceans by temperature
        if (temp == 0) return Biome::FrozenOcean;
        if (C < -0.455) return temp == 4 ? Biome::WarmOcean : temp == 3 ? Biome::LukewarmOcean : Biome::DeepOcean;
        static constexpr Biome kOcean[5] = {Biome::FrozenOcean, Biome::ColdOcean, Biome::Ocean,
                                            Biome::LukewarmOcean, Biome::WarmOcean};
        return kOcean[temp];
    }
    if (c.height < 62.5 && c.peaksValleys < -0.7) return temp == 0 ? Biome::FrozenRiver : Biome::River;
    if (C < -0.11) { // coast
        if (E < -0.35) return Biome::StonyShore;
        return temp == 0 ? Biome::SnowyBeach : Biome::Beach;
    }
    // Mountains: peaks and slopes by height.
    if (c.mountain > 0.5 && c.height > 135.0) {
        if (temp >= 3) return Biome::StonyPeaks;
        return c.weirdness > 0.0 ? Biome::JaggedPeaks : Biome::FrozenPeaks;
    }
    if (c.mountain > 0.3 && c.height > 100.0) {
        if (temp <= 1) return hum >= 2 ? Biome::Grove : Biome::SnowySlopes;
        if (temp == 2) return Biome::Meadow;
        return Biome::WindsweptHills;
    }
    if (std::abs(E) < 0.2 && std::abs(c.weirdness) > 0.45 && c.height > 82.0) return Biome::WindsweptHills;
    if (temp >= 2 && hum >= 3 && E > 0.55 && C < 0.2) return Biome::Swamp;
    switch (temp) {
    case 0: return hum <= 1 ? Biome::SnowyPlains : Biome::SnowyTaiga;
    case 1: return hum <= 1 ? Biome::Plains : Biome::Taiga;
    case 2: return hum <= 1 ? Biome::Plains : hum == 2 ? Biome::Forest : hum == 3 ? Biome::BirchForest : Biome::Forest;
    case 3: return hum <= 1 ? Biome::Savanna : hum == 2 ? Biome::Plains : Biome::Forest;
    default:
        if (hum <= 1 && c.weirdness > 0.3) return Biome::Badlands;
        return hum <= 2 ? Biome::Desert : Biome::Savanna;
    }
}

// --- Generation --------------------------------------------------------------------------

namespace {

struct TreeShape {
    // TreeKind's values, then features planned like trees (they reach across chunks).
    enum Kind { Oak, Birch, Spruce, Acacia, Jungle, MegaJungle, DarkOak, Cherry, BrownMushroom = 100, RedMushroom, IceSpike } kind;
};

// Heights of the M18.2 kinds: trees as saplings grow them; huge mushrooms 5-7 (wiki:
// Huge Mushroom); ice spikes 7-15, 1 in 20 a tall one of 30-49 (wiki: Ice Spikes shows
// spikes up to ~50; the split is ours).
int treeKindHeight(TreeShape::Kind k, Xoroshiro& rng) {
    switch (k) {
    case TreeShape::BrownMushroom:
    case TreeShape::RedMushroom: return 5 + static_cast<int>(rng.nextInt(3));
    case TreeShape::IceSpike: return rng.nextInt(20) == 0 ? 30 + static_cast<int>(rng.nextInt(20)) : 7 + static_cast<int>(rng.nextInt(9));
    default: return treeHeight(static_cast<TreeKind>(k), rng);
    }
}

// The wood (leaves row) of a tree kind.
int woodOf(TreeShape::Kind k) {
    switch (k) {
    case TreeShape::Jungle:
    case TreeShape::MegaJungle: return 4;
    case TreeShape::DarkOak: return 5;
    case TreeShape::Cherry: return 6;
    default: return static_cast<int>(k);
    }
}

// Trees per chunk (expected count) and which kinds grow in a biome (our tuning of
// vanilla's per-biome tree placement; wiki: Tree).
double treeDensity(Biome b) {
    switch (b) {
    case Biome::Forest: return 9.0;
    case Biome::BirchForest: return 9.0;
    case Biome::Taiga: return 8.0;
    case Biome::SnowyTaiga: return 6.0;
    case Biome::Grove: return 4.0;
    case Biome::Swamp: return 2.0;
    case Biome::Savanna: return 1.0;
    case Biome::WindsweptHills: return 1.0;
    case Biome::Meadow: return 0.25;
    case Biome::Plains: return 0.12;
    case Biome::SnowyPlains: return 0.1;
    // Overworld 2 (our densities after the wiki's descriptions; the plan holds 10).
    case Biome::Jungle: return 10.0;
    case Biome::DarkForest: return 10.0;
    case Biome::OldGrowthSpruceTaiga: return 9.0;
    case Biome::FlowerForest: return 6.0;
    case Biome::CherryGrove: return 3.0;
    case Biome::SparseJungle: return 1.5;
    case Biome::WoodedBadlands: return 2.0;
    case Biome::MushroomFields: return 0.6; // huge mushrooms
    case Biome::IceSpikes: return 4.0;      // ice spikes
    default: return 0.0;
    }
}

TreeShape::Kind treeKind(Biome b, Xoroshiro& rng) {
    switch (b) {
    case Biome::BirchForest: return TreeShape::Birch;
    case Biome::Forest: return rng.nextFloat() < 0.2f ? TreeShape::Birch : TreeShape::Oak;
    case Biome::Taiga:
    case Biome::SnowyTaiga:
    case Biome::Grove:
    case Biome::SnowyPlains: return TreeShape::Spruce;
    case Biome::WindsweptHills: return rng.nextFloat() < 0.5f ? TreeShape::Spruce : TreeShape::Oak;
    case Biome::Savanna: return TreeShape::Acacia;
    case Biome::Jungle: {
        const float r = rng.nextFloat();
        return r < 0.15f ? TreeShape::MegaJungle : r < 0.75f ? TreeShape::Jungle : TreeShape::Oak;
    }
    case Biome::SparseJungle: return rng.nextFloat() < 0.6f ? TreeShape::Jungle : TreeShape::Oak;
    case Biome::DarkForest: {
        // Mostly dark oaks, some oak/birch, now and then a huge mushroom (wiki: Dark Forest).
        const float r = rng.nextFloat();
        return r < 0.65f   ? TreeShape::DarkOak
               : r < 0.8f  ? TreeShape::Oak
               : r < 0.92f ? TreeShape::Birch
               : r < 0.96f ? TreeShape::BrownMushroom
                           : TreeShape::RedMushroom;
    }
    case Biome::FlowerForest: return rng.nextFloat() < 0.3f ? TreeShape::Birch : TreeShape::Oak;
    case Biome::OldGrowthSpruceTaiga: return TreeShape::Spruce;
    case Biome::CherryGrove: return TreeShape::Cherry;
    case Biome::MushroomFields: return rng.nextFloat() < 0.5f ? TreeShape::BrownMushroom : TreeShape::RedMushroom;
    case Biome::IceSpikes: return TreeShape::IceSpike;
    default: return TreeShape::Oak;
    }
}

} // namespace

bool OverworldGenerator::groundCarved(int32_t x, int32_t y, int32_t z) const {
    // The combined (terrain + caves) density at a block, with the same corners and
    // interpolation as generate(): <= 0 means a cave opened the ground there.
    const int32_t x0 = floorDiv(x, kCellW) * kCellW, z0 = floorDiv(z, kCellW) * kCellW;
    const int32_t y0 = kOverworldHeight.minY + floorDiv(y - kOverworldHeight.minY, kCellH) * kCellH;
    const double fx = (x - x0) / double(kCellW), fz = (z - z0) / double(kCellW),
                 fy = (y - y0) / double(kCellH);
    double d[2][2][2];
    for (int dz = 0; dz < 2; ++dz)
        for (int dx = 0; dx < 2; ++dx) {
            const Column c = column(x0 + dx * kCellW, z0 + dz * kCellW);
            for (int dy = 0; dy < 2; ++dy) {
                const int32_t cx = x0 + dx * kCellW, cy = y0 + dy * kCellH, cz = z0 + dz * kCellW;
                const double t = terrainDensity(cx, cy, cz, c);
                d[dy][dz][dx] = t > 0.0 ? std::min(t, caveDensity(cx, cy, cz, c)) : t;
            }
        }
    auto layer = [&](int dy) { return lerp(lerp(d[dy][0][0], d[dy][0][1], fx), lerp(d[dy][1][0], d[dy][1][1], fx), fz); };
    return lerp(layer(0), layer(1), fy) <= 0.0;
}

const OverworldGenerator::TreePlan& OverworldGenerator::treePlan(int32_t cx, int32_t cz) const {
    // Each chunk's trees are planned once per worker and reused by its 8 neighbours
    // (they all need it). Direct-mapped by chunk position; keyed by generator too.
    struct Entry {
        const OverworldGenerator* gen = nullptr;
        uint64_t seed = 0;
        int version = 0;
        int32_t cx = 0, cz = 0;
        TreePlan plan;
    };
    static thread_local std::array<Entry, 512> cache;
    Entry& e = cache[size_t((uint32_t(cx) * 0x9E3779B1u ^ uint32_t(cz) * 0x85EBCA77u) >> 23)]; // (9 bits)
    // (keyed by seed and version too: another generator may reuse a freed address)
    if (e.gen == this && e.seed == m_seed && e.version == m_version && e.cx == cx && e.cz == cz) return e.plan;
    e = {this, m_seed, m_version, cx, cz, {}};
    Xoroshiro rng(chunkSeed(m_seed, cx, cz, 300));
    const int attempts = 10;
    for (int a = 0; a < attempts; ++a) {
        const int32_t wx = cx * 16 + static_cast<int32_t>(rng.nextInt(16));
        const int32_t wz = cz * 16 + static_cast<int32_t>(rng.nextInt(16));
        const float roll = rng.nextFloat();
        const Column col = column(wx, wz);
        const Biome biome = biomeAt(col);
        if (roll >= treeDensity(biome) / attempts) continue;
        const int ground = surfaceY(wx, wz);
        if (ground < kSeaLevel) continue; // no trees under water
        const TreeShape::Kind kind = treeKind(biome, rng);
        // Not on high grove peaks (snow), not where a cave opened the ground.
        if (biome == Biome::Grove && ground > 160) continue;
        int height = 0;
        switch (kind) {
        case TreeShape::Oak: height = 4 + static_cast<int>(rng.nextInt(3)); break;
        case TreeShape::Birch: height = 5 + static_cast<int>(rng.nextInt(3)); break;
        case TreeShape::Spruce: height = 6 + static_cast<int>(rng.nextInt(4)); break;
        case TreeShape::Acacia: height = 5 + static_cast<int>(rng.nextInt(2)); break;
        default: height = treeKindHeight(kind, rng); break;
        }
        if (groundCarved(wx, ground, wz)) continue; // would float over a cave
        if (m_version >= 2 && inRavine(wx, ground, wz)) continue; // or a ravine
        // Only on grass/dirt: steep meadow/grove/windswept ground is bare stone.
        if (biome == Biome::Meadow || biome == Biome::Grove || biome == Biome::WindsweptHills) {
            const bool steep = std::abs(surfaceY(wx + 1, wz) - surfaceY(wx - 1, wz)) >= 4 ||
                               std::abs(surfaceY(wx, wz + 1) - surfaceY(wx, wz - 1)) >= 4;
            if (steep) continue;
        }
        e.plan.trees[size_t(e.plan.count++)] = {wx, wz, ground, static_cast<uint8_t>(kind),
                                                 static_cast<uint8_t>(height)};
    }
    return e.plan;
}

void OverworldGenerator::placeTrees(BlockStateId* blocks, ChunkPos pos, int32_t cx, int32_t cz) const {
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    Buf chunk{blocks};
    const int32_t baseX = pos.x * 16, baseZ = pos.z * 16;
    const TreePlan& plan = treePlan(cx, cz);
    for (int t = 0; t < plan.count; ++t) {
        const auto& tree = plan.trees[size_t(t)];
        const int32_t wx = tree.wx, wz = tree.wz;
        const int ground = tree.ground, height = tree.height;
        const auto kind = static_cast<TreeShape::Kind>(tree.kind);
        // Sets a block of this chunk where it is air (or, for solid parts, a plant).
        auto place = [&](int32_t x, int32_t y, int32_t z, BlockStateId s, bool solid) {
            const int lx = x - baseX, lz = z - baseZ;
            if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(y)) return;
            const BlockStateId cur = chunk.get(lx, y, lz);
            if (cur == 0 || (solid && (cur == B.shortGrass || cur == B.fern || cur == B.snowLayer))) chunk.set(lx, y, lz, s);
        };
        if (kind == TreeShape::BrownMushroom || kind == TreeShape::RedMushroom) {
            // Huge mushrooms (wiki: Huge Mushroom): a stem, and a flat 7x7 brown cap with
            // cut corners, or a red dome hanging down 3 blocks around the stem's top.
            const int32_t y0 = ground + 1, top = y0 + height - 1;
            for (int32_t y = y0; y < top; ++y)
                place(wx, y, wz, B.stem, true);
            if (kind == TreeShape::BrownMushroom) {
                for (int ox = -3; ox <= 3; ++ox)
                    for (int oz = -3; oz <= 3; ++oz)
                        if (std::abs(ox) + std::abs(oz) < 6) place(wx + ox, top, wz + oz, B.brownCap, true);
            } else {
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oz = -1; oz <= 1; ++oz)
                        place(wx + ox, top, wz + oz, B.redCap, true);
                for (int32_t y = top - 3; y < top; ++y)
                    for (int ox = -2; ox <= 2; ++ox)
                        for (int oz = -2; oz <= 2; ++oz)
                            if ((std::abs(ox) == 2 || std::abs(oz) == 2) && !(std::abs(ox) == 2 && std::abs(oz) == 2))
                                place(wx + ox, y, wz + oz, B.redCap, true);
            }
            continue;
        }
        if (kind == TreeShape::IceSpike) {
            // A packed-ice spike tapering from radius 1-2 (tall ones 3) to a point, its
            // base sunk 2 blocks (wiki: Ice Spikes).
            const int baseR = height >= 30 ? 3 : 1 + static_cast<int>((uint32_t(wx * 31 + wz) >> 3) & 1);
            for (int i = -2; i < height; ++i) {
                const double r = baseR * (1.0 - std::max(0, i) / double(height)) + 0.3;
                for (int ox = -baseR; ox <= baseR; ++ox)
                    for (int oz = -baseR; oz <= baseR; ++oz)
                        if (ox * ox + oz * oz <= r * r) {
                            const int lx = wx + ox - baseX, lz = wz + oz - baseZ;
                            if (lx < 0 || lx > 15 || lz < 0 || lz > 15) continue;
                            const int y = ground + 1 + i;
                            if (kOverworldHeight.contains(y) && chunk.get(lx, y, lz) != B.bedrock)
                                chunk.set(lx, y, lz, B.packedIce);
                        }
            }
            continue;
        }
        BlockStateId log = B.oakLog;
        switch (kind) {
        case TreeShape::Birch: log = B.birchLog; break;
        case TreeShape::Spruce: log = B.spruceLog; break;
        case TreeShape::Acacia: log = B.acaciaLog; break;
        case TreeShape::Jungle:
        case TreeShape::MegaJungle: log = B.jungleLog; break;
        case TreeShape::DarkOak: log = B.darkOakLog; break;
        case TreeShape::Cherry: log = B.cherryLog; break;
        default: break;
        }
        // Writes into this chunk only; logs replace air/leaves/plants, leaves only air.
        // Generated leaves carry their distance to the trunk (vanilla: 1..6, so they
        // never decay).
        const int wood = woodOf(kind);
        auto put = [&](int32_t x, int32_t y, int32_t z, int distance) {
            const bool isLog = distance == 0;
            const BlockStateId s = isLog ? log : B.leaves[wood][distance - 1];
            const int lx = x - baseX, lz = z - baseZ;
            if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(y)) return;
            const BlockStateId cur = chunk.get(lx, y, lz);
            const BlockId curBlock = reg.blockOf(cur);
            if (cur == 0 || (isLog && (curBlock == blocks::OakLeaves || curBlock == blocks::BirchLeaves ||
                                       curBlock == blocks::SpruceLeaves || curBlock == blocks::AcaciaLeaves ||
                                       curBlock == blocks::JungleLeaves || curBlock == blocks::DarkOakLeaves ||
                                       curBlock == blocks::CherryLeaves || cur == B.shortGrass || cur == B.fern))) {
                chunk.set(lx, y, lz, s);
            }
        };
        Xoroshiro shape(chunkSeed(m_seed, wx, wz, 301)); // leaf corners: per tree
        treeShape(static_cast<TreeKind>(kind), wx, ground + 1, wz, height, shape, put);
        // The ground under the trunk becomes dirt (vanilla), in this chunk only; a 2x2
        // trunk gets dirt (or a log down to the ground) under all four.
        const bool wide = twoByTwo(static_cast<TreeKind>(kind));
        for (int k = 0; k < (wide ? 4 : 1); ++k) {
            const int lx = wx + (k & 1) - baseX, lz = wz + (k >> 1) - baseZ;
            if (lx < 0 || lx > 15 || lz < 0 || lz > 15) continue;
            for (int y = ground; y > ground - 4 && wide && chunk.get(lx, y, lz) == 0; --y)
                chunk.set(lx, y, lz, log); // over a dip: the trunk reaches down
            const BlockId g = reg.blockOf(chunk.get(lx, ground, lz));
            if (g == blocks::GrassBlock || g == blocks::Podzol) chunk.set(lx, ground, lz, B.dirt);
        }
    }
}

void OverworldGenerator::generate(Chunk& out) const {
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    static thread_local std::vector<BlockStateId> blockArray(size_t(kOverworldHeight.sections()) * Section::kVolume);
    Buf chunk{blockArray.data()};
    const ChunkPos pos = out.pos();
    const int32_t cx = pos.x, cz = pos.z;
    const int32_t baseX = cx * 16, baseZ = cz * 16;

    // 1. Density corners (terrain, and terrain combined with caves).
    static thread_local std::array<Column, kCornersXZ * kCornersXZ> cornerCols;
    static thread_local std::array<double, kCornersXZ * kCornersY * kCornersXZ> terrain, combined;
    for (int i = 0; i < kCornersXZ; ++i)
        for (int k = 0; k < kCornersXZ; ++k)
            cornerCols[size_t(k * kCornersXZ + i)] = column(baseX + i * kCellW, baseZ + k * kCellW);
    auto ci = [](int i, int j, int k) { return size_t((j * kCornersXZ + k) * kCornersXZ + i); };
    for (int k = 0; k < kCornersXZ; ++k)
        for (int i = 0; i < kCornersXZ; ++i) {
            const Column& col = cornerCols[size_t(k * kCornersXZ + i)];
            const int32_t x = baseX + i * kCellW, z = baseZ + k * kCellW;
            for (int j = 0; j < kCornersY; ++j) {
                const int32_t y = kOverworldHeight.minY + j * kCellH;
                const double t = terrainDensity(x, y, z, col);
                terrain[ci(i, j, k)] = t;
                combined[ci(i, j, k)] = t > 0.0 ? std::min(t, caveDensity(x, y, z, col)) : t;
            }
        }

    // 2. Biomes per 4x4 column (all heights; no cave biomes yet).
    std::array<Biome, 16> columnBiome{};
    std::array<Column, 16> columnCol{};
    for (int qz = 0; qz < 4; ++qz)
        for (int qx = 0; qx < 4; ++qx) {
            columnCol[size_t(qz * 4 + qx)] = column(baseX + qx * 4 + 2, baseZ + qz * 4 + 2);
            columnBiome[size_t(qz * 4 + qx)] = biomeAt(columnCol[size_t(qz * 4 + qx)]);
        }
    auto biomes = std::make_shared<ChunkBiomes>();
    for (int s = 0; s < kOverworldHeight.sections(); ++s)
        for (int qy = 0; qy < 4; ++qy)
            for (int qz = 0; qz < 4; ++qz)
                for (int qx = 0; qx < 4; ++qx)
                    biomes->cells[size_t(ChunkBiomes::index(s, qx, qy, qz))] = columnBiome[size_t(qz * 4 + qx)];

    // Flooded caves (overworld4, our simple aquifers): a column is wet where its terrain
    // height (interpolated from the corner columns, the same in every chunk) is below
    // sea level + 2; cave air below the sea there is water, sealed with stone toward
    // dry columns and above the lava. The corners just outside the chunk are needed
    // for its edge columns' neighbours (only when something here is wet).
    const bool aquifers = m_version >= 4 &&
                          std::any_of(cornerCols.begin(), cornerCols.end(),
                                      [](const Column& c) { return c.height < kSeaLevel + 2.0; });
    std::array<double, kCornersXZ> edgeW{}, edgeE{}, edgeN{}, edgeS{};
    if (aquifers)
        for (int k = 0; k < kCornersXZ; ++k) {
            edgeW[size_t(k)] = column(baseX - kCellW, baseZ + k * kCellW).height;
            edgeE[size_t(k)] = column(baseX + 16 + kCellW, baseZ + k * kCellW).height;
            edgeN[size_t(k)] = column(baseX + k * kCellW, baseZ - kCellW).height;
            edgeS[size_t(k)] = column(baseX + k * kCellW, baseZ + 16 + kCellW).height;
        }
    auto wetAt = [&](int x, int z) { // x, z in -1..16 (not both outside)
        auto corner = [&](int i, int k) { // corner grid index, -1..5
            if (i < 0) return edgeW[size_t(k)];
            if (i >= kCornersXZ) return edgeE[size_t(k)];
            if (k < 0) return edgeN[size_t(i)];
            if (k >= kCornersXZ) return edgeS[size_t(i)];
            return cornerCols[size_t(k * kCornersXZ + i)].height;
        };
        const int i = floorDiv(x, kCellW), k = floorDiv(z, kCellW);
        const double fx = (x - i * kCellW) / double(kCellW), fz = (z - k * kCellW) / double(kCellW);
        const double h = lerp(lerp(corner(i, k), corner(i + 1, k), fx), lerp(corner(i, k + 1), corner(i + 1, k + 1), fx), fz);
        return h < kSeaLevel + 2.0;
    };

    // 3. Fill: stone / deepslate where the density is solid; sea and lava elsewhere.
    std::array<int, 256> topY;
    topY.fill(kOverworldHeight.minY - 1);
    std::array<bool, kOverworldHeight.sections()> sectionAir{};
    for (int s = 0; s < kOverworldHeight.sections(); ++s) {
        const int baseY = kOverworldHeight.minY + s * 16;
        BlockStateId* buffer = blockArray.data() + size_t(s) * Section::kVolume;
        // Interpolation never leaves the range of its corners: a section above the
        // sea whose corner densities are all <= 0 is all air.
        if (baseY >= kSeaLevel) {
            const int j0 = (baseY - kOverworldHeight.minY) / kCellH;
            double maxD = -1e300;
            for (int j = j0; j <= std::min(j0 + 2, kCornersY - 1); ++j)
                for (int k = 0; k < kCornersXZ; ++k)
                    for (int i = 0; i < kCornersXZ; ++i)
                        maxD = std::max(maxD, combined[ci(i, j, k)]);
            if (maxD <= 0.0) {
                std::fill_n(buffer, Section::kVolume, B.air);
                sectionAir[size_t(s)] = true;
                continue;
            }
        }
        for (int ly = 0; ly < 16; ++ly) {
            const int y = baseY + ly;
            const int j = (y - kOverworldHeight.minY) / kCellH;
            const double fy = ((y - kOverworldHeight.minY) % kCellH) / double(kCellH);
            for (int z = 0; z < 16; ++z) {
                const int k = z / kCellW;
                const double fz = (z % kCellW) / double(kCellW);
                for (int x = 0; x < 16; ++x) {
                    const int i = x / kCellW;
                    const double fx = (x % kCellW) / double(kCellW);
                    auto tri = [&](const std::array<double, kCornersXZ * kCornersY * kCornersXZ>& d) {
                        const double c00 = lerp(d[ci(i, j, k)], d[ci(i + 1, j, k)], fx);
                        const double c10 = lerp(d[ci(i, j, k + 1)], d[ci(i + 1, j, k + 1)], fx);
                        const double c01 = lerp(d[ci(i, j + 1, k)], d[ci(i + 1, j + 1, k)], fx);
                        const double c11 = lerp(d[ci(i, j + 1, k + 1)], d[ci(i + 1, j + 1, k + 1)], fx);
                        return lerp(lerp(c00, c10, fz), lerp(c01, c11, fz), fy);
                    };
                    const double d = j + 1 < kCornersY ? tri(combined) : -1.0;
                    BlockStateId b = B.air;
                    const int32_t wx = baseX + x, wz = baseZ + z;
                    if (y == kOverworldHeight.minY || (y <= kOverworldHeight.minY + 4 && positional(m_seed, wx, y, wz, 10) <
                                                         (kOverworldHeight.minY + 5 - y) / 5.0)) {
                        b = B.bedrock; // vanilla: bedrock thins out over y -63..-60
                    } else if (d > 0.0) {
                        const bool deep = y < 0 || (y < 8 && positional(m_seed, wx, y, wz, 11) < (8 - y) / 8.0);
                        b = deep ? B.deepslate : B.stone;
                        topY[size_t(z * 16 + x)] = y;
                    } else if (y < kSeaLevel) {
                        // Open water below sea level; cave air (terrain solid there) stays
                        // dry, or lava at the bottom of the world - flooded under the sea
                        // in overworld4 (flooded caves).
                        const bool cave = j + 1 < kCornersY && tri(terrain) > 0.0;
                        b = cave ? (y < kLavaLevel ? B.lava : B.air) : B.water;
                        if (cave && aquifers && y >= kLavaLevel && wetAt(x, z)) {
                            const bool barrier = y == kLavaLevel || !wetAt(x - 1, z) || !wetAt(x + 1, z) ||
                                                 !wetAt(x, z - 1) || !wetAt(x, z + 1);
                            b = barrier ? (y < 0 ? B.deepslate : B.stone) : B.water;
                        }
                    }
                    buffer[Section::index(x, ly, z)] = b;
                }
            }
        }
    }

    // 4. Surface rules per column (biome-dependent top layers).
    auto top = [&](int x, int z) { return topY[size_t(std::clamp(z, 0, 15) * 16 + std::clamp(x, 0, 15))]; };
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const int ty = top(x, z);
            if (ty < kOverworldHeight.minY + 5) continue;
            const Biome biome = columnBiome[size_t((z / 4) * 4 + x / 4)];
            const int32_t wx = baseX + x, wz = baseZ + z;
            const int depth = 3 + static_cast<int>(std::floor(m_surface.noise2d(wx, wz) * 4.0 + 0.5));
            // Height change over 2 blocks; one-sided at the chunk border (no grid
            // lines of snow/grass on cliffs every 16 blocks).
            auto span = [&](int a0, int a1, bool alongX) {
                const int lo = std::clamp(a0, 0, 15), hi = std::clamp(a1, 0, 15);
                const int l2 = hi - lo == 2 ? lo : (lo == 0 ? 0 : 13), h2 = l2 + 2;
                return alongX ? std::abs(top(h2, z) - top(l2, z)) : std::abs(top(x, h2) - top(x, l2));
            };
            const bool steep = span(x - 1, x + 1, true) >= 4 || span(z - 1, z + 1, false) >= 4;
            const bool underwater = ty < kSeaLevel - 1;
            for (int k = 0; k < 16; ++k) {
                const int y = ty - k;
                const BlockStateId cur = chunk.get(x, y, z);
                if (cur != B.stone && cur != B.deepslate) break; // reached a cave or bedrock
                BlockStateId b = cur;
                if (underwater) {
                    if (k >= depth) break;
                    const bool warm = biome == Biome::WarmOcean || biome == Biome::LukewarmOcean ||
                                      biome == Biome::DeepLukewarmOcean || biome == Biome::Beach ||
                                      biome == Biome::Desert;
                    if (biome == Biome::River || biome == Biome::Swamp)
                        b = positional(m_seed, wx, 0, wz, 12) < 0.15 ? B.clay : (k == 0 ? B.dirt : B.dirt);
                    else
                        b = (warm || ty > kSeaLevel - 8) ? B.sand : B.gravel;
                    if (biome == Biome::River) b = k == 0 ? B.sand : B.gravel;
                } else {
                    switch (biome) {
                    case Biome::Desert:
                        if (k < depth + 1) b = B.sand;
                        else if (k < depth + 4) b = B.sandstone;
                        break;
                    case Biome::Beach:
                    case Biome::SnowyBeach:
                        if (k < depth) b = B.sand;
                        else if (k < depth + 3) b = B.sandstone;
                        break;
                    case Biome::Badlands:
                    case Biome::ErodedBadlands:
                        if (k == 0) b = B.redSand;
                        else if (k < 12) b = B.terracotta;
                        break;
                    case Biome::WoodedBadlands: // grassy, coarse plateau tops over terracotta
                        if (k == 0) b = y >= 95 ? (positional(m_seed, wx, y, wz, 14) < 0.4 ? B.coarseDirt : B.grass) : B.redSand;
                        else if (k < 12) b = y >= 95 && k < 3 ? B.dirt : B.terracotta;
                        break;
                    case Biome::MushroomFields:
                        b = k == 0 ? B.mycelium : k < depth ? B.dirt : cur;
                        break;
                    case Biome::IceSpikes: // snow blocks over dirt (wiki: Ice Spikes)
                        b = k == 0 ? B.snow : k < depth ? B.dirt : cur;
                        break;
                    case Biome::OldGrowthSpruceTaiga: { // podzol and coarse dirt patches
                        const double p = positional(m_seed, wx, 0, wz, 15);
                        b = k == 0 ? (p < 0.55 ? B.podzol : p < 0.7 ? B.coarseDirt : B.grass) : k < depth ? B.dirt : cur;
                        break;
                    }
                    case Biome::StonyShore:
                    case Biome::StonyPeaks:
                        if (biome == Biome::StonyPeaks && k < 2 && positional(m_seed, wx, y, wz, 13) < 0.3)
                            b = B.calcite;
                        break; // stone
                    case Biome::FrozenPeaks:
                    case Biome::JaggedPeaks:
                    case Biome::SnowySlopes:
                        if (!steep && k < 2) b = B.snow;
                        else if (!steep && biome == Biome::FrozenPeaks && k < 4) b = B.packedIce;
                        break;
                    case Biome::WindsweptHills:
                        if (steep) break;
                        b = k == 0 ? B.grass : k < depth ? B.dirt : cur;
                        break;
                    default:
                        if (steep && (biome == Biome::Meadow || biome == Biome::Grove)) break;
                        b = k == 0 ? B.grass : k < depth ? B.dirt : cur;
                        break;
                    }
                }
                if (b != cur) chunk.set(x, y, z, b);
            }
        }

    // 4b. Overworld 2 (M18.1): ravines are carved after the surface (vanilla carvers),
    //     then lava lakes (vanilla's first feature step).
    if (m_version >= 2) {
        carveRavines(blockArray.data(), cx, cz, topY);
        placeLavaLakes(blockArray.data(), cx, cz, topY);
    }
    // 4c. Dungeons and mineshafts (vanilla's underground structures step, before ores).
    GeneratedEntities entities;
    if (m_version >= 2) {
        placeDungeons(blockArray.data(), cx, cz, *std::max_element(topY.begin(), topY.end()), entities);
        placeMineshafts(blockArray.data(), cx, cz, entities);
        placeStrongholds(blockArray.data(), cx, cz, entities);
    }

    // 5. Ores and stone blobs (wiki: Ore - attempts per chunk, sizes, height ranges).
    Xoroshiro ores(chunkSeed(m_seed, cx, cz, 200));
    const int maxTop = *std::max_element(topY.begin(), topY.end());
    // Ores replace the overworld's base stones: stone and its granite/diorite/andesite
    // blobs get the stone variant, deepslate and tuff the deepslate one (wiki: each
    // ore's "replaceable" blocks).
    auto replaceAt = [&](int x, int y, int z, const Blocks::Ore& ore) {
        if (x < 0 || x > 15 || z < 0 || z > 15 || !kOverworldHeight.contains(y)) return;
        const BlockStateId cur = chunk.get(x, y, z);
        if (cur == B.stone || cur == B.granite || cur == B.diorite || cur == B.andesite)
            chunk.set(x, y, z, ore.stone);
        else if (cur == B.deepslate || cur == B.tuff)
            chunk.set(x, y, z, ore.deep);
    };
    // A blob along a short random line with spheres of varying radius (the classic
    // ore-vein shape), clipped to this chunk.
    auto vein = [&](const Blocks::Ore& ore, int size, int y) {
        const double cxl = ores.nextInt(16), czl = ores.nextInt(16);
        const double angle = ores.nextFloat() * std::numbers::pi;
        const double len = size / 7.0; // our proportions
        const double x1 = cxl + std::sin(angle) * len, x2 = cxl - std::sin(angle) * len;
        const double z1 = czl + std::cos(angle) * len, z2 = czl - std::cos(angle) * len;
        const double y1 = y + static_cast<int>(ores.nextInt(3)) - 1, y2 = y + static_cast<int>(ores.nextInt(3)) - 1;
        for (int n = 0; n < size; ++n) {
            const double t = n / double(size);
            const double px = lerp(x1, x2, t), py = lerp(y1, y2, t), pz = lerp(z1, z2, t);
            const double r = ((std::sin(std::numbers::pi * t) + 1.0) * ores.nextDouble() * size / 15.0 + 1.0) / 2.0;
            // Clipped to this chunk and to the stone (ores replace nothing above it).
            const int bx0 = std::max(0, int(std::floor(px - r))), bx1 = std::min(15, int(std::floor(px + r)));
            const int by0 = std::max(kOverworldHeight.minY, int(std::floor(py - r))), by1 = std::min(maxTop, int(std::floor(py + r)));
            const int bz0 = std::max(0, int(std::floor(pz - r))), bz1 = std::min(15, int(std::floor(pz + r)));
            for (int bx = bx0; bx <= bx1; ++bx)
                for (int by = by0; by <= by1; ++by)
                    for (int bz = bz0; bz <= bz1; ++bz) {
                        const double dx = (bx + 0.5 - px) / r, dy = (by + 0.5 - py) / r, dz = (bz + 0.5 - pz) / r;
                        if (dx * dx + dy * dy + dz * dz < 1.0) replaceAt(bx, by, bz, ore);
                    }
        }
    };
    auto uniform = [&](int lo, int hi) { return lo + static_cast<int>(ores.nextInt(uint32_t(hi - lo + 1))); };
    auto triangle = [&](int lo, int hi) { // peak in the middle (vanilla "trapezoid" ranges)
        return lo + static_cast<int>((ores.nextFloat() + ores.nextFloat()) * 0.5f * float(hi - lo));
    };
    auto spread = [&](const Blocks::Ore& ore, int attempts, int size, auto height) {
        for (int i = 0; i < attempts; ++i)
            vein(ore, size, height());
    };
    const Blocks::Ore graniteOre{B.granite, B.granite}, dioriteOre{B.diorite, B.diorite},
        andesiteOre{B.andesite, B.andesite}, tuffOre{B.tuff, B.tuff}, dirtOre{B.dirt, B.dirt},
        gravelOre{B.gravel, B.gravel};
    // Stone blobs (wiki: Andesite/Tuff - 2 per chunk, up to ~860 blocks; plus a 1/6
    // chance of an upper blob at 64..128), dirt 7x and gravel 14x (wiki: Dirt, Gravel).
    for (const Blocks::Ore* blob : {&graniteOre, &dioriteOre, &andesiteOre}) {
        spread(*blob, 2, 64, [&] { return uniform(0, 60); });
        if (ores.nextInt(6) == 0) spread(*blob, 1, 64, [&] { return uniform(64, 128); });
    }
    spread(tuffOre, 2, 64, [&] { return uniform(-64, 0); });
    spread(dirtOre, 7, 33, [&] { return uniform(0, 160); });
    spread(gravelOre, 14, 33, [&] { return uniform(-64, 320); });
    spread(B.coal, 30, 17, [&] { return uniform(136, 320); });
    spread(B.coal, 20, 17, [&] { return triangle(0, 192); });
    spread(B.iron, 90, 9, [&] { return triangle(80, 384); });
    spread(B.iron, 10, 9, [&] { return triangle(-24, 56); });
    spread(B.iron, 10, 4, [&] { return uniform(-64, 72); });
    spread(B.copper, 16, 10, [&] { return triangle(-16, 112); });
    spread(B.gold, 4, 9, [&] { return triangle(-64, 32); });
    if (ores.nextInt(2) == 0) spread(B.gold, 1, 9, [&] { return uniform(-64, -48); });
    spread(B.redstone, 4, 8, [&] { return uniform(-64, 15); });
    spread(B.redstone, 8, 8, [&] { return triangle(-96, -32); });
    spread(B.lapis, 2, 7, [&] { return triangle(-32, 32); });
    spread(B.lapis, 4, 7, [&] { return uniform(-64, 64); });
    spread(B.diamond, 7, 4, [&] { return triangle(-144, 16); });
    if (ores.nextInt(9) == 0) spread(B.diamond, 1, 12, [&] { return triangle(-144, 16); });
    spread(B.diamond, 4, 8, [&] { return triangle(-144, 16); }); // buried batch
    spread(B.diamond, 2, 8, [&] { return uniform(-63, -4); });   // medium batch
    if (columnBiome[5] == Biome::Badlands) spread(B.gold, 50, 9, [&] { return uniform(32, 256); });
    const Biome centreBiome = columnBiome[5];
    if (centreBiome == Biome::WindsweptHills || centreBiome == Biome::StonyPeaks ||
        centreBiome == Biome::JaggedPeaks || centreBiome == Biome::FrozenPeaks ||
        centreBiome == Biome::Meadow || centreBiome == Biome::Grove || centreBiome == Biome::SnowySlopes)
        spread(B.emerald, 100, 3, [&] { return triangle(-16, 480); });

    // 5b. Springs (vanilla's fluid springs step, after ores).
    if (m_version >= 2) placeSprings(blockArray.data(), out, cx, cz, maxTop);

    // 6. Trees: this chunk's and its neighbours' (their canopies reach in).
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            placeTrees(blockArray.data(), pos, cx + dx, cz + dz);

    // 7. Plants on the surface (this chunk only).
    Xoroshiro plants(chunkSeed(m_seed, cx, cz, 400));
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const float roll = plants.nextFloat();
            const uint32_t pick = plants.nextInt(1000);
            const int ty = top(x, z);
            if (ty < kSeaLevel - 1 || !kOverworldHeight.contains(ty + 1) || chunk.get(x, ty + 1, z) != B.air) continue;
            const BlockStateId ground = chunk.get(x, ty, z);
            const Biome biome = columnBiome[size_t((z / 4) * 4 + x / 4)];
            BlockStateId plant = 0;
            if (ground == B.grass || ground == B.snowyGrass || ground == B.podzol) {
                float grassChance = 0.0f, flowerChance = 0.0f;
                switch (biome) {
                case Biome::Plains: grassChance = 0.35f; flowerChance = 0.03f; break;
                case Biome::Meadow: grassChance = 0.45f; flowerChance = 0.08f; break;
                case Biome::Savanna: grassChance = 0.4f; break;
                case Biome::Forest:
                case Biome::BirchForest: grassChance = 0.15f; flowerChance = 0.02f; break;
                case Biome::Taiga:
                case Biome::SnowyTaiga: grassChance = 0.12f; break;
                case Biome::Swamp:
                case Biome::WindsweptHills: grassChance = 0.1f; break;
                case Biome::Jungle:
                case Biome::SparseJungle: grassChance = 0.4f; flowerChance = 0.01f; break;
                case Biome::DarkForest: grassChance = 0.12f; flowerChance = 0.01f; break;
                case Biome::FlowerForest: grassChance = 0.1f; flowerChance = 0.25f; break;
                case Biome::CherryGrove: grassChance = 0.35f; flowerChance = 0.02f; break;
                case Biome::OldGrowthSpruceTaiga: grassChance = 0.25f; break;
                default: grassChance = 0.05f; break;
                }
                if (roll < flowerChance) {
                    const bool many = biome == Biome::Plains || biome == Biome::Meadow || biome == Biome::FlowerForest;
                    plant = B.flowers[pick % (many ? 5 : 2)];
                } else if (roll < flowerChance + grassChance) {
                    const bool ferns = biome == Biome::Taiga || biome == Biome::SnowyTaiga ||
                                       biome == Biome::OldGrowthSpruceTaiga || biome == Biome::Jungle ||
                                       biome == Biome::SparseJungle;
                    plant = ferns && pick % 2 ? B.fern : B.shortGrass;
                }
            } else if ((ground == B.sand && biome == Biome::Desert) || ground == B.redSand || ground == B.terracotta) {
                if (roll < 0.01f) plant = B.deadBush;
            }
            if (plant) chunk.set(x, ty + 1, z, plant);
        }

    // 7b. More vegetation (M18.1): sugar cane, pumpkins, cacti, mushrooms.
    if (m_version >= 2) placeVegetation(blockArray.data(), cx, cz, topY, columnBiome);
    // 7c. Surface structures (ours after the plants, so no tree grows through them).
    if (m_version >= 4) placeOceanFloor(blockArray.data(), cx, cz, topY, columnBiome);

    if (m_version >= 2) {
        placeStructures(blockArray.data(), cx, cz, entities);
        placeVillages(blockArray.data(), cx, cz, topY, entities);
    }

    // 8. Top layer (vanilla's last feature step): where the temperature at the top
    //    block is below 0.15 - the biome's, minus 1/800 per block above y 80 (wiki:
    //    Biome › Temperature) - water freezes and sky-exposed ground gets a snow
    //    layer (grass under it becomes snowy).
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            int y = kOverworldHeight.maxY();
            while (y > kOverworldHeight.minY && chunk.get(x, y, z) == B.air)
                --y;
            const Biome biome = columnBiome[size_t((z / 4) * 4 + x / 4)];
            const float temp = (biome == Biome::DeepFrozenOcean ? 0.0f : biomeInfo(biome).temperature) -
                               std::max(0, y - 80) / 800.0f; // (deep frozen oceans: frozen by their surface rule)
            if (temp >= 0.15f) continue;
            const BlockStateId surface = chunk.get(x, y, z);
            if (surface == B.water) {
                chunk.set(x, y, z, B.ice);
            } else if (y + 1 <= kOverworldHeight.maxY() && reg.collides(surface)) {
                chunk.set(x, y + 1, z, B.snowLayer);
                if (surface == B.grass) chunk.set(x, y, z, B.snowyGrass);
            }
        }

    // 9. Encode the sections once (all-air sections stay empty).
    for (int sec = 0; sec < kOverworldHeight.sections(); ++sec) {
        const BlockStateId* b = blockArray.data() + size_t(sec) * Section::kVolume;
        if (sectionAir[size_t(sec)] && std::all_of(b, b + Section::kVolume, [](BlockStateId v) { return v == 0; }))
            out.mutableSection(sec).fill(0);
        else
            out.mutableSection(sec).assign(b);
    }
    out.setBiomes(biomes);
    for (int i = 0; i < entities.count; ++i) {
        const GeneratedEntity& e = entities.list[size_t(i)];
        if (e.spawnMob) { // (M24.4) a mob that lives here (swamp hut witch), never despawning
            Xoroshiro mr(chunkSeed(m_seed, cx, cz, 680 + uint64_t(i)));
            MobData mob;
            mob.type = e.mob;
            mob.uuidHi = (mr.nextLong() & ~0xF000ull) | 0x4000ull;
            mob.uuidLo = (mr.nextLong() & ~(3ull << 62)) | (2ull << 62);
            mob.pos = mob.prevPos = mob.goal = glm::dvec3(baseX + e.x + 0.5, e.y + 0.05, baseZ + e.z + 0.5);
            mob.yaw = mob.prevYaw = mob.headYaw = mob.prevHeadYaw = mr.nextFloat() * 360.0f - 180.0f;
            mob.health = mobInfo(e.mob).maxHealth;
            mob.persistent = true;
            out.mobs().push_back(mob);
            continue;
        }
        if (e.villager) { // (M24.1) a villager of the village, standing on its floor
            Xoroshiro vr(chunkSeed(m_seed, cx, cz, 660 + uint64_t(i)));
            MobData v;
            v.type = MobType::Villager;
            v.uuidHi = (vr.nextLong() & ~0xF000ull) | 0x4000ull;
            v.uuidLo = (vr.nextLong() & ~(3ull << 62)) | (2ull << 62);
            v.pos = v.prevPos = v.goal = glm::dvec3(baseX + e.x + 0.5, e.y + 0.6, baseZ + e.z + 0.5);
            v.yaw = v.prevYaw = v.headYaw = v.prevHeadYaw = vr.nextFloat() * 360.0f - 180.0f;
            v.health = mobInfo(MobType::Villager).maxHealth;
            v.villagerType = e.villagerType;
            v.profession = uint8_t(e.nitwit ? Profession::Nitwit : Profession::None);
            v.persistent = true;
            v.poiSearch = int16_t(vr.nextInt(40));
            out.mobs().push_back(v);
            continue;
        }
        if (out.chest(e.x, e.y, e.z) || out.spawner(e.x, e.y, e.z) || out.furnace(e.x, e.y, e.z)) continue; // (once)
        const BlockId here = reg.blockOf(out.get(e.x, e.y, e.z));
        if (e.furnace) {
            if (here == blocks::Furnace) out.addFurnace(e.x, e.y, e.z);
            continue;
        }
        if (here == (e.chest ? blocks::Chest : blocks::Spawner)) { // (not overwritten since; any facing)
            if (e.chest) {
                Xoroshiro loot(chunkSeed(m_seed, cx, cz, 640 + uint64_t(i)));
                fillChest(e.loot, loot, out.addChest(e.x, e.y, e.z).items);
            } else {
                out.addSpawner(e.x, e.y, e.z).mob = e.mob;
            }
        }
    }

    // 10. Animals with new chunks (wiki: Spawn › Chunk generation): cow biomes get a
    //     herd in 1 of 10 chunks, standing on grass (our rate: see deviations).
    Xoroshiro animals(chunkSeed(m_seed, cx, cz, 500));
    const Biome herdBiome = columnBiome[5];
    // Cow biomes (wiki: Cow › Spawning): not meadows; swamps and snowy taigas too.
    const bool grassy = herdBiome == Biome::Plains || herdBiome == Biome::Forest || herdBiome == Biome::BirchForest ||
                        herdBiome == Biome::Taiga || herdBiome == Biome::SnowyTaiga || herdBiome == Biome::Savanna ||
                        herdBiome == Biome::WindsweptHills || herdBiome == Biome::Swamp ||
                        herdBiome == Biome::FlowerForest || herdBiome == Biome::DarkForest ||
                        herdBiome == Biome::CherryGrove || herdBiome == Biome::OldGrowthSpruceTaiga ||
                        herdBiome == Biome::Jungle || herdBiome == Biome::SparseJungle;
    if (grassy && animals.nextInt(10) == 0) {
        // A group of 4 spread +-5 blocks around a random spot, inside this chunk
        // (wiki: Mob spawning › Chunk generation).
        const int centreX = static_cast<int>(animals.nextInt(16)), centreZ = static_cast<int>(animals.nextInt(16));
        // Which animal (M16.3): vanilla's passive spawn weights for grassy biomes -
        // sheep 12, pig 10, chicken 10, cow 8 (wiki: Spawn › Java Edition). A separate
        // stream, so herd positions stay as they were before more kinds existed.
        Xoroshiro kindRng(chunkSeed(m_seed, cx, cz, 501));
        const uint32_t roll = kindRng.nextInt(40);
        const MobType kind = roll < 12 ? MobType::Sheep : roll < 22 ? MobType::Pig : roll < 32 ? MobType::Chicken : MobType::Cow;
        for (int i = 0; i < 4; ++i) {
            const int x = centreX + static_cast<int>(animals.nextInt(11)) - 5;
            const int z = centreZ + static_cast<int>(animals.nextInt(11)) - 5;
            const uint64_t hi = animals.nextLong(), lo = animals.nextLong();
            const float yaw = animals.nextFloat() * 360.0f - 180.0f;
            if (x < 0 || x > 15 || z < 0 || z > 15) continue;
            const int y = top(x, z);
            const BlockStateId above = chunk.get(x, y + 1, z);
            if (y < kSeaLevel || blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::GrassBlock ||
                (above != B.air && blockRegistry().blockOf(above) != blocks::Snow))
                continue;
            MobData cow;
            cow.type = kind;
            if (kind == MobType::Sheep) {
                // wiki: Sheep › Spawning - white 81.836%, black/gray/light gray 5%, brown 3%, pink 0.164%
                const double r = kindRng.nextDouble() * 100.0;
                cow.woolColour = r < 5.0 ? 15 : r < 10.0 ? 7 : r < 15.0 ? 8 : r < 18.0 ? 12 : r < 18.164 ? 6 : 0;
            }
            cow.uuidHi = hi;
            cow.uuidLo = lo;
            cow.pos = cow.prevPos = cow.goal = glm::dvec3(baseX + x + 0.5, y + 1.0, baseZ + z + 0.5);
            cow.yaw = cow.prevYaw = cow.headYaw = cow.prevHeadYaw = yaw;
            cow.health = mobInfo(kind).maxHealth;
            if (kind == MobType::Chicken) cow.eggTicks = 6000 + static_cast<int>(kindRng.nextInt(6000));
            cow.persistent = true; // animals from world generation never despawn
            out.mobs().push_back(cow);
        }
    }
    // Turtles (overworld4, M25.3b; wiki: Turtle › Spawning): 2-5 on the sand of a beach in
    // 1 of 10 beach chunks, each at home where it stands (its own random stream).
    if (m_version >= 4 && herdBiome == Biome::Beach) {
        Xoroshiro tr(chunkSeed(m_seed, cx, cz, 502));
        if (tr.nextInt(10) == 0) {
            const int n = 2 + int(tr.nextInt(4));
            for (int i = 0; i < n; ++i) {
                const int x = int(tr.nextInt(16)), z = int(tr.nextInt(16));
                const uint64_t hi = tr.nextLong(), lo = tr.nextLong();
                const float yaw = tr.nextFloat() * 360.0f - 180.0f;
                const int y = top(x, z);
                if (y < kSeaLevel - 1 || blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Sand ||
                    chunk.get(x, y + 1, z) != B.air)
                    continue;
                MobData t;
                t.type = MobType::Turtle;
                t.uuidHi = (hi & ~0xF000ull) | 0x4000ull;
                t.uuidLo = (lo & ~(3ull << 62)) | (2ull << 62);
                t.pos = t.prevPos = t.goal = glm::dvec3(baseX + x + 0.5, y + 1.0, baseZ + z + 0.5);
                t.yaw = t.prevYaw = t.headYaw = t.prevHeadYaw = yaw;
                t.health = mobInfo(MobType::Turtle).maxHealth;
                t.home = {baseX + x, y + 1, baseZ + z};
                t.persistent = true;
                out.mobs().push_back(t);
            }
        }
    }
}

// --- Overworld 2 features (M18.1) ----------------------------------------------------
// Vanilla's numbers for these features are game data (not on the wiki), so the rates
// and sizes below are our own tuning after the classic feature shapes; see
// game-design.md › deviations.

bool OverworldGenerator::ravine(int32_t cx, int32_t cz, Ravine& out) const {
    Xoroshiro r(chunkSeed(m_seed, cx, cz, 600));
    out.count = 0;
    if (r.nextInt(100) != 0) return false; // 1 chunk in 100 starts one
    double x = cx * 16 + static_cast<int>(r.nextInt(16)); // (signed: negative chunks)
    double y = 10 + static_cast<int>(r.nextInt(58));      // y 10..67
    double z = cz * 16 + static_cast<int>(r.nextInt(16));
    float yaw = r.nextFloat() * 2.0f * std::numbers::pi_v<float>;
    float pitch = (r.nextFloat() - 0.5f) * 0.25f;
    const float thickness = (r.nextFloat() * 2.0f + r.nextFloat()) * 2.0f; // 0..6, mostly ~3
    const int length = kRavineSteps - static_cast<int>(r.nextInt(28));
    // Walls: the horizontal size wobbles per y in bands of 1-3 blocks.
    for (size_t i = 0; i < out.rough.size(); ++i)
        out.rough[i] = (i == 0 || r.nextInt(3) == 0) ? 1.0f + r.nextFloat() * r.nextFloat() : out.rough[i - 1];
    float dYaw = 0.0f, dPitch = 0.0f;
    for (int step = 0; step < length; ++step) {
        // Widest in the middle, 3x as tall as wide (a deep slot).
        float h = 1.5f + std::sin(float(step) * std::numbers::pi_v<float> / float(length)) * thickness;
        float v = h * 3.0f;
        h *= r.nextFloat() * 0.25f + 0.75f;
        v *= r.nextFloat() * 0.25f + 0.75f;
        const float cp = std::cos(pitch);
        x += std::cos(yaw) * cp;
        y += std::sin(pitch);
        z += std::sin(yaw) * cp;
        pitch = pitch * 0.7f + dPitch * 0.05f;
        yaw += dYaw * 0.05f;
        dPitch = dPitch * 0.8f + (r.nextFloat() - r.nextFloat()) * r.nextFloat() * 2.0f;
        dYaw = dYaw * 0.5f + (r.nextFloat() - r.nextFloat()) * r.nextFloat() * 4.0f;
        if (r.nextInt(4) == 0) continue; // skipped steps roughen the floor and walls
        out.steps[size_t(out.count++)] = {float(x), float(y), float(z), h, v};
    }
    return true;
}

namespace {

// Inside one ravine step: a squashed ellipsoid whose width varies per y.
bool inStep(const OverworldGenerator::RavineStep& st, const std::array<float, kOverworldHeight.height>& rough,
            int32_t x, int32_t y, int32_t z) {
    const float dx = (float(x) + 0.5f - st.x) / st.h, dz = (float(z) + 0.5f - st.z) / st.h;
    const float dy = (float(y) + 0.5f - st.y) / st.v;
    return (dx * dx + dz * dz) * rough[size_t(y - kOverworldHeight.minY)] + dy * dy / 6.0f < 1.0f;
}

} // namespace

bool OverworldGenerator::inRavine(int32_t x, int32_t y, int32_t z) const {
    if (!kOverworldHeight.contains(y)) return false;
    static thread_local Ravine rv;
    const int32_t cx = x >> 4, cz = z >> 4;
    for (int32_t sz = cz - kRavineReach; sz <= cz + kRavineReach; ++sz)
        for (int32_t sx = cx - kRavineReach; sx <= cx + kRavineReach; ++sx) {
            if (!ravine(sx, sz, rv)) continue;
            for (int i = 0; i < rv.count; ++i)
                if (inStep(rv.steps[size_t(i)], rv.rough, x, y, z)) return true;
        }
    return false;
}

void OverworldGenerator::carveRavines(BlockStateId* blocks, int32_t cx, int32_t cz, std::array<int, 256>& topY) const {
    const Blocks& B = blockSet();
    Buf chunk{blocks};
    const int32_t baseX = cx * 16, baseZ = cz * 16;
    static thread_local Ravine rv;
    bool carved = false;
    for (int32_t sz = cz - kRavineReach; sz <= cz + kRavineReach; ++sz)
        for (int32_t sx = cx - kRavineReach; sx <= cx + kRavineReach; ++sx) {
            if (!ravine(sx, sz, rv)) continue;
            for (int i = 0; i < rv.count; ++i) {
                const RavineStep& st = rv.steps[size_t(i)];
                const int x0 = std::max(0, int(std::floor(st.x - st.h)) - baseX);
                const int x1 = std::min(15, int(std::floor(st.x + st.h)) - baseX);
                const int z0 = std::max(0, int(std::floor(st.z - st.h)) - baseZ);
                const int z1 = std::min(15, int(std::floor(st.z + st.h)) - baseZ);
                if (x0 > x1 || z0 > z1) continue;
                const int y0 = std::max(kOverworldHeight.minY + 1, int(std::floor(st.y - st.v)));
                const int y1 = std::min(kOverworldHeight.maxY(), int(std::floor(st.y + st.v)));
                // A step that reaches the floor of a sea or river fills with water below
                // the sea level instead of air (vanilla's flooded ravines). Decided from
                // the step alone, so every chunk it crosses agrees (no seams).
                const Column col = column(int32_t(std::floor(st.x)), int32_t(std::floor(st.z)));
                const bool wet = col.height < kSeaLevel - 0.5 && st.y + st.v >= col.height - 2.0;
                for (int x = x0; x <= x1; ++x)
                    for (int z = z0; z <= z1; ++z)
                        for (int y = y1; y >= y0; --y) { // top down: grass moves onto the dirt below
                            if (!inStep(st, rv.rough, baseX + x, y, baseZ + z)) continue;
                            const BlockStateId cur = chunk.get(x, y, z);
                            if (cur == B.air || cur == B.bedrock || cur == B.lava || cur == B.water) continue;
                            // Below the lava level carvers leave lava (as caves).
                            chunk.set(x, y, z, y < kLavaLevel ? B.lava : wet && y < kSeaLevel ? B.water : B.air);
                            if (cur == B.grass && chunk.get(x, y - 1, z) == B.dirt) chunk.set(x, y - 1, z, B.grass);
                            carved = true;
                        }
            }
        }
    if (!carved) return;
    // The surface may have been cut: drop each column's top to its new highest solid.
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            int& t = topY[size_t(z * 16 + x)];
            while (t > kOverworldHeight.minY && !blockRegistry().collides(chunk.get(x, t, z)))
                --t;
        }
}

void OverworldGenerator::placeLavaLakes(BlockStateId* blocks, int32_t cx, int32_t cz,
                                        const std::array<int, 256>& topY) const {
    // Lava lakes (wiki: Lava lake): small, shallow pools with an air pocket above,
    // stone around the lava; rare at the surface, more common underground above y 0.
    // Ours fit inside the chunk: a 16x8x16 box of 4-7 overlapping ellipsoids, lava in
    // its lower 4 layers.
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    Buf chunk{blocks};
    Xoroshiro r(chunkSeed(m_seed, cx, cz, 610));
    const int minTop = *std::min_element(topY.begin(), topY.end());
    for (int kind = 0; kind < 2; ++kind) {
        const bool surface = kind == 0;
        if (r.nextInt(surface ? 200 : 9) != 0) continue;
        int baseY;
        if (surface) {
            baseY = topY[8 * 16 + 8] - 4; // the lava's top layer just under the ground
            if (baseY + 4 < kSeaLevel) continue;
        } else {
            if (minTop - 12 <= 0) continue;
            baseY = static_cast<int>(r.nextInt(uint32_t(minTop - 12))); // above y 0, under the surface
        }
        std::array<bool, 16 * 16 * 8> shape{};
        auto cell = [&](int x, int y, int z) -> bool {
            return x >= 0 && x < 16 && z >= 0 && z < 16 && y >= 0 && y < 8 && shape[size_t((y * 16 + z) * 16 + x)];
        };
        const int blobs = 4 + static_cast<int>(r.nextInt(4));
        for (int b = 0; b < blobs; ++b) {
            const double xs = r.nextDouble() * 6.0 + 3.0, ys = r.nextDouble() * 4.0 + 2.0, zs = r.nextDouble() * 6.0 + 3.0;
            const double px = r.nextDouble() * (16.0 - xs - 2.0) + 1.0 + xs / 2.0;
            const double py = r.nextDouble() * (8.0 - ys - 4.0) + 2.0 + ys / 2.0;
            const double pz = r.nextDouble() * (16.0 - zs - 2.0) + 1.0 + zs / 2.0;
            for (int y = 1; y < 7; ++y)
                for (int z = 1; z < 15; ++z)
                    for (int x = 1; x < 15; ++x) {
                        const double dx = (x - px) / (xs / 2.0), dy = (y - py) / (ys / 2.0), dz = (z - pz) / (zs / 2.0);
                        if (dx * dx + dy * dy + dz * dz < 1.0) shape[size_t((y * 16 + z) * 16 + x)] = true;
                    }
        }
        // The rim: no fluid around the air pocket, solid ground around the lava.
        bool ok = true;
        for (int y = 0; y < 8 && ok; ++y)
            for (int z = 0; z < 16 && ok; ++z)
                for (int x = 0; x < 16 && ok; ++x) {
                    if (cell(x, y, z)) continue;
                    const bool edge = cell(x + 1, y, z) || cell(x - 1, y, z) || cell(x, y + 1, z) || cell(x, y - 1, z) ||
                                      cell(x, y, z + 1) || cell(x, y, z - 1);
                    if (!edge) continue;
                    const BlockStateId s = chunk.get(x, baseY + y, z);
                    if (s == B.water || s == B.lava) ok = false;
                    else if (y < 4 && !reg.collides(s)) ok = false;
                }
        if (!ok) continue;
        for (int y = 0; y < 8; ++y)
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    if (cell(x, y, z)) chunk.set(x, baseY + y, z, y < 4 ? B.lava : B.air);
        // Solid blocks touching the lava (beside or under it) turn to stone.
        auto lavaCell = [&](int x, int y, int z) { return y >= 0 && y < 4 && cell(x, y, z); };
        for (int y = 0; y < 4; ++y)
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    if (cell(x, y, z)) continue;
                    if (!lavaCell(x + 1, y, z) && !lavaCell(x - 1, y, z) && !lavaCell(x, y, z + 1) &&
                        !lavaCell(x, y, z - 1) && !lavaCell(x, y + 1, z))
                        continue;
                    const BlockStateId s = chunk.get(x, baseY + y, z);
                    if (reg.collides(s) && s != B.bedrock) chunk.set(x, baseY + y, z, B.stone);
                }
    }
}

void OverworldGenerator::placeSprings(BlockStateId* blocks, Chunk& out, int32_t cx, int32_t cz, int maxTop) const {
    // Springs (wiki: Spring): a lone fluid source in a wall or a cave's dead end -
    // stone above and below, stone on 3 sides and one open side - flowing out. Water up to y 192, lava
    // biased to the bottom. Ours: 50 water and 40 lava attempts per chunk (most land
    // in solid rock or above the ground and do nothing).
    const Blocks& B = blockSet();
    Buf chunk{blocks};
    Xoroshiro r(chunkSeed(m_seed, cx, cz, 620));
    auto rock = [&](BlockStateId s) {
        return s == B.stone || s == B.deepslate || s == B.granite || s == B.diorite || s == B.andesite || s == B.tuff;
    };
    uint64_t order = 0;
    for (int i = 0; i < 90; ++i) {
        const bool lava = i >= 50;
        const int x = 1 + static_cast<int>(r.nextInt(14)), z = 1 + static_cast<int>(r.nextInt(14));
        int y;
        if (lava) { // very biased to the bottom
            const uint32_t a = r.nextInt(uint32_t(kOverworldHeight.height - 8)) + 8;
            const uint32_t b = r.nextInt(a) + 8;
            y = kOverworldHeight.minY + static_cast<int>(r.nextInt(b));
        } else {
            y = kOverworldHeight.minY + static_cast<int>(r.nextInt(192 - kOverworldHeight.minY));
        }
        if (y <= kOverworldHeight.minY + 4 || y >= maxTop) continue;
        const BlockStateId here = chunk.get(x, y, z);
        if ((here != B.air && !rock(here)) || !rock(chunk.get(x, y + 1, z)) || !rock(chunk.get(x, y - 1, z))) continue;
        int rocks = 0, holes = 0;
        for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}}) {
            const BlockStateId n = chunk.get(x + d[0], y, z + d[1]);
            rocks += rock(n);
            holes += n == B.air;
        }
        if (rocks != 3 || holes != 1) continue;
        chunk.set(x, y, z, lava ? B.lava : B.water);
        // It starts flowing once the chunk is in the world (a pending fluid tick:
        // water 5, lava 30 ticks; a delay until the chunk is placed).
        out.blockTicks().push_back({static_cast<int8_t>(x), static_cast<int8_t>(z), static_cast<int16_t>(y), 0,
                                    lava ? blocks::Lava : blocks::Water, lava ? 30 : 5, order++});
    }
    if (!out.blockTicks().empty()) out.ticksRelative = true;
}

void OverworldGenerator::placeDungeons(BlockStateId* blocks, int32_t cx, int32_t cz, int maxTop,
                                       GeneratedEntities& out) const {
    // Monster rooms (wiki: Monster Room): 14 attempts a chunk - 10 at y 0..320, 4 at
    // y -58..-1; a room 7 or 9 wide each way (5 or 7 inside), 6 tall with floor and
    // ceiling, placed only where floor and ceiling are solid and its walls have 1-5
    // two-high openings at floor level. Walls cobblestone, floor 75% mossy, a spawner
    // in the middle (zombie 50%, skeleton 25%, spider 25%), 2 chests (3 tries each) on
    // a floor spot touching exactly one wall. Ours fit inside the chunk.
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    Buf chunk{blocks};
    Xoroshiro r(chunkSeed(m_seed, cx, cz, 650));
    for (int attempt = 0; attempt < 14; ++attempt) {
        if (out.count + 3 > int(out.list.size())) break; // room for a room's chests and spawner
        const int rx = 2 + static_cast<int>(r.nextInt(2)), rz = 2 + static_cast<int>(r.nextInt(2));
        const int ox = rx + 1 + static_cast<int>(r.nextInt(uint32_t(14 - 2 * rx))); // walls inside 0..15
        const int oz = rz + 1 + static_cast<int>(r.nextInt(uint32_t(14 - 2 * rz)));
        const int yTry = attempt < 10 ? static_cast<int>(r.nextInt(321)) : -58 + static_cast<int>(r.nextInt(58));
        const uint64_t roomSeed = r.nextLong();
        // Our noise caves leave fewer spots that fit vanilla's opening rule than its
        // carver caves do, so each attempt looks 24 heights down from its pick (ours).
        int y0 = 0;
        bool found = false;
        for (int s = 0; s < 24 && !found; ++s) {
            y0 = std::min(yTry, maxTop - 5) - s;
            if (y0 - 1 <= kOverworldHeight.minY || (attempt >= 10 && y0 < -58)) break;
            // Openings on the wall ring first (cheap to reject in solid rock), then a
            // solid floor and ceiling under and over the whole room.
            int openings = 0;
            for (int dx = -rx - 1; dx <= rx + 1 && openings <= 5; ++dx)
                for (int dz = -rz - 1; dz <= rz + 1; ++dz) {
                    if (std::abs(dx) != rx + 1 && std::abs(dz) != rz + 1) continue;
                    const int x = ox + dx, z = oz + dz;
                    if (chunk.get(x, y0, z) == B.air && chunk.get(x, y0 + 1, z) == B.air) ++openings;
                }
            if (openings < 1 || openings > 5) continue;
            bool ok = true;
            for (int dx = -rx - 1; dx <= rx + 1 && ok; ++dx)
                for (int dz = -rz - 1; dz <= rz + 1 && ok; ++dz)
                    ok = reg.collides(chunk.get(ox + dx, y0 - 1, oz + dz)) && reg.collides(chunk.get(ox + dx, y0 + 4, oz + dz));
            found = ok;
        }
        if (!found) continue;
        Xoroshiro room(roomSeed);
        for (int dx = -rx - 1; dx <= rx + 1; ++dx)
            for (int dz = -rz - 1; dz <= rz + 1; ++dz)
                for (int dy = 4; dy >= -1; --dy) {
                    const int x = ox + dx, z = oz + dz, y = y0 + dy;
                    const bool wall = std::abs(dx) == rx + 1 || std::abs(dz) == rz + 1;
                    const BlockStateId cur = chunk.get(x, y, z);
                    if (dy == -1) { // floor (under the walls too)
                        if (reg.collides(cur)) chunk.set(x, y, z, room.nextInt(4) != 0 ? B.mossyCobblestone : B.cobblestone);
                    } else if (dy == 4 || wall) {
                        if (reg.collides(cur) && cur != B.chest) chunk.set(x, y, z, B.cobblestone);
                    } else if (cur != B.chest) {
                        chunk.set(x, y, z, B.air);
                    }
                }
        auto addEntity = [&](int x, int y, int z, bool chest, MobType mob) {
            if (out.count < int(out.list.size()))
                out.list[size_t(out.count++)] = {static_cast<int8_t>(x), static_cast<int8_t>(z), static_cast<int16_t>(y),
                                                 chest, mob};
        };
        for (int c = 0; c < 2; ++c)
            for (int t = 0; t < 3; ++t) {
                const int x = ox + static_cast<int>(room.nextInt(uint32_t(2 * rx + 1))) - rx;
                const int z = oz + static_cast<int>(room.nextInt(uint32_t(2 * rz + 1))) - rz;
                if (chunk.get(x, y0, z) != B.air) continue;
                int walls = 0;
                for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}})
                    walls += reg.collides(chunk.get(x + d[0], y0, z + d[1]));
                if (walls != 1) continue;
                chunk.set(x, y0, z, B.chest);
                addEntity(x, y0, z, true, MobType::Zombie);
                break;
            }
        const uint32_t m = room.nextInt(4);
        chunk.set(ox, y0, oz, B.spawner);
        addEntity(ox, y0, oz, false, m < 2 ? MobType::Zombie : m == 2 ? MobType::Skeleton : MobType::Spider);
    }
}

namespace {

// Writes a structure into one chunk: local structure coordinates (x across, z deep,
// y up from the floor) are rotated by `rot` quarter turns around the footprint and
// clipped to the chunk being generated. Chests become block entities later.
struct StructureBuilder {
    Buf chunk;
    int32_t baseX, baseZ;   // the chunk's origin
    int32_t ox, oy, oz;     // the structure's corner (world) and floor y
    int w, d, rot;          // footprint and rotation
    OverworldGenerator::GeneratedEntities* entities;

    bool toChunk(int x, int z, int& lx, int& lz) const {
        int rx = x, rz = z;
        switch (rot & 3) {
        case 1: rx = d - 1 - z, rz = x; break;
        case 2: rx = w - 1 - x, rz = d - 1 - z; break;
        case 3: rx = z, rz = w - 1 - x; break;
        default: break;
        }
        lx = ox + rx - baseX;
        lz = oz + rz - baseZ;
        return lx >= 0 && lx < 16 && lz >= 0 && lz < 16;
    }
    void set(int x, int y, int z, BlockStateId s) {
        int lx, lz;
        if (toChunk(x, z, lx, lz)) chunk.set(lx, oy + y, lz, s);
    }
    // A state with a horizontal "facing" turned like the structure (local east = +x
    // ends up south, west or north for rotations 1-3).
    BlockStateId turned(BlockStateId s) const {
        static constexpr std::string_view kOrder[4] = {"east", "south", "west", "north"};
        const auto& reg = blockRegistry();
        const auto f = reg.value(s, "facing");
        if (!f) return s;
        for (int i = 0; i < 4; ++i)
            if (*f == kOrder[i]) return reg.with(s, "facing", kOrder[(i + rot) & 3]).value_or(s);
        return s;
    }
    void fill(int x0, int y0, int z0, int x1, int y1, int z1, BlockStateId s) {
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    set(x, y, z, s);
    }
    // Walls of a box (hollow), its inside air.
    void room(int x0, int y0, int z0, int x1, int y1, int z1, BlockStateId wall) {
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x) {
                    const bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
                    set(x, y, z, edge ? wall : BlockStateId{0});
                }
    }
    // Solid ground under (x, z) from the floor down to the terrain.
    void foundation(int x, int z, BlockStateId s, int maxDepth = 12) {
        int lx, lz;
        if (!toChunk(x, z, lx, lz)) return;
        for (int y = oy - 1; y > oy - 1 - maxDepth; --y) {
            const BlockStateId cur = chunk.get(lx, y, lz);
            if (blockRegistry().collides(cur)) break;
            chunk.set(lx, y, lz, s);
        }
    }
    void furnace(int x, int y, int z) {
        int lx, lz;
        if (!toChunk(x, z, lx, lz) || entities->full()) return;
        chunk.set(lx, oy + y, lz, turned(blockSet().furnace));
        entities->list[size_t(entities->count++)] = {static_cast<int8_t>(lx), static_cast<int8_t>(lz),
                                                     static_cast<int16_t>(oy + y), false, MobType::Zombie,
                                                     LootTable::SimpleDungeon, true};
    }
    void villager(int x, int y, int z, uint8_t type, bool nitwit) { // (M24.1)
        int lx, lz;
        if (!toChunk(x, z, lx, lz) || entities->full()) return;
        OverworldGenerator::GeneratedEntity e{static_cast<int8_t>(lx), static_cast<int8_t>(lz), static_cast<int16_t>(oy + y), false,
                          MobType::Villager};
        e.villager = true;
        e.villagerType = type;
        e.nitwit = nitwit;
        entities->list[size_t(entities->count++)] = e;
    }
    void mob(int x, int y, int z, MobType type) { // (M24.4)
        int lx, lz;
        if (!toChunk(x, z, lx, lz) || entities->full()) return;
        OverworldGenerator::GeneratedEntity e{static_cast<int8_t>(lx), static_cast<int8_t>(lz),
                                              static_cast<int16_t>(oy + y), false, type};
        e.spawnMob = true;
        entities->list[size_t(entities->count++)] = e;
    }
    void chest(int x, int y, int z, LootTable loot) {
        int lx, lz;
        if (!toChunk(x, z, lx, lz) || entities->full()) return; // (no chest without its contents)
        const Blocks& B = blockSet();
        chunk.set(lx, oy + y, lz, turned(B.chest));
        if (entities->count < int(entities->list.size()))
            entities->list[size_t(entities->count++)] = {static_cast<int8_t>(lx), static_cast<int8_t>(lz),
                                                         static_cast<int16_t>(oy + y), true, MobType::Zombie, loot};
    }
};

} // namespace

// Ocean structures (overworld4, M25.4; wiki: Shipwreck, Ocean Ruins, Buried Treasure).
// Basic versions after the wiki's descriptions: a spruce hull on the sea floor (or
// beached), parts of it broken away, with its supply, map and treasure chests; ruins
// of stone bricks (cold oceans) or sandstone (warm) with a chest; and, in 1 of 100
// beach chunks, a chest of treasure under the sand.
void OverworldGenerator::placeOceanStructures(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const {
    const auto& reg = blockRegistry();
    static const BlockStateId planks = *reg.parse("minecraft:spruce_planks");
    static const BlockStateId logX = *reg.parse("minecraft:spruce_log[axis=z]");
    static const BlockStateId logY = *reg.parse("minecraft:spruce_log[axis=y]");
    static const BlockStateId fence = reg.defaultState(*reg.findBlock("spruce_fence"));
    static const BlockStateId oakPlanks = *reg.parse("minecraft:oak_planks");
    static const BlockStateId bricks = reg.defaultState(blocks::StoneBricks);
    static const BlockStateId mossy = reg.defaultState(blocks::MossyStoneBricks);
    static const BlockStateId cracked = reg.defaultState(blocks::CrackedStoneBricks);
    static const BlockStateId sandstone = reg.defaultState(blocks::Sandstone);
    static const BlockStateId cutSandstone = *reg.parse("minecraft:cut_sandstone");
    static const BlockStateId gravel = reg.defaultState(blocks::Gravel);
    static const BlockStateId water = reg.defaultState(blocks::Water);
    auto isSea = [](Biome b) { return isOcean(b); };
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            const ChunkPos start{cx + dx, cz + dz};
            const int32_t sx = start.x * 16, sz = start.z * 16;
            // ---- Shipwrecks: 7 x 20, upright, on the floor (or on a beach) ----
            if (isSpreadCandidate(m_seed, kShipwrecks, start)) {
                const Biome biome = biomeAt(column(sx + 8, sz + 8));
                const bool beached = biome == Biome::Beach || biome == Biome::SnowyBeach;
                if (isSea(biome) || beached) {
                    Xoroshiro r(chunkSeed(m_seed, start.x, start.z, 720));
                    const int ground = surfaceY(sx + 3, sz + 10) + 1;
                    StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, sx, ground, sz, 7, 20,
                                        static_cast<int>(r.nextInt(4)), &out};
                    // The hull: a keel, sides narrowing toward the bow, a deck at 4.
                    for (int z = 0; z < 20; ++z) {
                        const int half = z < 3 ? 1 : z > 16 ? 2 : 3; // (stern, bow narrower)
                        for (int x = 3 - half; x <= 3 + half; ++x) sb.set(x, 0, z, planks);
                        for (int y = 1; y <= 3; ++y) {
                            sb.set(3 - half, y, z, planks);
                            sb.set(3 + half, y, z, planks);
                        }
                        for (int x = 3 - half + 1; x <= 3 + half - 1; ++x)
                            for (int y = 1; y <= 3; ++y) sb.set(x, y, z, beached ? BlockStateId{0} : water);
                        if (z % 3 != 1) // (deck planks with gaps)
                            for (int x = 3 - half; x <= 3 + half; ++x) sb.set(x, 4, z, planks);
                        sb.set(3 - half, 5, z, fence); // the rail
                        sb.set(3 + half, 5, z, fence);
                    }
                    sb.fill(3, 0, 0, 3, 0, 19, logX); // keel
                    for (int y = 5; y <= 12; ++y) sb.set(3, y, 9, logY); // the mast
                    sb.room(1, 5, 0, 5, 8, 4, oakPlanks); // the stern cabin
                    sb.fill(2, 6, 4, 4, 7, 4, 0);
                    // Broken away: a random third of the hull's top, as wrecks are.
                    const int broken = int(r.nextInt(3));
                    sb.fill(0, 3 + broken, 12 + int(r.nextInt(4)), 6, 13, 19, beached ? BlockStateId{0} : water);
                    sb.chest(2, 6, 2, LootTable::ShipwreckMap);       // captain's cabin
                    sb.chest(4, 1, 9, LootTable::ShipwreckSupply);    // the hold
                    sb.chest(3, 1, 15, LootTable::ShipwreckTreasure); // the bow
                    for (int x = 0; x < 7; ++x)
                        for (int z = 0; z < 20; ++z) sb.foundation(x, z, gravel, 4);
                }
            }
            // ---- Ocean ruins: 5 x 5 (big: 9 x 9), stone bricks or sandstone ----
            if (isSpreadCandidate(m_seed, kOceanRuins, start)) {
                const Biome biome = biomeAt(column(sx + 8, sz + 8));
                if (isSea(biome)) {
                    const bool warm = biome == Biome::WarmOcean || biome == Biome::LukewarmOcean ||
                                      biome == Biome::DeepLukewarmOcean;
                    Xoroshiro r(chunkSeed(m_seed, start.x, start.z, 721));
                    const bool big = r.nextInt(10) < 3; // (wiki: 30% big)
                    const int size = big ? 9 : 5;
                    const int ground = surfaceY(sx + size / 2, sz + size / 2) + 1;
                    StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, sx + 2, ground, sz + 2, size, size,
                                        static_cast<int>(r.nextInt(4)), &out};
                    auto stone = [&] {
                        const uint32_t pick = r.nextInt(10);
                        if (warm) return pick < 7 ? sandstone : cutSandstone;
                        return pick < 5 ? bricks : pick < 8 ? mossy : cracked;
                    };
                    sb.fill(0, -1, 0, size - 1, -1, size - 1, warm ? sandstone : bricks); // floor
                    for (int x = 0; x < size; ++x)
                        for (int z = 0; z < size; ++z) {
                            const bool wall = x == 0 || z == 0 || x == size - 1 || z == size - 1;
                            if (!wall) continue;
                            // Crumbled walls: lower toward one corner, gaps here and there.
                            const int h = std::max(0, (big ? 5 : 3) - (x + z) / (big ? 4 : 3) - int(r.nextInt(2)));
                            for (int y = 0; y < h; ++y)
                                if (r.nextInt(6) != 0) sb.set(x, y, z, stone());
                        }
                    sb.chest(size / 2, 0, size / 2, big ? LootTable::UnderwaterRuinBig : LootTable::UnderwaterRuinSmall);
                    for (int x = 0; x < size; ++x)
                        for (int z = 0; z < size; ++z) sb.foundation(x, z, warm ? sandstone : bricks, 4);
                }
            }
        }
    // ---- Buried treasure: this chunk only, 1 in 100 beach chunks (wiki) ----
    {
        Xoroshiro r(chunkSeed(m_seed, cx, cz, 722));
        if (r.nextInt(100) == 0) {
            const int32_t x = cx * 16 + 9, z = cz * 16 + 9;
            const Biome biome = biomeAt(column(x, z));
            if (biome == Biome::Beach || biome == Biome::SnowyBeach) {
                const int ground = surfaceY(x, z);
                StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, x, ground - 2, z, 1, 1, 0, &out};
                sb.chest(0, 0, 0, LootTable::BuriedTreasure); // (under 2 blocks of sand)
            }
        }
    }
}

void OverworldGenerator::placeStructures(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const {
    const Blocks& B = blockSet();
    if (m_version >= 3) placeOutposts(blocks, cx, cz, out);
    if (m_version >= 4) placeOceanStructures(blocks, cx, cz, out);
    struct Kind {
        const RandomSpread* spread;
        int w, d;
    };
    static constexpr Kind kKinds[] = {{&kDesertPyramids, 21, 21}, {&kJunglePyramids, 12, 15}, {&kIgloos, 7, 7}, {&kSwampHuts, 7, 9}};
    for (int k = 0; k < 4; ++k) {
        const Kind& kind = kKinds[k];
        // Candidates of the regions around this chunk (a structure reaches < 2 chunks).
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
                const ChunkPos start{cx + dx, cz + dz};
                if (!isSpreadCandidate(m_seed, *kind.spread, start)) continue;
                const int32_t sx = start.x * 16, sz = start.z * 16;
                const Biome biome = biomeAt(column(sx + 8, sz + 8));
                const bool fits = k == 0   ? biome == Biome::Desert
                                  : k == 1 ? biome == Biome::Jungle
                                  : k == 2 ? biome == Biome::SnowyPlains || biome == Biome::SnowyTaiga || biome == Biome::SnowySlopes
                                           : biome == Biome::Swamp;
                if (!fits) continue;
                Xoroshiro r(chunkSeed(m_seed, start.x, start.z, 700 + uint64_t(k)));
                const int ground = surfaceY(sx + kind.w / 2, sz + kind.d / 2);
                if (ground < kSeaLevel - 1 && k != 3) continue; // under water: not here
                StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, sx, ground, sz, kind.w, kind.d,
                                    static_cast<int>(r.nextInt(4)), &out};
                if (k == 0) { // ---- Desert pyramid (wiki: Desert Pyramid) ----
                    // A 21x21 hall of sandstone, its roof stepping in to a peak, two
                    // corner towers at the front, an orange/blue terracotta floor
                    // pattern over a buried chamber of 4 chests and 9 TNT.
                    for (int x = 0; x < 21; ++x)
                        for (int z = 0; z < 21; ++z)
                            sb.foundation(x, z, B.sandstone, 20);
                    sb.room(0, 0, 0, 20, 5, 20, B.sandstone);
                    for (int step = 1; step <= 9; ++step) // stepped roof
                        sb.room(step, 5 + step - 1, step, 20 - step, 5 + step, 20 - step, B.sandstone);
                    sb.fill(1, 1, 1, 19, 4, 19, 0);            // the hall
                    sb.fill(6, 5, 6, 14, 9, 14, 0);            // its tall middle under the peak
                    sb.fill(1, 0, 1, 19, 0, 19, B.sandstone);  // floor
                    for (int x = 6; x <= 14; ++x)              // the terracotta star
                        for (int z = 6; z <= 14; ++z)
                            if (std::abs(x - 10) == std::abs(z - 10) || x == 10 || z == 10)
                                sb.set(x, 0, z, B.orangeTerracotta);
                    sb.set(10, 0, 10, B.blueTerracotta);
                    for (int t = 0; t < 2; ++t) { // front towers with an orange band
                        const int tx = t == 0 ? 0 : 16;
                        sb.room(tx, 0, 0, tx + 4, 14, 4, B.sandstone);
                        for (int x = tx; x <= tx + 4; ++x) {
                            sb.set(x, 11, 0, B.orangeTerracotta);
                            sb.set(x, 13, 0, B.cutSandstone);
                        }
                        sb.set(tx + 2, 12, 0, B.chiseledSandstone);
                    }
                    sb.fill(9, 1, 0, 11, 3, 0, 0); // the entrance
                    sb.fill(8, 4, 0, 12, 4, 0, B.cutSandstone);
                    // The chamber: 9 deep under the blue terracotta, a chest in each wall,
                    // TNT under its floor (no pressure plate yet: the trap can't go off).
                    sb.fill(10, -8, 10, 10, -1, 10, 0);
                    sb.room(7, -13, 7, 13, -8, 13, B.sandstone);
                    sb.fill(9, -13, 9, 11, -13, 11, B.sandstone);
                    sb.fill(9, -14, 9, 11, -14, 11, B.tnt);
                    sb.set(10, -8, 10, 0); // open to the shaft
                    sb.chest(10, -12, 8, LootTable::DesertPyramid);
                    sb.chest(10, -12, 12, LootTable::DesertPyramid);
                    sb.chest(8, -12, 10, LootTable::DesertPyramid);
                    sb.chest(12, -12, 10, LootTable::DesertPyramid);
                } else if (k == 1) { // ---- Jungle temple (wiki: Jungle Pyramid) ----
                    // Three floors of mossy and plain cobblestone, 12x15, a stepped top,
                    // chiseled stone bricks over the door, 2 loot chests below.
                    auto stone = [&](int x, int y, int z) {
                        return positional(m_seed, sx + x, ground + y, sz + z, 16) < 0.4 ? B.mossyCobblestone : B.cobblestone;
                    };
                    for (int x = 0; x < 12; ++x)
                        for (int z = 0; z < 15; ++z)
                            sb.foundation(x, z, B.cobblestone, 20);
                    for (int y = -4; y <= 9; ++y) {
                        const int in = y <= 3 ? 0 : y <= 7 ? 1 : 2;
                        for (int x = in; x < 12 - in; ++x)
                            for (int z = in; z < 15 - in; ++z) {
                                const bool shell = x == in || x == 11 - in || z == in || z == 14 - in || y == -4 ||
                                                   y == 0 || y == 4 || y == 8 || y == 9;
                                sb.set(x, y, z, shell ? stone(x, y, z) : BlockStateId{0});
                            }
                    }
                    sb.fill(5, 1, 0, 6, 3, 0, 0); // door
                    sb.fill(5, 4, 0, 6, 4, 0, B.chiseledStoneBricks);
                    sb.fill(5, 0, 6, 6, 0, 7, 0); // a hole down to the cellar
                    sb.chest(2, -3, 12, LootTable::JunglePyramid);
                    sb.chest(9, -3, 3, LootTable::JunglePyramid);
                } else if (k == 2) { // ---- Igloo (wiki: Igloo) ----
                    // A snow dome with ice windows, a bed, furnace, crafting table and a
                    // redstone torch (no basement yet: no ladders or trapdoors).
                    for (int y = 0; y <= 4; ++y)
                        for (int x = 0; x < 7; ++x)
                            for (int z = 0; z < 7; ++z) {
                                const double ex = (x - 3) / 3.5, ez = (z - 3) / 3.5, ey = y / 4.5;
                                const double d2 = ex * ex + ez * ez + ey * ey;
                                if (d2 >= 1.0) continue;
                                const bool shell = d2 > 0.45 || y == 0;
                                sb.set(x, y, z, shell ? B.snow : BlockStateId{0});
                            }
                    for (int x = 0; x < 7; ++x)
                        for (int z = 0; z < 7; ++z)
                            sb.foundation(x, z, B.snow, 4);
                    sb.fill(3, 1, 0, 3, 2, 1, 0); // doorway
                    sb.set(0, 2, 3, B.ice);
                    sb.set(6, 2, 3, B.ice);
                    sb.set(2, 1, 5, sb.turned(B.bedFoot)); // facing local +x: the head
                    sb.set(3, 1, 5, sb.turned(B.bedHead));
                    sb.furnace(4, 1, 2);
                    sb.set(5, 1, 3, B.craftingTable);
                    sb.set(1, 1, 3, B.redstoneTorch);
                } else { // ---- Swamp hut (wiki: Swamp Hut) ----
                    // A spruce plank room on oak log stilts, 2 tall inside, a porch, a
                    // crafting table (no cauldron, flower pot or witch yet).
                    const int floor = std::max(ground, kSeaLevel - 1) + 3;
                    sb.oy = floor;
                    for (const auto& p : {std::array{1, 2}, std::array{5, 2}, std::array{1, 7}, std::array{5, 7}}) {
                        sb.set(p[0], 0, p[1], B.oakLog);
                        sb.foundation(p[0], p[1], B.oakLog, 12);
                    }
                    sb.fill(1, 0, 1, 5, 0, 7, B.sprucePlanks);
                    sb.room(1, 0, 2, 5, 4, 7, B.sprucePlanks);
                    sb.fill(0, 4, 1, 6, 4, 8, B.sprucePlanks); // roof
                    sb.fill(3, 1, 2, 3, 2, 2, 0);              // door to the porch
                    sb.set(1, 2, 4, 0);                        // windows
                    sb.set(5, 2, 4, 0);
                    sb.set(4, 1, 6, B.craftingTable);
                    if (m_version >= 3) { // overworld3 (M24.4): the witch at home and her cauldron
                        sb.set(2, 1, 6, blockRegistry().defaultState(blocks::Cauldron));
                        sb.mob(3, 1, 4, MobType::Witch);
                    }
                }
            }
    }
}

namespace {

// A mineshaft piece: a box in world coordinates and what it is.
struct MinePiece {
    enum Kind : uint8_t { Room, Corridor, Crossing, Stairs } kind;
    int32_t x0, y0, z0, x1, y1, z1;
    uint8_t dir; // corridors and stairs: 0 +z, 1 -x, 2 -z, 3 +x
    int8_t chestAt = -1; // corridor: the section with a chest (-1 none)
    bool intersects(const MinePiece& o) const {
        return x0 <= o.x1 && x1 >= o.x0 && y0 <= o.y1 && y1 >= o.y0 && z0 <= o.z1 && z1 >= o.z0;
    }
};

struct MinePlan {
    int count = 0;
    std::array<MinePiece, 96> pieces{};
};

// The whole mineshaft from its start chunk (deterministic; wiki: Mineshaft - a 10x10
// room, 3x3 corridors with supports, 5x5 crossings, stairs; ours: depth 8, 80 blocks
// out, no overlapping pieces).
void planMineshaft(uint64_t seed, int32_t cx, int32_t cz, int startY, MinePlan& plan) {
    Xoroshiro r(mixSeed(mixSeed(mixSeed(seed, 0x4d494e45u), static_cast<uint32_t>(cx)), static_cast<uint32_t>(cz)));
    r.nextDouble(); // (the candidate roll)
    plan.count = 0;
    const int32_t sx = cx * 16 + 3, sz = cz * 16 + 3;
    auto fits = [&](const MinePiece& p) {
        if (plan.count >= int(plan.pieces.size())) return false;
        if (std::abs(p.x0 - sx) > 80 || std::abs(p.x1 - sx) > 80 || std::abs(p.z0 - sz) > 80 || std::abs(p.z1 - sz) > 80)
            return false;
        if (p.y0 < kOverworldHeight.minY + 6) return false;
        for (int i = 0; i < plan.count; ++i)
            if (plan.pieces[size_t(i)].intersects(p)) return false;
        return true;
    };
    static constexpr int kDx[4] = {0, -1, 0, 1}, kDz[4] = {1, 0, -1, 0};
    // Grows a piece leaving (x, y, z) heading `dir`.
    auto grow = [&](auto& self, int32_t x, int32_t y, int32_t z, int dir, int depth) -> void {
        if (depth > 8) return;
        const uint32_t roll = r.nextInt(100);
        MinePiece p{};
        p.dir = static_cast<uint8_t>(dir);
        // A box `len` long, `half` to each side of the axis, `h` tall, starting at (x,y,z).
        auto box = [&](int len, int half, int h, int dy) {
            const int32_t ex = x + kDx[dir] * (len - 1), ez = z + kDz[dir] * (len - 1);
            const int32_t sideX = kDz[dir] != 0 ? half : 0, sideZ = kDx[dir] != 0 ? half : 0;
            p.x0 = std::min(x, ex) - sideX;
            p.x1 = std::max(x, ex) + sideX;
            p.z0 = std::min(z, ez) - sideZ;
            p.z1 = std::max(z, ez) + sideZ;
            p.y0 = std::min(y, y + dy);
            p.y1 = std::max(y, y + dy) + h - 1;
        };
        if (roll < 70 || depth == 0) {
            p.kind = MinePiece::Corridor;
            const int sections = 2 + static_cast<int>(r.nextInt(3));
            box(sections * 5, 1, 3, 0);
            p.chestAt = r.nextInt(30) == 0 ? static_cast<int8_t>(r.nextInt(uint32_t(sections))) : int8_t{-1};
            if (!fits(p)) return;
            plan.pieces[size_t(plan.count++)] = p;
            const int32_t ex = x + kDx[dir] * sections * 5, ez = z + kDz[dir] * sections * 5;
            self(self, ex, y, ez, dir, depth + 1);
            // Side branches now and then.
            if (r.nextInt(3) == 0) {
                const int at = 2 + static_cast<int>(r.nextInt(uint32_t(sections * 5 - 4)));
                const int side = r.nextInt(2) == 0 ? 1 : 3;
                const int nd = (dir + side) & 3;
                self(self, x + kDx[dir] * at + kDx[nd] * 2, y, z + kDz[dir] * at + kDz[nd] * 2, nd, depth + 1);
            }
        } else if (roll < 90) {
            p.kind = MinePiece::Crossing;
            box(5, 2, 3, 0);
            if (!fits(p)) return;
            plan.pieces[size_t(plan.count++)] = p;
            const int32_t mx = x + kDx[dir] * 2, mz = z + kDz[dir] * 2;
            for (int t : {0, 1, 3}) {
                const int nd = (dir + t) & 3;
                self(self, mx + kDx[nd] * 3, y, mz + kDz[nd] * 3, nd, depth + 1);
            }
        } else {
            p.kind = MinePiece::Stairs; // 8 long, 5 down
            box(8, 1, 3, -5);
            if (!fits(p)) return;
            plan.pieces[size_t(plan.count++)] = p;
            self(self, x + kDx[dir] * 8, y - 5, z + kDz[dir] * 8, dir, depth + 1);
        }
    };
    MinePiece room{MinePiece::Room, sx - 5, startY, sz - 5, sx + 4, startY + 4, sz + 4, 0};
    plan.pieces[size_t(plan.count++)] = room;
    for (int side = 0; side < 4; ++side) {
        const int exits = 1 + static_cast<int>(r.nextInt(4));
        for (int e = 0; e < exits; ++e) {
            const int along = -3 + static_cast<int>(r.nextInt(7));
            // Just outside the room's wall (it spans s-5 .. s+4).
            const int32_t x = sx + (kDx[side] > 0 ? 5 : kDx[side] < 0 ? -6 : along);
            const int32_t z = sz + (kDz[side] > 0 ? 5 : kDz[side] < 0 ? -6 : along);
            grow(grow, x, startY, z, side, 0);
        }
    }
}

} // namespace

void OverworldGenerator::placeMineshafts(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const {
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    Buf chunk{blocks};
    const int32_t baseX = cx * 16, baseZ = cz * 16;
    // Plans are cached per worker: the ~170 chunks around a start all need it.
    struct Cached {
        uint64_t seed = 0;
        int32_t sx = INT32_MIN, sz = 0;
        MinePlan plan;
    };
    static thread_local std::array<Cached, 16> cache;
    for (int32_t sz = cz - 6; sz <= cz + 6; ++sz)
        for (int32_t sx = cx - 6; sx <= cx + 6; ++sx) {
            if (!isMineshaftCandidate(m_seed, {sx, sz})) continue;
            Cached& e = cache[size_t((uint32_t(sx) * 7u + uint32_t(sz) * 13u) & 15u)];
            if (e.seed != m_seed || e.sx != sx || e.sz != sz) {
                // Start height: well under the surface there (ours: y -40..30).
                const int surface = surfaceY(sx * 16 + 8, sz * 16 + 8);
                const int startY = std::min(surface - 20, -40 + static_cast<int>(positional(m_seed, sx, 0, sz, 17) * 70.0));
                e.seed = m_seed, e.sx = sx, e.sz = sz;
                planMineshaft(m_seed, sx, sz, startY, e.plan);
            }
            const MinePlan& plan = e.plan;
            for (int i = 0; i < plan.count; ++i) {
                const MinePiece& p = plan.pieces[size_t(i)];
                if (p.x1 < baseX || p.x0 > baseX + 15 || p.z1 < baseZ || p.z0 > baseZ + 15) continue;
                auto carve = [&](int32_t x, int32_t y, int32_t z, BlockStateId s) {
                    const int lx = x - baseX, lz = z - baseZ;
                    if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(y)) return;
                    const BlockStateId cur = chunk.get(lx, y, lz);
                    if (cur == B.bedrock || cur == B.water || cur == B.chest || cur == B.spawner) return;
                    chunk.set(lx, y, lz, s);
                };
                auto floorUnder = [&](int32_t x, int32_t y, int32_t z) { // a plank bridge over gaps
                    const int lx = x - baseX, lz = z - baseZ;
                    if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(y)) return;
                    if (!reg.collides(chunk.get(lx, y, lz))) chunk.set(lx, y, lz, B.oakPlanks);
                };
                if (p.kind == MinePiece::Stairs) {
                    // A slope: each step one lower every 8/5 blocks along the way.
                    const int dx = p.dir == 3 ? 1 : p.dir == 1 ? -1 : 0, dz = p.dir == 0 ? 1 : p.dir == 2 ? -1 : 0;
                    const bool alongX = dx != 0;
                    const int32_t len = alongX ? p.x1 - p.x0 + 1 : p.z1 - p.z0 + 1;
                    for (int32_t t = 0; t < len; ++t) {
                        const int32_t step = p.y1 - 2 - (t * 5) / 8;
                        for (int s = -1; s <= 1; ++s) {
                            const int32_t x = alongX ? (dx > 0 ? p.x0 + t : p.x1 - t) : (p.x0 + p.x1) / 2 + s;
                            const int32_t z = alongX ? (p.z0 + p.z1) / 2 + s : (dz > 0 ? p.z0 + t : p.z1 - t);
                            for (int h = 0; h < 3; ++h)
                                carve(x, step + h, z, B.air);
                            floorUnder(x, step - 1, z);
                        }
                    }
                    continue;
                }
                for (int32_t x = p.x0; x <= p.x1; ++x)
                    for (int32_t z = p.z0; z <= p.z1; ++z) {
                        for (int32_t y = p.y0; y <= p.y1; ++y)
                            carve(x, y, z, B.air);
                        floorUnder(x, p.y0 - 1, z);
                    }
                if (p.kind == MinePiece::Room) // a dirt floor (wiki)
                    for (int32_t x = p.x0; x <= p.x1; ++x)
                        for (int32_t z = p.z0; z <= p.z1; ++z)
                            carve(x, p.y0 - 1, z, B.dirt);
                if (p.kind != MinePiece::Corridor) continue;
                // Supports every 5 blocks: two posts and a beam (vanilla: fences under
                // planks; ours: planks), and the chest on one side.
                const bool alongX = p.dir == 1 || p.dir == 3;
                const int32_t len = alongX ? p.x1 - p.x0 + 1 : p.z1 - p.z0 + 1;
                for (int32_t t = 2; t < len; t += 5) {
                    for (int s = -1; s <= 1; ++s) {
                        const int32_t x = alongX ? p.x0 + t : (p.x0 + p.x1) / 2 + s;
                        const int32_t z = alongX ? (p.z0 + p.z1) / 2 + s : p.z0 + t;
                        if (s != 0) {
                            carve(x, p.y0, z, B.oakPlanks);
                            carve(x, p.y0 + 1, z, B.oakPlanks);
                        }
                        carve(x, p.y0 + 2, z, B.oakPlanks);
                    }
                }
                if (p.chestAt >= 0) {
                    const int32_t t = p.chestAt * 5 + 4;
                    const int32_t x = alongX ? p.x0 + t : p.x0, z = alongX ? p.z0 : p.z0 + t;
                    const int lx = x - baseX, lz = z - baseZ;
                    if (lx >= 0 && lx < 16 && lz >= 0 && lz < 16 && kOverworldHeight.contains(p.y0) &&
                        out.count < int(out.list.size())) {
                        chunk.set(lx, p.y0, lz, B.chest);
                        out.list[size_t(out.count++)] = {static_cast<int8_t>(lx), static_cast<int8_t>(lz),
                                                         static_cast<int16_t>(p.y0), true, MobType::Zombie,
                                                         LootTable::Mineshaft};
                    }
                }
            }
        }
}

namespace {

// A stronghold piece in local terms: `half` blocks to each side of its axis, `len`
// long from its entrance wall, `h` tall, all including walls; placed at `x, y, z` (the
// entrance wall's middle, floor level) facing `dir` (0 +z, 1 -x, 2 -z, 3 +x).
struct HoldPiece {
    enum Kind : uint8_t { Stairs, FiveWay, Corridor, Altar, Storeroom, Library, Portal } kind;
    int32_t x, y, z;
    uint8_t dir;
    int half, len, h;
    int32_t x0, z0, x1, z1; // world box
    bool intersects(const HoldPiece& o) const {
        return x0 <= o.x1 && x1 >= o.x0 && z0 <= o.z1 && z1 >= o.z0 && y <= o.y + o.h - 1 && y + h - 1 >= o.y;
    }
};
constexpr int kFx[4] = {0, -1, 0, 1}, kFz[4] = {1, 0, -1, 0};

// Local (u across, v forward) to world for a piece.
inline void holdWorld(const HoldPiece& p, int u, int v, int32_t& wx, int32_t& wz) {
    wx = p.x + kFx[p.dir] * v - kFz[p.dir] * u;
    wz = p.z + kFz[p.dir] * v + kFx[p.dir] * u;
}

struct HoldPlan {
    int count = 0;
    std::array<HoldPiece, 72> pieces{};
};

void planStronghold(uint64_t seed, ChunkPos start, int startY, HoldPlan& plan) {
    Xoroshiro r(mixSeed(mixSeed(mixSeed(seed, 0x484f4c44u), static_cast<uint32_t>(start.x)), static_cast<uint32_t>(start.z)));
    plan.count = 0;
    const int32_t cx = start.x * 16, cz = start.z * 16;
    bool portal = false;
    auto make = [&](HoldPiece::Kind kind, int32_t x, int32_t y, int32_t z, int dir) {
        HoldPiece p{kind, x, y, z, static_cast<uint8_t>(dir), 2, 5, 5, 0, 0, 0, 0};
        switch (kind) {
        case HoldPiece::Stairs: p.half = 2, p.len = 5, p.h = 11; break;
        case HoldPiece::FiveWay:
        case HoldPiece::Storeroom: p.half = 5, p.len = 11, p.h = 7; break;
        case HoldPiece::Corridor: p.half = 2, p.len = 5 + static_cast<int>(r.nextInt(5)), p.h = 5; break;
        case HoldPiece::Altar: p.half = 2, p.len = 7, p.h = 5; break;
        case HoldPiece::Library: p.half = 4, p.len = 15, p.h = 7; break;
        case HoldPiece::Portal: p.half = 5, p.len = 16, p.h = 8; break;
        }
        int32_t ax, az, bx, bz;
        holdWorld(p, -p.half, 0, ax, az);
        holdWorld(p, p.half, p.len - 1, bx, bz);
        p.x0 = std::min(ax, bx), p.x1 = std::max(ax, bx), p.z0 = std::min(az, bz), p.z1 = std::max(az, bz);
        return p;
    };
    auto fits = [&](const HoldPiece& p) {
        if (plan.count >= int(plan.pieces.size())) return false;
        if (std::abs(p.x0 - cx) > 112 || std::abs(p.x1 - cx) > 112 || std::abs(p.z0 - cz) > 112 || std::abs(p.z1 - cz) > 112)
            return false;
        for (int i = 0; i < plan.count; ++i)
            if (plan.pieces[size_t(i)].intersects(p)) return false;
        return true;
    };
    // Adds a piece through an exit at local (u, v) of `from`, heading `dir` (a door is
    // cut through both walls when built).
    auto grow = [&](auto& self, const HoldPiece& from, int u, int v, int dir, int depth, bool towardPortal) -> void {
        if (depth > 7) return;
        int32_t x, z;
        holdWorld(from, u, v, x, z);
        HoldPiece::Kind kind;
        if (towardPortal) {
            kind = depth >= 3 ? HoldPiece::Portal : HoldPiece::Corridor;
        } else {
            const uint32_t roll = r.nextInt(100);
            kind = roll < 35   ? HoldPiece::Corridor
                   : roll < 45 ? HoldPiece::Altar
                   : roll < 65 ? HoldPiece::FiveWay
                   : roll < 75 ? HoldPiece::Storeroom
                   : roll < 85 ? HoldPiece::Library
                               : HoldPiece::Kind(255);
            if (kind == HoldPiece::Kind(255)) return;
        }
        const HoldPiece p = make(kind, x, from.y, z, dir);
        if (!fits(p)) {
            if (towardPortal && kind != HoldPiece::Portal) { // try the portal room right here
                const HoldPiece q = make(HoldPiece::Portal, x, from.y, z, dir);
                if (fits(q)) {
                    plan.pieces[size_t(plan.count++)] = q;
                    portal = true;
                }
            }
            return;
        }
        plan.pieces[size_t(plan.count++)] = p;
        if (kind == HoldPiece::Portal) portal = true;
        const HoldPiece& me = plan.pieces[size_t(plan.count - 1)];
        switch (kind) {
        case HoldPiece::Corridor:
        case HoldPiece::Altar: self(self, me, 0, me.len, dir, depth + 1, towardPortal); break;
        case HoldPiece::FiveWay:
        case HoldPiece::Storeroom:
            self(self, me, 0, me.len, dir, depth + 1, false);
            self(self, me, me.half + 1, me.len / 2, (dir + 1) & 3, depth + 1, false); // (+u is dir + 1)
            self(self, me, -me.half - 1, me.len / 2, (dir + 3) & 3, depth + 1, false);
            break;
        default: break; // library, portal: dead ends
        }
    };
    const int dir = static_cast<int>(r.nextInt(4));
    HoldPiece stairs = make(HoldPiece::Stairs, cx, startY, cz, dir);
    plan.pieces[size_t(plan.count++)] = stairs;
    // Below the stairs, a five-way crossing; its way forward leads to the portal room.
    HoldPiece five = make(HoldPiece::FiveWay, 0, 0, 0, dir);
    {
        int32_t x, z;
        holdWorld(stairs, 0, stairs.len, x, z);
        five = make(HoldPiece::FiveWay, x, startY, z, dir);
    }
    if (!fits(five)) return;
    plan.pieces[size_t(plan.count++)] = five;
    const HoldPiece f = five;
    grow(grow, f, 0, f.len, dir, 1, true);
    grow(grow, f, f.half + 1, f.len / 2, (dir + 1) & 3, 1, false);
    grow(grow, f, -f.half - 1, f.len / 2, (dir + 3) & 3, 1, false);
    (void)portal;
}

} // namespace

std::optional<glm::ivec2> OverworldGenerator::nearestStronghold(double x, double z) const {
    std::optional<glm::ivec2> best;
    double bestD = 0.0;
    for (int i = 0; i < m_strongholdCount; ++i) {
        const ChunkPos c = m_strongholds[size_t(i)];
        const double dx = c.x * 16 - x, dz = c.z * 16 - z, d = dx * dx + dz * dz;
        if (!best || d < bestD) {
            best = glm::ivec2(c.x * 16, c.z * 16);
            bestD = d;
        }
    }
    return best;
}

void OverworldGenerator::placeStrongholds(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const {
    const Blocks& B = blockSet();
    Buf chunk{blocks};
    const int32_t baseX = cx * 16, baseZ = cz * 16;
    struct Cached {
        uint64_t seed = 0;
        int32_t sx = INT32_MIN, sz = 0;
        HoldPlan plan;
    };
    static thread_local std::array<Cached, 4> cache; // (a worker meets one stronghold at a time)
    for (int s = 0; s < m_strongholdCount; ++s) {
        const ChunkPos start = m_strongholds[size_t(s)];
        if (std::abs(start.x - cx) > 9 || std::abs(start.z - cz) > 9) continue;
        Cached& e = cache[size_t(s & 3)];
        if (e.seed != m_seed || e.sx != start.x || e.sz != start.z) {
            // Underground: its top well below the surface (ours: floor y 0..30).
            const int startY = std::min(surfaceY(start.x * 16, start.z * 16) - 22,
                                        static_cast<int>(positional(m_seed, start.x, 0, start.z, 18) * 30.0));
            e.seed = m_seed, e.sx = start.x, e.sz = start.z;
            planStronghold(m_seed, start, startY, e.plan);
        }
        const HoldPlan& plan = e.plan;
        for (int i = 0; i < plan.count; ++i) {
            const HoldPiece& p = plan.pieces[size_t(i)];
            if (p.x1 < baseX || p.x0 > baseX + 15 || p.z1 < baseZ || p.z0 > baseZ + 15) continue;
            auto put = [&](int u, int y, int v, BlockStateId st) {
                int32_t wx, wz;
                holdWorld(p, u, v, wx, wz);
                const int lx = wx - baseX, lz = wz - baseZ;
                if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(p.y + y)) return;
                if (chunk.get(lx, p.y + y, lz) == B.bedrock) return;
                chunk.set(lx, p.y + y, lz, st);
            };
            auto brick = [&](int u, int y, int v) {
                int32_t wx, wz;
                holdWorld(p, u, v, wx, wz);
                const double roll = positional(m_seed, wx, p.y + y, wz, 19);
                return roll < 0.2 ? B.mossyStoneBricks : roll < 0.4 ? B.crackedStoneBricks : B.stoneBricks;
            };
            auto chestAt = [&](int u, int y, int v, LootTable loot) {
                int32_t wx, wz;
                holdWorld(p, u, v, wx, wz);
                const int lx = wx - baseX, lz = wz - baseZ;
                if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || out.count >= int(out.list.size())) return;
                chunk.set(lx, p.y + y, lz, B.chest);
                out.list[size_t(out.count++)] = {static_cast<int8_t>(lx), static_cast<int8_t>(lz),
                                                 static_cast<int16_t>(p.y + y), true, MobType::Zombie, loot};
            };
            // Shell and air.
            for (int v = 0; v < p.len; ++v)
                for (int u = -p.half; u <= p.half; ++u)
                    for (int y = 0; y < p.h; ++y) {
                        const bool wall = v == 0 || v == p.len - 1 || std::abs(u) == p.half || y == 0 || y == p.h - 1;
                        put(u, y, v, wall ? brick(u, y, v) : BlockStateId{0});
                    }
            // Doorways: the entrance (and the stairs' exit at the bottom), 3 wide, 3 tall;
            // side and far exits of rooms with ways on.
            auto door = [&](int u, int v, bool alongU) {
                for (int k = -1; k <= 1; ++k)
                    for (int y = 1; y <= 3; ++y)
                        put(alongU ? u : u + k, y, alongU ? v + k : v, 0);
            };
            if (p.kind != HoldPiece::Stairs) door(0, 0, false);
            if (p.kind == HoldPiece::Stairs || p.kind == HoldPiece::Corridor || p.kind == HoldPiece::Altar ||
                p.kind == HoldPiece::FiveWay || p.kind == HoldPiece::Storeroom)
                door(0, p.len - 1, false);
            if (p.kind == HoldPiece::FiveWay || p.kind == HoldPiece::Storeroom) {
                door(p.half, p.len / 2, true);
                door(-p.half, p.len / 2, true);
            }
            switch (p.kind) {
            case HoldPiece::Stairs:
                // A spiral of steps up the shaft (vanilla: stone brick stairs; ours: blocks).
                for (int y = 1; y < p.h - 1; ++y) {
                    static constexpr int kRing[8][2] = {{-1, 1}, {0, 1}, {1, 1}, {1, 2}, {1, 3}, {0, 3}, {-1, 3}, {-1, 2}};
                    const int* c = kRing[(p.h - 1 - y) % 8];
                    put(c[0], y, c[1], B.stoneBricks);
                }
                for (int u = -1; u <= 1; ++u) // open to the surface way up? no: the top is closed (vanilla)
                    put(u, p.h - 1, 2, brick(u, p.h - 1, 2));
                break;
            case HoldPiece::Altar: chestAt(p.half - 1, 1, p.len / 2, LootTable::StrongholdCorridor); break;
            case HoldPiece::Storeroom: chestAt(p.half - 1, 1, 2, LootTable::StrongholdCrossing); break;
            case HoldPiece::Library:
                for (int v = 1; v < p.len - 1; ++v)
                    for (int y = 1; y <= 4; ++y) {
                        put(-p.half + 1, y, v, B.bookshelf);
                        put(p.half - 1, y, v, B.bookshelf);
                    }
                for (int u = -p.half + 1; u <= p.half - 1; ++u)
                    for (int y = 1; y <= 4; ++y)
                        put(u, y, p.len - 2, B.bookshelf);
                for (int y = 1; y <= 3; ++y) // keep the doorway clear
                    for (int u = -1; u <= 1; ++u)
                        put(u, y, 1, 0);
                chestAt(0, 1, p.len - 3, LootTable::StrongholdLibrary);
                break;
            case HoldPiece::Portal: {
                // A raised platform with a lava pool under the 12-frame ring, steps up
                // from the door; each frame holds an eye 1 time in 10 (wiki).
                for (int v = 6; v <= 12; ++v)
                    for (int u = -3; u <= 3; ++u) {
                        put(u, 1, v, B.stoneBricks);
                        put(u, 2, v, std::abs(u) <= 1 && v >= 7 && v <= 11 ? B.lava : B.stoneBricks); // 15 lava (wiki)
                    }
                for (int u = -1; u <= 1; ++u) {
                    put(u, 1, 4, B.stoneBricks);
                    put(u, 1, 5, B.stoneBricks);
                    put(u, 2, 5, B.stoneBricks);
                }
                // The ring: rows across at v 7 and 11, columns along at u -2 and 2; each
                // frame faces the middle. Local "forward" is the piece's dir.
                Xoroshiro eyes(mixSeed(mixSeed(mixSeed(m_seed, 0x45594553u), static_cast<uint32_t>(start.x)),
                                       static_cast<uint32_t>(start.z) * 131u + static_cast<uint64_t>(i)));
                const int fwd = p.dir, back = (p.dir + 2) & 3, right = (p.dir + 3) & 3, left = (p.dir + 1) & 3;
                auto frame = [&](int u, int v, int facing) {
                    const bool eye = eyes.nextInt(10) == 0;
                    put(u, 3, v, eye ? B.frameEye[facing] : B.frame[facing]);
                };
                for (int u = -1; u <= 1; ++u) {
                    frame(u, 7, fwd);
                    frame(u, 11, back);
                }
                for (int v = 8; v <= 10; ++v) { // the -u column faces +u (dir + 1), and back
                    frame(-2, v, left);
                    frame(2, v, right);
                }
                break;
            }
            default: break;
            }
        }
    }
}

void OverworldGenerator::placeOutposts(BlockStateId* blocks, int32_t cx, int32_t cz, GeneratedEntities& out) const {
    // Pillager outposts (overworld3, M24.4; wiki: Pillager Outpost): on their grid in
    // village biomes, never within 10 chunks of a village. Ours: a 7x7 watchtower of
    // dark oak and birch on a cobblestone base - closed lower rooms, an open top with a
    // fence and the loot chest, a ladder inside - and pillagers keeping watch.
    const auto& reg = blockRegistry();
    static const BlockStateId cobble = reg.defaultState(blocks::Cobblestone);
    static const BlockStateId log = *reg.parse("minecraft:dark_oak_log[axis=y]");
    static const BlockStateId darkPlanks = *reg.parse("minecraft:dark_oak_planks");
    static const BlockStateId birch = *reg.parse("minecraft:birch_planks");
    static const BlockStateId fence = reg.defaultState(*reg.findBlock("dark_oak_fence"));
    static const BlockStateId ladder = *reg.parse("minecraft:ladder[facing=north]"); // (on the wall to its south)
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            const ChunkPos start{cx + dx, cz + dz};
            if (!isSpreadCandidate(m_seed, kOutposts, start)) continue;
            // Only 1 in 5 candidates (wiki: Structure set - pillager outposts have frequency
            // 0.2; ours rolls it from its own seed).
            if (Xoroshiro fr(chunkSeed(m_seed, start.x, start.z, 165745296)); fr.nextFloat() >= 0.2f) continue;
            const int32_t sx = start.x * 16 + 4, sz = start.z * 16 + 4;
            const Biome biome = biomeAt(column(sx + 3, sz + 3));
            if (!(biome == Biome::Plains || biome == Biome::Desert || biome == Biome::Savanna || biome == Biome::Taiga ||
                  biome == Biome::SnowyPlains || biome == Biome::Meadow))
                continue;
            bool nearVillage = false;
            for (int vz = -10; vz <= 10 && !nearVillage; ++vz)
                for (int vx = -10; vx <= 10 && !nearVillage; ++vx)
                    nearVillage = isSpreadCandidate(m_seed, kVillages, {start.x + vx, start.z + vz});
            if (nearVillage) continue;
            const int ground = surfaceY(sx + 3, sz + 3);
            if (ground < kSeaLevel) continue;
            Xoroshiro r(chunkSeed(m_seed, start.x, start.z, 710));
            StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, sx, ground + 1, sz, 7, 7, static_cast<int>(r.nextInt(4)), &out};
            for (int x = 0; x < 7; ++x)
                for (int z = 0; z < 7; ++z)
                    sb.foundation(x, z, cobble, 12);
            sb.fill(0, 0, 0, 6, 0, 6, cobble);
            for (int y = 1; y <= 9; ++y) // walls: dark oak below, birch above; windows and a door
                for (int x = 0; x < 7; ++x)
                    for (int z = 0; z < 7; ++z) {
                        const bool edge = x == 0 || x == 6 || z == 0 || z == 6;
                        const bool corner = (x == 0 || x == 6) && (z == 0 || z == 6);
                        if (!edge || corner) continue;
                        const bool window = (y == 3 || y == 7) && (x == 3 || z == 3);
                        const bool door = z == 0 && x == 3 && y <= 2;
                        sb.set(x, y, z, window || door ? BlockStateId{0} : y <= 4 ? darkPlanks : birch);
                    }
            sb.fill(1, 1, 1, 5, 4, 5, 0);
            sb.fill(1, 6, 1, 5, 9, 5, 0);
            sb.fill(0, 5, 0, 6, 5, 6, birch);   // the floors
            sb.fill(0, 10, 0, 6, 10, 6, birch);
            sb.fill(0, 15, 0, 6, 15, 6, birch); // the lookout's floor
            for (int y = 1; y <= 15; ++y) sb.set(3, y, 5, sb.turned(ladder)); // (a shaft through the floors)
            for (int y = 1; y <= 16; ++y) // the corner posts, unbroken through the floors
                for (const auto& [px, pz] : {std::pair{0, 0}, std::pair{6, 0}, std::pair{0, 6}, std::pair{6, 6}})
                    sb.set(px, y, pz, log);
            for (int x = 0; x < 7; ++x)
                for (int z = 0; z < 7; ++z)
                    if ((x == 0 || x == 6 || z == 0 || z == 6) && !((x == 0 || x == 6) && (z == 0 || z == 6)))
                        sb.set(x, 16, z, fence);
            sb.chest(3, 16, 3, LootTable::PillagerOutpost);
            sb.mob(2, 1, 2, MobType::Pillager);
            sb.mob(4, 6, 2, MobType::Pillager);
            sb.mob(2, 16, 2, MobType::Pillager);
        }
}

void OverworldGenerator::placeVillages(BlockStateId* blocks, int32_t cx, int32_t cz, const std::array<int, 256>& topY,
                                       GeneratedEntities& out) const {
    // Villages (wiki: Village): plains, desert, savanna, taiga and snowy ones on the
    // village grid (spacing 34, separation 8). Ours: a well in the middle, 5-9 houses
    // or farms 14-30 blocks around it with their doors toward it, 3-wide dirt paths
    // to each, in the biome's materials. Chests in the larger houses.
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    for (int dz = -4; dz <= 4; ++dz)
        for (int dx = -4; dx <= 4; ++dx) {
            const ChunkPos start{cx + dx, cz + dz};
            if (!isSpreadCandidate(m_seed, kVillages, start)) continue;
            const int32_t mx = start.x * 16 + 8, mz = start.z * 16 + 8;
            const Biome biome = biomeAt(column(mx, mz));
            const bool desert = biome == Biome::Desert;
            const bool savanna = biome == Biome::Savanna;
            // (snowy taiga villages are Bedrock-only)
            const bool taiga = biome == Biome::Taiga || biome == Biome::SnowyPlains;
            if (!(biome == Biome::Plains || biome == Biome::Meadow || desert || savanna || taiga)) continue;
            const int ground = surfaceY(mx, mz);
            if (ground < kSeaLevel) continue;
            const BlockStateId wall = desert ? B.cutSandstone : savanna ? B.acaciaPlanks : taiga ? B.sprucePlanks : B.oakPlanks;
            const BlockStateId post = desert ? B.chiseledSandstone : savanna ? B.acaciaLog : taiga ? B.spruceLog : B.oakLog;
            const BlockStateId roof = desert ? B.smoothSandstone : savanna ? B.acaciaPlanks : taiga ? B.spruceLog : B.oakLog;
            const BlockStateId floor = desert ? B.sandstone : B.cobblestone;
            const LootTable loot = desert ? LootTable::VillageDesertHouse : LootTable::VillagePlainsHouse;
            const uint8_t villagerType = uint8_t(desert                       ? VillagerType::Desert
                                                 : savanna                    ? VillagerType::Savanna
                                                 : biome == Biome::SnowyPlains ? VillagerType::Snow
                                                 : taiga                      ? VillagerType::Taiga
                                                                              : VillagerType::Plains);
            Xoroshiro r(chunkSeed(m_seed, start.x, start.z, 720));
            // Paths first (houses go over them). A path cell sits on this chunk's top
            // block, which turns to a dirt path; plants on it go.
            auto path = [&](int32_t x, int32_t z) {
                const int lx = x - cx * 16, lz = z - cz * 16;
                if (lx < 0 || lx > 15 || lz < 0 || lz > 15) return;
                const int y = topY[size_t(lz * 16 + lx)];
                const BlockStateId top = Buf{blocks}.get(lx, y, lz);
                if (top != B.grass && top != B.snowyGrass && top != B.dirt && top != B.sand && top != B.coarseDirt &&
                    top != B.podzol)
                    return;
                Buf{blocks}.set(lx, y, lz, B.dirtPath);
                const BlockStateId above = Buf{blocks}.get(lx, y + 1, lz);
                if (above != 0 && !reg.collides(above)) Buf{blocks}.set(lx, y + 1, lz, 0);
            };
            struct House {
                int32_t x, z; // corner
                int w, d, rot, kind; // kind 0 small, 1 big, 2 farm
            };
            std::array<House, 9> houses{};
            int count = 0;
            const int n = 5 + static_cast<int>(r.nextInt(5));
            for (int i = 0; i < n; ++i) {
                const double angle = i * 2.0 * std::numbers::pi / n + (r.nextDouble() - 0.5) * 0.5;
                const double dist = 14.0 + r.nextDouble() * 16.0;
                const int kind = static_cast<int>(r.nextInt(10)) < 3 ? 2 : static_cast<int>(r.nextInt(2));
                const int w = kind == 2 ? 9 : kind == 1 ? 7 : 5, d = kind == 2 ? 7 : kind == 1 ? 7 : 5;
                const int32_t hx = mx + static_cast<int32_t>(std::lround(std::cos(angle) * dist));
                const int32_t hz = mz + static_cast<int32_t>(std::lround(std::sin(angle) * dist));
                // Door toward the well: pick the rotation whose front (local z = 0) faces it.
                const int32_t vx = mx - hx, vz = mz - hz;
                const int rot = std::abs(vx) > std::abs(vz) ? (vx > 0 ? 1 : 3) : (vz > 0 ? 2 : 0);
                const int ww = rot & 1 ? d : w, dd = rot & 1 ? w : d;
                House h{hx - ww / 2, hz - dd / 2, w, d, rot, kind};
                bool clear = std::abs(hx - mx) > 6 || std::abs(hz - mz) > 6;
                for (int j = 0; j < count && clear; ++j) {
                    const House& o = houses[size_t(j)];
                    const int ow = o.rot & 1 ? o.d : o.w, od = o.rot & 1 ? o.w : o.d;
                    clear = h.x + ww + 1 < o.x || o.x + ow + 1 < h.x || h.z + dd + 1 < o.z || o.z + od + 1 < h.z;
                }
                if (!clear) continue;
                houses[size_t(count++)] = h;
                // The path from the well to the door (3 wide), straight in steps.
                const int32_t doorX = h.x + ww / 2, doorZ = h.z + dd / 2;
                const int steps = std::max(std::abs(doorX - mx), std::abs(doorZ - mz));
                for (int s = 0; s <= steps; ++s) {
                    const int32_t px = mx + (doorX - mx) * s / std::max(1, steps), pz = mz + (doorZ - mz) * s / std::max(1, steps);
                    for (int k = -1; k <= 1; ++k) {
                        path(px + k, pz);
                        path(px, pz + k);
                    }
                }
            }
            // The well: a cobblestone ring with water, 4x4, on the ground.
            StructureBuilder well{Buf{blocks}, cx * 16, cz * 16, mx - 2, ground, mz - 2, 4, 4, 0, &out};
            for (int x = 0; x < 4; ++x)
                for (int z = 0; z < 4; ++z) {
                    well.foundation(x, z, B.cobblestone, 8);
                    const bool rim = x == 0 || x == 3 || z == 0 || z == 3;
                    well.set(x, 0, z, rim ? B.cobblestone : B.water);
                    well.set(x, -1, z, rim ? B.cobblestone : B.water);
                    well.set(x, -2, z, B.cobblestone);
                    well.set(x, 1, z, rim ? B.cobblestone : BlockStateId{0});
                    if (x != 0 && x != 3 && z != 0 && z != 3) well.set(x, 1, z, 0);
                }
            for (int i = 0; i < count; ++i) {
                const House& h = houses[size_t(i)];
                const int hw = h.rot & 1 ? h.d : h.w, hd = h.rot & 1 ? h.w : h.d;
                if (h.x + hw < cx * 16 || h.x > cx * 16 + 15 || h.z + hd < cz * 16 || h.z > cz * 16 + 15) continue;
                const int hy = surfaceY(h.x + 2, h.z + 2) + 1;
                StructureBuilder sb{Buf{blocks}, cx * 16, cz * 16, h.x, hy, h.z, h.w, h.d, h.rot, &out};
                Xoroshiro hr(chunkSeed(m_seed, h.x, h.z, 721));
                if (h.kind == 2) { // a farm: logs around farmland with a water row
                    for (int x = 0; x < h.w; ++x)
                        for (int z = 0; z < h.d; ++z) {
                            sb.foundation(x, z, B.dirt, 6);
                            const bool edge = x == 0 || x == h.w - 1 || z == 0 || z == h.d - 1;
                            sb.set(x, -1, z, edge ? post : x == h.w / 2 ? B.water : B.farmland);
                            sb.set(x, 0, z, 0);
                            sb.set(x, 1, z, 0);
                        }
                    const BlockStateId crop = B.crops[hr.nextInt(4)];
                    for (int x = 1; x < h.w - 1; ++x)
                        for (int z = 1; z < h.d - 1; ++z)
                            if (x != h.w / 2) sb.set(x, 0, z, crop);
                    continue;
                }
                // A house: floor, plank walls with corner posts, windows, a door gap,
                // a flat roof with a ridge; a chest in big ones, a torch, a crafting table.
                for (int x = 0; x < h.w; ++x)
                    for (int z = 0; z < h.d; ++z)
                        sb.foundation(x, z, floor, 10);
                sb.fill(0, 0, 0, h.w - 1, 0, h.d - 1, floor);
                for (int y = 1; y <= 3; ++y)
                    for (int x = 0; x < h.w; ++x)
                        for (int z = 0; z < h.d; ++z) {
                            const bool corner = (x == 0 || x == h.w - 1) && (z == 0 || z == h.d - 1);
                            const bool edge = x == 0 || x == h.w - 1 || z == 0 || z == h.d - 1;
                            sb.set(x, y, z, corner ? post : edge ? wall : BlockStateId{0});
                        }
                sb.fill(0, 4, 0, h.w - 1, 4, h.d - 1, desert ? roof : wall);
                if (!desert) sb.fill(h.w / 2, 5, 0, h.w / 2, 5, h.d - 1, roof);
                sb.fill(h.w / 2, 1, 0, h.w / 2, 2, 0, 0);          // door gap
                sb.set(0, 2, h.d / 2, B.glass);                    // windows
                sb.set(h.w - 1, 2, h.d / 2, B.glass);
                sb.set(1, 1, h.d - 2, B.craftingTable);
                sb.set(h.w - 2, 1, 1, B.torch);
                if (h.kind == 1) sb.chest(h.w - 2, 1, h.d - 2, loot);
                if (m_version >= 3) {
                    // overworld3 (M24.1): a bed per villager (one in small houses, two in big
                    // ones) and a job site; each bed's villager stands in the house.
                    static const BlockStateId bedHead =
                        *reg.parse("minecraft:red_bed[facing=north,occupied=false,part=head]");
                    static const BlockStateId bedFoot =
                        *reg.parse("minecraft:red_bed[facing=north,occupied=false,part=foot]");
                    static const std::array<BlockStateId, 9> jobSites = [&] {
                        std::array<BlockStateId, 9> j{};
                        const char* names[9] = {"composter",  "cartography_table", "fletching_table",
                                                "cauldron",   "lectern",           "stonecutter",
                                                "loom",       "smithing_table",    "grindstone[face=floor,facing=north]"};
                        for (int k = 0; k < 9; ++k) j[size_t(k)] = *reg.parse(std::string("minecraft:") + names[k]);
                        return j;
                    }();
                    auto bed = [&](int x, int z) { // the head at (x, z), the foot to its south (local)
                        sb.set(x, 1, z, sb.turned(bedHead));
                        sb.set(x, 1, z + 1, sb.turned(bedFoot));
                        sb.villager(x, 1, z + 1, villagerType, hr.nextInt(10) == 0);
                    };
                    bed(1, 1);
                    if (h.kind == 1) bed(3, 4);
                    sb.set(h.w - 2, 1, h.d / 2, sb.turned(jobSites[hr.nextInt(9)]));
                }
            }
            if (m_version >= 3) { // the bell by the well: the village's meeting point
                StructureBuilder bell{Buf{blocks}, cx * 16, cz * 16, mx + 2, ground, mz - 1, 1, 1, 0, &out};
                bell.foundation(0, 0, B.cobblestone, 8);
                bell.set(0, 0, 0, B.cobblestone);
                bell.set(0, 1, 0, reg.defaultState(blocks::Bell));
            }
        }
}

void OverworldGenerator::placeVegetation(BlockStateId* blocks, int32_t cx, int32_t cz,
                                         const std::array<int, 256>& topY, const std::array<Biome, 16>& biomes) const {
    const Blocks& B = blockSet();
    const auto& reg = blockRegistry();
    Buf chunk{blocks};
    Xoroshiro r(chunkSeed(m_seed, cx, cz, 630));
    auto biomeOf = [&](int x, int z) { return biomes[size_t((z / 4) * 4 + x / 4)]; };
    auto top = [&](int x, int z) { return topY[size_t(z * 16 + x)]; };
    auto inside = [](int x, int z) { return x >= 0 && x < 16 && z >= 0 && z < 16; };
    // A patch: `tries` positions spread around a random centre (vanilla random_patch),
    // clipped to this chunk; `place` decides and places at the column's top.
    auto patch = [&](int tries, int spread, auto&& place) {
        const int ox = static_cast<int>(r.nextInt(16)), oz = static_cast<int>(r.nextInt(16));
        for (int t = 0; t < tries; ++t) {
            const int x = ox + static_cast<int>(r.nextInt(uint32_t(spread * 2 + 1))) - spread;
            const int z = oz + static_cast<int>(r.nextInt(uint32_t(spread * 2 + 1))) - spread;
            const uint32_t roll = r.nextInt(1u << 16);
            if (!inside(x, z)) continue;
            const int y = top(x, z) + 1;
            if (y <= kSeaLevel - 1 || !kOverworldHeight.contains(y + 3) || chunk.get(x, y, z) != B.air) continue;
            place(x, y, z, roll);
        }
    };

    // Sugar cane (wiki: Sugar Cane › Natural generation): one patch in every desert
    // chunk, 1 in 3 in swamps, 1 in 5 in badlands, 1 in 6 elsewhere - not in cherry
    // groves, meadows or the snowy mountains; on grass, dirt or sand with water beside
    // the block it stands on; 2, 3 or 4 tall at 11, 5 and 2 in 18.
    const Biome centre = biomes[5];
    auto tall = [](uint32_t roll) { const uint32_t r = roll % 18; return r < 11 ? 0 : r < 16 ? 1 : 2; };
    const bool noCane = centre == Biome::CherryGrove || centre == Biome::Meadow || centre == Biome::Grove ||
                        centre == Biome::SnowySlopes || centre == Biome::FrozenPeaks || centre == Biome::JaggedPeaks ||
                        centre == Biome::StonyPeaks;
    const uint32_t caneOdds = centre == Biome::Desert ? 1 : centre == Biome::Swamp ? 3
                              : centre == Biome::Badlands || centre == Biome::WoodedBadlands || centre == Biome::ErodedBadlands ? 5
                                                                                                                                 : 6;
    const int canePatches = !noCane && r.nextInt(caneOdds) == 0 ? 1 : 0;
    for (int p = 0; p < canePatches; ++p)
        patch(20, 4, [&](int x, int y, int z, uint32_t roll) {
            const BlockStateId g = chunk.get(x, y - 1, z);
            if (g != B.grass && g != B.dirt && g != B.sand && g != B.redSand && g != B.coarseDirt) return;
            bool water = false;
            for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}})
                water = water || (inside(x + d[0], z + d[1]) && chunk.get(x + d[0], y - 1, z + d[1]) == B.water);
            if (!water) return;
            const int height = 2 + tall(roll);
            for (int k = 0; k < height && chunk.get(x, y + k, z) == B.air; ++k)
                chunk.set(x, y + k, z, B.sugarCane);
        });

    // Pumpkins (wiki: Pumpkin › Generation): a rare patch on grass (1 chunk in 300).
    if (r.nextInt(300) == 0)
        patch(64, 7, [&](int x, int y, int z, uint32_t) {
            if (chunk.get(x, y - 1, z) == B.grass) chunk.set(x, y, z, B.pumpkin);
        });

    // Cacti (wiki: Cactus › Generation): deserts and badlands, 1-3 tall, on sand with
    // nothing solid beside them (kept off the chunk edge, where we can't see).
    // (deserts twice as many as badlands; 1, 2 or 3 tall at 11, 5 and 2 in 18)
    const int cactusPatches = centre == Biome::Desert ? 2 : centre == Biome::Badlands || centre == Biome::ErodedBadlands ? 1 : 0;
    for (int p = 0; p < cactusPatches; ++p)
        patch(10, 4, [&](int x, int y, int z, uint32_t roll) {
            if (x < 1 || x > 14 || z < 1 || z > 14) return;
            const BlockStateId g = chunk.get(x, y - 1, z);
            if (g != B.sand && g != B.redSand) return;
            const int height = 1 + tall(roll);
            for (int k = 0; k < height; ++k) {
                bool clear = chunk.get(x, y + k, z) == B.air;
                for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}}) {
                    const BlockStateId n = chunk.get(x + d[0], y + k, z + d[1]);
                    clear = clear && !reg.collides(n) && n != B.lava;
                }
                if (!clear) break;
                chunk.set(x, y + k, z, B.cactus);
            }
        });

    // Mushrooms (wiki: Brown Mushroom › Natural generation): a patch in 1 chunk in 4
    // where the light is 12 or less (red ours: 1 in 8); ours only on the surface where
    // something covers them, or on mycelium/podzol.
    for (int kind = 0; kind < 2; ++kind) {
        const uint32_t odds = 4u << kind;
        if (r.nextInt(odds) != 0) continue;
        const BlockStateId m = kind == 0 ? B.brownMushroom : B.redMushroom;
        patch(64, 7, [&](int x, int y, int z, uint32_t) {
            const BlockStateId g = chunk.get(x, y - 1, z);
            if (g == B.mycelium || g == B.podzol) { // any light there (vanilla #mushroom_grow_block)
                chunk.set(x, y, z, m);
                return;
            }
            if (!reg.opaqueCube(g)) return;
            for (int k = 1; k <= 16; ++k) // covered (vanilla: light below 13)
                if (chunk.get(x, y + k, z) != B.air) {
                    chunk.set(x, y, z, m);
                    return;
                }
        });
    }
    (void)biomeOf;
}

glm::dvec3 OverworldGenerator::findSpawn() const {
    // Nearest dry, non-ocean column to the origin on a widening ring (vanilla picks
    // a suitable land spot near 0,0).
    for (int r = 0; r <= 1024; r += 8) {
        for (int i = -r; i <= r; i += 8) {
            const int pts[4][2] = {{i, -r}, {i, r}, {-r, i}, {r, i}};
            for (const auto& p : pts) {
                const Column c = column(p[0], p[1]);
                const Biome b = biomeAt(c);
                if (isOcean(b) || b == Biome::River || b == Biome::FrozenRiver) continue;
                const int y = surfaceY(p[0], p[1]);
                if (y >= kSeaLevel) return {p[0] + 0.5, y + 1.0, p[1] + 0.5};
            }
        }
    }
    return {0.5, surfaceY(0, 0) + 1.0, 0.5};
}

// Ocean floors (overworld4, M25.1; wiki: Ocean, Warm Ocean, Frozen Ocean, Kelp,
// Seagrass, Sea Pickle, Coral Reef, Iceberg): seagrass in every unfrozen ocean (and
// rivers), kelp forests in normal, cold and lukewarm oceans, coral reefs and sea pickles
// in warm oceans, icebergs in frozen oceans. Chunk-local, from this chunk's own random
// stream; icebergs are pure shapes from their start chunk, so neighbours agree.
void OverworldGenerator::placeOceanFloor(BlockStateId* blocks, int32_t cx, int32_t cz, const std::array<int, 256>& topY,
                                         const std::array<Biome, 16>& columnBiome) const {
    const auto& r = blockRegistry();
    Buf chunk{blocks};
    struct Ocean {
        BlockStateId water, sand, gravel, packedIce, blueIce, snow, seagrass, tallLower, tallUpper, kelp, kelpPlant;
        BlockStateId coralBlock[5], coral[5], fan[5], pickle[4];
    };
    static const Ocean O = [&] {
        Ocean o{};
        auto S = [&](BlockId b) { return r.defaultState(b); };
        o.water = S(blocks::Water);
        o.sand = S(blocks::Sand);
        o.gravel = S(blocks::Gravel);
        o.packedIce = S(blocks::PackedIce);
        o.blueIce = S(blocks::BlueIce);
        o.snow = S(blocks::SnowBlock);
        o.seagrass = S(blocks::Seagrass);
        o.tallLower = r.set(S(blocks::TallSeagrass), properties::doorHalf, 1);
        o.tallUpper = r.set(S(blocks::TallSeagrass), properties::doorHalf, 0);
        o.kelp = S(blocks::Kelp);
        o.kelpPlant = S(blocks::KelpPlant);
        for (int k = 0; k < 5; ++k) {
            const std::string kind = kCoralKinds[k];
            o.coralBlock[k] = S(*r.findBlock(kind + "_coral_block"));
            o.coral[k] = S(*r.findBlock(kind + "_coral"));    // (waterlogged by default)
            o.fan[k] = S(*r.findBlock(kind + "_coral_fan"));
        }
        for (int n = 0; n < 4; ++n) o.pickle[n] = r.set(S(blocks::SeaPickle), properties::pickles, n);
        return o;
    }();
    const int32_t baseX = cx * 16, baseZ = cz * 16;

    // Icebergs (wiki: Iceberg): about one start in 8 frozen-ocean chunks, a packed ice
    // mound of radius 4-9 rising 4-15 blocks with a snow cap, as deep below the surface as it is
    // high (at most to the floor), blue ice in its core now and then.
    struct Berg {
        int x, z, radius, height;
        bool blue;
    };
    std::array<Berg, 9> bergs{};
    int bergCount = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Xoroshiro br(chunkSeed(m_seed, cx + dx, cz + dz, 700));
            if (br.nextInt(8) != 0) continue;
            const int sx = (cx + dx) * 16 + 4 + int(br.nextInt(8)), sz = (cz + dz) * 16 + 4 + int(br.nextInt(8));
            const Column c = column(sx, sz);
            const Biome b = biomeAt(c);
            if ((b != Biome::FrozenOcean && b != Biome::DeepFrozenOcean) || c.height > kSeaLevel - 6) continue;
            bergs[size_t(bergCount++)] = {sx, sz, 4 + int(br.nextInt(6)), 4 + int(br.nextInt(12)), br.nextInt(3) == 0};
        }
    for (int i = 0; i < bergCount; ++i) {
        const Berg& g = bergs[size_t(i)];
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                const double dx = baseX + x - g.x, dz = baseZ + z - g.z;
                // A lumpy outline: the radius wobbles with the direction (same in every chunk).
                const double wobble = 1.0 + 0.25 * std::sin(std::atan2(dz, dx) * 3.0 + g.x * 0.37);
                const double t = std::sqrt(dx * dx + dz * dz) / (g.radius * wobble);
                if (t >= 1.0) continue;
                const double shape = 1.0 - t * t;
                const int top = kSeaLevel - 1 + int(std::lround(g.height * shape));
                const int bottom = std::max(topY[size_t(z * 16 + x)] + 1, kSeaLevel - 1 - int(std::lround(g.height * 1.4 * shape)));
                for (int y = bottom; y <= top; ++y) {
                    const BlockStateId cur = chunk.get(x, y, z);
                    if (cur != 0 && cur != O.water) continue;
                    BlockStateId b = O.packedIce;
                    if (y == top && y >= kSeaLevel) b = O.snow;
                    else if (g.blue && t < 0.45 && y < kSeaLevel - 2) b = O.blueIce;
                    chunk.set(x, y, z, b);
                }
            }
    }

    Xoroshiro rng(chunkSeed(m_seed, cx, cz, 710));
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            const float roll = rng.nextFloat(), roll2 = rng.nextFloat();
            const uint32_t pick = rng.nextInt(1000);
            const Biome biome = columnBiome[size_t((z / 4) * 4 + x / 4)];
            const bool river = biome == Biome::River;
            if (!isOcean(biome) && !river) continue;
            const int ty = topY[size_t(z * 16 + x)];
            const int depth = kSeaLevel - 1 - ty; // water blocks above the floor
            if (depth < 1 || chunk.get(x, ty + 1, z) != O.water) continue;
            const BlockStateId floor = chunk.get(x, ty, z);
            if (!r.collides(floor) || floor == O.packedIce) continue;
            const bool frozen = biome == Biome::FrozenOcean || biome == Biome::DeepFrozenOcean;
            const bool warm = biome == Biome::WarmOcean;
            const int32_t wx = baseX + x, wz = baseZ + z;
            if (warm) {
                // Coral reefs: patches (8x8 cells, a third of them reef) of coral block
                // stacks 1-3 high, one kind per 3x3 cell, with corals and fans on top.
                const bool reef = positional(m_seed, wx >> 3, 0, wz >> 3, 720) < 0.35;
                const int kind = int(positional(m_seed, floorDiv(wx, 3), 0, floorDiv(wz, 3), 721) * 5.0) % 5;
                int y = ty + 1;
                if (reef && roll < 0.55f && depth >= 3) {
                    const int h = std::min(1 + int(pick % 3), depth - 2);
                    for (int k = 0; k < h; ++k) chunk.set(x, y++, z, O.coralBlock[kind]);
                }
                if ((reef && roll2 < 0.5f) || roll2 < 0.08f) {
                    chunk.set(x, y, z, (pick & 1) ? O.coral[(kind + int(pick >> 4)) % 5] : O.fan[(kind + int(pick >> 5)) % 5]);
                } else if (roll2 > 0.97f) {
                    chunk.set(x, y, z, O.pickle[pick % 4]); // (sea pickles: glowing clumps of 1-4)
                } else if (roll2 > 0.6f && !reef) {
                    chunk.set(x, y, z, O.seagrass);
                }
                continue;
            }
            if (frozen) continue; // (no plants under the ice)
            const bool kelpy = !river && biome != Biome::WarmOcean;
            if (kelpy && roll < 0.07f && depth >= 3) {
                // Kelp: a stalk of 1-10 blocks (never out of the water), tipped by a
                // growing kelp with a random age (wiki: Kelp › Generation).
                const int h = std::min(1 + int(pick % 10), depth);
                for (int k = 0; k < h - 1; ++k) chunk.set(x, ty + 1 + k, z, O.kelpPlant);
                chunk.set(x, ty + h, z, r.set(O.kelp, properties::age25, 20 + int(pick % 5)));
            } else if (roll < (river ? 0.25f : 0.38f)) {
                // Seagrass; a quarter tall where there is room (wiki: Seagrass).
                if (depth >= 2 && roll2 < 0.25f) {
                    chunk.set(x, ty + 1, z, O.tallLower);
                    chunk.set(x, ty + 2, z, O.tallUpper);
                } else {
                    chunk.set(x, ty + 1, z, O.seagrass);
                }
            }
        }
}

} // namespace mc::world
