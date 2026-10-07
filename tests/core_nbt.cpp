// NBT and compression (wiki: NBT format, Region file format).
#include "core/Compression.h"
#include "core/Nbt.h"

#include <doctest/doctest.h>

#include <string>

using namespace mc;

TEST_CASE("zlib and gzip round-trip; gzip checks its CRC") {
    std::vector<uint8_t> data;
    for (int i = 0; i < 10000; ++i)
        data.push_back(static_cast<uint8_t>((i * 7) % 13));
    const auto z = zlibCompress(data);
    CHECK(z.size() < data.size());
    CHECK(zlibDecompress(z) == data);
    auto g = gzipCompress(data);
    REQUIRE(g.size() > 18);
    CHECK(g[0] == 0x1F);
    CHECK(g[1] == 0x8B);
    CHECK(gzipDecompress(g) == data);
    g[g.size() - 6] ^= 0xFF; // corrupt the CRC
    CHECK_FALSE(gzipDecompress(g).has_value());
    CHECK(crc32(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>("123456789"), 9)) ==
          0xCBF43926u); // the standard CRC-32 check value
}

TEST_CASE("NBT: the spec's hello_world example encodes byte for byte") {
    // wiki: NBT format - TAG_Compound("hello world") { TAG_String("name"): "Bananrama" }
    nbt::Compound c;
    c.put("name", std::string("Bananrama"));
    const auto bytes = nbt::write(c, "hello world");
    const std::vector<uint8_t> expected = {
        0x0A, 0x00, 0x0B, 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',
        0x08, 0x00, 0x04, 'n', 'a', 'm', 'e', 0x00, 0x09, 'B', 'a', 'n', 'a', 'n', 'r',
        'a',  'm',  'a',  0x00};
    CHECK(bytes == expected);
    std::string name;
    const auto back = nbt::read(bytes, &name);
    REQUIRE(back.has_value());
    CHECK(name == "hello world");
    REQUIRE(back->string("name"));
    CHECK(*back->string("name") == "Bananrama");
}

TEST_CASE("NBT: every tag type round-trips; truncated input is rejected") {
    nbt::Compound c;
    c.put("b", int8_t{-5});
    c.put("s", int16_t{-300});
    c.put("i", int32_t{123456789});
    c.put("l", int64_t{-1234567890123LL});
    c.put("f", 1.5f);
    c.put("d", -2.25);
    c.put("ba", std::vector<int8_t>{1, -2, 3});
    c.put("ia", std::vector<int32_t>{7, -8});
    c.put("la", std::vector<int64_t>{1LL << 40, -1});
    nbt::Compound inner;
    inner.put("x", int32_t{4});
    c.put("c", inner);
    c.put("list", nbt::listOf(nbt::TagType::Double, {1.0, 2.0, 3.0}));
    c.put("empty", nbt::listOf(nbt::TagType::End, {}));
    const auto bytes = nbt::write(c);
    const auto back = nbt::read(bytes);
    REQUIRE(back.has_value());
    CHECK(back->integer("b") == -5);
    CHECK(back->integer("s") == -300);
    CHECK(back->integer("i") == 123456789);
    CHECK(back->integer("l") == -1234567890123LL);
    CHECK(back->real("f") == 1.5);
    CHECK(back->real("d") == -2.25);
    CHECK(*back->byteArray("ba") == std::vector<int8_t>{1, -2, 3});
    CHECK(*back->longArray("la") == std::vector<int64_t>{1LL << 40, -1});
    REQUIRE(back->compound("c"));
    CHECK(back->compound("c")->integer("x") == 4);
    REQUIRE(back->list("list"));
    CHECK(back->list("list")->items.size() == 3);
    CHECK(back->list("list")->elementType == nbt::TagType::Double);
    CHECK(back->list("empty")->items.empty());
    for (size_t cut : {size_t(1), bytes.size() / 2, bytes.size() - 1})
        CHECK_FALSE(nbt::read(std::span(bytes).first(cut)).has_value());
}
