#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace mc {

// zlib (RFC 1950) and gzip (RFC 1952) streams, as used by vanilla saves: region
// chunks are zlib, level.dat is gzip. Backed by stb's deflate/inflate (the stb
// implementations for the whole program live in Compression.cpp).
std::vector<uint8_t> zlibCompress(std::span<const uint8_t> data);
std::optional<std::vector<uint8_t>> zlibDecompress(std::span<const uint8_t> data);
std::vector<uint8_t> gzipCompress(std::span<const uint8_t> data);
std::optional<std::vector<uint8_t>> gzipDecompress(std::span<const uint8_t> data);

uint32_t crc32(std::span<const uint8_t> data);

} // namespace mc
