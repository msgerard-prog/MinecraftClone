#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::gfx {

// Read-only .zip reader (resource packs, and client .jar files, which are zips).
// Supports "stored" and "deflate" entries via stb_image's inflate; no zip64,
// no encryption. Loads the whole archive into memory (load time only).
class ZipArchive {
public:
    bool open(const std::string& path);
    bool openMemory(std::vector<uint8_t> bytes); // for tests

    bool contains(std::string_view name) const;
    std::optional<std::vector<uint8_t>> read(std::string_view name) const;
    // Every file name in the archive ("assets/minecraft/textures/block/stone.png").
    std::vector<std::string> names() const;

private:
    struct Entry {
        uint32_t localHeader = 0;
        uint32_t compressedSize = 0;
        uint32_t size = 0;
        uint16_t method = 0;
    };
    bool parseDirectory();

    std::vector<uint8_t> m_bytes;
    std::unordered_map<std::string, Entry> m_entries;
};

} // namespace mc::gfx
