#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <set>

using namespace mc::world;

TEST_CASE("air is block 0 and state 0") {
    const auto& r = blockRegistry();
    CHECK(r.defaultState(blocks::Air) == 0);
    CHECK(r.isAir(0));
    CHECK_FALSE(r.opaqueCube(0));
    CHECK(r.toString(0) == "minecraft:air");
}

TEST_CASE("state count is the product of property value counts") {
    const auto& r = blockRegistry();
    CHECK(r.block(blocks::Stone).stateCount == 1);
    CHECK(r.block(blocks::OakLog).stateCount == 3);     // axis x|y|z
    CHECK(r.block(blocks::GrassBlock).stateCount == 2); // snowy true|false
    size_t total = 0;
    for (size_t b = 0; b < r.blockCount(); ++b)
        total += r.block(BlockId(b)).stateCount;
    CHECK(total == r.stateCount());
}

TEST_CASE("default states match vanilla") {
    const auto& r = blockRegistry();
    CHECK(r.toString(r.defaultState(blocks::OakLog)) == "minecraft:oak_log[axis=y]");
    CHECK(r.toString(r.defaultState(blocks::GrassBlock)) == "minecraft:grass_block[snowy=false]");
    CHECK(r.toString(r.defaultState(blocks::Stone)) == "minecraft:stone");
}

TEST_CASE("every state string round-trips through parse") {
    const auto& r = blockRegistry();
    std::set<std::string> seen;
    for (size_t s = 0; s < r.stateCount(); ++s) {
        const auto text = r.toString(BlockStateId(s));
        CHECK(seen.insert(text).second); // all unique
        const auto parsed = r.parse(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == s);
    }
}

TEST_CASE("parse accepts defaults, short ids and spaces; rejects junk") {
    const auto& r = blockRegistry();
    CHECK(r.parse("oak_log") == r.defaultState(blocks::OakLog));
    const auto x = r.parse("minecraft:oak_log[ axis = x ]");
    REQUIRE(x.has_value());
    CHECK(r.value(*x, "axis") == "x");
    CHECK_FALSE(r.parse("minecraft:oak_log[axis=w]").has_value());
    CHECK_FALSE(r.parse("minecraft:oak_log[color=red]").has_value());
    CHECK_FALSE(r.parse("minecraft:not_a_block").has_value());
    CHECK_FALSE(r.parse("minecraft:oak_log[axis=x").has_value());
}

TEST_CASE("with() changes one property and keeps the block") {
    const auto& r = blockRegistry();
    const BlockStateId y = r.defaultState(blocks::OakLog);
    const auto z = r.with(y, "axis", "z");
    REQUIRE(z.has_value());
    CHECK(r.blockOf(*z) == blocks::OakLog);
    CHECK(r.value(*z, "axis") == "z");
    CHECK(r.with(*z, "axis", "y") == y);
    CHECK_FALSE(r.with(y, "snowy", "true").has_value()); // logs have no snowy
}

TEST_CASE("opaque cube flag drives face culling") {
    const auto& r = blockRegistry();
    CHECK(r.opaqueCube(r.defaultState(blocks::Stone)));
    CHECK(r.opaqueCube(r.defaultState(blocks::OakLog)));
    CHECK(r.layer(0) == RenderLayer::Invisible);
}
