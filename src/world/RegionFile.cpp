#include "world/RegionFile.h"

#include "core/Compression.h"
#include "core/Log.h"

namespace mc::world {

namespace {

uint32_t readBe32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
void putBe32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v >> 24);
    p[1] = uint8_t(v >> 16);
    p[2] = uint8_t(v >> 8);
    p[3] = uint8_t(v);
}

} // namespace

bool RegionFile::open(const std::filesystem::path& path, bool create) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (!create) return false;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary);
        const std::vector<char> header(2 * kSector, 0);
        file.write(header.data(), std::streamsize(header.size()));
        if (!file) return false;
    }
    m_file.open(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!m_file) return false;
    std::vector<uint8_t> header(2 * kSector, 0);
    m_file.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size()));
    if (m_file.gcount() < static_cast<std::streamsize>(header.size())) {
        // Truncated header (e.g. crash while creating): treat as empty, rewrite it.
        m_file.clear();
        std::fill(header.begin(), header.end(), uint8_t(0));
        m_file.seekp(0);
        m_file.write(reinterpret_cast<const char*>(header.data()), std::streamsize(header.size()));
    }
    m_file.clear();
    m_file.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(m_file.tellg());
    m_used.assign(std::max<size_t>(2, (size + kSector - 1) / kSector), false);
    m_used[0] = m_used[1] = true;
    for (int i = 0; i < 1024; ++i) {
        m_locations[size_t(i)] = readBe32(&header[size_t(i) * 4]);
        m_timestamps[size_t(i)] = readBe32(&header[kSector + size_t(i) * 4]);
        const uint32_t offset = m_locations[size_t(i)] >> 8, count = m_locations[size_t(i)] & 0xFF;
        if (m_locations[size_t(i)] == 0) continue;
        if (offset < 2 || count == 0 || offset + count > m_used.size()) {
            MC_LOG_WARN("Region %s: chunk %d has an invalid location; ignored",
                        path.filename().string().c_str(), i);
            m_locations[size_t(i)] = 0; // points outside the file: ignore
            continue;
        }
        bool overlaps = false;
        for (uint32_t s = offset; s < offset + count; ++s)
            overlaps = overlaps || m_used[s];
        if (overlaps) { // shares sectors with another chunk: drop it, keep the first
            MC_LOG_WARN("Region %s: chunk %d overlaps another; ignored",
                        path.filename().string().c_str(), i);
            m_locations[size_t(i)] = 0;
            continue;
        }
        for (uint32_t s = offset; s < offset + count; ++s)
            m_used[s] = true;
    }
    return true;
}

std::optional<std::vector<uint8_t>> RegionFile::read(int index) {
    const uint32_t loc = m_locations[size_t(index)];
    if (loc == 0) return std::nullopt;
    const uint32_t offset = loc >> 8, count = loc & 0xFF;
    if (count == 0) return std::nullopt;
    m_file.clear();
    m_file.seekg(std::streamoff(offset) * kSector);
    uint8_t head[5];
    m_file.read(reinterpret_cast<char*>(head), 5);
    if (!m_file) return std::nullopt;
    const uint32_t length = readBe32(head);
    if (length < 1 || length > count * uint32_t(kSector) - 4) return std::nullopt;
    const uint8_t type = head[4];
    std::vector<uint8_t> data(length - 1);
    m_file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()));
    if (!m_file) return std::nullopt;
    switch (type) {
    case 1: return gzipDecompress(data);
    case 2: return zlibDecompress(data);
    case 3: return data; // uncompressed
    default:
        MC_LOG_WARN("Region: chunk %d uses unsupported compression %d", index, type);
        return std::nullopt;
    }
}

void RegionFile::writeHeaderEntry(int index) {
    uint8_t b[4];
    putBe32(b, m_locations[size_t(index)]);
    m_file.seekp(std::streamoff(index) * 4);
    m_file.write(reinterpret_cast<const char*>(b), 4);
    putBe32(b, m_timestamps[size_t(index)]);
    m_file.seekp(kSector + std::streamoff(index) * 4);
    m_file.write(reinterpret_cast<const char*>(b), 4);
}

bool RegionFile::write(int index, std::span<const uint8_t> nbt, uint32_t timestamp) {
    const std::vector<uint8_t> z = zlibCompress(nbt);
    const size_t total = 5 + z.size();
    const auto needed = static_cast<uint32_t>((total + kSector - 1) / kSector);
    if (needed > kMaxSectors) {
        MC_LOG_WARN("Region: chunk %d is too large (%zu bytes)", index, total);
        return false;
    }
    // First fit while the old copy is still marked used (it stays valid until the
    // header points elsewhere).
    const uint32_t old = m_locations[size_t(index)];
    uint32_t start = 2, run = 0;
    for (uint32_t s = 2; s < m_used.size() && run < needed; ++s) {
        if (m_used[s]) {
            run = 0;
            start = s + 1;
        } else {
            ++run;
        }
    }
    if (run < needed) start = static_cast<uint32_t>(m_used.size()) - run; // extend the file
    if (start + needed > m_used.size()) m_used.resize(start + needed, false);
    for (uint32_t s = start; s < start + needed; ++s)
        m_used[s] = true;

    std::vector<uint8_t> buf(size_t(needed) * kSector, 0); // padded to whole sectors
    putBe32(buf.data(), static_cast<uint32_t>(z.size() + 1));
    buf[4] = 2; // zlib
    std::copy(z.begin(), z.end(), buf.begin() + 5);
    m_file.clear();
    m_file.seekp(std::streamoff(start) * kSector);
    m_file.write(reinterpret_cast<const char*>(buf.data()), std::streamsize(buf.size()));
    m_file.flush();
    if (!m_file) {
        for (uint32_t s = start; s < start + needed; ++s)
            m_used[s] = false;
        return false;
    }
    m_locations[size_t(index)] = start << 8 | needed;
    m_timestamps[size_t(index)] = timestamp;
    writeHeaderEntry(index);
    m_file.flush();
    for (uint32_t s = old >> 8; s < (old >> 8) + (old & 0xFF); ++s) // now free the old copy
        m_used[s] = false;
    return static_cast<bool>(m_file);
}

} // namespace mc::world
