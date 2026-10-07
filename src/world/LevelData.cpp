#include "world/LevelData.h"

#include "core/Compression.h"
#include "core/Files.h"
#include "core/Log.h"
#include "core/Nbt.h"
#include "world/ChunkSerializer.h"

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
    data.put("GameType", int32_t{1}); // creative
    data.put("allowCommands", int8_t{1});
    data.put("initialized", int8_t{1});
    data.put("SpawnX", static_cast<int32_t>(pos[0]));
    data.put("SpawnY", static_cast<int32_t>(pos[1]));
    data.put("SpawnZ", static_cast<int32_t>(pos[2]));
    Compound version;
    version.put("Id", kDataVersion);
    version.put("Name", std::string("1.21.1"));
    version.put("Snapshot", int8_t{0});
    data.put("Version", std::move(version));
    Compound gen;
    gen.put("seed", static_cast<int64_t>(seed));
    gen.put("generate_features", int8_t{1});
    data.put("WorldGenSettings", std::move(gen));

    Compound player;
    player.put("Pos", listOf(TagType::Double, {pos[0], pos[1], pos[2]}));
    player.put("Rotation", listOf(TagType::Float, {yaw, pitch}));
    Compound abilities;
    abilities.put("flying", static_cast<int8_t>(flying ? 1 : 0));
    abilities.put("mayfly", int8_t{1});
    abilities.put("instabuild", int8_t{1});
    abilities.put("invulnerable", int8_t{1});
    player.put("abilities", std::move(abilities));
    player.put("playerGameType", int32_t{1});
    player.put("SelectedItemSlot", int32_t{selectedSlot});
    std::vector<Tag> inventory;
    for (int i = 0; i < 9; ++i) {
        if (hotbar[size_t(i)].empty()) continue;
        Compound item;
        // Item id = block id (no properties): vanilla items don't carry states.
        const std::string& s = hotbar[size_t(i)];
        item.put("id", s.substr(0, s.find('[')));
        item.put("count", int32_t{1});
        item.put("Slot", static_cast<int8_t>(i));
        item.put("MinecraftCloneState", s); // the exact state (e.g. log axis)
        inventory.emplace_back(std::move(item));
    }
    player.put("Inventory", listOf(TagType::Compound, std::move(inventory)));
    data.put("Player", std::move(player));

    Compound ours;
    ours.put("generator", std::string(flat ? "flat" : "terrain"));
    data.put("MinecraftClone", std::move(ours));

    Compound root;
    root.put("Data", std::move(data));
    const auto bytes = gzipCompress(write(root));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    // Write to a temp file, then replace (vanilla keeps level.dat_old as backup).
    const auto tmp = dir / "level.dat_new";
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        if (!f) return false;
    }
    if (std::filesystem::exists(dir / "level.dat"))
        std::filesystem::rename(dir / "level.dat", dir / "level.dat_old", ec);
    std::filesystem::rename(tmp, dir / "level.dat", ec);
    return !ec;
}

std::optional<LevelData> LevelData::load(const std::filesystem::path& dir) {
    std::ifstream f(dir / "level.dat", std::ios::binary);
    if (!f) return std::nullopt;
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto raw = gzipDecompress(gz);
    const auto root = raw ? read(*raw) : std::nullopt;
    const Compound* data = root ? root->compound("Data") : nullptr;
    if (!data) {
        MC_LOG_ERROR("level.dat in %s is unreadable", dir.string().c_str());
        return std::nullopt;
    }
    LevelData l;
    if (auto s = data->string("LevelName")) l.name = *s;
    l.dayTime = data->integer("DayTime").value_or(0);
    l.gameTime = data->integer("Time").value_or(0);
    if (const Compound* gen = data->compound("WorldGenSettings"))
        l.seed = static_cast<uint64_t>(gen->integer("seed").value_or(0));
    if (const Compound* ours = data->compound("MinecraftClone"))
        if (auto g = ours->string("generator")) l.flat = *g == "flat";
    if (const Compound* p = data->compound("Player")) {
        if (const List* pos = p->list("Pos"); pos && pos->items.size() == 3)
            for (int i = 0; i < 3; ++i)
                if (auto d = pos->items[size_t(i)].get<double>()) l.pos[i] = *d;
        if (const List* rot = p->list("Rotation"); rot && rot->items.size() == 2) {
            if (auto v = rot->items[0].get<float>()) l.yaw = *v;
            if (auto v = rot->items[1].get<float>()) l.pitch = *v;
        }
        if (const Compound* a = p->compound("abilities")) l.flying = a->integer("flying").value_or(0) != 0;
        l.selectedSlot = static_cast<int>(p->integer("SelectedItemSlot").value_or(0)) % 9;
        if (const List* inv = p->list("Inventory"))
            for (const Tag& t : inv->items) {
                const Compound* item = t.get<Compound>();
                if (!item) continue;
                const auto slot = item->integer("Slot").value_or(-1);
                if (slot < 0 || slot > 8) continue;
                const std::string* st = item->string("MinecraftCloneState");
                if (!st) st = item->string("id");
                if (st) l.hotbar[size_t(slot)] = *st;
            }
    }
    return l;
}

} // namespace mc::world
