#include "rendering/ZipArchive.h"

#include "core/Log.h"

#include <fstream>
#include <iterator>

#include <stb_image.h> // stbi_zlib_decode_noheader_buffer (raw deflate)

namespace mc::gfx {

namespace {

// Zip structures are little-endian (APPNOTE.TXT, the .ZIP file format spec).
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

constexpr uint32_t kEndOfCentralDir = 0x06054b50;
constexpr uint32_t kCentralHeader = 0x02014b50;
constexpr uint32_t kLocalHeader = 0x04034b50;

} // namespace

bool ZipArchive::open(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    return openMemory(std::move(bytes));
}

bool ZipArchive::openMemory(std::vector<uint8_t> bytes) {
    m_bytes = std::move(bytes);
    m_entries.clear();
    return parseDirectory();
}

bool ZipArchive::parseDirectory() {
    const size_t n = m_bytes.size();
    if (n < 22) return false;
    // The end-of-central-directory record is at the end, before an optional comment.
    size_t eocd = std::string::npos;
    const size_t stop = n > 22 + 0xFFFF ? n - 22 - 0xFFFF : 0;
    for (size_t i = n - 22 + 1; i-- > stop;) {
        if (u32(&m_bytes[i]) == kEndOfCentralDir) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) return false;
    const uint16_t count = u16(&m_bytes[eocd + 10]);
    const uint32_t dirOffset = u32(&m_bytes[eocd + 16]);
    if (count == 0xFFFF || dirOffset == 0xFFFFFFFF) {
        MC_LOG_WARN("Zip: zip64 archives are not supported");
        return false;
    }

    size_t p = dirOffset;
    for (uint16_t i = 0; i < count; ++i) {
        if (p + 46 > n || u32(&m_bytes[p]) != kCentralHeader) return false;
        Entry e;
        e.method = u16(&m_bytes[p + 10]);
        e.compressedSize = u32(&m_bytes[p + 20]);
        e.size = u32(&m_bytes[p + 24]);
        const uint16_t nameLen = u16(&m_bytes[p + 28]);
        const uint16_t extraLen = u16(&m_bytes[p + 30]);
        const uint16_t commentLen = u16(&m_bytes[p + 32]);
        e.localHeader = u32(&m_bytes[p + 42]);
        if (p + 46 + nameLen > n) return false;
        std::string name(reinterpret_cast<const char*>(&m_bytes[p + 46]), nameLen);
        if (!name.empty() && name.back() != '/') m_entries.emplace(std::move(name), e);
        p += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

bool ZipArchive::contains(std::string_view name) const {
    return m_entries.contains(std::string(name));
}

std::optional<std::vector<uint8_t>> ZipArchive::read(std::string_view name) const {
    const auto it = m_entries.find(std::string(name));
    if (it == m_entries.end()) return std::nullopt;
    const Entry& e = it->second;
    const size_t h = e.localHeader;
    if (h + 30 > m_bytes.size() || u32(&m_bytes[h]) != kLocalHeader) return std::nullopt;
    const size_t data = h + 30 + u16(&m_bytes[h + 26]) + u16(&m_bytes[h + 28]);
    if (data + e.compressedSize > m_bytes.size()) return std::nullopt;
    if (e.size == 0) return std::vector<uint8_t>{};
    // Validate the claimed size before allocating: stored entries are copied as is;
    // deflate expands at most ~1032:1, and resource files are far below 256 MiB.
    constexpr uint64_t kMaxEntry = 256ull << 20;
    if (e.method == 0 && e.compressedSize != e.size) return std::nullopt;
    if (e.size > kMaxEntry || e.size > uint64_t{e.compressedSize} * 1032 + 64) return std::nullopt;

    std::vector<uint8_t> out(e.size);
    if (e.method == 0) { // stored
        std::copy_n(&m_bytes[data], e.size, out.begin());
        return out;
    }
    if (e.method == 8) { // deflate
        const int got = stbi_zlib_decode_noheader_buffer(
            reinterpret_cast<char*>(out.data()), static_cast<int>(out.size()),
            reinterpret_cast<const char*>(&m_bytes[data]), static_cast<int>(e.compressedSize));
        if (got != static_cast<int>(e.size)) return std::nullopt;
        return out;
    }
    MC_LOG_WARN("Zip: %.*s uses unsupported method %u", static_cast<int>(name.size()), name.data(),
                e.method);
    return std::nullopt;
}

std::vector<std::string> ZipArchive::names() const {
    std::vector<std::string> out;
    out.reserve(m_entries.size());
    for (const auto& [name, e] : m_entries)
        out.push_back(name);
    return out;
}

} // namespace mc::gfx
