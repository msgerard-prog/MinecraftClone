#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <vector>

namespace mc::world {

// One Anvil region file r.<x>.<z>.mca holding 32x32 chunks (wiki: Region file
// format): an 8 KiB header (1024 sector locations + 1024 timestamps), then chunk
// payloads in 4 KiB sectors: big-endian length, compression type (2 = zlib), data.
// Not thread-safe: ChunkStorage serialises access.
class RegionFile {
public:
    static constexpr int kSector = 4096;
    static constexpr int kMaxSectors = 255; // larger chunks need .mcc files (unsupported)

    // Opens or creates the file. Returns false on IO errors.
    bool open(const std::filesystem::path& path);

    // Local chunk index: (z & 31) * 32 + (x & 31).
    static int index(int chunkX, int chunkZ) { return (chunkZ & 31) * 32 + (chunkX & 31); }

    // Decompressed chunk NBT, or nullopt if absent / unreadable.
    std::optional<std::vector<uint8_t>> read(int index);
    // Compresses (zlib) and writes; reuses the old sectors when it fits, otherwise
    // the first free run (or the end of the file). Returns false if too large.
    bool write(int index, std::span<const uint8_t> nbt, uint32_t timestamp);
    bool has(int index) const { return m_locations[size_t(index)] != 0; }

private:
    void writeHeaderEntry(int index);

    std::fstream m_file;
    std::vector<uint32_t> m_locations = std::vector<uint32_t>(1024, 0);  // offset << 8 | count
    std::vector<uint32_t> m_timestamps = std::vector<uint32_t>(1024, 0);
    std::vector<bool> m_used; // per sector
};

} // namespace mc::world
