// Item frames and paintings (M28.3a; wiki: Item Frame, Painting).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Paintings.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct Scene {
    World world;
    Xoroshiro rng{5};
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) world.createChunk({cx, cz});
        // A stone wall at x = 3, y 60..65, z -3..3.
        for (int y = 60; y <= 65; ++y)
            for (int z = -3; z <= 3; ++z) world.setBlock({3, y, z}, blockRegistry().defaultState(blocks::Stone));
    }
    std::vector<MobData*> hanging() {
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (isHanging(m.type)) out.push_back(&m);
        });
        return out;
    }
};

} // namespace

TEST_CASE("paintings: the biggest canvas that fits, centred on the wall; none on floors") {
    Scene s;
    REQUIRE(Mobs::placeHanging(s.world, MobType::Painting, {3, 62, 0}, Direction::West, s.rng));
    auto h = s.hanging();
    REQUIRE(h.size() == 1);
    const PaintingVariant& v = kPaintings[h[0]->woolColour];
    CHECK(v.width * v.height == 16); // (4x4 fits: 6 tall, 7 wide)
    CHECK(h[0]->pos.x == doctest::Approx(3.0 - 1.0 / 32.0));
    const Aabb b = Mobs::box(*h[0]);
    CHECK(b.max.y - b.min.y == doctest::Approx(4.0));
    CHECK(b.max.z - b.min.z == doctest::Approx(4.0));
    // Another one can't overlap it; on the floor it can't hang at all.
    CHECK_FALSE(Mobs::placeHanging(s.world, MobType::Painting, {3, 62, 0}, Direction::West, s.rng));
    CHECK_FALSE(Mobs::placeHanging(s.world, MobType::Painting, {3, 65, 0}, Direction::Up, s.rng));
}

TEST_CASE("item frames: take an item, turn it, a hit takes it out, the wall going drops them") {
    Scene s;
    REQUIRE(Mobs::placeHanging(s.world, MobType::ItemFrame, {3, 61, 2}, Direction::West, s.rng));
    MobData& f = *s.hanging()[0];
    const ItemStack stick{*itemRegistry().find("stick"), 5};
    CHECK(Mobs::useItemFrame(s.world, f, stick)); // (one stick goes in)
    CHECK(Mobs::frameItem(s.world, f).count == 1);
    CHECK_FALSE(Mobs::useItemFrame(s.world, f, stick)); // full: turns it
    CHECK(f.node == 1);
    ItemEntities items;
    CHECK(Mobs::popFrameItem(s.world, f, items, s.rng));
    CHECK(Mobs::frameItem(s.world, f).empty());
    CHECK(items.items().size() == 1);

    // The wall block goes: after the 100-tick check the frame drops as an item.
    s.world.setBlock({3, 61, 2}, 0);
    CHECK_FALSE(Mobs::hangingSurvives(s.world, f));
}

TEST_CASE("item frames and paintings are saved as vanilla's entities") {
    Scene s;
    REQUIRE(Mobs::placeHanging(s.world, MobType::GlowItemFrame, {3, 61, 2}, Direction::West, s.rng));
    MobData& f = *s.hanging()[0];
    Mobs::useItemFrame(s.world, f, {*itemRegistry().find("compass"), 1});
    Mobs::useItemFrame(s.world, f, {}); // turned once
    Chunk& c = *s.world.chunk({0, 0});
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(c, 0));
    const nbt::List* list = n.list("Entities");
    REQUIRE(list);
    REQUIRE(list->items.size() == 1);
    const nbt::Compound& e = *list->items[0].get<nbt::Compound>();
    CHECK(*e.string("id") == "minecraft:glow_item_frame");
    CHECK(e.integer("Facing") == int(Direction::West));
    CHECK(e.integer("ItemRotation") == 1);
    REQUIRE(e.compound("Item"));
    CHECK(*e.compound("Item")->string("id") == "minecraft:compass");

    Chunk back({0, 0}, s.world.height());
    entitiesFromNbt(n, back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].type == MobType::GlowItemFrame);
    CHECK(back.mobs()[0].home == glm::ivec3(3, 61, 2));
    REQUIRE(back.mobStore(back.mobs()[0].uuidHi));
    CHECK(itemRegistry().item((*back.mobStore(back.mobs()[0].uuidHi))[0].item).id == "minecraft:compass");
}

#include "gameplay/Recipes.h"

TEST_CASE("armor stands: placed facing the player, dressed and undressed, two quick hits break them") {
    Scene s;
    for (int z = -3; z <= 3; ++z)
        for (int x = -3; x <= 3; ++x) s.world.setBlock({x, 59, z}, blockRegistry().defaultState(blocks::Stone));
    REQUIRE(Mobs::placeArmorStand(s.world, {0, 60, 0}, 10.0f, s.rng));
    MobData* stand = nullptr;
    s.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs())
            if (m.type == MobType::ArmorStand) stand = &m;
    });
    REQUIRE(stand);
    CHECK(stand->yaw == doctest::Approx(180.0f)); // (facing back at the player, 45-degree steps)

    ItemStack helmet{*itemRegistry().find("iron_helmet"), 1};
    CHECK(Mobs::useArmorStand(s.world, *stand, helmet, 61.8));
    CHECK(helmet.empty());
    CHECK(stand->worn[0] == 3); // iron
    ItemStack boots{*itemRegistry().find("golden_boots"), 1};
    CHECK(Mobs::useArmorStand(s.world, *stand, boots, 61.0)); // (armor goes to its own slot wherever clicked)
    CHECK(stand->worn[3] == 4);
    ItemStack hand{};
    CHECK(Mobs::useArmorStand(s.world, *stand, hand, 61.7)); // empty hand at head height: the helmet
    CHECK(itemRegistry().item(hand.item).id == "minecraft:iron_helmet");
    CHECK(stand->worn[0] == 0);

    CHECK_FALSE(Mobs::hitArmorStand(s.world, *stand, false)); // shakes
    CHECK(Mobs::hitArmorStand(s.world, *stand, false));       // a second hit within 5 ticks
    CHECK(stand->health <= 0.0f);

    auto stack = [](const char* id) { return ItemStack{*itemRegistry().find(id), 1}; };
    std::array<ItemStack, 9> g{};
    g[0] = g[1] = g[2] = g[4] = g[6] = g[8] = stack("stick");
    g[7] = stack("smooth_stone_slab");
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:armor_stand");
}

TEST_CASE("armor stands save their armor as vanilla's equipment") {
    Scene s;
    REQUIRE(Mobs::placeArmorStand(s.world, {0, 60, 0}, 0.0f, s.rng));
    MobData* stand = nullptr;
    s.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs())
            if (m.type == MobType::ArmorStand) stand = &m;
    });
    REQUIRE(stand);
    ItemStack chest{*itemRegistry().find("diamond_chestplate"), 1};
    REQUIRE(Mobs::useArmorStand(s.world, *stand, chest, 61.0));
    Chunk& c = *s.world.chunk({0, 0});
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(c, 0));
    const nbt::Compound& e = *n.list("Entities")->items[0].get<nbt::Compound>();
    CHECK(*e.string("id") == "minecraft:armor_stand");
    REQUIRE(e.compound("equipment"));
    CHECK(*e.compound("equipment")->compound("chest")->string("id") == "minecraft:diamond_chestplate");
    Chunk back({0, 0}, s.world.height());
    entitiesFromNbt(n, back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].worn[1] == 5);
}
