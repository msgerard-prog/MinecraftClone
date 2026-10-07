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

TEST_CASE("1.21 build height is -64..319 in 24 sections") {
    CHECK(kHeight == 384);
    CHECK(kSectionsPerChunk == 24);
    CHECK(sectionIndex(-64) == 0);
    CHECK(sectionIndex(-49) == 0);
    CHECK(sectionIndex(-48) == 1);
    CHECK(sectionIndex(319) == 23);
    CHECK(isInBuildHeight(-64));
    CHECK_FALSE(isInBuildHeight(320));
}
