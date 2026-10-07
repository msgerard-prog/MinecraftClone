#pragma once

#include "rendering/ZipArchive.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc::gfx {

// One source of resource files, addressed by vanilla paths such as
// "assets/minecraft/textures/block/stone.png". Either a folder (its root holds
// `assets/`) or a zip: a resource pack .zip or a client .jar.
class ResourcePack {
public:
    // Returns nullptr if `path` is neither a folder nor a readable zip/jar.
    static std::unique_ptr<ResourcePack> open(const std::filesystem::path& path);

    const std::string& name() const { return m_name; }
    std::optional<std::vector<uint8_t>> read(std::string_view path) const;
    // Files directly inside `folder` (no subfolders) whose names end with `suffix`.
    // Returns paths relative to `folder` ("stone.png").
    std::vector<std::string> list(std::string_view folder, std::string_view suffix) const;

private:
    std::string m_name;
    std::filesystem::path m_folder;    // folder packs
    std::unique_ptr<ZipArchive> m_zip; // zip / jar packs
};

// Packs in priority order: later packs override earlier ones, file by file
// (like vanilla's resource pack list, where packs on top win).
class PackStack {
public:
    void add(std::unique_ptr<ResourcePack> pack) { m_packs.push_back(std::move(pack)); }
    // Adds every folder / .zip / .jar inside `dir`, sorted by name (later = higher).
    void addAllIn(const std::filesystem::path& dir);

    // The file from the highest-priority pack that has it.
    std::optional<std::vector<uint8_t>> read(std::string_view path) const;
    // Union of list() over all packs, sorted, without duplicates.
    std::vector<std::string> list(std::string_view folder, std::string_view suffix) const;
    size_t size() const { return m_packs.size(); }
    const ResourcePack& pack(size_t i) const { return *m_packs[i]; }

private:
    std::vector<std::unique_ptr<ResourcePack>> m_packs;
};

} // namespace mc::gfx
