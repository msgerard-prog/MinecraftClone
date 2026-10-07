#include "world/LevelData.h"

#include "core/Compression.h"
#include "core/Files.h"
#include "core/Log.h"
#include "core/Nbt.h"
#include "world/ChunkSerializer.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace mc::world {

using namespace mc::nbt;

bool LevelData::save(const std::filesystem::path& dir) const {
    Compound data;
    data.put("DataVersion", kDataVersion);
    data.put("version", int32_t{19133}); // Anvil
    data.put("LevelName", name);
    data.put("DayTime", dayTime);
    data.put("Time", gameTime);
    data.put("GameType", int32_t{survival ? 0 : 1});
    data.put("allowCommands", int8_t{1});
    data.put("initialized", int8_t{1});
    data.put("SpawnX", spawn[0]);
    data.put("SpawnY", spawn[1]);
    data.put("SpawnZ", spawn[2]);
    data.put("SpawnAngle", 0.0f);
    data.put("LastPlayed", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     std::chrono::system_clock::now().time_since_epoch())
                                                     .count()));
    Compound version;
    version.put("Id", kDataVersion);
    version.put("Name", std::string("1.21.1"));
    version.put("Series", std::string("main"));
    version.put("Snapshot", int8_t{0});
    data.put("Version", std::move(version));
    Compound gen;
    gen.put("seed", static_cast<int64_t>(seed));
    gen.put("generate_features", int8_t{1});
    gen.put("bonus_chest", int8_t{0});
    data.put("WorldGenSettings", std::move(gen));

    Compound player;
    player.put("Pos", listOf(TagType::Double, {pos[0], pos[1], pos[2]}));
    player.put("Rotation", listOf(TagType::Float, {yaw, pitch}));
    player.put("Dimension", dimension);
    Compound abilities;
    abilities.put("flying", static_cast<int8_t>(flying ? 1 : 0));
    abilities.put("mayfly", static_cast<int8_t>(survival ? 0 : 1));
    abilities.put("instabuild", static_cast<int8_t>(survival ? 0 : 1));
    abilities.put("invulnerable", static_cast<int8_t>(survival ? 0 : 1));
    abilities.put("mayBuild", int8_t{1});
    abilities.put("flySpeed", 0.05f);
    abilities.put("walkSpeed", 0.1f);
    player.put("abilities", std::move(abilities));
    player.put("playerGameType", int32_t{survival ? 0 : 1});
    player.put("Health", health);
    player.put("foodLevel", int32_t{food});
    player.put("foodSaturationLevel", saturation);
    player.put("foodExhaustionLevel", exhaustion);
    player.put("foodTickTimer", int32_t{foodTimer});
    player.put("SelectedItemSlot", int32_t{selectedSlot});
    std::vector<Tag> items; // vanilla's Inventory list
    for (const SavedItem& it : inventory) {
        Compound item;
        const std::string& s = it.state;
        item.put("id", it.id);
        item.put("count", int32_t{it.count});
        item.put("Slot", static_cast<int8_t>(it.slot));
        Compound components;
        if (it.damage > 0) components.put("minecraft:damage", int32_t{it.damage});
        // The exact state (e.g. log axis) as vanilla's block_state item component.
        if (const size_t open = s.find('['); open != std::string::npos) {
            Compound props;
            std::string_view list(s.data() + open + 1, s.size() - open - 2);
            while (!list.empty()) {
                const size_t comma = std::min(list.find(','), list.size());
                const std::string_view kv = list.substr(0, comma);
                if (const size_t eq = kv.find('='); eq != std::string_view::npos)
                    props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
                list.remove_prefix(std::min(comma + 1, list.size()));
            }
            components.put("minecraft:block_state", std::move(props));
        }
        if (!components.entries.empty()) item.put("components", std::move(components));
        items.emplace_back(std::move(item));
    }
    player.put("Inventory", listOf(TagType::Compound, std::move(items)));
    data.put("Player", std::move(player));

    Compound ours;
    ours.put("generator", flat ? std::string("flat") : generator);
    std::vector<Tag> portalTags;
    for (const Portal& p : portals) {
        Compound c;
        c.put("dimension", p.dimension);
        c.put("x", p.x);
        c.put("y", p.y);
        c.put("z", p.z);
        portalTags.emplace_back(std::move(c));
    }
    ours.put("portals", listOf(TagType::Compound, std::move(portalTags)));
    data.put("MinecraftClone", std::move(ours));

    Compound root;
    root.put("Data", std::move(data));
    const auto bytes = gzipCompress(write(root));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto tmp = dir / "level.dat_new";
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        f.close(); // a failed flush (disk full) shows up here
        if (!f) return false;
    }
    // Backup by copy, so level.dat exists at every moment; then one replacing rename.
    if (std::filesystem::exists(dir / "level.dat", ec))
        std::filesystem::copy_file(dir / "level.dat", dir / "level.dat_old",
                                   std::filesystem::copy_options::overwrite_existing, ec);
    ec.clear();
    std::filesystem::rename(tmp, dir / "level.dat", ec);
    return !ec;
}

namespace {

std::optional<Compound> readLevelFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto raw = gzipDecompress(gz);
    auto root = raw ? read(*raw) : std::nullopt;
    if (!root || !root->compound("Data")) return std::nullopt;
    return root;
}

} // namespace

std::optional<LevelData> LevelData::load(const std::filesystem::path& dir) {
    std::optional<Compound> root;
    for (const char* name : {"level.dat", "level.dat_old", "level.dat_new"}) {
        root = readLevelFile(dir / name);
        if (root) {
            if (std::string_view(name) != "level.dat")
                MC_LOG_WARN("level.dat in %s is missing or unreadable; using %s",
                            dir.string().c_str(), name);
            break;
        }
    }
    if (!root) return std::nullopt;
    const Compound* data = root->compound("Data");
    LevelData l;
    if (auto s = data->string("LevelName")) l.name = *s;
    l.dayTime = data->integer("DayTime").value_or(0);
    l.gameTime = data->integer("Time").value_or(0);
    l.spawn[0] = static_cast<int32_t>(data->integer("SpawnX").value_or(0));
    l.spawn[1] = static_cast<int32_t>(data->integer("SpawnY").value_or(64));
    l.spawn[2] = static_cast<int32_t>(data->integer("SpawnZ").value_or(0));
    if (const Compound* gen = data->compound("WorldGenSettings"))
        l.seed = static_cast<uint64_t>(gen->integer("seed").value_or(0));
    if (const Compound* ours = data->compound("MinecraftClone")) {
        if (auto g = ours->string("generator")) {
            l.flat = *g == "flat";
            if (!l.flat) l.generator = *g;
        }
        if (const List* portals = ours->list("portals"))
            for (const Tag& t : portals->items)
                if (const Compound* c = t.get<Compound>(); c && c->string("dimension"))
                    l.portals.push_back({*c->string("dimension"), static_cast<int32_t>(c->integer("x").value_or(0)),
                                         static_cast<int32_t>(c->integer("y").value_or(0)),
                                         static_cast<int32_t>(c->integer("z").value_or(0))});
    }
    if (const Compound* p = data->compound("Player")) {
        if (const List* pos = p->list("Pos"); pos && pos->items.size() == 3)
            for (int i = 0; i < 3; ++i)
                if (auto d = pos->items[size_t(i)].get<double>()) l.pos[i] = *d;
        if (const List* rot = p->list("Rotation"); rot && rot->items.size() == 2) {
            if (auto v = rot->items[0].get<float>()) l.yaw = *v;
            if (auto v = rot->items[1].get<float>()) l.pitch = *v;
        }
        if (auto d = p->string("Dimension")) l.dimension = *d;
        if (const Compound* a = p->compound("abilities")) l.flying = a->integer("flying").value_or(0) != 0;
        l.survival = p->integer("playerGameType").value_or(data->integer("GameType").value_or(1)) == 0;
        if (auto h = p->real("Health")) l.health = static_cast<float>(*h);
        l.food = static_cast<int>(p->integer("foodLevel").value_or(20));
        if (auto v = p->real("foodSaturationLevel")) l.saturation = static_cast<float>(*v);
        if (auto v = p->real("foodExhaustionLevel")) l.exhaustion = static_cast<float>(*v);
        l.foodTimer = static_cast<int>(p->integer("foodTickTimer").value_or(0));
        l.selectedSlot = static_cast<int>(p->integer("SelectedItemSlot").value_or(0)) % 9;
        if (const List* inv = p->list("Inventory"))
            for (const Tag& t : inv->items) {
                const Compound* item = t.get<Compound>();
                if (!item) continue;
                const auto slot = item->integer("Slot").value_or(-1);
                if (slot < 0 || slot > 35) continue;
                const std::string* id = item->string("id");
                if (!id) continue;
                SavedItem saved;
                saved.slot = static_cast<int>(slot);
                saved.id = *id;
                saved.count = static_cast<int>(item->integer("count").value_or(1));
                std::string st = *id;
                const Compound* comps = item->compound("components");
                if (comps) saved.damage = static_cast<int>(comps->integer("minecraft:damage").value_or(0));
                const Compound* props = comps ? comps->compound("minecraft:block_state") : nullptr;
                if (props && !props->entries.empty()) {
                    st += '[';
                    for (size_t i = 0; i < props->entries.size(); ++i) {
                        const std::string* v = props->entries[i].value.get<std::string>();
                        if (i) st += ',';
                        st += props->entries[i].name + '=' + (v ? *v : std::string());
                    }
                    st += ']';
                    saved.state = std::move(st);
                }
                l.inventory.push_back(std::move(saved));
            }
    }
    return l;
}

} // namespace mc::world
