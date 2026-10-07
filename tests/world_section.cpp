#include "world/Chunk.h"
#include "world/World.h"

#include <doctest/doctest.h>

#include <vector>

using namespace mc::world;

TEST_CASE("new section is all air with no index array") {
    Section s;
    CHECK(s.isEmpty());
    CHECK(s.bitsPerEntry() == 0);
    CHECK(s.get(5, 5, 5) == 0);
}

TEST_CASE("second distinct state grows to vanilla's 4-bit minimum") {
    Section s;
    s.set(1, 2, 3, 7);
    CHECK(s.bitsPerEntry() == 4);
    CHECK(s.get(1, 2, 3) == 7);
    CHECK(s.get(0, 0, 0) == 0);
    CHECK(s.nonAirCount() == 1);
}

TEST_CASE("palette grows 4 -> 5 bits on the 17th state, direct past 8 bits") {
    Section s;
    for (int i = 1; i <= 15; ++i)
        s.set(i, 0, 0, BlockStateId(100 + i)); // 16 with air
    CHECK(s.bitsPerEntry() == 4);
    s.set(0, 1, 0, 500); // 17th distinct
    CHECK(s.bitsPerEntry() == 5);
    for (int i = 0; i < 300; ++i)
        s.set(i % 16, 2 + i / 256, (i / 16) % 16, BlockStateId(1000 + i));
    CHECK(s.isDirect());
    CHECK(s.bitsPerEntry() == Section::kDirectBits);
    // Everything written before the switches is still there.
    for (int i = 1; i <= 15; ++i)
        CHECK(s.get(i, 0, 0) == 100 + i);
    CHECK(s.get(0, 1, 0) == 500);
    CHECK(s.get(299 % 16, 2 + 299 / 256, (299 / 16) % 16) == 1299);
    // The non-air count and bulk decode survive every encoding change.
    CHECK(s.nonAirCount() == 15 + 1 + 300);
    std::vector<BlockStateId> all(Section::kVolume);
    s.copyTo(all.data());
    for (int i = 0; i < Section::kVolume; ++i)
        CHECK(all[i] == s.getIndex(i));
}

TEST_CASE("copyTo matches get for every block") {
    Section s;
    for (int i = 0; i < Section::kVolume; i += 7) {
        s.set(i % 16, i / 256, (i / 16) % 16, BlockStateId(1 + i % 40));
    }
    std::vector<BlockStateId> all(Section::kVolume);
    s.copyTo(all.data());
    for (int i = 0; i < Section::kVolume; ++i)
        CHECK(all[i] == s.getIndex(i));
}

TEST_CASE("non-air count tracks sets and clears") {
    Section s;
    s.set(0, 0, 0, 3);
    s.set(1, 0, 0, 3);
    s.set(0, 0, 0, 0);
    CHECK(s.nonAirCount() == 1);
    s.fill(3);
    CHECK(s.nonAirCount() == Section::kVolume);
    CHECK(s.bitsPerEntry() == 0);
}

TEST_CASE("a single-state section is tiny compared with a flat array") {
    Section s;
    s.fill(1);
    CHECK(s.memoryBytes() < 128);
}

TEST_CASE("world get/set across negative chunk borders") {
    World w;
    w.createChunk({-1, -1});
    w.createChunk({0, 0});
    w.setBlock({-1, 70, -1}, 5);
    w.setBlock({0, -64, 0}, 6);
    w.setBlock({0, 320, 0}, 7); // above build height: ignored
    CHECK(w.getBlock({-1, 70, -1}) == 5);
    CHECK(w.chunk({-1, -1})->get(15, 70, 15) == 5);
    CHECK(w.getBlock({0, -64, 0}) == 6);
    CHECK(w.getBlock({0, 320, 0}) == 0);
    CHECK(w.getBlock({100, 0, 100}) == 0); // unloaded chunk reads as air
}

TEST_CASE("chunk key packs x and z like vanilla's ChunkPos.toLong") {
    CHECK(ChunkPos{1, 0}.key() == 1);
    CHECK(ChunkPos{0, 1}.key() == (int64_t{1} << 32));
    CHECK(ChunkPos{-1, 0}.key() == 0xFFFFFFFFll);
}

TEST_CASE("vanilla storage layout: longs per bit width (entries never span longs)") {
    // 4096 entries, floor(64 / bits) per long.
    Section s;
    s.set(0, 0, 0, 1);
    CHECK(s.bitsPerEntry() == 4);
    CHECK(s.data().size() == 256);
    for (int i = 2; i <= 16; ++i)
        s.set(i, 0, 0, BlockStateId(i));
    CHECK(s.bitsPerEntry() == 5);
    CHECK(s.data().size() == 342); // 12 per long
    for (int i = 17; i <= 32; ++i)
        s.set(i % 16, 1, 0, BlockStateId(i));
    CHECK(s.bitsPerEntry() == 6);
    CHECK(s.data().size() == 410); // 10 per long
    for (int i = 33; i <= 64; ++i)
        s.set(i % 16, 2 + i / 48, 0, BlockStateId(i));
    CHECK(s.bitsPerEntry() == 7);
    CHECK(s.data().size() == 456); // 9 per long
    for (int i = 65; i <= 128; ++i)
        s.set(i % 16, 4, i / 16, BlockStateId(i));
    CHECK(s.bitsPerEntry() == 8);
    CHECK(s.data().size() == 512);
    for (int i = 129; i <= 300; ++i)
        s.set(i % 16, 5 + i / 256, (i / 16) % 16, BlockStateId(i));
    CHECK(s.isDirect());
    CHECK(s.data().size() == 1024); // 16 bits: 4 per long
}

TEST_CASE("vanilla storage layout: index order and bit positions") {
    // Index (y*16 + z)*16 + x; at 5 bits, entry 12 starts at bit 0 of long 1.
    CHECK(Section::index(1, 0, 0) == 1);
    CHECK(Section::index(0, 0, 1) == 16);
    CHECK(Section::index(0, 1, 0) == 256);
    Section s;
    for (int i = 1; i <= 16; ++i)
        s.set(i - 1, 15, 15, BlockStateId(1000 + i)); // 17 states
    REQUIRE(s.bitsPerEntry() == 5);
    s.set(12, 0, 0, 1001); // palette index of 1001 is 1 (air is 0)
    CHECK((s.data()[1] & 31u) == 1u);
    CHECK((s.data()[0] >> (11 * 5) & 31u) == 0u); // entry 11 is still air
}
