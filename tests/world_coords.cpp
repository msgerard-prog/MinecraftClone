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
