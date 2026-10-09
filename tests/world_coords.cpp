#include "world/Coords.h"

#include <doctest/doctest.h>

using namespace mc::world;

TEST_CASE("blockToChunk floors negative coordinates like vanilla") {
    CHECK(blockToChunk(0) == 0);
    CHECK(blockToChunk(15) == 0);
    CHECK(blockToChunk(16) == 1);
    CHECK(blockToChunk(-1) == -1);
    CHECK(blockToChunk(-16) == -1);
    CHECK(blockToChunk(-17) == -2);
}

TEST_CASE("blockToLocal is always 0..15") {
    CHECK(blockToLocal(0) == 0);
    CHECK(blockToLocal(17) == 1);
    CHECK(blockToLocal(-1) == 15);
    CHECK(blockToLocal(-16) == 0);
}

TEST_CASE("1.21 heights: Overworld -64..319 in 24 sections; Nether and End 0..255 in 16") {
    CHECK(kOverworldHeight.height == 384);
    CHECK(kMaxSections == 24);
    CHECK(kOverworldHeight.sectionIndex(-64) == 0);
    CHECK(kOverworldHeight.sectionIndex(-49) == 0);
    CHECK(kOverworldHeight.sectionIndex(-48) == 1);
    CHECK(kOverworldHeight.sectionIndex(319) == 23);
    CHECK(kOverworldHeight.minSection() == -4);
    CHECK(kOverworldHeight.contains(-64));
    CHECK_FALSE(kOverworldHeight.contains(320));
    CHECK(kNetherHeight.sections() == 16);
    CHECK(kNetherHeight.sectionIndex(0) == 0);
    CHECK(kNetherHeight.maxY() == 255);
    CHECK(kNetherHeight.minSection() == 0);
    CHECK_FALSE(kNetherHeight.contains(-1));
    CHECK_FALSE(kEndHeight.contains(256));
}

// M31.2: the dense chunk grid.
#include "world/World.h"
#include "core/WorkQueue.h"

TEST_CASE("M31.2: the world's chunk grid - slots by x/z mod 128, an overflow for clashes") {
    using namespace mc::world;
    World w;
    Chunk& a = w.createChunk({3, 5});
    Chunk& b = w.createChunk({3 + 128, 5}); // same slot: overflow
    Chunk& c = w.createChunk({-125, 5 - 256}); // same slot again
    CHECK(w.chunkCount() == 3);
    CHECK(w.chunk({3, 5}) == &a);
    CHECK(w.chunk({131, 5}) == &b);
    CHECK(w.chunk({-125, -251}) == &c);
    CHECK(w.chunk({4, 5}) == nullptr);
    int seen = 0;
    w.forEachChunk([&](const Chunk&) { ++seen; });
    CHECK(seen == 3);
    // Removing the grid's chunk lets an overflow one take the slot; all stay findable.
    REQUIRE(w.removeChunk({3, 5}) != nullptr);
    CHECK(w.chunkCount() == 2);
    CHECK(w.chunk({3, 5}) == nullptr);
    CHECK(w.chunk({131, 5}) == &b);
    CHECK(w.chunk({-125, -251}) == &c);
    CHECK(w.removeChunk({3, 5}) == nullptr);
    // Replacing a chunk at the same place keeps the count.
    w.createChunk({131, 5});
    CHECK(w.chunkCount() == 2);
}

TEST_CASE("M31.2: the work queue keeps FIFO order as its ring grows and wraps") {
    mc::WorkQueue<int> q;
    int next = 0, expect = 0;
    for (int round = 0; round < 50; ++round) {
        for (int i = 0; i < 7; ++i) q.push(next++);
        for (int i = 0; i < 5; ++i) {
            const auto v = q.tryPop();
            REQUIRE(v);
            CHECK(*v == expect++);
        }
    }
    while (auto v = q.tryPop()) CHECK(*v == expect++);
    CHECK(expect == next);
}
