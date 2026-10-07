#include "world/OverworldGenerator.h"

#include "world/Blocks.h"
#include "world/Random.h"
#include "world/TreeFeature.h"

#include <algorithm>
#include <array>
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
    case Biome::FrozenOcean: return true;
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
    // Leaves by kind (oak, birch, spruce, acacia) and distance 1..7 (index d-1).
    BlockStateId leaves[4][7];
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
        const BlockStateId leafDefaults[4] = {x.oakLeaves, x.birchLeaves, x.spruceLeaves, x.acaciaLeaves};
        for (int k = 0; k < 4; ++k)
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
      m_noodleB(mixSeed(seed, 119), -6, 2), m_surface(mixSeed(seed, 120), -4, 2) {}

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
    // land (cave entrances), but not under the sea (no aquifers yet: they would
    // leave dry holes in the sea floor). All stay above the bedrock floor.
    const double floor = std::clamp((y - (kOverworldHeight.minY + 5.0)) / 4.0, 0.0, 1.0);
    const double fade = std::clamp((c.height - 12.0 - y) / 8.0, 0.0, 1.0) * floor;
    const bool land = c.height > kSeaLevel + 3;
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
    enum Kind { Oak, Birch, Spruce, Acacia } kind;
};

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
        int32_t cx = 0, cz = 0;
        TreePlan plan;
    };
    static thread_local std::array<Entry, 64> cache;
    Entry& e = cache[size_t((uint32_t(cx) * 31u + uint32_t(cz) * 17u) & 63u)];
    if (e.gen == this && e.cx == cx && e.cz == cz) return e.plan;
    e = {this, cx, cz, {}};
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
        BlockStateId log = B.oakLog;
        switch (kind) {
        case TreeShape::Oak: break;
        case TreeShape::Birch: log = B.birchLog; break;
        case TreeShape::Spruce: log = B.spruceLog; break;
        case TreeShape::Acacia: log = B.acaciaLog; break;
        }
        // Writes into this chunk only; logs replace air/leaves/plants, leaves only air.
        // Generated leaves carry their distance to the trunk (vanilla: 1..6, so they
        // never decay).
        const int kindIndex = static_cast<int>(kind);
        auto put = [&](int32_t x, int32_t y, int32_t z, int distance) {
            const bool isLog = distance == 0;
            const BlockStateId s = isLog ? log : B.leaves[kindIndex][distance - 1];
            const int lx = x - baseX, lz = z - baseZ;
            if (lx < 0 || lx > 15 || lz < 0 || lz > 15 || !kOverworldHeight.contains(y)) return;
            const BlockStateId cur = chunk.get(lx, y, lz);
            const BlockId curBlock = reg.blockOf(cur);
            if (cur == 0 || (isLog && (curBlock == blocks::OakLeaves || curBlock == blocks::BirchLeaves ||
                                       curBlock == blocks::SpruceLeaves || curBlock == blocks::AcaciaLeaves ||
                                       cur == B.shortGrass || cur == B.fern))) {
                chunk.set(lx, y, lz, s);
            }
        };
        Xoroshiro shape(chunkSeed(m_seed, wx, wz, 301)); // leaf corners: per tree
        treeShape(static_cast<TreeKind>(kindIndex), wx, ground + 1, wz, height, shape, put);
        // The ground under the trunk becomes dirt (vanilla), in this chunk only.
        const int lx = wx - baseX, lz = wz - baseZ;
        if (lx >= 0 && lx < 16 && lz >= 0 && lz < 16 && reg.blockOf(chunk.get(lx, ground, lz)) == blocks::GrassBlock)
            chunk.set(lx, ground, lz, B.dirt);
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
                        // dry, or lava at the bottom of the world.
                        const bool cave = j + 1 < kCornersY && tri(terrain) > 0.0;
                        b = cave ? (y < kLavaLevel ? B.lava : B.air) : B.water;
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
                                      biome == Biome::Beach || biome == Biome::Desert;
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
                        if (k == 0) b = B.redSand;
                        else if (k < 12) b = B.terracotta;
                        break;
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
            if (ground == B.grass || ground == B.snowyGrass) {
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
                default: grassChance = 0.05f; break;
                }
                if (roll < flowerChance) {
                    plant = B.flowers[pick % (biome == Biome::Plains || biome == Biome::Meadow ? 5 : 2)];
                } else if (roll < flowerChance + grassChance) {
                    plant = (biome == Biome::Taiga || biome == Biome::SnowyTaiga) && pick % 2 ? B.fern : B.shortGrass;
                }
            } else if ((ground == B.sand && biome == Biome::Desert) || ground == B.redSand || ground == B.terracotta) {
                if (roll < 0.01f) plant = B.deadBush;
            }
            if (plant) chunk.set(x, ty + 1, z, plant);
        }

    // 7b. More vegetation (M18.1): sugar cane, pumpkins, cacti, mushrooms.
    if (m_version >= 2) placeVegetation(blockArray.data(), cx, cz, topY, columnBiome);

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
            const float temp = biomeInfo(biome).temperature - std::max(0, y - 80) / 800.0f;
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

    // 10. Animals with new chunks (wiki: Spawn › Chunk generation): cow biomes get a
    //     herd in 1 of 10 chunks, standing on grass (our rate: see deviations).
    Xoroshiro animals(chunkSeed(m_seed, cx, cz, 500));
    const Biome herdBiome = columnBiome[5];
    // Cow biomes (wiki: Cow › Spawning): not meadows; swamps and snowy taigas too.
    const bool grassy = herdBiome == Biome::Plains || herdBiome == Biome::Forest || herdBiome == Biome::BirchForest ||
                        herdBiome == Biome::Taiga || herdBiome == Biome::SnowyTaiga || herdBiome == Biome::Savanna ||
                        herdBiome == Biome::WindsweptHills || herdBiome == Biome::Swamp;
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
                // A step touching water (under seas and rivers) fills with water below the
                // sea level instead of air, as vanilla's ravines under oceans.
                bool wet = false;
                for (int x = x0; x <= x1 && !wet; ++x)
                    for (int z = z0; z <= z1 && !wet; ++z)
                        for (int y = y0 - 1; y <= y1 + 1 && !wet; ++y)
                            wet = chunk.get(x, y, z) == B.water;
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

    // Sugar cane (wiki: Sugar Cane › Generation): on grass, dirt or sand with water
    // beside the block it stands on, 2-4 tall. 10 patches a chunk (deserts and
    // swamps more).
    const Biome centre = biomes[5];
    const int canePatches = centre == Biome::Swamp ? 20 : centre == Biome::Desert ? 13 : 10;
    for (int p = 0; p < canePatches; ++p)
        patch(20, 4, [&](int x, int y, int z, uint32_t roll) {
            const BlockStateId g = chunk.get(x, y - 1, z);
            if (g != B.grass && g != B.dirt && g != B.sand && g != B.redSand && g != B.coarseDirt) return;
            bool water = false;
            for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}})
                water = water || (inside(x + d[0], z + d[1]) && chunk.get(x + d[0], y - 1, z + d[1]) == B.water);
            if (!water) return;
            const int height = 2 + static_cast<int>(roll % 3 == 0 ? (roll >> 2) % 3 : (roll >> 2) % 2); // 2-4, mostly 2-3
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
    const int cactusPatches = centre == Biome::Desert ? 3 : centre == Biome::Badlands ? 2 : 0;
    for (int p = 0; p < cactusPatches; ++p)
        patch(10, 4, [&](int x, int y, int z, uint32_t roll) {
            if (x < 1 || x > 14 || z < 1 || z > 14) return;
            const BlockStateId g = chunk.get(x, y - 1, z);
            if (g != B.sand && g != B.redSand) return;
            const int height = 1 + static_cast<int>(roll % 3 == 0 ? (roll >> 2) % 3 : (roll >> 2) % 2);
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

    // Mushrooms (wiki: Mushroom › Generation): in the shade of trees - commonest in
    // taigas and swamps (brown 1 chunk in 4, red 1 in 8; elsewhere 1 in 32 / 64).
    const bool shady = centre == Biome::Taiga || centre == Biome::SnowyTaiga || centre == Biome::Swamp;
    for (int kind = 0; kind < 2; ++kind) {
        const uint32_t odds = (shady ? 4u : 32u) << kind;
        if (r.nextInt(odds) != 0) continue;
        const BlockStateId m = kind == 0 ? B.brownMushroom : B.redMushroom;
        patch(64, 7, [&](int x, int y, int z, uint32_t) {
            if (!reg.opaqueCube(chunk.get(x, y - 1, z))) return;
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

} // namespace mc::world
