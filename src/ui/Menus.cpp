#include "ui/Menus.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iterator>

namespace mc::ui {

using gfx::argb;

namespace {

constexpr int kRowHeight = 36;

MenuAction title(Menu& m, MenuState& st, uint16_t dirt, const char* version) {
    m.tiledBackground(dirt);
    const float cx = float(m.width()) / 2.0f;
    const float top = float(m.height()) / 4.0f;
    // Our name in large letters (not vanilla's logo, which is Mojang's), a splash line.
    m.text("MinecraftClone", cx, top - 34.0f, argb(0xFFFFFFFF), true, 3.0f);
    const float pulse =
        1.0f + 0.06f * std::abs(std::sin(float(m.input().timeMs % 1000) / 1000.0f * 6.2832f));
    m.text(st.splash.c_str(), cx + 60.0f, top - 6.0f, argb(0xFFFFFF00), true, pulse);
    MenuAction a = MenuAction::None;
    if (m.button("Singleplayer", cx - 100.0f, top + 48.0f, 200.0f)) {
        st.screen = MenuScreen::WorldList;
        st.selected = st.worlds.empty() ? -1 : 0;
        st.scroll = 0;
    }
    if (m.button("Options...", cx - 100.0f, top + 72.0f, 98.0f)) {
        st.optionsBack = MenuScreen::Title;
        st.screen = MenuScreen::Options;
    }
    if (m.button("Quit Game", cx + 2.0f, top + 72.0f, 98.0f) || m.input().escape)
        a = MenuAction::Quit;
    char line[96];
    std::snprintf(line, sizeof(line), "MinecraftClone %s", version);
    m.text(line, 2.0f, float(m.height()) - 10.0f, argb(0xFFFFFFFF));
    const char* note = "Not affiliated with Mojang";
    m.text(note, float(m.width()) - 2.0f - float(m.batch().textWidth(note)),
           float(m.height()) - 10.0f, argb(0xFFFFFFFF));
    return a;
}

MenuAction worldList(Menu& m, MenuState& st, uint16_t dirt) {
    m.tiledBackground(dirt);
    const float cx = float(m.width()) / 2.0f;
    m.text("Select World", cx, 12.0f, argb(0xFFFFFFFF), true);
    // The list: rows of 36 pixels between the header and the buttons, scrolled by the
    // wheel; a click selects, a double click plays.
    const float listTop = 32.0f, listBottom = float(m.height()) - 64.0f;
    const int visible = std::max(1, int((listBottom - listTop) / kRowHeight));
    const int count = int(st.worlds.size());
    if (m.input().wheel != 0.0) st.scroll -= int(m.input().wheel);
    st.scroll = std::clamp(st.scroll, 0, std::max(0, count - visible));
    m.batch().fill(0, listTop - 2.0f, float(m.width()), listBottom - listTop + 4.0f,
                   gfx::rgba(0, 0, 0, 110));
    MenuAction a = MenuAction::None;
    const float rowX = cx - 110.0f, rowW = 220.0f;
    for (int i = st.scroll; i < count && i < st.scroll + visible; ++i) {
        const float y = listTop + float(i - st.scroll) * kRowHeight;
        const auto& w = st.worlds[size_t(i)];
        if (m.hovered(rowX, y, rowW, kRowHeight - 2) && m.input().click) {
            if (st.selected == i && st.lastClickRow == i && m.input().timeMs - st.lastClickMs < 300)
                a = MenuAction::PlayWorld;
            st.selected = i;
            st.lastClickRow = i;
            st.lastClickMs = m.input().timeMs;
        }
        if (st.selected == i) {
            m.batch().fill(rowX - 2.0f, y - 2.0f, rowW + 4.0f, kRowHeight, argb(0xFF808080));
            m.batch().fill(rowX - 1.0f, y - 1.0f, rowW + 2.0f, kRowHeight - 2.0f, argb(0xFF000000));
        }
        m.text(w.name.c_str(), rowX + 2.0f, y + 1.0f, argb(0xFFFFFFFF));
        char line[160];
        const std::time_t t = std::time_t(w.lastPlayed / 1000);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char date[32];
        std::strftime(date, sizeof(date), "%d/%m/%Y %H:%M", &tm);
        std::snprintf(line, sizeof(line), "%s (%s)", w.folder.c_str(), date);
        m.text(line, rowX + 2.0f, y + 12.0f, argb(0xFF808080));
        std::snprintf(line, sizeof(line), "%s Mode%s", w.survival ? "Survival" : "Creative",
                      w.flat ? ", Superflat" : "");
        m.text(line, rowX + 2.0f, y + 23.0f, argb(0xFF808080));
    }
    if (count == 0)
        m.text("No worlds yet - create one!", cx, listTop + 20.0f, argb(0xFFA0A0A0), true);
    const bool any = st.selected >= 0 && st.selected < count;
    const float b1 = float(m.height()) - 52.0f, b2 = float(m.height()) - 28.0f;
    if (m.button("Play Selected World", cx - 154.0f, b1, 150.0f, any)) a = MenuAction::PlayWorld;
    if (m.button("Create New World", cx + 4.0f, b1, 150.0f)) {
        st.screen = MenuScreen::CreateWorld;
        st.newName = "New World";
        st.newSeed.clear();
        st.newSurvival = true;
        st.newFlat = false;
        m.setFocus(0);
    }
    if (m.button("Delete", cx - 154.0f, b2, 150.0f, any)) st.screen = MenuScreen::ConfirmDelete;
    if (m.button("Cancel", cx + 4.0f, b2, 150.0f) || m.input().escape)
        st.screen = MenuScreen::Title;
    return a;
}

MenuAction createWorld(Menu& m, MenuState& st, uint16_t dirt) {
    m.tiledBackground(dirt);
    const float cx = float(m.width()) / 2.0f;
    m.text("Create New World", cx, 16.0f, argb(0xFFFFFFFF), true);
    m.text("World Name", cx - 100.0f, 40.0f, argb(0xFFA0A0A0));
    m.textField(st.newName, cx - 100.0f, 52.0f, 200.0f);
    m.text("Seed for the World Generator", cx - 100.0f, 80.0f, argb(0xFFA0A0A0));
    m.textField(st.newSeed, cx - 100.0f, 92.0f, 200.0f);
    m.text("Leave blank for a random seed", cx - 100.0f, 116.0f, argb(0xFF808080));
    if (m.button(st.newSurvival ? "Game Mode: Survival" : "Game Mode: Creative", cx - 100.0f,
                 132.0f, 200.0f))
        st.newSurvival = !st.newSurvival;
    if (m.button(st.newFlat ? "World Type: Superflat" : "World Type: Default", cx - 100.0f, 156.0f,
                 200.0f))
        st.newFlat = !st.newFlat;
    static constexpr const char* kDifficulty[4] = {"Difficulty: Peaceful", "Difficulty: Easy",
                                                   "Difficulty: Normal", "Difficulty: Hard"};
    if (m.button(kDifficulty[st.newDifficulty & 3], cx - 100.0f, 180.0f, 200.0f))
        st.newDifficulty = (st.newDifficulty + 1) & 3;
    MenuAction a = MenuAction::None;
    const float b = float(m.height()) - 28.0f;
    if (m.button("Create New World", cx - 154.0f, b, 150.0f) || m.input().enter)
        a = MenuAction::CreateWorld;
    if (m.button("Cancel", cx + 4.0f, b, 150.0f) || m.input().escape)
        st.screen = MenuScreen::WorldList;
    return a;
}

MenuAction confirmDelete(Menu& m, MenuState& st, uint16_t dirt) {
    m.tiledBackground(dirt, gfx::rgba(64, 32, 32));
    const float cx = float(m.width()) / 2.0f;
    m.text("Are you sure you want to delete this world?", cx, 70.0f, argb(0xFFFFFFFF), true);
    char line[160];
    const char* name = st.selected >= 0 && st.selected < int(st.worlds.size())
                           ? st.worlds[size_t(st.selected)].name.c_str()
                           : "";
    std::snprintf(line, sizeof(line), "'%s' will be lost forever! (A long time!)", name);
    m.text(line, cx, 90.0f, argb(0xFFA0A0A0), true);
    MenuAction a = MenuAction::None;
    if (m.button("Delete", cx - 154.0f, 120.0f, 150.0f)) a = MenuAction::DeleteWorld;
    if (m.button("Cancel", cx + 4.0f, 120.0f, 150.0f) || m.input().escape)
        st.screen = MenuScreen::WorldList;
    return a;
}

MenuAction optionsScreen(Menu& m, MenuState& st, GameOptions& o, uint16_t dirt) {
    if (st.optionsBack == MenuScreen::Pause)
        m.dim();
    else
        m.tiledBackground(dirt);
    const float cx = float(m.width()) / 2.0f;
    m.text("Options", cx, 15.0f, argb(0xFFFFFFFF), true);
    bool changed = false;
    char label[64];
    const float l = cx - 155.0f, r = cx + 5.0f;
    float y = 40.0f;
    // FOV 30..110 (vanilla labels 70 "Normal" and 110 "Quake Pro").
    float v = (o.fov - 30.0f) / 80.0f;
    const int fov = int(std::lround(o.fov));
    std::snprintf(label, sizeof(label),
                  fov == 70    ? "FOV: Normal"
                  : fov == 110 ? "FOV: Quake Pro"
                               : "FOV: %d",
                  fov);
    if (m.slider(label, l, y, 150.0f, v)) {
        o.fov = std::round(30.0f + v * 80.0f);
        changed = true;
    }
    v = float(o.renderDistance - 2) / 30.0f;
    std::snprintf(label, sizeof(label), "Render Distance: %d chunks", o.renderDistance);
    if (m.slider(label, r, y, 150.0f, v)) {
        o.renderDistance = 2 + int(std::lround(v * 30.0f));
        changed = true;
    }
    y += 24.0f;
    v = float(o.simulationDistance - 5) / 27.0f;
    std::snprintf(label, sizeof(label), "Simulation Distance: %d", o.simulationDistance);
    if (m.slider(label, l, y, 150.0f, v)) {
        o.simulationDistance = 5 + int(std::lround(v * 27.0f));
        changed = true;
    }
    v = o.sensitivity;
    std::snprintf(label, sizeof(label), "Sensitivity: %d%%",
                  int(std::lround(o.sensitivity * 200.0f)));
    if (m.slider(label, r, y, 150.0f, v)) {
        o.sensitivity = v;
        changed = true;
    }
    y += 24.0f;
    if (o.guiScale == 0)
        std::snprintf(label, sizeof(label), "GUI Scale: Auto");
    else
        std::snprintf(label, sizeof(label), "GUI Scale: %d", o.guiScale);
    if (m.button(label, l, y, 150.0f)) {
        o.guiScale = (o.guiScale + 1) % 5;
        changed = true;
    }
    v = o.masterVolume;
    if (o.masterVolume <= 0.0f)
        std::snprintf(label, sizeof(label), "Master Volume: OFF");
    else
        std::snprintf(label, sizeof(label), "Master Volume: %d%%",
                      int(std::lround(o.masterVolume * 100.0f)));
    if (m.slider(label, r, y, 150.0f, v)) {
        o.masterVolume = v;
        changed = true;
    }
    y += 24.0f;
    if (m.button(o.clouds ? "Clouds: Fancy" : "Clouds: OFF", l, y, 150.0f)) {
        o.clouds = !o.clouds;
        changed = true;
    }
    if (m.button(o.vsync ? "VSync: ON" : "VSync: OFF", r, y, 150.0f)) {
        o.vsync = !o.vsync;
        changed = true;
    }
    y += 24.0f;
    if (m.button(o.bobView ? "View Bobbing: ON" : "View Bobbing: OFF", l, y, 150.0f)) {
        o.bobView = !o.bobView;
        changed = true;
    }
    if (m.button("Done", cx - 100.0f, float(m.height()) - 28.0f, 200.0f) || m.input().escape) {
        st.screen = st.optionsBack;
        return MenuAction::OptionsClosed;
    }
    return changed ? MenuAction::OptionsChanged : MenuAction::None;
}

MenuAction pause(Menu& m, MenuState& st) {
    m.dim();
    const float cx = float(m.width()) / 2.0f;
    const float top = float(m.height()) / 4.0f;
    m.text("Game Menu", cx, top - 16.0f, argb(0xFFFFFFFF), true);
    if (m.button("Back to Game", cx - 102.0f, top + 8.0f, 204.0f) || m.input().escape)
        return MenuAction::Resume;
    if (m.button("Advancements", cx - 102.0f, top + 32.0f, 100.0f, st.adv != nullptr)) { // (M28.5c)
        st.screen = MenuScreen::Advancements;
        st.advScroll = 0;
    }
    if (m.button("Statistics", cx + 2.0f, top + 32.0f, 100.0f, st.stats != nullptr)) {
        st.screen = MenuScreen::Statistics;
        st.statsScroll = 0;
    }
    if (m.button("Options...", cx - 102.0f, top + 56.0f, 204.0f)) {
        st.optionsBack = MenuScreen::Pause;
        st.screen = MenuScreen::Options;
    }
    if (m.button("Save and Quit to Title", cx - 102.0f, top + 80.0f, 204.0f))
        return MenuAction::SaveAndQuit;
    return MenuAction::None;
}

MenuAction commandBlockScreen(Menu& m, MenuState& st) {
    m.dim();
    const float cx = float(m.width()) / 2.0f;
    const float top = float(m.height()) / 4.0f;
    m.text("Set Console Command for Block", cx, top - 16.0f, argb(0xFFFFFFFF), true);
    m.text("Console Command", cx - 150.0f, top, argb(0xFFA0A0A0));
    m.setFocus(0);
    m.textField(st.command, cx - 150.0f, top + 12.0f, 300.0f, 256);
    m.text("Previous Output", cx - 150.0f, top + 40.0f, argb(0xFFA0A0A0));
    m.text(st.commandOutput, cx - 150.0f, top + 52.0f, argb(0xFFFFFFFF));
    static constexpr const char* kModes[3] = {"Impulse", "Chain", "Repeat"};
    if (m.button(kModes[st.commandMode % 3], cx - 150.0f, top + 70.0f, 96.0f)) st.commandMode = (st.commandMode + 1) % 3;
    if (m.button(st.commandConditional ? "Conditional" : "Unconditional", cx - 48.0f, top + 70.0f, 96.0f))
        st.commandConditional = !st.commandConditional;
    if (m.button(st.commandAlways ? "Always Active" : "Needs Redstone", cx + 54.0f, top + 70.0f, 96.0f))
        st.commandAlways = !st.commandAlways;
    if (m.button("Done", cx - 150.0f, top + 100.0f, 146.0f) || m.input().enter) return MenuAction::CommandBlockDone;
    if (m.button("Cancel", cx + 4.0f, top + 100.0f, 146.0f) || m.input().escape) return MenuAction::Resume;
    return MenuAction::None;
}

// "minecraft:oak_log" -> "Oak Log" (our names come from the ids).
void prettyName(std::string_view id, char* out, size_t size) {
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    size_t n = 0;
    bool start = true;
    for (char c : id) {
        if (n + 1 >= size) break;
        if (c == '_') {
            out[n++] = ' ';
            start = true;
        } else {
            out[n++] = start && c >= 'a' && c <= 'z' ? char(c - 32) : c;
            start = false;
        }
    }
    out[n] = 0;
}

MenuAction statisticsScreen(Menu& m, MenuState& st) {
    // Vanilla's Statistics screen (wiki: Statistics): General, then per item and per mob.
    // Ours draws rows straight from the counters each frame (no lists kept).
    m.dim();
    const float cx = float(m.width()) / 2.0f;
    m.text("Statistics", cx, 8.0f, argb(0xFFFFFFFF), true);
    static constexpr const char* kTabs[3] = {"General", "Items", "Mobs"};
    for (int t = 0; t < 3; ++t)
        if (m.button(kTabs[t], cx - 154.0f + float(t) * 104.0f, 22.0f, 100.0f, st.statsTab != t)) {
            st.statsTab = t;
            st.statsScroll = 0;
        }
    const float listTop = 48.0f, listBottom = float(m.height()) - 32.0f, rowH = 11.0f;
    const int visible = std::max(1, int((listBottom - listTop) / rowH));
    m.batch().fill(0, listTop - 2.0f, float(m.width()), listBottom - listTop + 4.0f,
                   gfx::rgba(0, 0, 0, 110));
    const world::Statistics* s = st.stats;
    const float left = cx - 150.0f, right = cx + 150.0f;
    int row = 0; // rows counted so far (scrolled ones are skipped)
    char name[64], value[32];
    auto line = [&](const char* label, const char* text, uint32_t colour) {
        const int r = row++ - st.statsScroll;
        if (r < 0 || r >= visible) return;
        const float y = listTop + float(r) * rowH;
        m.text(label, left, y, colour);
        m.text(text, right - float(m.batch().textWidth(text)), y, colour);
    };
    if (s && st.statsTab == 0) {
        for (int i = 0; i < int(world::Stat::Count); ++i) {
            const auto stat = world::Stat(i);
            world::Statistics::formatValue(stat, s->get(stat), value, sizeof(value));
            std::snprintf(name, sizeof(name), "%.*s", int(world::Statistics::label(stat).size()),
                          world::Statistics::label(stat).data());
            line(name, value, row % 2 ? argb(0xFFFFFFFF) : argb(0xFFB0B0B0));
        }
    } else if (s && st.statsTab == 1) {
        line("Item: mined / crafted / used / broken / picked up / dropped", "", argb(0xFFFFFF55));
        const auto& items = world::itemRegistry();
        for (size_t it = 1; it < items.count(); ++it) {
            const auto id = world::ItemId(it);
            const world::BlockId b = items.item(id).block;
            const int64_t mined = b ? s->mined(b) : 0;
            int64_t any = mined;
            for (int k = 1; k < int(world::ItemStat::Count); ++k)
                any += s->item(world::ItemStat(k), id);
            if (any == 0) continue;
            prettyName(items.item(id).id, name, sizeof(name));
            std::snprintf(value, sizeof(value), "%lld / %lld / %lld / %lld / %lld / %lld",
                          (long long)mined, (long long)s->item(world::ItemStat::Crafted, id),
                          (long long)s->item(world::ItemStat::Used, id),
                          (long long)s->item(world::ItemStat::Broken, id),
                          (long long)s->item(world::ItemStat::PickedUp, id),
                          (long long)s->item(world::ItemStat::Dropped, id));
            line(name, value, row % 2 ? argb(0xFFFFFFFF) : argb(0xFFB0B0B0));
        }
    } else if (s) {
        for (int t = 0; t < int(world::MobType::Count); ++t) {
            const auto type = world::MobType(t);
            if (s->killed(type) == 0 && s->killedBy(type) == 0) continue;
            prettyName(world::mobInfo(type).id, name, sizeof(name));
            std::snprintf(value, sizeof(value), "killed %lld, killed you %lld",
                          (long long)s->killed(type), (long long)s->killedBy(type));
            line(name, value, row % 2 ? argb(0xFFFFFFFF) : argb(0xFFB0B0B0));
        }
    }
    if (row == 0 || (st.statsTab == 1 && row == 1))
        m.text("Nothing yet", cx, listTop + 20.0f, argb(0xFFA0A0A0), true);
    if (m.input().wheel != 0.0) st.statsScroll -= int(m.input().wheel) * 3;
    st.statsScroll = std::clamp(st.statsScroll, 0, std::max(0, row - visible));
    if (m.button("Done", cx - 100.0f, float(m.height()) - 26.0f, 200.0f) || m.input().escape)
        st.screen = MenuScreen::Pause;
    return MenuAction::None;
}

MenuAction advancementsScreen(Menu& m, MenuState& st) {
    // Vanilla's Advancements screen (wiki: Advancement) is a tree per tab; ours lists each
    // tab's advancements in order: made ones lit (frame colour), the rest grey.
    m.dim();
    const float cx = float(m.width()) / 2.0f;
    const auto all = world::advancements();
    char head[64];
    std::snprintf(head, sizeof(head), "Advancements (%d/%d)", st.adv ? st.adv->doneCount() : 0,
                  int(all.size()));
    m.text(head, cx, 8.0f, argb(0xFFFFFFFF), true);
    static constexpr const char* kTabs[5] = {"Minecraft", "Nether", "The End", "Adventure",
                                             "Husbandry"};
    for (int t = 0; t < 5; ++t)
        if (m.button(kTabs[t], cx - 160.0f + float(t) * 64.0f, 22.0f, 62.0f, st.advTab != t)) {
            st.advTab = t;
            st.advScroll = 0;
        }
    const float listTop = 48.0f, listBottom = float(m.height()) - 32.0f, rowH = 11.0f;
    const int visible = std::max(1, int((listBottom - listTop) / rowH));
    m.batch().fill(0, listTop - 2.0f, float(m.width()), listBottom - listTop + 4.0f,
                   gfx::rgba(0, 0, 0, 110));
    const float left = cx - 150.0f;
    int row = 0;
    char text[128];
    auto line = [&](const char* s, float indent, uint32_t colour) {
        const int r = row++ - st.advScroll;
        if (r < 0 || r >= visible) return;
        m.text(s, left + indent, listTop + float(r) * rowH, colour);
    };
    for (size_t i = 0; i < all.size(); ++i) {
        const world::Advancement& a = all[i];
        if (int(a.tab) != st.advTab) continue;
        const bool made = st.adv && st.adv->done(int(i));
        // Vanilla frames: task (square), goal (rounded), challenge (spiked, purple title).
        const char* mark = a.frame == world::AdvFrame::Challenge ? "*"
                           : a.frame == world::AdvFrame::Goal    ? "o"
                                                                 : "-";
        std::snprintf(text, sizeof(text), "%s %s %.*s", made ? "[x]" : "[ ]", mark,
                      int(a.title.size()), a.title.data());
        line(text, 0.0f,
             !made                                   ? argb(0xFF909090)
             : a.frame == world::AdvFrame::Challenge ? argb(0xFFFF55FF)
                                                     : argb(0xFFFFFF55));
        std::snprintf(text, sizeof(text), "%.*s", int(a.description.size()), a.description.data());
        line(text, 24.0f, made ? argb(0xFF55FF55) : argb(0xFF707070));
    }
    if (m.input().wheel != 0.0) st.advScroll -= int(m.input().wheel) * 3;
    st.advScroll = std::clamp(st.advScroll, 0, std::max(0, row - visible));
    if (m.button("Done", cx - 100.0f, float(m.height()) - 26.0f, 200.0f) || m.input().escape)
        st.screen = MenuScreen::Pause;
    return MenuAction::None;
}

} // namespace

MenuAction drawMenu(Menu& menu, MenuState& state, GameOptions& options, uint16_t dirtSprite,
                    const char* version) {
    switch (state.screen) {
    case MenuScreen::Title:
        return title(menu, state, dirtSprite, version);
    case MenuScreen::WorldList:
        return worldList(menu, state, dirtSprite);
    case MenuScreen::CreateWorld:
        return createWorld(menu, state, dirtSprite);
    case MenuScreen::ConfirmDelete:
        return confirmDelete(menu, state, dirtSprite);
    case MenuScreen::Options:
        return optionsScreen(menu, state, options, dirtSprite);
    case MenuScreen::Pause:
        return pause(menu, state);
    case MenuScreen::Statistics:
        return statisticsScreen(menu, state);
    case MenuScreen::Advancements:
        return advancementsScreen(menu, state);
    case MenuScreen::CommandBlock:
        return commandBlockScreen(menu, state);
    case MenuScreen::None:
        break;
    }
    return MenuAction::None;
}

const char* splashText(uint32_t random) {
    static constexpr const char* kSplashes[] = {
        "Made from scratch!", "Now with weather!",        "C++20!",
        "20 ticks a second!", "Cubes all the way!",       "Original textures!",
        "Paletted!",          "Also try the real thing!", "Floor division!",
        "No Mojang code!",    "Learn how it works!",      "Synthesized sounds!",
    };
    return kSplashes[random % std::size(kSplashes)];
}

} // namespace mc::ui
