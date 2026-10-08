// Maps (M28.2b, see the header; wiki: Map, Map item format).
#include "world/Maps.h"

#include "core/Compression.h"
#include "core/Log.h"
#include "core/Nbt.h"
#include "world/Blocks.h"
#include "world/Dimension.h"
#include "world/World.h"

#include <cmath>
#include <fstream>
#include <string_view>
#include <vector>

namespace mc::world {

namespace {

// Vanilla's base map colours, by id (wiki: Map item format › Base colors).
constexpr uint32_t kBase[62] = {
    0x000000, 0x7FB238, 0xF7E9A3, 0xC7C7C7, 0xFF0000, 0xA0A0FF, 0xA7A7A7, 0x007C00, 0xFFFFFF,
    0xA4A8B8, 0x976D4D, 0x707070, 0x4040FF, 0x8F7748, 0xFFFCF5, 0xD87F33, 0xB24CD8, 0x6699D8,
    0xE5E533, 0x7FCC19, 0xF27FA5, 0x4C4C4C, 0x999999, 0x4C7F99, 0x7F3FB2, 0x334CB2, 0x664C33,
    0x667F33, 0x993333, 0x191919, 0xFAEE4D, 0x5CDBD5, 0x4A80FF, 0x00D93A, 0x815631, 0x700200,
    0xD1B1A1, 0x9F5224, 0x95576C, 0x706C8A, 0xBA8524, 0x677535, 0xA04D4E, 0x392923, 0x876B62,
    0x575C5C, 0x7A4958, 0x4C3E5C, 0x4C3223, 0x4C522A, 0x8E3C2E, 0x251610, 0xBD3031, 0x943F61,
    0x5C191D, 0x167E86, 0x3A8E8C, 0x562C3E, 0x14B485, 0x646464, 0xD8AF93, 0x7FA796};
enum Base : uint8_t {
    None = 0,
    Grass = 1,
    Sand = 2,
    Wool = 3,
    Fire = 4,
    Ice = 5,
    Metal = 6,
    Plant = 7,
    Snow = 8,
    Clay = 9,
    Dirt = 10,
    Stone = 11,
    Water = 12,
    Wood = 13,
    Quartz = 14,
    Orange = 15,
    Black = 29,
    Gold = 30,
    Diamond = 31,
    Lapis = 32,
    Emerald = 33,
    Podzol = 34,
    Nether = 35,
    TerracottaWhite = 36,
    TerracottaGray = 43,
    CrimsonNylium = 52,
    CrimsonStem = 53,
    CrimsonHyphae = 54,
    WarpedNylium = 55,
    WarpedStem = 56,
    WarpedHyphae = 57,
    WarpedWart = 58,
    Deepslate = 59,
    RawIron = 60,
    GlowLichen = 61
};

bool has(std::string_view s, std::string_view part) {
    return s.find(part) != std::string_view::npos;
}

// Our assignment of vanilla's colours by block name (vanilla gives each block its own;
// the common blocks follow the wiki's examples, the rest go by material).
uint8_t colourByName(std::string_view n) {
    if (n.starts_with("minecraft:")) n.remove_prefix(10);
    // Dyed blocks: the dye's colour (white wool is "snow"); terracotta its own set.
    for (int c = 0; c < 16; ++c) {
        const std::string_view dye = kDyeColours[c];
        if (!n.starts_with(dye) || n.size() <= dye.size() || n[dye.size()] != '_') continue;
        const std::string_view rest = n.substr(dye.size() + 1);
        if (rest == "terracotta" || rest == "glazed_terracotta")
            return uint8_t(TerracottaWhite + c);
        if (rest == "wool" || rest == "carpet" || rest == "concrete" || rest == "concrete_powder" ||
            rest == "bed" || rest == "shulker_box" || rest == "banner" || rest == "candle")
            return c == 0 ? Snow : uint8_t(Orange + c - 1);
        if (rest.find("stained_glass") != std::string_view::npos)
            return c == 0 ? Snow : uint8_t(Orange + c - 1);
    }
    if (n == "air" || n == "cave_air" || n == "void_air" || n == "glass" || n == "glass_pane" ||
        n == "light" || n == "barrier" || has(n, "torch") || n == "tripwire" ||
        n == "redstone_wire" || has(n, "button") || n == "lever" || n == "ladder" || n == "rail" ||
        has(n, "_rail") || n == "fire" || n == "soul_fire" || n == "structure_void" ||
        (has(n, "pressure_plate") && !has(n, "stone") && !has(n, "polished")))
        return None;
    if (n == "water" || n == "bubble_column" || n == "kelp" || n == "kelp_plant" ||
        n == "seagrass" || n == "tall_seagrass")
        return Water;
    if (n == "lava" || n == "tnt" || n == "redstone_block" || n == "fire_coral_block") return Fire;
    if (n == "grass_block" || n == "slime_block") return Grass;
    if (has(n, "leaves") || has(n, "sapling"))
        return Plant; // (any wood's leaves - before the wood colours)
    if (n == "mycelium") return 24;
    if (n == "podzol" || has(n, "spruce") || n == "campfire") return Podzol;
    if (has(n, "crimson_nylium")) return CrimsonNylium;
    if (has(n, "crimson_hyphae")) return CrimsonHyphae;
    if (has(n, "crimson")) return CrimsonStem;
    if (has(n, "warped_nylium")) return WarpedNylium;
    if (has(n, "warped_hyphae")) return WarpedHyphae;
    if (n == "warped_wart_block") return WarpedWart;
    if (has(n, "warped")) return WarpedStem;
    if (has(n, "leaves") || has(n, "sapling") || n == "short_grass" || n == "tall_grass" ||
        n == "fern" || n == "large_fern" || has(n, "vine") || n == "sugar_cane" || n == "cactus" ||
        n == "lily_pad" || has(n, "tulip") || n == "dandelion" || n == "poppy" ||
        has(n, "orchid") || n == "allium" || n == "azure_bluet" || n == "oxeye_daisy" ||
        n == "cornflower" || n == "lily_of_the_valley" || n == "sunflower" || n == "lilac" ||
        n == "rose_bush" || n == "peony" || n == "wheat" || n == "carrots" || n == "potatoes" ||
        n == "beetroots" || n == "bamboo" || n == "sweet_berry_bush" || has(n, "dripleaf") ||
        n == "azalea" || n == "flowering_azalea" || n == "spore_blossom" ||
        has(n, "mangrove_propagule"))
        return Plant;
    if (n == "moss_block" || n == "moss_carpet") return 27;
    if (n == "snow" || n == "snow_block" || n == "powder_snow" || n == "white_wool") return Snow;
    if (n == "ice" || n == "packed_ice" || n == "blue_ice" || n == "frosted_ice") return Ice;
    if (n == "clay") return Clay;
    if (n == "glow_lichen") return GlowLichen;
    if (n == "raw_iron_block") return RawIron;
    if (has(n, "deepslate")) return Deepslate;
    if (has(n, "tuff")) return TerracottaGray;
    if (n == "calcite" || has(n, "cherry")) return TerracottaWhite;
    if (n == "terracotta") return Orange;
    if (has(n, "sandstone") && has(n, "red")) return Orange;
    if (n == "red_sand") return Orange;
    if (n == "sand" || has(n, "sandstone") || n == "glowstone" || has(n, "end_stone") ||
        has(n, "birch") || n == "bone_block" || n == "turtle_egg" || n == "scaffolding")
        return Sand;
    if (n == "dirt" || n == "coarse_dirt" || n == "farmland" || n == "dirt_path" ||
        n == "rooted_dirt" || has(n, "granite") || has(n, "jungle") || n == "mud" ||
        has(n, "packed_mud") || has(n, "mud_brick"))
        return Dirt;
    if (has(n, "acacia")) return Orange;
    if (has(n, "dark_oak")) return 26;
    if (has(n, "mangrove")) return 28;
    if (has(n, "pale_oak")) return Quartz;
    if (has(n, "oak") || has(n, "planks") || n == "crafting_table" ||
        (has(n, "chest") && !has(n, "ender")) || n == "bookshelf" || n == "barrel" ||
        n == "note_block" || n == "jukebox" || n == "composter" || has(n, "_sign") || n == "loom" ||
        n == "cartography_table" || n == "fletching_table" || n == "smithing_table" ||
        n == "lectern" || n == "beehive" || n == "bee_nest")
        return Wood;
    if (has(n, "quartz") || has(n, "diorite") || n == "sea_lantern") return Quartz;
    if (has(n, "nether_brick") || n == "netherrack" || has(n, "nether_wart") ||
        n == "magma_block" || has(n, "nether_gold_ore") || n == "nether_quartz_ore")
        return Nether;
    if (n == "obsidian" || n == "crying_obsidian" || has(n, "blackstone") || n == "coal_block" ||
        n == "basalt" || n == "polished_basalt" || n == "smooth_basalt" || n == "soul_sand" ||
        n == "soul_soil" || has(n, "ancient_debris") || has(n, "netherite"))
        return Black;
    if (n == "gold_block" || n == "raw_gold_block") return Gold;
    if (n == "diamond_block" || (has(n, "prismarine") && n != "prismarine")) return Diamond;
    if (n == "lapis_block") return Lapis;
    if (n == "emerald_block") return Emerald;
    if (n == "iron_block" || n == "iron_bars" || has(n, "iron_door") || has(n, "iron_trapdoor") ||
        n == "anvil" || has(n, "anvil") || n == "cauldron" || n == "hopper" ||
        n == "heavy_weighted_pressure_plate" || n == "brewing_stand" || n == "lantern" ||
        n == "chain")
        return Metal;
    if (has(n, "copper")) return Orange;
    if (has(n, "wool") || n == "cobweb") return Wool;
    if (has(n, "amethyst")) return 24;
    if (has(n, "sculk")) return Black;
    if (n == "prismarine") return 23;
    if (has(n, "coral")) return Plant;
    // Stone and everything else made of it (ores, bricks, furnaces, gravel...).
    return Stone;
}

const std::vector<uint8_t>& colourTable() {
    static const std::vector<uint8_t> table = [] {
        const BlockRegistry& r = blockRegistry();
        std::vector<uint8_t> t(r.blockCount(), 0);
        for (size_t b = 0; b < t.size(); ++b)
            t[b] = colourByName(r.block(BlockId(b)).id);
        return t;
    }();
    return table;
}

} // namespace

uint8_t mapColorOf(BlockId block) {
    const auto& t = colourTable();
    return block < t.size() ? t[block] : 0;
}

uint32_t mapColorRgb(uint8_t color) {
    static constexpr int kShade[4] = {180, 220, 255, 135};
    const uint32_t base = color >> 2 < 62 ? kBase[color >> 2] : 0;
    if (color >> 2 == 0) return 0;
    const int m = kShade[color & 3];
    const uint32_t r = ((base >> 16) & 255) * m / 255, g = ((base >> 8) & 255) * m / 255,
                   b = (base & 255) * m / 255;
    return r << 16 | g << 8 | b;
}

int Maps::create(int32_t x, int32_t z, int scale, uint8_t dimension) {
    // vanilla: size = 128 << scale; centre = floor((x + 64) / size) * size + size / 2 - 64
    const int size = 128 << scale;
    auto centre = [&](int32_t v) {
        return int32_t(std::floor((double(v) + 64.0) / size)) * size + size / 2 - 64;
    };
    MapData m;
    m.centerX = centre(x);
    m.centerZ = centre(z);
    m.scale = uint8_t(scale);
    m.dimension = dimension;
    const int id = m_next++;
    m_maps[id] = m;
    return id;
}

MapData* Maps::get(int id) {
    const auto it = m_maps.find(id);
    return it == m_maps.end() ? nullptr : &it->second;
}

const MapData* Maps::get(int id) const {
    const auto it = m_maps.find(id);
    return it == m_maps.end() ? nullptr : &it->second;
}

glm::dvec2 Maps::pixelOf(const MapData& map, double x, double z) {
    const double s = double(1 << map.scale);
    return {(x - map.centerX) / s + 64.0, (z - map.centerZ) / s + 64.0};
}

namespace {

// The top block that shows on a map in column (x, z): its height, colour and, for water,
// how deep the water is (0 = unloaded or nothing).
struct Top {
    int y = 0;
    uint8_t colour = 0;
    int waterDepth = 0;
};

Top topOf(const World& world, int32_t x, int32_t z) {
    Top t;
    const Chunk* c = world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c) return t;
    const auto& r = blockRegistry();
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    const int minY = c->height().minY;
    for (int si = c->sectionCount() - 1; si >= 0; --si) {
        const Section& sec = c->section(si);
        if (sec.nonAirCount() == 0) continue;
        for (int ly = 15; ly >= 0; --ly) {
            const BlockId b = r.blockOf(sec.get(lx, ly, lz));
            const uint8_t col = mapColorOf(b);
            if (col == 0) continue;
            if (t.colour == Water) { // counting down through the water
                if (col == Water) {
                    ++t.waterDepth;
                    continue;
                }
                return t;
            }
            t.y = minY + si * 16 + ly;
            t.colour = col;
            if (col != Water) return t;
            t.waterDepth = 1;
        }
    }
    return t;
}

} // namespace

void Maps::update(const World& world, MapData& map, const glm::dvec3& player, int64_t tick) {
    if (map.locked) return;
    const int s = 1 << map.scale;
    const glm::dvec2 at = pixelOf(map, player.x, player.z);
    const int px = int(std::floor(at.x)), pz = int(std::floor(at.y));
    // Pixels: 128 blocks at every scale, 64 in the Nether (wiki: Map).
    const int radius = (map.dimension == uint8_t(Dimension::Nether) ? 64 : 128) / s;
    for (int x = std::max(0, px - radius); x < std::min(128, px + radius); ++x) {
        if ((x & 15) != (tick & 15)) continue; // a sixteenth of the columns each tick
        // The pixel north of the first one gives the first height to compare with.
        const int32_t wx = map.centerX + (x - 64) * s + s / 2;
        int prevY =
            topOf(world, wx, map.centerZ + (std::max(0, pz - radius) - 64) * s + s / 2 - s).y;
        for (int z = std::max(0, pz - radius); z < std::min(128, pz + radius); ++z) {
            const int dx = x - px, dz = z - pz;
            const int32_t wz = map.centerZ + (z - 64) * s + s / 2;
            const Top t = topOf(world, wx, wz);
            const int y = t.y;
            if (dx * dx + dz * dz >= (radius - 2) * (radius - 2) || t.colour == 0) {
                prevY = y;
                continue;
            }
            // Shading (wiki: Map › Colors): water darker with depth (dithered); land
            // brighter facing north-up slopes and darker facing down ones.
            int shade = 1;
            if (t.colour == Water) {
                const double d = t.waterDepth * 0.1 + double((x + z) & 1) * 0.2;
                shade = d < 0.5 ? 2 : d > 0.9 ? 0 : 1;
            } else {
                const double d = double(y - prevY) * 4.0 / double(map.scale + 4) +
                                 (double((x + z) & 1) - 0.5) * 0.4;
                shade = d > 0.6 ? 2 : d < -0.6 ? 0 : 1;
            }
            prevY = y;
            const uint8_t colour = uint8_t(t.colour * 4 + shade);
            uint8_t& cell = map.colors[size_t(z * 128 + x)];
            if (cell != colour) {
                cell = colour;
                ++map.version;
            }
        }
    }
}

bool Maps::save(const std::filesystem::path& worldDir) {
    using namespace nbt;
    std::error_code ec;
    const auto dir = worldDir / "data";
    std::filesystem::create_directories(dir, ec);
    auto writeFile = [](const std::filesystem::path& p, const Compound& root) {
        const auto bytes = gzipCompress(write(root));
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        return bool(f);
    };
    bool ok = true;
    for (const auto& [id, m] : m_maps) {
        if (const auto it = m_savedVersion.find(id);
            it != m_savedVersion.end() && it->second == m.version)
            continue;
        Compound data;
        data.put("scale", int8_t(m.scale));
        data.put("dimension", std::string(dimensionInfo(Dimension(m.dimension)).id));
        data.put("trackingPosition", int8_t{1});
        data.put("unlimitedTracking", int8_t{0});
        data.put("locked", int8_t(m.locked ? 1 : 0));
        data.put("xCenter", int32_t(m.centerX));
        data.put("zCenter", int32_t(m.centerZ));
        data.put("banners", listOf(TagType::Compound, {}));
        data.put("frames", listOf(TagType::Compound, {}));
        data.put("colors", std::vector<int8_t>(m.colors.begin(), m.colors.end()));
        Compound root;
        root.put("data", std::move(data));
        root.put("DataVersion", int32_t{4671});
        if (writeFile(dir / ("map_" + std::to_string(id) + ".dat"), root))
            m_savedVersion[id] = m.version;
        else
            ok = false;
    }
    if (m_next > 0) { // idcounts.dat: the last id handed out (1.21: data {map})
        Compound counts;
        counts.put("map", int32_t(m_next - 1));
        Compound root;
        root.put("data", std::move(counts));
        root.put("DataVersion", int32_t{4671});
        ok = writeFile(dir / "idcounts.dat", root) && ok;
    }
    return ok;
}

bool Maps::load(const std::filesystem::path& worldDir) {
    using namespace nbt;
    const auto dir = worldDir / "data";
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return true;
    auto readFile = [](const std::filesystem::path& p) -> std::optional<Compound> {
        std::ifstream f(p, std::ios::binary);
        if (!f) return std::nullopt;
        const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
        const auto raw = gzipDecompress(gz);
        return raw ? read(*raw) : std::nullopt;
    };
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        const std::string name = e.path().filename().string();
        if (!name.starts_with("map_") || !name.ends_with(".dat")) continue;
        int id = 0;
        try {
            id = std::stoi(name.substr(4, name.size() - 8));
        } catch (...) {
            continue;
        }
        const auto root = readFile(e.path());
        const Compound* data = root ? root->compound("data") : nullptr;
        if (!data) {
            MC_LOG_WARN("Unreadable map %s", name.c_str());
            continue;
        }
        MapData m;
        m.scale = uint8_t(std::clamp<int64_t>(data->integer("scale").value_or(0), 0, 4));
        const std::string* dim = data->string("dimension");
        m.dimension = uint8_t(dim ? findDimension(*dim).value_or(Dimension::Overworld)
                                  : Dimension::Overworld);
        m.locked = data->integer("locked").value_or(0) != 0;
        m.centerX = int32_t(data->integer("xCenter").value_or(0));
        m.centerZ = int32_t(data->integer("zCenter").value_or(0));
        if (const auto* colors = data->byteArray("colors");
            colors && colors->size() == m.colors.size())
            for (size_t i = 0; i < m.colors.size(); ++i)
                m.colors[i] = uint8_t((*colors)[i]);
        m_maps[id] = m;
        m_savedVersion[id] = 0;
        m_next = std::max(m_next, id + 1);
    }
    if (const auto counts = readFile(dir / "idcounts.dat"))
        if (const Compound* d = counts->compound("data"))
            m_next = std::max(m_next, int(d->integer("map").value_or(-1)) + 1);
    return true;
}

} // namespace mc::world
