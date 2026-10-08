// M27.1: the blocks of the remaining biomes (two-block plants, mud, moss, pale moss).
#include "gameplay/Mining.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <doctest/doctest.h>

#include <ostream> // (doctest prints string_view)

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

struct Garden {
    World world;
    BlockUpdates updates{world};
    Garden() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, S(blocks::GrassBlock));
            }
    }
    BlockId at(int x, int y, int z) { return R().blockOf(world.getBlock({x, y, z})); }
};

} // namespace

TEST_CASE("a two-block plant stands on soil with room above; its halves go together (M27.1)") {
    Garden g;
    const auto placed = BlockUpdates::placement(g.world, S(blocks::Sunflower), {2, 64, 2}, Direction::Up, 0, 0, 0.0);
    REQUIRE(placed);
    g.world.updateBlock({2, 64, 2}, *placed);
    CHECK(g.at(2, 64, 2) == blocks::Sunflower);
    CHECK(g.at(2, 65, 2) == blocks::Sunflower);
    CHECK(R().get(g.world.getBlock({2, 65, 2}), properties::doorHalf) == 0); // (upper)
    // Breaking the upper half breaks the lower, which drops the flower.
    g.updates.drops().clear();
    g.world.updateBlock({2, 65, 2}, 0);
    CHECK(g.at(2, 64, 2) == 0);
    REQUIRE(g.updates.drops().size() == 1);
    // No room above, or no soil: it can't go there.
    g.world.updateBlock({5, 65, 5}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::placement(g.world, S(blocks::Peony), {5, 64, 5}, Direction::Up, 0, 0, 0.0));
    g.world.updateBlock({7, 63, 7}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::placement(g.world, S(blocks::Lilac), {7, 64, 7}, Direction::Up, 0, 0, 0.0));
    // Mud and moss are soil too (vanilla #dirt).
    g.world.updateBlock({9, 63, 9}, S(blocks::Mud));
    CHECK(BlockUpdates::placement(g.world, S(blocks::RoseBush), {9, 64, 9}, Direction::Up, 0, 0, 0.0));
}

TEST_CASE("bone meal grows short grass into tall grass and copies tall flowers (M27.1)") {
    Garden g;
    g.world.updateBlock({1, 64, 1}, S(blocks::ShortGrass));
    REQUIRE(g.updates.boneMeal({1, 64, 1}));
    CHECK(g.at(1, 64, 1) == blocks::TallGrass);
    CHECK(g.at(1, 65, 1) == blocks::TallGrass);
    g.world.updateBlock({3, 64, 3}, R().set(S(blocks::Peony), properties::doorHalf, 1));
    g.updates.drops().clear();
    REQUIRE(g.updates.boneMeal({3, 64, 3}));
    REQUIRE(g.updates.drops().size() == 1);
    CHECK(g.updates.drops()[0].stack.item == itemRegistry().blockItem(blocks::Peony));
}

TEST_CASE("tall grass drops two short grass with shears, only the lower half drops (M27.1)") {
    Xoroshiro rng(4);
    std::vector<ItemStack> out;
    const BlockStateId lower = R().set(S(blocks::TallGrass), properties::doorHalf, 1);
    blockDrops(lower, {*itemRegistry().find("shears"), 1}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::ShortGrass));
    CHECK(out[0].count == 2);
    out.clear();
    blockDrops(R().set(S(blocks::Sunflower), properties::doorHalf, 0), {}, rng, out);
    CHECK(out.empty());
    blockDrops(S(blocks::PaleHangingMoss), {}, rng, out);
    CHECK(out.empty()); // (shears or Silk Touch only)
}

TEST_CASE("pale hanging moss hangs from a block; the lowest piece is the tip (M27.1)") {
    Garden g;
    g.world.updateBlock({4, 70, 4}, S(blocks::PaleMossBlock));
    REQUIRE(BlockUpdates::placement(g.world, S(blocks::PaleHangingMoss), {4, 69, 4}, Direction::Down, 0, 0, 0.0));
    g.world.updateBlock({4, 69, 4}, S(blocks::PaleHangingMoss));
    g.world.updateBlock({4, 68, 4}, S(blocks::PaleHangingMoss));
    CHECK(R().get(g.world.getBlock({4, 69, 4}), properties::mossTip) == 1); // (not the tip)
    CHECK(R().get(g.world.getBlock({4, 68, 4}), properties::mossTip) == 0);
    g.world.updateBlock({4, 70, 4}, 0); // the block above goes: the strand falls
    CHECK(g.at(4, 69, 4) == 0);
    CHECK(g.at(4, 68, 4) == 0);
}

TEST_CASE("mud, packed mud, mud bricks, moss carpets and tall-flower dyes are craftable (M27.1)") {
    auto made = [](const char* name, int count) {
        for (const Recipe& r : craftingRecipes())
            if (r.result.item == *itemRegistry().find(name) && r.result.count == count) return true;
        return false;
    };
    CHECK(made("packed_mud", 1));
    CHECK(made("mud_bricks", 4));
    CHECK(made("moss_carpet", 3));
    CHECK(made("pale_moss_carpet", 3));
    CHECK(made("yellow_dye", 2));
    CHECK(made("pink_dye", 2));
}

// --- overworld6 (M27.1b) ----------------------------------------------------------------

#include "world/OverworldGenerator.h"

namespace {

std::optional<ChunkPos> findBiome6(const OverworldGenerator& gen, Biome want, int reach = 400) {
    for (int ring = 0; ring <= reach; ring += 2)
        for (int cz = -ring; cz <= ring; cz += 2)
            for (int cx = -ring; cx <= ring; cx += 2) {
                if (std::max(std::abs(cx), std::abs(cz)) != ring) continue;
                const auto col = gen.column(cx * 16 + 8, cz * 16 + 8);
                // (on land: mangrove swamps sit at the sea, the others above it)
                if (gen.biomeAt(col) == want && col.height > (want == Biome::MangroveSwamp ? 60.0 : 67.0))
                    return ChunkPos{cx, cz};
            }
    return std::nullopt;
}

// Block counts over the 3x3 chunks around a centre.
std::vector<int> countAround(const OverworldGenerator& gen, ChunkPos centre) {
    std::vector<int> n(R().blockCount(), 0);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk c({centre.x + dx, centre.z + dz});
            gen.generate(c);
            for (int y = 40; y < 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) ++n[R().blockOf(c.get(x, y, z))];
        }
    return n;
}

} // namespace

TEST_CASE("overworld6 places every remaining surface biome, each with its features (M27.1)") {
    const OverworldGenerator gen(42);
    REQUIRE(gen.kind() == "overworld6");
    struct Want {
        Biome biome;
        std::vector<BlockId> blocks;
    };
    const Want wants[] = {
        {Biome::SunflowerPlains, {blocks::Sunflower}},
        {Biome::OldGrowthBirchForest, {blocks::BirchLog}},
        {Biome::OldGrowthPineTaiga, {blocks::SpruceLog, blocks::Podzol, blocks::LargeFern}},
        {Biome::SavannaPlateau, {blocks::AcaciaLog}},
        {Biome::WindsweptGravellyHills, {blocks::Gravel}},
        {Biome::WindsweptForest, {blocks::SpruceLog}},
        {Biome::BambooJungle, {blocks::Bamboo, blocks::JungleLog}},
        {Biome::MangroveSwamp, {blocks::Mud, blocks::MangroveLog, blocks::MangroveRoots}},
        {Biome::PaleGarden, {blocks::PaleOakLog, blocks::PaleMossBlock, blocks::PaleHangingMoss}},
        {Biome::WindsweptSavanna, {}},
    };
    for (const Want& w : wants) {
        const auto at = findBiome6(gen, w.biome);
        INFO(biomeInfo(w.biome).id);
        REQUIRE(at.has_value());
        if (w.blocks.empty()) continue;
        const auto n = countAround(gen, *at);
        for (const BlockId b : w.blocks) {
            INFO(R().block(b).id);
            CHECK(n[b] > 0);
        }
    }
}

TEST_CASE("overworld6: giant spruces have 2x2 trunks; mangroves stand on roots (M27.1)") {
    const OverworldGenerator gen(42);
    const auto pine = findBiome6(gen, Biome::OldGrowthPineTaiga);
    REQUIRE(pine);
    int wide = 0;
    for (int dz = -2; dz <= 2 && wide == 0; ++dz)
        for (int dx = -2; dx <= 2 && wide == 0; ++dx) {
            Chunk c({pine->x + dx, pine->z + dz});
            gen.generate(c);
            for (int y = 60; y < 160; ++y)
                for (int z = 0; z < 15; ++z)
                    for (int x = 0; x < 15; ++x) {
                        auto log = [&](int a, int b, int h) { return R().blockOf(c.get(a, h, b)) == blocks::SpruceLog; };
                        if (log(x, z, y) && log(x + 1, z, y) && log(x, z + 1, y) && log(x + 1, z + 1, y) &&
                            log(x, z, y + 6) && log(x + 1, z + 1, y + 6))
                            ++wide;
                    }
        }
    CHECK(wide > 0);
}

TEST_CASE("overworld6 output is pinned (re-pinned while M27 builds it)") {
    const OverworldGenerator gen(42);
    const auto pale = findBiome6(gen, Biome::PaleGarden);
    REQUIRE(pale);
    Chunk c(*pale);
    gen.generate(c);
    uint64_t h = 1469598103934665603ull;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                for (const char ch : R().toString(c.get(x, y, z))) {
                    h ^= uint8_t(ch);
                    h *= 1099511628211ull;
                }
    MESSAGE("overworld6 hash " << h);
    CHECK(h == 320080373009731636ull);
}

TEST_CASE("generator kinds map to their versions; every kind names itself back (M27.1 regression)") {
    // (the game once built every kind after overworld4 as version 5, so new worlds
    // silently stayed on the previous generator)
    CHECK(OverworldGenerator::versionOf("overworld") == 1);
    CHECK(OverworldGenerator::versionOf("terrain") == 0);
    CHECK(OverworldGenerator::versionOf("overworld9") == 0);
    for (int v = 1; v <= OverworldGenerator::kNewest; ++v) {
        const OverworldGenerator gen(1, v);
        CHECK(OverworldGenerator::versionOf(gen.kind()) == v);
    }
}

#include "world/BlockShapes.h"

TEST_CASE("double slabs and mangrove roots are solid to walk on; a tipped big dripleaf is not (regression)") {
    // (an empty shape means no collision: double slabs and mangrove roots once had one)
    const auto dbl = R().set(R().defaultState(*R().findBlock("oak_slab")), properties::slabType, 2);
    CHECK(collisionShape(dbl).count == 1);
    CHECK(collisionShape(R().defaultState(blocks::MangroveRoots)).count == 1);
    const BlockStateId leaf = R().defaultState(blocks::BigDripleaf);
    CHECK(collisionShape(leaf).count == 1);
    CHECK(collisionShape(R().set(leaf, properties::tilt, 3)).count == 0);
}
