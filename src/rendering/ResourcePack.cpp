#include "rendering/ResourcePack.h"

#include "core/Log.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace mc::gfx {

namespace fs = std::filesystem;

std::unique_ptr<ResourcePack> ResourcePack::open(const fs::path& path) {
    auto pack = std::make_unique<ResourcePack>();
    pack->m_name = path.filename().string();
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        pack->m_folder = path;
        return pack;
    }
    const std::string ext = path.extension().string();
    if (ext == ".zip" || ext == ".jar") {
        pack->m_zip = std::make_unique<ZipArchive>();
        if (pack->m_zip->open(path.string())) return pack;
        MC_LOG_WARN("Resource pack %s: not a readable zip", pack->m_name.c_str());
    }
    return nullptr;
}

std::optional<std::vector<uint8_t>> ResourcePack::read(std::string_view path) const {
    if (m_zip) return m_zip->read(path);
    std::ifstream file(m_folder / fs::path(std::string(path)), std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}

std::vector<std::string> ResourcePack::list(std::string_view folder,
                                            std::string_view suffix) const {
    std::vector<std::string> out;
    auto accept = [&](std::string_view file) {
        if (file.size() >= suffix.size() && file.substr(file.size() - suffix.size()) == suffix &&
            file.find('/') == std::string_view::npos) {
            out.emplace_back(file);
        }
    };
    if (m_zip) {
        for (const std::string& name : m_zip->names()) {
            if (name.size() > folder.size() &&
                std::string_view(name).substr(0, folder.size()) == folder) {
                accept(std::string_view(name).substr(folder.size()));
            }
        }
    } else {
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(m_folder / fs::path(std::string(folder)), ec)) {
            if (e.is_regular_file()) accept(e.path().filename().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void PackStack::addAllIn(const fs::path& dir) {
    std::error_code ec;
    std::vector<fs::path> entries;
    for (const auto& e : fs::directory_iterator(dir, ec))
        entries.push_back(e.path());
    // Client .jar files are the Default pack in vanilla: always at the bottom, below
    // every user pack. Then folders and .zip packs in name order (later wins).
    std::sort(entries.begin(), entries.end(), [](const fs::path& a, const fs::path& b) {
        const bool ja = a.extension() == ".jar", jb = b.extension() == ".jar";
        if (ja != jb) return ja;
        return a < b;
    });
    for (const auto& p : entries) {
        if (auto pack = ResourcePack::open(p)) {
            MC_LOG_INFO("Resource pack: %s", pack->name().c_str());
            add(std::move(pack));
        }
    }
}

std::optional<std::vector<uint8_t>> PackStack::read(std::string_view path) const {
    for (auto it = m_packs.rbegin(); it != m_packs.rend(); ++it) {
        if (auto bytes = (*it)->read(path)) return bytes;
    }
    return std::nullopt;
}

std::vector<std::string> PackStack::list(std::string_view folder, std::string_view suffix) const {
    std::vector<std::string> out;
    for (const auto& pack : m_packs) {
        auto files = pack->list(folder, suffix);
        out.insert(out.end(), files.begin(), files.end());
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace mc::gfx
