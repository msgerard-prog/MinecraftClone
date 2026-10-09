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
    const OverworldGenerator gen(42, 6);
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

TEST_CASE("overworld6 output is pinned (frozen as of v0.27.0)") {
    const OverworldGenerator gen(42, 6);
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

TEST_CASE("overworld6 lush caves and dripstone caves under the land, with their features (M27.2c)") {
    const OverworldGenerator gen(42);
    for (const Biome want : {Biome::LushCaves, Biome::DripstoneCaves}) {
        INFO(biomeInfo(want).id);
        std::optional<ChunkPos> at;
        for (int ring = 0; ring <= 200 && !at; ring += 2)
            for (int cz = -ring; cz <= ring && !at; cz += 2)
                for (int cx = -ring; cx <= ring && !at; cx += 2)
                    if (std::max(std::abs(cx), std::abs(cz)) == ring &&
                        OverworldGenerator::caveBiome(gen.column(cx * 16 + 8, cz * 16 + 8)) == want)
                        at = ChunkPos{cx, cz};
        REQUIRE(at);
        int moss = 0, vines = 0, drip = 0, points = 0, cells = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                Chunk c({at->x + dx, at->z + dz});
                gen.generate(c);
                REQUIRE(c.biomes());
                cells += c.biomes()->at(8, 0, 8) == want;
                for (int y = kOverworldHeight.minY; y < 100; ++y)
                    for (int z = 0; z < 16; ++z)
                        for (int x = 0; x < 16; ++x) {
                            const BlockId b = R().blockOf(c.get(x, y, z));
                            moss += b == blocks::MossBlock;
                            vines += b == blocks::CaveVines || b == blocks::CaveVinesPlant;
                            drip += b == blocks::DripstoneBlock;
                            points += b == blocks::PointedDripstone;
                        }
            }
        CHECK(cells > 0);
        if (want == Biome::LushCaves) {
            CHECK(moss > 20);
            CHECK(vines > 0);
        } else {
            CHECK(drip > 20);
            CHECK(points > 0);
        }
    }
}

#include "world/StructurePlacement.h"

TEST_CASE("overworld6: the deep dark under the mountains with sculk; ancient cities in it (M27.3b)") {
    const OverworldGenerator gen(42);
    // An ancient city: the first grid candidate within reach in the deep dark.
    std::optional<ChunkPos> city;
    for (int cz = -150; cz <= 150 && !city; ++cz)
        for (int cx = -150; cx <= 150 && !city; ++cx)
            if (isSpreadCandidate(42, kAncientCities, {cx, cz}) &&
                OverworldGenerator::deepDark(gen.column(cx * 16 - 16 + 24, cz * 16 - 16 + 24)))
                city = ChunkPos{cx, cz};
    REQUIRE(city);
    int reinforced = 0, summoning = 0, sculk = 0, chests = 0, dark = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk c({city->x + dx, city->z + dz});
            gen.generate(c);
            dark += c.biomes()->at(8, -51, 8) == Biome::DeepDark;
            chests += int(c.chests().size());
            for (int y = -60; y < -30; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockStateId s = c.get(x, y, z);
                        const BlockId b = R().blockOf(s);
                        reinforced += b == blocks::ReinforcedDeepslate;
                        sculk += b == blocks::Sculk;
                        summoning += b == blocks::SculkShrieker && R().get(s, properties::canSummon) == 0;
                    }
        }
    CHECK(dark > 0);
    CHECK(reinforced > 50);
    CHECK(sculk > 100);
    CHECK(summoning > 0);
    CHECK(chests >= 4);
}

TEST_CASE("overworld6 amethyst geodes: basalt, calcite and amethyst shells with buds inside (M27.4a)") {
    const OverworldGenerator gen(42);
    int amethyst = 0, budding = 0, buds = 0, calcite = 0, basalt = 0;
    for (int cz = 0; cz < 10 && budding == 0; ++cz)
        for (int cx = 0; cx < 10; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int y = -60; y < 40; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = R().blockOf(c.get(x, y, z));
                        amethyst += b == blocks::AmethystBlock;
                        budding += b == blocks::BuddingAmethyst;
                        buds += isAmethystBud(b);
                        calcite += b == blocks::Calcite;
                        basalt += b == blocks::SmoothBasalt;
                    }
        }
    CHECK(amethyst > 20);
    CHECK(budding > 0);
    CHECK(buds > 0);
    CHECK(calcite > 20);
    CHECK(basalt > 20);
}

TEST_CASE("budding amethyst grows buds to clusters; a cluster gives 4 shards to a pickaxe (M27.4a)") {
    Garden g;
    g.world.updateBlock({4, 66, 4}, S(blocks::BuddingAmethyst));
    g.updates.setRandomTicks({0, 0}, 1, 1000);
    bool cluster = false;
    for (int t = 1; t < 20000 && !cluster; ++t) {
        g.updates.setTime(t);
        g.updates.tick();
        for (int d = 0; d < 6 && !cluster; ++d) {
            const glm::ivec3 n = kDirectionNormals[d];
            cluster = g.at(4 + n.x, 66 + n.y, 4 + n.z) == blocks::AmethystCluster;
        }
    }
    CHECK(cluster);
    Xoroshiro rng(3);
    std::vector<ItemStack> out;
    blockDrops(S(blocks::AmethystCluster), {*itemRegistry().find("iron_pickaxe"), 1}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count == 4);
    out.clear();
    blockDrops(S(blocks::BuddingAmethyst), {*itemRegistry().find("iron_pickaxe"), 1}, rng, out);
    CHECK(out.empty());
}

TEST_CASE("overworld6 ruined portals: a broken obsidian frame, netherrack about, a loot chest (M27.4b)") {
    const OverworldGenerator gen(42);
    int found = 0;
    for (int cz = -60; cz <= 60 && found == 0; ++cz)
        for (int cx = -60; cx <= 60 && found == 0; ++cx) {
            if (!isSpreadCandidate(42, kRuinedPortals, {cx, cz})) continue;
            if (gen.surfaceY(cx * 16 + 6, cz * 16 + 6) < OverworldGenerator::kSeaLevel) continue;
            Chunk c({cx, cz});
            gen.generate(c);
            int obsidian = 0, rack = 0;
            for (int y = 40; y < 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = R().blockOf(c.get(x, y, z));
                        obsidian += b == blocks::Obsidian || b == blocks::CryingObsidian;
                        rack += b == blocks::Netherrack || b == blocks::MagmaBlock;
                    }
            CHECK(obsidian >= 6);
            CHECK(rack > 10);
            CHECK(c.chests().size() >= 1);
            ++found;
        }
    CHECK(found == 1);
}

TEST_CASE("overworld6 woodland mansions in dark forests: dark oak, chests, vindicators and an allay (M27.4c)") {
    const OverworldGenerator gen(42);
    std::optional<ChunkPos> at;
    for (int cz = -300; cz <= 300 && !at; ++cz)
        for (int cx = -300; cx <= 300 && !at; ++cx) {
            if (!isSpreadCandidate(42, kMansions, {cx, cz})) continue;
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 16, cz * 16 + 12));
            if ((b == Biome::DarkForest || b == Biome::PaleGarden) &&
                gen.surfaceY(cx * 16 + 16, cz * 16 + 12) >= OverworldGenerator::kSeaLevel)
                at = ChunkPos{cx, cz};
        }
    REQUIRE(at);
    int planks = 0, chests = 0;
    std::vector<MobType> mobs;
    for (int dz = 0; dz <= 1; ++dz)
        for (int dx = 0; dx <= 1; ++dx) {
            Chunk c({at->x + dx, at->z + dz});
            gen.generate(c);
            chests += int(c.chests().size());
            for (const MobData& m : c.mobs()) mobs.push_back(m.type);
            for (int y = 60; y < 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) planks += R().block(R().blockOf(c.get(x, y, z))).id == "minecraft:dark_oak_planks";
        }
    CHECK(planks > 400);
    CHECK(chests >= 3);
    CHECK(std::count(mobs.begin(), mobs.end(), MobType::Vindicator) >= 2);
    CHECK(std::count(mobs.begin(), mobs.end(), MobType::Allay) == 1);
}

TEST_CASE("overworld6 trial chambers: a tuff hall with trial spawners (a breeze's), vaults and supply chests (M27.4d)") {
    const OverworldGenerator gen(42);
    std::optional<ChunkPos> at;
    for (int cz = -60; cz <= 60 && !at; ++cz)
        for (int cx = -60; cx <= 60 && !at; ++cx)
            if (isSpreadCandidate(42, kTrialChambers, {cx, cz})) {
                const auto col = gen.column(cx * 16 + 16, cz * 16 + 16);
                if (!OverworldGenerator::deepDark(col) && col.height >= -6) at = ChunkPos{cx, cz};
            }
    REQUIRE(at);
    int trial = 0, breezes = 0, vaults = 0, chests = 0;
    for (int dz = -1; dz <= 2; ++dz)
        for (int dx = -1; dx <= 2; ++dx) {
            Chunk c({at->x + dx, at->z + dz});
            gen.generate(c);
            chests += int(c.chests().size());
            for (const auto& e : c.spawners()) {
                trial += e.data.trial;
                breezes += e.data.trial && e.data.mob == MobType::Breeze;
            }
            for (int y = -32; y < -20; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) vaults += R().blockOf(c.get(x, y, z)) == blocks::Vault;
        }
    CHECK(trial == 7);
    CHECK(breezes >= 1);
    CHECK(vaults == 2);
    CHECK(chests >= 3);
}

#include "world/ChunkSerializer.h"
#include "world/Loot.h"

TEST_CASE("suspicious sand keeps its loot table through dusting and a save; loot tables have vanilla's names (M27.5)") {
    Garden g;
    g.world.updateBlock({3, 62, 3}, S(blocks::SuspiciousSand));
    Chunk* c = g.world.chunk({0, 0});
    REQUIRE(c->brushable(3, 62, 3));
    c->brushable(3, 62, 3)->table = uint8_t(LootTable::ArchaeologyDesertPyramid);
    g.world.updateBlock({3, 62, 3}, R().set(S(blocks::SuspiciousSand), properties::dusted, 2));
    REQUIRE(c->brushable(3, 62, 3));
    CHECK(c->brushable(3, 62, 3)->table == uint8_t(LootTable::ArchaeologyDesertPyramid));
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*c, 0)), back));
    REQUIRE(back.brushable(3, 62, 3));
    CHECK(back.brushable(3, 62, 3)->table == uint8_t(LootTable::ArchaeologyDesertPyramid));
    // Brushed clean: plain sand, and the loot is gone with it.
    g.world.updateBlock({3, 62, 3}, S(blocks::Sand));
    CHECK(c->brushable(3, 62, 3) == nullptr);
    CHECK(lootTableName(LootTable::ArchaeologyTrailRare) == "minecraft:archaeology/trail_ruins_rare");
    CHECK(lootTableFromName("minecraft:chests/simple_dungeon") == LootTable::SimpleDungeon);
    for (int i = 0; i < int(LootTable::Count); ++i) CHECK(lootTableFromName(lootTableName(LootTable(i))) == LootTable(i));
    // Desert pyramid archaeology turns up sherds among other things.
    Xoroshiro rng(5);
    int sherds = 0;
    for (int i = 0; i < 200; ++i) {
        const ItemStack it = rollOne(LootTable::ArchaeologyDesertPyramid, rng);
        sherds += !it.empty() && itemRegistry().item(it.item).id.ends_with("_pottery_sherd");
    }
    CHECK(sherds > 50);
}

TEST_CASE("overworld6 trail ruins: buried rooms of mud bricks and terracotta full of suspicious gravel (M27.5b)") {
    const OverworldGenerator gen(42);
    std::optional<ChunkPos> at;
    for (int cz = -150; cz <= 150 && !at; ++cz)
        for (int cx = -150; cx <= 150 && !at; ++cx) {
            if (!isSpreadCandidate(42, kTrailRuins, {cx, cz})) continue;
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 7, cz * 16 + 7));
            if ((b == Biome::Taiga || b == Biome::SnowyTaiga || b == Biome::OldGrowthPineTaiga ||
                 b == Biome::OldGrowthSpruceTaiga || b == Biome::OldGrowthBirchForest || b == Biome::Jungle) &&
                gen.surfaceY(cx * 16 + 7, cz * 16 + 7) >= OverworldGenerator::kSeaLevel)
                at = ChunkPos{cx, cz};
        }
    REQUIRE(at);
    Chunk c(*at);
    gen.generate(c);
    int suspicious = 0, mud = 0;
    for (int y = 40; y < 200; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                const BlockId b = R().blockOf(c.get(x, y, z));
                suspicious += b == blocks::SuspiciousGravel;
                mud += R().block(b).id == "minecraft:mud_bricks";
            }
    CHECK(suspicious > 10);
    CHECK(mud > 50);
    CHECK(int(c.brushables().size()) == suspicious);
    for (const auto& e : c.brushables())
        CHECK((e.data.table == uint8_t(LootTable::ArchaeologyTrailCommon) || e.data.table == uint8_t(LootTable::ArchaeologyTrailRare)));
}

TEST_CASE("torchflower and pitcher crops grow into their flowers; a sniffer egg hatches (M27.5c)") {
    Garden g;
    const auto& r = R();
    for (int x = 2; x <= 4; ++x) g.world.updateBlock({x, 63, 2}, S(blocks::Farmland));
    g.world.updateBlock({2, 64, 2}, S(blocks::TorchflowerCrop));
    g.world.updateBlock({4, 64, 2}, S(blocks::PitcherCrop));
    for (int i = 0; i < 6; ++i) g.updates.boneMeal({2, 64, 2}), g.updates.boneMeal({4, 64, 2});
    CHECK(g.at(2, 64, 2) == blocks::Torchflower);
    CHECK(g.at(4, 64, 2) == blocks::PitcherPlant);
    CHECK(g.at(4, 65, 2) == blocks::PitcherPlant);
    g.world.updateBlock({6, 64, 6}, S(blocks::SnifferEgg));
    for (int t = 1; t <= 24100 && g.updates.hatched().empty(); ++t) {
        g.updates.setTime(t);
        g.updates.tick();
    }
    REQUIRE_FALSE(g.updates.hatched().empty());
    CHECK(g.updates.hatched()[0].type == MobType::Sniffer);
    CHECK(g.at(6, 64, 6) == 0);
    (void)r;
}

#include "world/Enchantments.h"

TEST_CASE("M27 review regressions: grown pitcher plants keep to farmland; pistons break suspicious blocks; random enchantments keep their old range") {
    Garden g;
    g.world.updateBlock({4, 63, 2}, S(blocks::Farmland));
    g.world.updateBlock({4, 64, 2}, S(blocks::PitcherCrop));
    for (int i = 0; i < 6; ++i) g.updates.boneMeal({4, 64, 2});
    REQUIRE(g.at(4, 64, 2) == blocks::PitcherPlant);
    g.world.updateBlock({5, 64, 2}, S(blocks::Stone)); // (a neighbour update)
    g.world.updateBlock({5, 64, 2}, 0);
    CHECK(g.at(4, 64, 2) == blocks::PitcherPlant);
    CHECK(g.at(4, 65, 2) == blocks::PitcherPlant);
    // A piston pushing suspicious sand breaks it.
    g.world.updateBlock({8, 64, 8}, R().set(R().defaultState(blocks::Piston), properties::facing6, 5)); // (east)
    g.world.updateBlock({9, 64, 8}, S(blocks::SuspiciousSand));
    g.world.updateBlock({8, 64, 9}, S(blocks::RedstoneBlock));
    for (int t = 1; t < 6; ++t) {
        g.updates.setTime(t);
        g.updates.tick();
    }
    CHECK(g.at(10, 64, 8) != blocks::SuspiciousSand);
    CHECK(g.world.chunk({0, 0})->brushables().empty());
    // Random enchantments never hand out Swift Sneak and keep the pre-M27 range.
    CHECK(kRandomEnchantments == uint32_t(Enchantment::Channeling));
    Xoroshiro rng(2);
    for (int i = 0; i < 300; ++i) {
        std::array<ItemStack, 27> slots{};
        fillChest(LootTable::DesertPyramid, rng, slots);
        for (const ItemStack& s : slots) CHECK(enchantLevel(s, Enchantment::SwiftSneak) == 0);
    }
}

#include "world/NetherGenerator.h"

TEST_CASE("overworld7 (M29.8): the M29 blocks generate; nether4 bastions keep piglin brutes") {
    const OverworldGenerator gen(42, 7); // (M33.2e: no longer the newest)
    CHECK(gen.kind() == "overworld7");
    const auto& r = blockRegistry();
    int lichen = 0, flowers = 0, cocoa = 0, pads = 0, melons = 0;
    for (int cz = -12; cz <= 12; cz += 2)
        for (int cx = -12; cx <= 12; cx += 2) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = r.blockOf(c.get(x, y, z));
                        lichen += b == blocks::GlowLichen;
                        flowers += r.likeOf(b) == blocks::Poppy && b != blocks::Poppy;
                        cocoa += b == blocks::Cocoa;
                        pads += b == blocks::LilyPad;
                        melons += b == blocks::Melon;
                    }
        }
    MESSAGE("lichen " << lichen << " flowers " << flowers << " cocoa " << cocoa << " pads " << pads << " melons " << melons);
    CHECK(lichen > 100);
    CHECK(flowers > 0);
    // Swamps carry lily pads (on their sea-level water), jungles cocoa and melons.
    std::optional<ChunkPos> swamp;
    for (int ring = 0; ring <= 300 && !swamp; ring += 2)
        for (int cz = -ring; cz <= ring && !swamp; cz += 2)
            for (int cx = -ring; cx <= ring && !swamp; cx += 2)
                if (std::max(std::abs(cx), std::abs(cz)) == ring && gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8)) == Biome::Swamp)
                    swamp = ChunkPos{cx, cz};
    REQUIRE(swamp);
    int swampPads = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk c({swamp->x + dx, swamp->z + dz});
            gen.generate(c);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    for (int y = 58; y < 72; ++y) swampPads += r.blockOf(c.get(x, y, z)) == blocks::LilyPad;
        }
    MESSAGE("swamp " << swamp->x << "," << swamp->z << " pads " << swampPads);
    CHECK(swampPads > 0);
    // Deep-ocean magma comes with its bubble column already generated (M29 perf review).
    int magma = 0, columns = 0;
    for (int ring = 0; ring <= 200 && magma < 3; ring += 2)
        for (int cz = -ring; cz <= ring && magma < 3; cz += 2)
            for (int cx = -ring; cx <= ring && magma < 3; cx += 2) {
                if (std::max(std::abs(cx), std::abs(cz)) != ring ||
                    gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8)) != Biome::DeepOcean)
                    continue;
                Chunk c({cx, cz});
                gen.generate(c);
                for (int y = kOverworldHeight.minY; y < 60; ++y)
                    for (int z = 0; z < 16; ++z)
                        for (int x = 0; x < 16; ++x)
                            if (r.blockOf(c.get(x, y, z)) == blocks::MagmaBlock &&
                                r.blockOf(c.get(x, y + 1, z)) != blocks::Stone) {
                                ++magma;
                                columns += r.blockOf(c.get(x, y + 1, z)) == blocks::BubbleColumn;
                            }
            }
    MESSAGE("magma " << magma << " columns " << columns);
    CHECK(magma > 0);
    CHECK(columns == magma);
    const auto jungle = findBiome6(gen, Biome::Jungle);
    REQUIRE(jungle);
    MESSAGE("jungle " << jungle->x << "," << jungle->z);
    int jungleCocoa = 0, jungleMelons = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({jungle->x + dx, jungle->z + dz});
            gen.generate(c);
            for (int y = 50; y < 160; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        jungleCocoa += r.blockOf(c.get(x, y, z)) == blocks::Cocoa;
                        jungleMelons += r.blockOf(c.get(x, y, z)) == blocks::Melon;
                    }
        }
    MESSAGE("jungle cocoa " << jungleCocoa << " melons " << jungleMelons);
    CHECK(jungleCocoa > 0);
    CHECK(jungleMelons > 0);
    // overworld6's chunks are untouched by it (the pin above), and nether4 adds brutes only:
    // two armed brutes in every bastion's start chunk.
    const NetherGenerator n4(42);
    CHECK(n4.kind() == "nether4");
    std::optional<ChunkPos> bastion;
    for (int z = -60; z <= 60 && !bastion; ++z)
        for (int x = -60; x <= 60 && !bastion; ++x)
            if (n4.complexAt({x, z}) == NetherGenerator::Complex::Bastion) bastion = ChunkPos{x, z};
    REQUIRE(bastion);
    int brutes = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({bastion->x + dx, bastion->z + dz}, kNetherHeight);
            n4.generate(c);
            for (const MobData& m : c.mobs())
                if (m.type == MobType::PiglinBrute) {
                    ++brutes;
                    CHECK(m.heldItem == *itemRegistry().find("golden_axe"));
                }
        }
    CHECK(brutes == 2);
}

namespace {
uint64_t nameHash(const Chunk& c, const HeightRange& height) { // (by state name: ids may move)
    uint64_t h = 1469598103934665603ull;
    for (int y = height.minY; y <= height.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                for (const char ch : R().toString(c.get(x, y, z))) {
                    h ^= uint8_t(ch);
                    h *= 1099511628211ull;
                }
    return h;
}
} // namespace

TEST_CASE("overworld7 and nether4 output is pinned (the new-world defaults as of v0.29.0)") {
    const OverworldGenerator gen(42, 7);
    const auto jungle = findBiome6(gen, Biome::Jungle);
    REQUIRE(jungle);
    Chunk c(*jungle);
    gen.generate(c);
    Chunk n({3, -5}, kNetherHeight);
    NetherGenerator(42, 4).generate(n);
    MESSAGE("overworld7 " << nameHash(c, kOverworldHeight) << " nether4 " << nameHash(n, kNetherHeight));
    CHECK(nameHash(c, kOverworldHeight) == 14997860066032220082ull);
    CHECK(nameHash(n, kNetherHeight) == 882395333977743428ull);
}

TEST_CASE("overworld8 (M33.2e; 26.2): sulfur caves - sulfur and cinnabar bands, spikes, potent sulfur; pinned") {
    const OverworldGenerator gen(42, 8);
    CHECK(gen.kind() == "overworld8");
    CHECK(OverworldGenerator::versionOf("overworld8") == 8);
    std::optional<ChunkPos> at;
    for (int ring = 0; ring <= 300 && !at; ring += 2)
        for (int cz = -ring; cz <= ring && !at; cz += 2)
            for (int cx = -ring; cx <= ring && !at; cx += 2) {
                if (std::max(std::abs(cx), std::abs(cz)) != ring) continue;
                if (OverworldGenerator::caveBiome(gen.column(cx * 16 + 8, cz * 16 + 8), 8) == Biome::SulfurCaves)
                    at = ChunkPos{cx, cz};
            }
    REQUIRE(at);
    MESSAGE("sulfur caves at chunk " << at->x << ", " << at->z);
    const auto& r = blockRegistry();
    const BlockId sulfur = *r.findBlock("sulfur"), cinnabar = *r.findBlock("cinnabar"),
                  spike = *r.findBlock("sulfur_spike"), potent = *r.findBlock("potent_sulfur");
    int s = 0, c = 0, spikes = 0, pots = 0;
    uint64_t h = 1469598103934665603ull;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk ch({at->x + dx, at->z + dz});
            gen.generate(ch);
            for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = r.blockOf(ch.get(x, y, z));
                        s += b == sulfur;
                        c += b == cinnabar;
                        spikes += b == spike;
                        pots += b == potent;
                        if (dx == 0 && dz == 0)
                            for (const char chr : r.toString(ch.get(x, y, z))) {
                                h ^= uint8_t(chr);
                                h *= 1099511628211ull;
                            }
                    }
        }
    MESSAGE("sulfur " << s << ", cinnabar " << c << ", spikes " << spikes << ", potent " << pots << ", hash " << h);
    CHECK(s > 500);
    CHECK(c > 500);
    CHECK(spikes > 0);
    CHECK(h == 4374325674733634202ull); // (frozen once released: new worlds make overworld8)
}

#include "world/TreeFeature.h"

TEST_CASE("M33.3a (26.3): a poplar is a tall slim trunk in a narrow column of leaves") {
    Xoroshiro rng(7);
    const int height = treeHeight(TreeKind::Poplar, rng);
    CHECK(height >= 7);
    CHECK(height <= 10);
    int logs = 0, leaves = 0, widest = 0;
    treeShape(TreeKind::Poplar, 0, 64, 0, height, rng, [&](int32_t x, int32_t, int32_t z, int d) {
        if (d == 0) ++logs;
        else ++leaves;
        widest = std::max({widest, std::abs(x), std::abs(z)});
    });
    CHECK(logs == height);
    CHECK(leaves > 20);
    CHECK(widest <= 2);
    CHECK(blockRegistry().findBlock("poplar_hanging_sign").has_value());
    CHECK(blockRegistry().findBlock("stripped_poplar_wood").has_value());
    CHECK(itemRegistry().find("poplar_chest_boat").has_value());
}

TEST_CASE("overworld8 (M33.3b; 26.3): the dappled forest - poplars in three colours, red shrubs, shelf mushrooms") {
    const OverworldGenerator gen(42, 8);
    const auto at = findBiome6(gen, Biome::DappledForest);
    REQUIRE(at);
    MESSAGE("dappled forest at chunk " << at->x << ", " << at->z);
    const auto& r = blockRegistry();
    const BlockId shrub = *r.findBlock("red_shrub"), shelf = *r.findBlock("shelf_mushroom"),
                  bigShelf = *r.findBlock("large_shelf_mushroom");
    int logs = 0, shrubs = 0, shelves = 0, colours[3] = {};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk ch({at->x + dx, at->z + dz});
            gen.generate(ch);
            for (int y = 40; y <= 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = r.blockOf(ch.get(x, y, z));
                        logs += b == blocks::PoplarLog;
                        shrubs += b == shrub;
                        shelves += b == shelf || b == bigShelf;
                        colours[0] += b == blocks::RedPoplarLeaves;
                        colours[1] += b == blocks::OrangePoplarLeaves;
                        colours[2] += b == blocks::YellowPoplarLeaves;
                    }
        }
    MESSAGE("poplar logs " << logs << ", shrubs " << shrubs << ", shelves " << shelves << ", leaves " << colours[0] << "/"
                           << colours[1] << "/" << colours[2]);
    CHECK(logs > 20);
    CHECK(shrubs > 5);
    CHECK(shelves > 0);
    CHECK(colours[0] + colours[1] + colours[2] > 100);
}
