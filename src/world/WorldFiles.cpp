#include "world/WorldFiles.h"

#include "core/Compression.h"
#include "core/Log.h"
#include "world/ChunkSerializer.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace mc::world {

namespace fs = std::filesystem;

fs::path dimensionFolder(const fs::path& world, Dimension d) { return world / std::string(dimensionInfo(d).folder); }

fs::path dimensionDataFolder(const fs::path& world, Dimension d) {
    return dimensionFolder(world, d) / "data" / "minecraft";
}

fs::path dataFolder(const fs::path& world) { return world / "data" / "minecraft"; }

fs::path playersFolder(const fs::path& world, std::string_view kind) { return world / "players" / std::string(kind); }

fs::path mapsFolder(const fs::path& world) { return dataFolder(world) / "maps"; }

std::string uuidFileName(uint64_t hi, uint64_t lo, std::string_view extension) {
    char name[40];
    std::snprintf(name, sizeof(name), "%08x-%04x-%04x-%04x-%012llx", unsigned(hi >> 32), unsigned(hi >> 16 & 0xFFFF),
                  unsigned(hi & 0xFFFF), unsigned(lo >> 48), static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFull));
    return std::string(name) + std::string(extension);
}

namespace {
fs::path withSuffix(fs::path p, const char* suffix) {
    p += suffix;
    return p;
}
} // namespace

bool writeNbtFile(const fs::path& file, const nbt::Compound& root) {
    const auto bytes = gzipCompress(nbt::write(root));
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    const fs::path tmp = withSuffix(file, "_new");
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        f.close(); // (a failed flush - a full disk - shows up here)
        if (!f) return false;
    }
    // (M34 review) the previous version stays as <name>_old, so a damaged file isn't the end
    if (fs::exists(file, ec)) fs::copy_file(file, withSuffix(file, "_old"), fs::copy_options::overwrite_existing, ec);
    ec.clear();
    fs::rename(tmp, file, ec); // (replaces the old file in one step)
    return !ec;
}

namespace {
std::optional<nbt::Compound> readOne(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return std::nullopt;
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto raw = gzipDecompress(gz);
    return raw ? nbt::read(*raw) : std::nullopt;
}
} // namespace

std::optional<nbt::Compound> readNbtFile(const fs::path& file) {
    if (auto c = readOne(file)) return c;
    std::error_code ec;
    if (auto c = readOne(withSuffix(file, "_old"))) {
        if (fs::exists(file, ec)) MC_LOG_WARN("%s is unreadable; using its _old copy", file.string().c_str());
        return c;
    }
    if (fs::exists(file, ec)) MC_LOG_ERROR("%s is unreadable", file.string().c_str());
    return std::nullopt;
}

bool writeSavedData(const fs::path& file, nbt::Compound data) {
    nbt::Compound root;
    root.put("data", std::move(data));
    root.put("DataVersion", kDataVersion);
    return writeNbtFile(file, root);
}

std::optional<nbt::Compound> readSavedData(const fs::path& file) {
    auto root = readNbtFile(file);
    if (!root) return std::nullopt;
    if (const nbt::Compound* data = root->compound("data")) return *data;
    return std::nullopt;
}

namespace {

// A folder's entries, listed before any of them moves (renaming while iterating is unspecified).
std::vector<fs::path> entries(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) out.push_back(e.path());
    return out;
}

// Moves `from` to `to`. A folder whose target already exists is merged entry by entry
// (an entry already there wins: it is the newer one). Returns false if anything stayed.
bool moveInto(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (!fs::exists(from, ec)) return true;
    fs::create_directories(to.parent_path(), ec);
    if (!fs::exists(to, ec)) {
        fs::rename(from, to, ec);
        if (!ec) return true;
        MC_LOG_ERROR("Can't move %s to %s: %s", from.string().c_str(), to.string().c_str(), ec.message().c_str());
        return false;
    }
    if (!fs::is_directory(from, ec) || !fs::is_directory(to, ec)) {
        // (M34 review) both exist - a world opened again by an older build after moving: the
        // newer file wins, the other is kept beside it as <name>.conflict, never deleted.
        const bool fromNewer = fs::last_write_time(from, ec) > fs::last_write_time(to, ec);
        const fs::path loser = withSuffix(to, ".conflict");
        fs::remove_all(loser, ec);
        if (fromNewer) {
            fs::rename(to, loser, ec);
            if (!ec) fs::rename(from, to, ec);
        } else {
            fs::rename(from, loser, ec);
        }
        MC_LOG_WARN("Both %s and %s existed; kept the newer, the other is %s", from.string().c_str(),
                    to.string().c_str(), loser.string().c_str());
        if (ec) MC_LOG_ERROR("Can't settle %s: %s", to.string().c_str(), ec.message().c_str());
        return !ec;
    }
    bool ok = true;
    for (const fs::path& p : entries(from)) ok = moveInto(p, to / p.filename()) && ok;
    fs::remove(from, ec); // (only once empty)
    return ok;
}

} // namespace

bool migrateLegacyLayout(const fs::path& world) {
    std::error_code ec;
    const bool legacy = fs::exists(world / "region", ec) || fs::exists(world / "entities", ec) ||
                        fs::exists(world / "DIM-1", ec) || fs::exists(world / "DIM1", ec) ||
                        fs::exists(world / "playerdata", ec) || fs::exists(world / "stats", ec) ||
                        fs::exists(world / "advancements", ec) || fs::exists(world / "data" / "idcounts.dat", ec);
    if (!legacy) return true;
    MC_LOG_INFO("Moving \"%s\" to the 26.1 world layout", world.filename().string().c_str());
    bool ok = true;
    // Dimensions: the Overworld's folders sat in the world folder itself.
    const fs::path overworld = dimensionFolder(world, Dimension::Overworld);
    for (const char* sub : {"region", "entities", "poi"}) ok = moveInto(world / sub, overworld / sub) && ok;
    ok = moveInto(world / "DIM-1", dimensionFolder(world, Dimension::Nether)) && ok;
    ok = moveInto(world / "DIM1", dimensionFolder(world, Dimension::End)) && ok;
    // Players.
    ok = moveInto(world / "playerdata", playersFolder(world, "data")) && ok;
    ok = moveInto(world / "stats", playersFolder(world, "stats")) && ok;
    ok = moveInto(world / "advancements", playersFolder(world, "advancements")) && ok;
    // Maps: data/map_<id>.dat -> data/minecraft/maps/<id>.dat, idcounts.dat -> last_id.dat.
    if (fs::exists(world / "data", ec)) {
        for (const fs::path& p : entries(world / "data")) {
            const std::string name = p.filename().string();
            if (name.starts_with("map_") && name.ends_with(".dat")) ok = moveInto(p, mapsFolder(world) / name.substr(4)) && ok;
        }
        ok = moveInto(world / "data" / "idcounts.dat", mapsFolder(world) / "last_id.dat") && ok;
    }
    return ok;
}

} // namespace mc::world
