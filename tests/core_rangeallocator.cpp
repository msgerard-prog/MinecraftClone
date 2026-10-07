#include "core/RangeAllocator.h"

#include <doctest/doctest.h>

TEST_CASE("RangeAllocator: first fit, free merges neighbours") {
    mc::RangeAllocator a(100);
    const auto r1 = a.allocate(30);
    const auto r2 = a.allocate(30);
    const auto r3 = a.allocate(30);
    REQUIRE((r1 && r2 && r3));
    CHECK(r2->offset == 30);
    CHECK_FALSE(a.allocate(20).has_value()); // only 10 left
    a.free(*r2);
    CHECK(a.allocate(25)->offset == 30); // reuses the hole
    a.free({30, 25});
    a.free(*r1);
    a.free(*r3);
    CHECK(a.freeTotal() == 100);
    CHECK(a.freeRangeCount() == 1); // fully merged
}

TEST_CASE("RangeAllocator: grow adds space at the end and merges") {
    mc::RangeAllocator a(10);
    CHECK(a.allocate(10).has_value());
    CHECK_FALSE(a.allocate(1).has_value());
    a.grow(20);
    const auto r = a.allocate(10);
    REQUIRE(r.has_value());
    CHECK(r->offset == 10);
}
