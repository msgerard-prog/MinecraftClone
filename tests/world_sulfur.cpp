// Potent sulfur (M33.2b; wiki: Potent Sulfur, 26.2): gas over shallow water, geysers over magma.
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }
} // namespace

TEST_CASE("M33.2b: potent sulfur under two water sources gasses and, over magma, erupts about every 32 s") {
    World world;
    BlockUpdates updates{world};
    world.setListener(&updates);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) world.createChunk({cx, cz});
    const BlockId potent = BlockUpdates::potentSulfur();
    REQUIRE(potent != 0);
    world.setBlock({4, 59, 4}, S(blocks::MagmaBlock));
    world.setBlock({4, 61, 4}, S(blocks::Water));
    world.setBlock({4, 62, 4}, S(blocks::Water));
    world.updateBlock({4, 60, 4}, S(potent)); // (placed: it starts its cycle)
    int64_t now = 0;
    int gasses = 0, eruptions = 0, firstEruption = -1, top = 0, events = 0;
    for (int t = 0; t < 1200; ++t) {
        updates.setTime(++now);
        updates.tick();
        gasses += int(updates.sulfurGas().size());
        for (const auto& e : world.levelEvents()) events += e.type == LevelEvent::Type::SulfurGas;
        world.levelEvents().clear();
        for (const auto& g : updates.geysers()) {
            ++eruptions;
            top = g.top;
            if (firstEruption < 0) firstEruption = t;
        }
        updates.sulfurGas().clear();
        updates.geysers().clear();
    }
    MESSAGE("gas reports " << gasses << ", eruptions " << eruptions << ", first at " << firstEruption);
    CHECK(gasses >= 50); // (every second while resting)
    CHECK(events == gasses); // (the bubbles drawn)
    CHECK(eruptions >= 1);
    CHECK(eruptions <= 6); // (about one in 32 s)
    CHECK(top == 62 + 10); // (2 deep: 10 blocks over the water)
    // Covered over: no gas.
    world.updateBlock({4, 63, 4}, S(blocks::Stone));
    gasses = 0;
    for (int t = 0; t < 200; ++t) {
        updates.setTime(++now);
        updates.tick();
        gasses += int(updates.sulfurGas().size());
        updates.sulfurGas().clear();
        updates.geysers().clear();
    }
    CHECK(gasses == 0);
}
