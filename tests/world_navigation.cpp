// Navigation items (M28.2; wiki: Compass, Lodestone, Recovery Compass, Clock).
#include "gameplay/Recipes.h"
#include "rendering/ItemIcons.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Dimension.h"
#include "world/ItemExtras.h"
#include "world/LevelData.h"

#include <doctest/doctest.h>

#include <array>
#include <filesystem>

using namespace mc;
using namespace mc::world;
using gfx::ItemIcons;

TEST_CASE("compass needle: straight ahead 0, right 8, behind 16, left 24 (of 32)") {
    const glm::dvec3 p(0.0, 64.0, 0.0);
    // Facing south (yaw 0, +Z): ahead is +Z, the right hand is west (-X).
    CHECK(ItemIcons::compassFrame(p, 0.0f, {0.0, 64.0, 100.0}) == 0);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {-100.0, 64.0, 0.0}) == 8);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {0.0, 64.0, -100.0}) == 16);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {100.0, 64.0, 0.0}) == 24);
    // Turning to face west (yaw 90) brings a western target straight ahead.
    CHECK(ItemIcons::compassFrame(p, 90.0f, {-100.0, 64.0, 0.0}) == 0);
    CHECK(ItemIcons::compassFrame(p, -270.0f, {-100.0, 64.0, 0.0}) == 0);
}

TEST_CASE("clock: noon frame 0, midnight half way round") {
    CHECK(ItemIcons::clockFrame(0.0) == 0);
    CHECK(ItemIcons::clockFrame(0.5) == 32);
    CHECK(ItemIcons::clockFrame(0.999) == 0);
}

TEST_CASE("lodestone compasses save vanilla's lodestone_tracker") {
    ItemStack c{*itemRegistry().find("compass"), 1};
    c.extra = addLodestoneTarget({{10, 70, -5}, uint8_t(Dimension::Nether)});
    const nbt::Compound n = itemToNbt(c, 0);
    const nbt::Compound* comps = n.compound("components");
    REQUIRE(comps);
    const nbt::Compound* tracker = comps->compound("minecraft:lodestone_tracker");
    REQUIRE(tracker);
    REQUIRE(tracker->compound("target"));
    CHECK(*tracker->compound("target")->string("dimension") == "minecraft:the_nether");
    const ItemStack back = itemFromNbtPublic(n);
    const auto t = lodestoneTarget(back.extra);
    REQUIRE(t);
    CHECK(t->hasTarget);
    CHECK(t->pos == BlockPos{10, 70, -5});
    CHECK(t->dimension == uint8_t(Dimension::Nether));
    // Two compasses bound to the same lodestone separately don't stack (different entries)
    // but a copy of one does.
    ItemStack copy = back;
    CHECK(copy.sameKind(back));
    CHECK_FALSE(back.sameKind({*itemRegistry().find("compass"), 1}));
}

TEST_CASE("the last death is saved as LastDeathLocation") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_lastdeath_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    LevelData l;
    l.hasLastDeath = true;
    l.lastDeath[0] = 5, l.lastDeath[1] = -20, l.lastDeath[2] = 7;
    l.lastDeathDimension = int(Dimension::End);
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    CHECK(back->hasLastDeath);
    CHECK(back->lastDeath[1] == -20);
    CHECK(back->lastDeathDimension == int(Dimension::End));
    std::filesystem::remove_all(dir);
}

TEST_CASE("compass, clock, recovery compass and lodestone recipes") {
    auto stack = [](const char* id) { return ItemStack{*itemRegistry().find(id), 1}; };
    std::array<ItemStack, 9> g{};
    g[1] = g[3] = g[5] = g[7] = stack("iron_ingot");
    g[4] = stack("redstone");
    auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:compass");
    g[1] = g[3] = g[5] = g[7] = stack("gold_ingot");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:clock");
    g.fill(stack("echo_shard"));
    g[4] = stack("compass");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:recovery_compass");
    g.fill(stack("chiseled_stone_bricks"));
    g[4] = stack("netherite_ingot");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:lodestone");
}

#include "gameplay/Cartography.h"
#include "gameplay/Commands.h"
#include "world/Maps.h"
#include "world/World.h"

TEST_CASE("map colours: vanilla's base colours by block; shades") {
    auto col = [](const char* id) { return mapColorOf(*blockRegistry().findBlock(id)); };
    CHECK(col("grass_block") == 1);
    CHECK(col("water") == 12);
    CHECK(col("stone") == 11);
    CHECK(col("spruce_leaves") == 7);
    CHECK(col("spruce_planks") == 34);
    CHECK(col("white_wool") == 8);
    CHECK(col("red_wool") == 28);
    CHECK(col("glass") == 0);
    CHECK(col("sand") == 2);
    CHECK(mapColorRgb(1 * 4 + 2) == 0x7FB238); // grass, full brightness
    const uint32_t darkWater =
        uint32_t((64 * 180 / 255) << 16 | (64 * 180 / 255) << 8 | (255 * 180 / 255));
    CHECK(mapColorRgb(12 * 4 + 0) == darkWater);
    CHECK(mapColorRgb(0) == 0);
}

TEST_CASE("maps: centred on vanilla's grid, drawn from the terrain, saved as map_<id>.dat") {
    Maps maps;
    const int a = maps.create(40, 40, 0, 0);
    CHECK(maps.get(a)->centerX == 0);
    const int b = maps.create(100, -65, 0, 0);
    CHECK(maps.get(b)->centerX == 128);
    CHECK(maps.get(b)->centerZ == -128);
    const int c = maps.create(10, 10, 1, 0); // 256-block cells
    CHECK(maps.get(c)->centerX == 64);
    CHECK(b != a);

    World world;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk& ch = world.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    ch.set(x, 63, z, blockRegistry().defaultState(blocks::GrassBlock));
                    if (cx == 1) ch.set(x, 64, z, blockRegistry().defaultState(blocks::Water));
                }
        }
    MapData& m = *maps.get(a);
    for (int t = 0; t < 16; ++t)
        Maps::update(world, m, {0.5, 64.0, 0.5}, t);
    CHECK(int(m.colors[size_t(64 * 128 + 64)] >> 2) == 1);  // grass at the centre
    CHECK(int(m.colors[size_t(64 * 128 + 84)] >> 2) == 12); // water to the east (x 20)
    CHECK(m.colors[size_t(64 * 128 + 120)] == 0);           // unloaded: nothing drawn
    CHECK(m.version > 0);

    const auto dir = std::filesystem::temp_directory_path() / "mc_maps_test";
    std::filesystem::remove_all(dir);
    REQUIRE(maps.save(dir));
    CHECK(std::filesystem::exists(dir / "data" / "map_0.dat"));
    CHECK(std::filesystem::exists(dir / "data" / "idcounts.dat"));
    Maps back;
    REQUIRE(back.load(dir));
    REQUIRE(back.get(a));
    CHECK(back.get(a)->colors == m.colors);
    CHECK(back.get(c)->scale == 1);
    CHECK(back.create(0, 0, 0, 0) == 3); // (ids go on after the saved ones)
    std::filesystem::remove_all(dir);
}

TEST_CASE("maps: copying and zooming out by crafting, the cartography table; map_id saved") {
    auto stack = [](const char* id) { return ItemStack{*itemRegistry().find(id), 1}; };
    ItemStack filled = stack("filled_map");
    filled.damage = 7;
    std::array<ItemStack, 9> g{};
    g[0] = filled;
    g[1] = g[2] = stack("map");
    auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(r->item == filled.item);
    CHECK(r->count == 3);
    CHECK(r->damage == 7);
    g.fill(stack("paper"));
    g[4] = filled;
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(r->state == kMapScale);
    CHECK(cartography(filled, stack("glass_pane")).state == kMapLock);
    CHECK(cartography(filled, stack("map")).count == 2);
    CHECK(cartography(filled, stack("stick")).empty());

    ItemStack marked = filled;
    marked.state = kMapScale;
    const nbt::Compound n = itemToNbt(marked, 0);
    CHECK(n.compound("components")->integer("minecraft:map_id") == 7);
    CHECK(n.compound("components")->integer("minecraft:map_post_processing") == 1);
    const ItemStack back = itemFromNbtPublic(n);
    CHECK(back.damage == 7);
    CHECK(back.state == kMapScale);
}

TEST_CASE("/item replace entity @s weapon.mainhand / offhand / hotbar.N") {
    Player player;
    Inventory inv;
    int64_t dayTime = 0;
    CommandContext ctx{player, inv, dayTime, 0, 42};
    CHECK(runCommand("/item replace entity @s weapon.mainhand with filled_map", ctx).ok);
    CHECK(itemRegistry().item(inv.selectedStack().item).id == "minecraft:filled_map");
    CHECK(runCommand("/item replace entity @s weapon.offhand with compass", ctx).ok);
    CHECK(itemRegistry().item(inv.offhand().item).id == "minecraft:compass");
    CHECK(runCommand("/item replace entity @s hotbar.3 with stick 5", ctx).ok);
    CHECK(inv.slot(3).count == 5);
    CHECK_FALSE(runCommand("/item replace entity @s hotbar.9 with stick", ctx).ok);
    CHECK_FALSE(runCommand("/item replace entity @s nowhere with stick", ctx).ok);
    // (M29.6) a container's slot: a shelf takes 3
    World w;
    w.createChunk({0, 0});
    w.setBlock({2, 64, 2}, blockRegistry().defaultState(blocks::Shelf));
    ctx.world = &w;
    CHECK(runCommand("/item replace block 2 64 2 container.1 with apple 3", ctx).ok);
    CHECK(w.chunk({0, 0})->chest(2, 64, 2)->items[1].count == 3);
    CHECK_FALSE(runCommand("/item replace block 2 64 2 container.3 with apple", ctx).ok);
    CHECK_FALSE(runCommand("/item replace block 5 64 5 container.0 with apple", ctx).ok);
}

#include "ui/BookScreen.h"

namespace {
gfx::FontMetrics monoFont() {
    gfx::FontMetrics f;
    f.advance.fill(6); // every glyph 6 px wide (5 + spacing)
    return f;
}
} // namespace

TEST_CASE("book screen: words wrap at 114 px, 14 lines a page, titles of 32") {
    const auto font = monoFont();
    // 19 glyphs fit a 114 px line; the break comes after the last whole word.
    std::vector<std::string> lines;
    const std::string text = "aaaa bbbb cccc dddd eeee ffff\nnext";
    ui::BookScreen::wrap(text, font, 114,
                         [&](size_t s, size_t n) { lines.push_back(text.substr(s, n)); });
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "aaaa bbbb cccc");
    CHECK(lines[1] == "dddd eeee ffff");
    CHECK(lines[2] == "next");
    CHECK(ui::BookScreen::lineCount("", font, 114) == 1);

    ui::BookScreen book;
    book.openEdit({}, 0);
    book.type(std::string(400, 'x'), font); // 19 a line x 14 lines = 266 at most
    CHECK(book.content().pages[0].size() == 266);
    book.type("\b\b", font);
    CHECK(book.content().pages[0].size() == 264);
    book.turn(1); // past the last written page: a new one
    CHECK(book.content().pages.size() == 2);
    CHECK(book.page() == 1);
    book.turn(1); // (an empty last page doesn't grow the book)
    CHECK(book.content().pages.size() == 2);
}

TEST_CASE(
    "books: copying keeps the original, one generation on; pages saved as vanilla's components") {
    BookContent original;
    original.title = "Notes";
    original.author = "Player";
    original.pages = {"first page", "second"};
    ItemStack written{*itemRegistry().find("written_book"), 1};
    written.extra = addBook(original);
    std::array<ItemStack, 9> g{};
    g[0] = written;
    g[1] = g[2] = ItemStack{*itemRegistry().find("writable_book"), 1};
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(r->count == 2);
    const ItemStack copy = bookCopy(*r, 2);
    REQUIRE(bookContent(copy.extra));
    CHECK(bookContent(copy.extra)->generation == 1);
    CHECK(bookContent(written.extra)->generation == 0);
    // A copy of a copy can't be copied again.
    ItemStack copy2 = copy;
    copy2.extra = bookCopy(copy, 1).extra;
    g[0] = copy2;
    CHECK_FALSE(craft(g, 3));

    const nbt::Compound n = itemToNbt(written, 0);
    const nbt::Compound* content =
        n.compound("components")->compound("minecraft:written_book_content");
    REQUIRE(content);
    CHECK(*content->string("author") == "Player");
    CHECK(content->list("pages")->items.size() == 2);
    const ItemStack back = itemFromNbtPublic(n);
    const auto b = bookContent(back.extra);
    REQUIRE(b);
    CHECK(b->title == "Notes");
    CHECK(b->pages[1] == "second");

    std::array<ItemStack, 4> q{};
    q[0] = ItemStack{*itemRegistry().find("book"), 1};
    q[1] = ItemStack{*itemRegistry().find("ink_sac"), 1};
    q[2] = ItemStack{*itemRegistry().find("feather"), 1};
    const auto quill = craft(q, 2);
    REQUIRE(quill);
    CHECK(itemRegistry().item(quill->item).id == "minecraft:writable_book");
}

TEST_CASE("equal books and lodestone targets share one entry (reloads don't grow the tables)") {
    mc::world::BookContent b;
    b.pages = {"same text"};
    CHECK(mc::world::addBook(b) == mc::world::addBook(b));
    const mc::world::LodestoneTarget t{{7, 64, -3}, 0};
    CHECK(mc::world::addLodestoneTarget(t) == mc::world::addLodestoneTarget(t));
    mc::world::LodestoneTarget other = t;
    other.pos.x = 8;
    CHECK(mc::world::addLodestoneTarget(other) != mc::world::addLodestoneTarget(t));
}
