#include "ui/CreativeInventory.h"

#include "world/Potions.h"

#include "ui/Hud.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace mc::ui {

namespace {

// Panel colours in the game's GUI style (our palette): light grey body, white
// top-left highlight, dark bottom-right shadow; slots are recessed.
constexpr uint32_t kBody = gfx::rgba(198, 198, 198);
constexpr uint32_t kLight = gfx::rgba(255, 255, 255);
constexpr uint32_t kDark = gfx::rgba(85, 85, 85);
constexpr uint32_t kEdge = gfx::rgba(0, 0, 0);
constexpr uint32_t kSlotFill = gfx::rgba(139, 139, 139);
constexpr uint32_t kHover = gfx::rgba(255, 255, 255, 128);

void slotFrame(gfx::GuiBatch& b, float x, float y) {
    b.fill(x, y, 18, 18, kSlotFill);
    b.fill(x, y, 17, 1, kDark);  // top
    b.fill(x, y, 1, 17, kDark);  // left
    b.fill(x + 1, y + 17, 17, 1, kLight); // bottom
    b.fill(x + 17, y + 1, 1, 17, kLight); // right
}

} // namespace

CreativeInventory::Tab CreativeInventory::tabOf(const world::ItemDef& def) {
    std::string_view id = def.id;
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    auto has = [&](std::string_view part) { return id.find(part) != std::string_view::npos; };
    auto is = [&](std::initializer_list<std::string_view> ids) {
        for (std::string_view x : ids)
            if (id == x) return true;
        return false;
    };
    if (def.spawnEgg != 0) return Tab::SpawnEggs;
    if (is({"command_block", "chain_command_block", "repeating_command_block", "command_block_minecart",
            "structure_block", "structure_void", "jigsaw", "barrier", "light", "debug_stick", "test_block",
            "test_instance_block", "spawner", "trial_spawner", "vault", "petrified_oak_slab", "knowledge_book"}))
        return Tab::Operator;
    if (def.block) {
        // Redstone (vanilla's Redstone Blocks tab).
        if (has("redstone") || is({"repeater", "comparator", "piston", "sticky_piston", "observer", "hopper",
                                   "dispenser", "dropper", "lever", "tnt", "target", "daylight_detector",
                                   "tripwire_hook", "note_block", "sculk_sensor", "calibrated_sculk_sensor",
                                   "trapped_chest", "crafter", "lectern", "slime_block", "honey_block"}) ||
            has("_button") || has("pressure_plate") || has("rail") || has("lightning_rod") || has("copper_bulb"))
            return Tab::Redstone;
        // Colored.
        if ((has("wool") || has("carpet") || has("terracotta") || has("concrete") || has("stained_glass") ||
             id.ends_with("_bed") || id.ends_with("candle") || id.ends_with("banner") || id.ends_with("shulker_box")) &&
            !has("moss"))
            return Tab::Colored;
        // Functional.
        if (has("crafting_table") || has("furnace") || is({"smoker", "chest", "barrel", "ender_chest", "grindstone",
                                                            "stonecutter", "loom", "cartography_table",
                                                            "smithing_table", "fletching_table", "enchanting_table",
                                                            "brewing_stand", "cauldron", "composter", "beacon",
                                                            "conduit", "bell", "lodestone", "respawn_anchor",
                                                            "beehive", "scaffolding", "ladder", "flower_pot",
                                                            "decorated_pot", "jukebox", "chiseled_bookshelf",
                                                            "end_portal_frame", "dragon_egg", "end_rod", "chain",
                                                            "iron_chain", "shelf"}) ||
            has("anvil") || has("campfire") || has("lantern") || has("torch") || has("sign") || has("_head") ||
            has("_skull") || id.ends_with("_shelf") || has("copper_chest") || has("golem_statue") || has("bars"))
            return Tab::Functional;
        // Natural.
        if (has("_ore") || has("leaves") || has("sapling") || has("propagule") || has("mushroom") || has("coral") ||
            has("dirt") || has("grass") || has("_log") || has("_stem") || has("nylium") || has("roots") ||
            has("dripleaf") || has("azalea") || has("vine") || has("moss") || has("sculk") || has("amethyst") ||
            has("raw_") || has("_egg") || has("fungus") || has("bamboo") || has("cactus") || has("kelp") ||
            has("seagrass") || has("snow") || has("ice") || has("flower") || has("tulip") || has("orchid") ||
            has("petals") || has("litter") || has("bush") || has("wart") || has("berr") || has("cocoa") ||
            has("chorus") || has("wheat") || has("carrots") || has("potatoes") || has("beetroots") ||
            has("plant") || has("sprouts") || has("_crop") || has("lichen") ||
            is({"stone", "deepslate", "granite", "diorite", "andesite", "tuff", "calcite", "dripstone_block",
                "pointed_dripstone", "sand", "red_sand", "gravel", "clay", "mud", "podzol", "mycelium", "farmland",
                "netherrack", "soul_sand", "soul_soil", "basalt", "blackstone", "end_stone", "obsidian",
                "crying_obsidian", "magma_block", "glowstone", "bedrock", "ancient_debris", "sponge", "wet_sponge",
                "sugar_cane", "lily_pad", "pumpkin", "carved_pumpkin", "melon", "dandelion", "poppy", "allium",
                "azure_bluet", "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose", "sunflower",
                "lilac", "rose_bush", "peony", "torchflower", "pitcher_plant", "fern", "large_fern", "dead_bush",
                "sea_pickle", "hanging_roots", "glow_lichen", "spore_blossom", "cobweb", "frogspawn", "bee_nest",
                "hay_block", "dried_ghast", "resin_clump", "pale_hanging_moss", "open_eyeblossom",
                "closed_eyeblossom", "creaking_heart", "suspicious_sand", "suspicious_gravel", "powder_snow"}))
            return Tab::Natural;
        return Tab::Building;
    }
    if (def.food > 0 || has("potion") || is({"milk_bucket", "cake", "honey_bottle", "ominous_bottle"})) return Tab::Food;
    if (def.tool == world::ToolType::Sword || def.tool == world::ToolType::Spear || def.armorSlot > 0 ||
        has("horse_armor") || has("arrow") ||
        is({"bow", "crossbow", "shield", "trident", "mace", "totem_of_undying", "wind_charge", "snowball", "egg",
            "blue_egg", "brown_egg", "wolf_armor", "turtle_helmet", "ender_pearl", "end_crystal"}))
        return Tab::Combat;
    if (def.tool != world::ToolType::None || has("bucket") || has("boat") || has("_raft") || has("minecart") ||
        has("music_disc") || has("bundle") ||
        is({"flint_and_steel", "shears", "fishing_rod", "carrot_on_a_stick", "warped_fungus_on_a_stick",
            "compass", "recovery_compass", "clock", "map", "filled_map", "spyglass", "brush", "lead", "name_tag",
            "saddle", "firework_rocket", "writable_book", "written_book", "goat_horn", "ender_eye", "elytra",
            "harness", "bone_meal", "armor_stand", "item_frame", "glow_item_frame", "painting"}) ||
        id.ends_with("_harness"))
        return Tab::Tools;
    return Tab::Ingredients;
}

const char* CreativeInventory::tabName(Tab t) {
    static constexpr const char* kNames[size_t(Tab::Count)] = {
        "Building Blocks", "Colored Blocks", "Natural Blocks", "Functional Blocks", "Redstone Blocks",
        "Operator Utilities", "Search Items", "Tools & Utilities", "Combat", "Food & Drinks", "Ingredients",
        "Spawn Eggs"};
    return kNames[size_t(t)];
}

void CreativeInventory::selectTab(Tab t) {
    m_tab = t;
    m_scrollRow = 0;
    m_scrollRemainder = 0.0;
    refilter();
}

void CreativeInventory::type(std::string_view text) {
    if (m_tab != Tab::Search) return;
    for (char c : text)
        if (c == '\b') {
            if (!m_query.empty()) m_query.pop_back();
        } else if (c >= 32 && c < 127 && m_query.size() < 50) {
            m_query.push_back(c);
        }
    m_scrollRow = 0;
    refilter();
}

void CreativeInventory::backspace() { type("\b"); }

void CreativeInventory::refilter() {
    m_shown.clear();
    if (m_tab != Tab::Search) {
        m_shown = m_tabItems[size_t(m_tab)];
        return;
    }
    // Every word of the query must appear in the name (vanilla matches the name and tags).
    std::string q;
    for (char c : m_query) q.push_back(c == '_' ? ' ' : char(std::tolower(static_cast<unsigned char>(c))));
    for (size_t i = 0; i < m_items.size(); ++i) {
        bool ok = true;
        size_t from = 0;
        while (ok && from < q.size()) {
            size_t to = q.find(' ', from);
            if (to == std::string::npos) to = q.size();
            if (to > from && m_searchNames[i].find(std::string_view(q).substr(from, to - from)) == std::string::npos)
                ok = false;
            from = to + 1;
        }
        if (ok) m_shown.push_back(int(i));
    }
}

void CreativeInventory::build(const gfx::BlockModels& models) {
    const auto& reg = world::blockRegistry();
    const auto& items = world::itemRegistry();
    m_items.clear();
    m_names.clear();
    for (size_t i = 1; i < items.count(); ++i) {
        const world::ItemDef& def = items.item(static_cast<world::ItemId>(i));
        if (def.block) {
            const world::BlockStateId s = reg.defaultState(def.block);
            if (s >= models.size() || !models[s].visible || models[s].fluid) continue;
        }
        std::string name = def.id;
        if (name.starts_with("minecraft:")) name.erase(0, 10);
        if (name == "potion" || name == "splash_potion") { // one of each potion (vanilla's tab)
            for (int p = 1; p < static_cast<int>(world::Potion::Count); ++p) {
                world::ItemStack s{static_cast<world::ItemId>(i), 1};
                s.potion = static_cast<uint8_t>(p);
                m_items.push_back(s);
                m_names.push_back(name + " " + std::string(world::potionInfo(static_cast<world::Potion>(p)).id));
            }
            continue;
        }
        m_items.push_back({static_cast<world::ItemId>(i), 1});
        m_names.push_back(std::move(name));
    }
    // (M30.6) the tabs and search names
    for (auto& t : m_tabItems) t.clear();
    m_searchNames.clear();
    for (size_t i = 0; i < m_items.size(); ++i) {
        m_tabItems[size_t(tabOf(items.item(m_items[i].item)))].push_back(int(i));
        std::string n = m_names[i];
        for (char& c : n) c = c == '_' ? ' ' : char(std::tolower(static_cast<unsigned char>(c)));
        m_searchNames.push_back(std::move(n));
    }
    static constexpr const char* kIcons[size_t(Tab::Count)] = {
        "bricks", "cyan_wool", "grass_block", "crafting_table", "redstone", "command_block", "compass",
        "diamond_pickaxe", "netherite_sword", "golden_apple", "iron_ingot", "pig_spawn_egg"};
    for (size_t t = 0; t < size_t(Tab::Count); ++t)
        if (const auto it = items.find(kIcons[t])) m_tabIcons[t] = {*it, 1};
    refilter();
}

void CreativeInventory::open() {
    m_open = true;
    m_carried = {};
}

void CreativeInventory::close() {
    m_open = false;
    m_carried = {};
}

int CreativeInventory::maxScrollRow() const {
    const int rows = (static_cast<int>(m_shown.size()) + kColumns - 1) / kColumns;
    return std::max(0, rows - kRows);
}

void CreativeInventory::scroll(double steps) {
    m_scrollRemainder += steps;
    const double whole = std::trunc(m_scrollRemainder);
    m_scrollRemainder -= whole;
    m_scrollRow = std::clamp(m_scrollRow - static_cast<int>(whole), 0, maxScrollRow());
}

void CreativeInventory::tabRect(Tab t, float left, float top, float& x, float& y) {
    const int i = int(t);
    const bool upper = i < kTopTabs;
    const int col = upper ? i : i - kTopTabs;
    // (the Search tab sits at the right end of the top row, as vanilla)
    x = left + (t == Tab::Search ? float(kWidth - kTabWidth) : float(col * (kTabWidth + 2)));
    y = upper ? top - float(kTabHeight) + 4.0f : top + float(kHeight) - 4.0f;
}

CreativeInventory::Hit CreativeInventory::hitTest(double mx, double my, int guiWidth,
                                                  int guiHeight) const {
    const double left = (guiWidth - kWidth) / 2, top = (guiHeight - kHeight) / 2;
    for (int t = 0; t < int(Tab::Count); ++t) {
        float tx = 0, ty = 0;
        tabRect(Tab(t), float(left), float(top), tx, ty);
        if (mx >= tx && mx < tx + kTabWidth && my >= ty && my < ty + kTabHeight &&
            !(my >= top && my < top + kHeight)) // (the panel wins where they overlap)
            return {Hit::TabButton, t};
    }
    const double x = mx - left, y = my - top;
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return {};
    auto cell = [&](double ox, double oy, int cols, int rows, int& col, int& row) {
        col = static_cast<int>(std::floor((x - ox) / kSlot));
        row = static_cast<int>(std::floor((y - oy) / kSlot));
        return x >= ox && y >= oy && col < cols && row < rows;
    };
    int col = 0, row = 0;
    if (cell(9, 18, kColumns, kRows, col, row)) {
        const int i = (m_scrollRow + row) * kColumns + col;
        if (i < static_cast<int>(m_shown.size())) return {Hit::Grid, m_shown[size_t(i)]};
        return {Hit::Panel, -1};
    }
    if (cell(9, 112, kColumns, 1, col, row)) return {Hit::HotbarSlot, col};
    return {Hit::Panel, -1};
}

void CreativeInventory::click(double mx, double my, int guiWidth, int guiHeight, Inventory& inventory) {
    const Hit h = hitTest(mx, my, guiWidth, guiHeight);
    switch (h.kind) {
    case Hit::Grid:
        // Creative: the grid is an infinite source; clicking it with something
        // carried deletes that and takes the clicked item.
        m_carried = m_items[size_t(h.index)];
        m_carried.count = static_cast<uint8_t>(world::itemRegistry().item(m_carried.item).maxStack);
        break;
    case Hit::HotbarSlot: {
        const world::ItemStack old = inventory.slot(h.index);
        inventory.setSlot(h.index, m_carried);
        m_carried = old;
        break;
    }
    case Hit::None: m_carried = {}; break; // outside the panel: drop it
    case Hit::Panel: break;
    case Hit::TabButton: selectTab(Tab(h.index)); break;
    }
}

void CreativeInventory::numberKey(int slot, double mx, double my, int guiWidth, int guiHeight,
                                  Inventory& inventory) {
    const Hit h = hitTest(mx, my, guiWidth, guiHeight);
    if (h.kind == Hit::Grid) { // a full stack, as vanilla's creative number keys
        world::ItemStack s = m_items[size_t(h.index)];
        s.count = static_cast<uint8_t>(world::itemRegistry().item(s.item).maxStack);
        inventory.setSlot(slot, s);
    }
    if (h.kind == Hit::HotbarSlot) { // swap two hotbar slots
        const world::ItemStack a = inventory.slot(h.index);
        inventory.setSlot(h.index, inventory.slot(slot));
        inventory.setSlot(slot, a);
    }
}

void CreativeInventory::draw(gfx::GuiBatch& b, const gfx::ItemIcons& icons,
                             const gfx::BlockModels& models, const Inventory& inventory,
                             int guiWidth, int guiHeight, double mx, double my) {
    // Dim the world behind (vanilla: gradient #C0101010 -> #D0101010).
    b.fill(0, 0, static_cast<float>(guiWidth), static_cast<float>(guiHeight),
           gfx::argb(0xC8101010));
    const float left = static_cast<float>((guiWidth - kWidth) / 2);
    const float top = static_cast<float>((guiHeight - kHeight) / 2);
    // Panel: black outline, bevel, body.
    b.fill(left + 1, top, kWidth - 2, kHeight, kEdge);
    b.fill(left, top + 1, kWidth, kHeight - 2, kEdge);
    b.fill(left + 1, top + 1, kWidth - 2, kHeight - 2, kBody);
    b.fill(left + 1, top + 1, kWidth - 3, 2, kLight);
    b.fill(left + 1, top + 1, 2, kHeight - 3, kLight);
    b.fill(left + 2, top + kHeight - 3, kWidth - 3, 2, kDark);
    b.fill(left + kWidth - 3, top + 2, 2, kHeight - 3, kDark);
    b.text(tabName(m_tab), left + 8, top + 6, gfx::argb(0xFF404040), /*shadow=*/false);
    // The tabs (M30.6): the open one joins the panel; the others sit behind it, darker.
    for (int t = 0; t < int(Tab::Count); ++t) {
        float tx = 0, ty = 0;
        tabRect(Tab(t), left, top, tx, ty);
        const bool active = Tab(t) == m_tab;
        const uint32_t body = active ? kBody : gfx::rgba(160, 160, 160);
        b.fill(tx, ty, kTabWidth, kTabHeight, kEdge);
        b.fill(tx + 1, ty + 1, kTabWidth - 2, kTabHeight - 2, body);
        if (active) { // (merged into the panel's edge)
            const bool upper = t < kTopTabs;
            b.fill(tx + 1, upper ? top : ty, kTabWidth - 2, 4, kBody);
        }
        icons.draw(b, models, m_tabIcons[size_t(t)], tx + 5, ty + (t < kTopTabs ? 6.0f : 7.0f), kIconGrassTint);
    }
    // The search box (Search tab): what has been typed, with a cursor.
    if (m_tab == Tab::Search) {
        const float bx = left + 81, by = top + 4;
        b.fill(bx, by, 89, 12, kEdge);
        b.fill(bx + 1, by + 1, 87, 10, gfx::rgba(0, 0, 0));
        b.text(m_query + "_", bx + 3, by + 2, gfx::argb(0xFFFFFFFF), false);
    }

    const Hit hover = hitTest(mx, my, guiWidth, guiHeight);
    for (int row = 0; row < kRows; ++row)
        for (int col = 0; col < kColumns; ++col) {
            const float x = left + 9 + col * kSlot - 1, y = top + 18 + row * kSlot - 1;
            slotFrame(b, x, y);
            const int i = (m_scrollRow + row) * kColumns + col;
            if (i < static_cast<int>(m_shown.size())) {
                const int item = m_shown[size_t(i)];
                icons.draw(b, models, m_items[size_t(item)], x + 1, y + 1, kIconGrassTint);
                if (hover.kind == Hit::Grid && hover.index == item) b.fill(x + 1, y + 1, 16, 16, kHover);
            }
        }
    for (int col = 0; col < Inventory::kHotbar; ++col) {
        const float x = left + 9 + col * kSlot - 1, y = top + 112 - 1;
        slotFrame(b, x, y);
        icons.draw(b, models, inventory.slot(col), x + 1, y + 1, kIconGrassTint);
        if (hover.kind == Hit::HotbarSlot && hover.index == col)
            b.fill(x + 1, y + 1, 16, 16, kHover);
    }
    // Scrollbar track and thumb (12x15).
    const float sx = left + 175, sy = top + 18;
    b.fill(sx - 1, sy - 1, 14, 112 - 18 + 2, kDark);
    b.fill(sx, sy, 12, 112 - 18, kSlotFill);
    const int maxRow = maxScrollRow();
    const float thumbY = sy + (maxRow > 0 ? (112.0f - 18 - 15) * m_scrollRow / maxRow : 0.0f);
    b.fill(sx, thumbY, 12, 15, maxRow > 0 ? kLight : kBody);

    // Tooltip for the hovered grid item or tab, then the carried item on the cursor.
    if ((hover.kind == Hit::Grid || hover.kind == Hit::TabButton) && m_carried.empty()) {
        const std::string name = hover.kind == Hit::Grid ? m_names[size_t(hover.index)] : tabName(Tab(hover.index));
        const float tx = static_cast<float>(mx) + 12, ty = static_cast<float>(my) - 12;
        const int w = b.textWidth(name);
        b.fill(tx - 3, ty - 3, static_cast<float>(w + 6), 14, gfx::argb(0xF0100010));
        b.fill(tx - 2, ty - 2, static_cast<float>(w + 4), 12, gfx::argb(0xFF2A0A5A));
        b.fill(tx - 1, ty - 1, static_cast<float>(w + 2), 10, gfx::argb(0xF0100010));
        b.text(name, tx, ty, gfx::argb(0xFFFFFFFF));
    }
    icons.draw(b, models, m_carried, static_cast<float>(mx) - 8, static_cast<float>(my) - 8,
               kIconGrassTint);
}

} // namespace mc::ui
