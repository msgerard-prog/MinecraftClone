// The pending-tick set behind BlockUpdates::hasTick (open addressing, backward shift).
#include "world/TickSet.h"

#include <doctest/doctest.h>

#include <set>

using mc::world::TickSet;

TEST_CASE("TickSet matches std::set through inserts and erases") {
    TickSet t;
    std::set<uint64_t> ref;
    uint64_t x = 12345;
    for (int i = 0; i < 20000; ++i) {
        x = x * 6364136223846793005ull + 1442695040888963407ull;
        const uint64_t key = (x >> 40) % 512; // many collisions and re-inserts
        if ((x >> 20) & 1) {
            t.insert(key);
            ref.insert(key);
        } else {
            t.erase(key);
            ref.erase(key);
        }
        if (i % 97 == 0)
            for (uint64_t k = 0; k < 512; ++k)
                REQUIRE(t.contains(k) == (ref.count(k) == 1));
    }
    CHECK(t.size() == ref.size());
    t.clear();
    CHECK_FALSE(t.contains(*ref.begin()));
}
