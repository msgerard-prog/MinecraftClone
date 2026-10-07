// GL-free parts of streaming: which chunks get meshes, and translucent draw order.
#include "rendering/ChunkMeshTracker.h"
#include "rendering/ChunkRenderer.h"
#include "world/ChunkLoader.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace mc::world;
using mc::gfx::ChunkMeshTracker;

TEST_CASE("a chunk is meshed once it and all 8 neighbours are loaded") {
    World w;
    ChunkMeshTracker t;
    std::vector<ChunkPos> ready;
    std::vector<ChunkPos> loaded;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x)
            if (!(x == 1 && z == 1)) {
                w.createChunk({x, z});
                loaded.push_back({x, z});
            }
    t.onLoaded(w, loaded, ready);
    CHECK(ready.empty()); // (1,1) still missing
    w.createChunk({1, 1});
    const std::vector<ChunkPos> last = {{1, 1}};
    t.onLoaded(w, last, ready);
    REQUIRE(ready.size() == 1);
    CHECK(ready[0] == ChunkPos{0, 0});
    // Loading it again never re-reports it.
    ready.clear();
    t.onLoaded(w, last, ready);
    CHECK(ready.empty());
}

TEST_CASE("unload then reload makes a chunk meshable again") {
    World w;
    ChunkMeshTracker t;
    std::vector<ChunkPos> all, ready;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x) {
            w.createChunk({x, z});
            all.push_back({x, z});
        }
    t.onLoaded(w, all, ready);
    CHECK(t.isMeshed({0, 0}));
    const std::vector<ChunkPos> gone = {{0, 0}};
    w.removeChunk({0, 0});
    t.onUnloaded(gone);
    CHECK_FALSE(t.isMeshed({0, 0}));
    ready.clear();
    w.createChunk({0, 0});
    t.onLoaded(w, gone, ready);
    CHECK(std::find(ready.begin(), ready.end(), ChunkPos{0, 0}) != ready.end());
}

TEST_CASE("translucent draws are ordered far to near with matching offsets") {
    using mc::gfx::DrawCommand;
    std::vector<DrawCommand> cmds = {{6, 1, 0, 0, 0}, {6, 1, 0, 4, 1}, {6, 1, 0, 8, 2}};
    std::vector<glm::vec4> offsets = {{0, 0, 0, 0}, {100, 0, 0, 0}, {40, 0, 0, 0}};
    std::vector<mc::gfx::DrawSortItem> scratch(3);
    std::vector<DrawCommand> outCmds(3);
    std::vector<glm::vec4> outOffsets(3);
    mc::gfx::sortDrawsBackToFront(cmds, offsets, scratch, outCmds, outOffsets);
    CHECK(outOffsets[0].x == 100); // farthest first
    CHECK(outOffsets[1].x == 40);
    CHECK(outOffsets[2].x == 0);
    for (uint32_t i = 0; i < 3; ++i) {
        CHECK(outCmds[i].baseInstance == i); // renumbered to its new slot
    }
    CHECK(outCmds[0].baseVertex == 4); // commands moved with their offsets
    CHECK(outCmds[2].baseVertex == 0);
}

TEST_CASE("chunk loader shuts down cleanly with jobs in flight") {
    World world;
    const TerrainGenerator gen(1);
    std::vector<ChunkPos> loaded, unloaded;
    {
        ChunkLoader loader(world, gen, 2);
        loader.setRenderDistance(32);
        loader.update({0, 0}, loaded, unloaded); // submits jobs, then destroyed at once
        CHECK(loader.pending() > 0);
    }
    CHECK(world.chunkCount() == 0); // nothing inserted after the loader is gone
}
