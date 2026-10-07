#include "core/Compression.h"

#include <array>
#include <cstdlib>
#include <cstring>

// The single definition of stb's image and image-write implementations (PNG only).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace mc {

namespace {

std::vector<uint8_t> take(char* p, int n) {
    std::vector<uint8_t> out(reinterpret_cast<uint8_t*>(p), reinterpret_cast<uint8_t*>(p) + n);
    std::free(p);
    return out;
}

} // namespace

std::vector<uint8_t> zlibCompress(std::span<const uint8_t> data) {
    int len = 0;
    unsigned char* z = stbi_zlib_compress(const_cast<unsigned char*>(data.data()),
                                          static_cast<int>(data.size()), &len, 6);
    if (!z) return {};
    std::vector<uint8_t> out(z, z + len);
    STBIW_FREE(z);
    return out;
}

std::optional<std::vector<uint8_t>> zlibDecompress(std::span<const uint8_t> data) {
    int len = 0;
    char* p = stbi_zlib_decode_malloc_guesssize_headerflag(
        reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()),
        static_cast<int>(data.size() * 4 + 64), &len, 1);
    if (!p) return std::nullopt;
    return take(p, len);
}

uint32_t crc32(std::span<const uint8_t> data) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : data)
        c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::vector<uint8_t> gzipCompress(std::span<const uint8_t> data) {
    // gzip = 10-byte header + raw deflate + CRC32 + size. stb makes a zlib stream:
    // strip its 2-byte header and 4-byte Adler-32 trailer to get the raw deflate.
    const std::vector<uint8_t> z = zlibCompress(data);
    if (z.size() < 6) return {};
    std::vector<uint8_t> out = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0xFF};
    out.insert(out.end(), z.begin() + 2, z.end() - 4);
    const uint32_t crc = crc32(data), size = static_cast<uint32_t>(data.size());
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(crc >> (8 * i)));
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(size >> (8 * i)));
    return out;
}

std::optional<std::vector<uint8_t>> gzipDecompress(std::span<const uint8_t> data) {
    if (data.size() < 18 || data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) return std::nullopt;
    const uint8_t flags = data[3];
    size_t pos = 10;
    if (flags & 4) { // FEXTRA
        if (pos + 2 > data.size()) return std::nullopt;
        pos += 2 + (data[pos] | (data[pos + 1] << 8));
    }
    auto skipString = [&] {
        while (pos < data.size() && data[pos] != 0)
            ++pos;
        ++pos;
    };
    if (flags & 8) skipString();  // FNAME
    if (flags & 16) skipString(); // FCOMMENT
    if (flags & 2) pos += 2;      // FHCRC
    if (pos + 8 > data.size()) return std::nullopt;
    const auto body = data.subspan(pos, data.size() - pos - 8);
    const size_t isize = data[data.size() - 4] | (data[data.size() - 3] << 8) |
                         (data[data.size() - 2] << 16) | (size_t(data[data.size() - 1]) << 24);
    int len = 0;
    char* p = stbi_zlib_decode_malloc_guesssize_headerflag(
        reinterpret_cast<const char*>(body.data()), static_cast<int>(body.size()),
        static_cast<int>(isize + 16), &len, 0);
    if (!p) return std::nullopt;
    auto out = take(p, len);
    const uint32_t crc = data[data.size() - 8] | (data[data.size() - 7] << 8) |
                         (data[data.size() - 6] << 16) | (uint32_t(data[data.size() - 5]) << 24);
    if (crc32(out) != crc) return std::nullopt;
    return out;
}

} // namespace mc
