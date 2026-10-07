#include "world/ChunkLoader.h"
#include "world/TerrainGenerator.h"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>

using namespace mc::world;

namespace {

void runUntilIdle(ChunkLoader& loader, ChunkPos center, std::vector<ChunkPos>& loaded,
                  std::vector<ChunkPos>& unloaded, int& loadedTotal, int& unloadedTotal) {
    for (int i = 0; i < 5000; ++i) {
        loader.update(center, loaded, unloaded);
        loadedTotal += static_cast<int>(loaded.size());
        unloadedTotal += static_cast<int>(unloaded.size());
        if (loader.pending() == 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("chunk loader did not finish");
}

} // namespace

TEST_CASE("chunk loader fills the render circle plus neighbours and unloads behind") {
    World world;
    const TerrainGenerator gen(42);
    ChunkLoader loader(world, gen, 2);
    loader.setRenderDistance(3);
    std::vector<ChunkPos> loaded, unloaded;
    int loadedTotal = 0, unloadedTotal = 0;
    runUntilIdle(loader, {0, 0}, loaded, unloaded, loadedTotal, unloadedTotal);

    int expected = 0;
    for (int dz = -5; dz <= 5; ++dz)
        for (int dx = -5; dx <= 5; ++dx)
            if (ChunkLoader::wanted(dx, dz, 3)) {
                ++expected;
                CHECK(world.chunk({dx, dz}) != nullptr);
            }
    CHECK(static_cast<int>(world.chunkCount()) == expected);
    CHECK(loadedTotal == expected);

    // Move far away: the old area unloads, the new one loads.
    runUntilIdle(loader, {40, 0}, loaded, unloaded, loadedTotal, unloadedTotal);
    CHECK(world.chunk({0, 0}) == nullptr);
    CHECK(world.chunk({40, 0}) != nullptr);
    CHECK(unloadedTotal == expected);
    CHECK(static_cast<int>(world.chunkCount()) == expected);
}

TEST_CASE("streamed chunks are identical to directly generated ones") {
    World world;
    const TerrainGenerator gen(99);
    ChunkLoader loader(world, gen, 3);
    loader.setRenderDistance(2);
    std::vector<ChunkPos> loaded, unloaded;
    int a = 0, b = 0;
    runUntilIdle(loader, {5, -3}, loaded, unloaded, a, b);
    Chunk direct({6, -2});
    gen.generate(direct);
    const Chunk* streamed = world.chunk({6, -2});
    REQUIRE(streamed);
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); y += 3)
        for (int z = 0; z < 16; z += 3)
            for (int x = 0; x < 16; x += 3)
                CHECK(streamed->get(x, y, z) == direct.get(x, y, z));
}

TEST_CASE("every chunk inside the render circle has all 8 neighbours wanted") {
    for (int rd : {2, 3, 8, 12, 32}) {
        for (int dz = -rd; dz <= rd; ++dz)
            for (int dx = -rd; dx <= rd; ++dx) {
                if (!ChunkLoader::inRadius(dx, dz, rd)) continue;
                for (int oz = -1; oz <= 1; ++oz)
                    for (int ox = -1; ox <= 1; ++ox)
                        CHECK(ChunkLoader::wanted(dx + ox, dz + oz, rd));
            }
    }
}

TEST_CASE("inRadius edge: a rounded circle (r^2 + r)") {
    CHECK(ChunkLoader::inRadius(12, 0, 12));
    CHECK(ChunkLoader::inRadius(8, 9, 12));       // 145 <= 156
    CHECK_FALSE(ChunkLoader::inRadius(9, 9, 12)); // 162 > 156
    CHECK_FALSE(ChunkLoader::inRadius(13, 0, 12));
}
