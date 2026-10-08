#include "world/ChunkSerializer.h"

#include "core/Log.h"
#include "world/ArmorTrims.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/ItemContainers.h"
#include "world/LevelData.h"
#include "world/Potions.h"
#include "world/RecipeIds.h"
#include "world/Villagers.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <unordered_map>

namespace mc::world {

ChunkSnapshot ChunkSnapshot::of(const Chunk& chunk, int64_t gameTime) {
    ChunkSnapshot s;
    s.pos = chunk.pos();
    s.gameTime = gameTime;
    s.biomes = chunk.biomes();
    s.furnaces = chunk.furnaces();
    s.chests = chunk.chests();
    s.spawners = chunk.spawners();
    s.brewing = chunk.brewingStands();
    s.signs = chunk.signs();
    s.campfires = chunk.campfires();
    s.beacons = chunk.beacons();
    s.jukeboxes = chunk.jukeboxes();
    s.comparators = chunk.comparators();
    s.hoppers = chunk.hoppers();
    s.dispensers = chunk.dispensers();
    s.mobs = chunk.mobs();
    s.blockTicks = chunk.blockTicks();
    if (!chunk.ticksRelative) // saved as delays (vanilla "t")
        for (auto& t : s.blockTicks)
            t.time -= gameTime;
    s.height = chunk.height();
    for (int i = 0; i < chunk.sectionCount(); ++i) {
        s.sections[size_t(i)] = chunk.shareSection(i);
        s.light[size_t(i)] = chunk.light(i);
    }
    return s;
}

namespace {

// The four heightmaps vanilla stores for a full chunk (wiki: Heightmap).
struct Heightmaps {
    static constexpr int kCount = 4;
    static constexpr const char* kNames[kCount] = {"MOTION_BLOCKING", "MOTION_BLOCKING_NO_LEAVES", "OCEAN_FLOOR",
                                                   "WORLD_SURFACE"};
    std::array<std::array<int, 256>, kCount> values{}; // height above the bottom; 0 = none
};

// Per-state flags for heightmaps, built once: 1 blocks motion (collides), 2 fluid,
// 4 leaves.
const std::vector<uint8_t>& heightmapFlags() {
    static const std::vector<uint8_t> flags = [] {
        const auto& reg = blockRegistry();
        std::vector<uint8_t> f(reg.stateCount(), 0);
        for (size_t i = 0; i < f.size(); ++i) {
            const auto s = static_cast<BlockStateId>(i);
            const BlockId b = reg.blockOf(s);
            f[i] = uint8_t((reg.collides(s) ? 1 : 0) | (b == blocks::Water || b == blocks::Lava ? 2 : 0) |
                           (reg.block(b).id.ends_with("_leaves") ? 4 : 0));
        }
        return f;
    }();
    return flags;
}

// Each column from the top down, stopping once all four maps have their block.
// `all` holds the chunk's states, section after section (section index order).
void computeHeightmaps(const std::vector<BlockStateId>& all, int height, Heightmaps& h) {
    const auto& flags = heightmapFlags();
    for (int column = 0; column < 256; ++column) {
        const int x = column & 15, z = column >> 4;
        int found = 0;
        for (int y = height - 1; y >= 0 && found != 15; --y) {
            const BlockStateId s = all[size_t((y >> 4) * Section::kVolume + Section::index(x, y & 15, z))];
            if (s == 0) continue;
            const uint8_t f = flags[s];
            const bool blocking = (f & 3) != 0;
            auto mark = [&](int bit, bool match) {
                if (match && !(found & (1 << bit))) {
                    h.values[size_t(bit)][size_t(column)] = y + 1;
                    found |= 1 << bit;
                }
            };
            mark(0, blocking);                  // MOTION_BLOCKING
            mark(1, blocking && !(f & 4));      // MOTION_BLOCKING_NO_LEAVES
            mark(2, (f & 1) != 0);              // OCEAN_FLOOR
            mark(3, true);                      // WORLD_SURFACE
        }
    }
}

// Packed like block states without spanning longs: ceil(log2(height + 1)) bits each
// (9 for 384 and 256), 64 / bits per long (7), 256 entries (37 longs).
std::vector<int64_t> packHeightmap(const std::array<int, 256>& values, const HeightRange& height) {
    const int bits = int(std::bit_width(uint32_t(height.height)));
    const int perLong = 64 / bits;
    std::vector<int64_t> data((256 + perLong - 1) / perLong, 0);
    for (int i = 0; i < 256; ++i)
        data[size_t(i / perLong)] |= int64_t(uint64_t(values[size_t(i)]) << ((i % perLong) * bits));
    return data;
}

// "minecraft:oak_log[axis=x]" -> {Name: "minecraft:oak_log", Properties: {axis: "x"}}
nbt::Compound paletteEntry(const std::string& text) {
    nbt::Compound e;
    const size_t open = text.find('[');
    e.put("Name", text.substr(0, open));
    if (open != std::string::npos) {
        nbt::Compound props;
        const std::string_view list(text.data() + open + 1, text.size() - open - 2);
        size_t start = 0;
        while (start < list.size()) {
            size_t end = list.find(',', start);
            if (end == std::string_view::npos) end = list.size();
            const std::string_view kv = list.substr(start, end - start);
            const size_t eq = kv.find('=');
            if (eq != std::string_view::npos)
                props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
            start = end + 1;
        }
        e.put("Properties", std::move(props));
    }
    return e;
}

std::string paletteText(const nbt::Compound& e) {
    const std::string* name = e.string("Name");
    if (!name) return {};
    std::string text = *name;
    if (const nbt::Compound* props = e.compound("Properties"); props && !props->entries.empty()) {
        text += '[';
        bool first = true;
        for (const auto& p : props->entries) {
            const std::string* v = p.value.get<std::string>();
            if (!v) continue;
            if (!first) text += ',';
            first = false;
            text += p.name + '=' + *v;
        }
        text += ']';
    }
    return text;
}

std::vector<int8_t> nibbles(const LightLayer& layer) {
    std::vector<int8_t> out(2048);
    for (int i = 0; i < 2048; ++i) {
        const int lo = layer.get(2 * i), hi = layer.get(2 * i + 1);
        out[size_t(i)] = static_cast<int8_t>(lo | (hi << 4));
    }
    return out;
}

} // namespace

namespace {

// An item stack as vanilla 1.20.5+ saves it: id, count, components.
nbt::Compound itemNbt(const ItemStack& s, int slot) {
    nbt::Compound c;
    if (slot >= 0) c.put("Slot", static_cast<int8_t>(slot)); // (no Slot inside a container component)
    c.put("id", itemRegistry().item(s.item).id);
    c.put("count", int32_t{s.count});
    nbt::Compound components;
    if (s.damage) components.put("minecraft:damage", int32_t{s.damage});
    if (s.state) { // exact block state (vanilla's minecraft:block_state component)
        nbt::Compound props;
        const std::string text = blockRegistry().toString(s.state);
        if (const size_t open = text.find('['); open != std::string::npos) {
            std::string_view list(text.data() + open + 1, text.size() - open - 2);
            while (!list.empty()) {
                const size_t comma = std::min(list.find(','), list.size());
                const std::string_view kv = list.substr(0, comma);
                if (const size_t eq = kv.find('='); eq != std::string_view::npos)
                    props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
                list.remove_prefix(std::min(comma + 1, list.size()));
            }
        }
        components.put("minecraft:block_state", std::move(props));
    }
    if (isEnchanted(s)) { // 1.21.5+: minecraft:enchantments is a plain id -> level map
        nbt::Compound ench;
        for (const uint16_t v : s.enchantments)
            if (v) ench.put(std::string(enchantmentInfo(Enchantment(v >> 8)).id), int32_t(v & 0xFF));
        components.put(itemRegistry().item(s.item).id == "minecraft:enchanted_book" ? "minecraft:stored_enchantments"
                                                                                  : "minecraft:enchantments",
                       std::move(ench));
    }
    if (s.repairCost) components.put("minecraft:repair_cost", int32_t{s.repairCost});
    if (s.potion) { // 1.20.5+ potion_contents { potion: "minecraft:<id>" }
        nbt::Compound contents;
        contents.put("potion", "minecraft:" + std::string(potionInfo(static_cast<Potion>(s.potion)).id));
        components.put("minecraft:potion_contents", std::move(contents));
    }
    if (s.trim) { // 1.20.5+ minecraft:trim {pattern, material}
        const int pattern = s.trim >> 8, material = s.trim & 0xFF;
        if (pattern >= 1 && pattern <= int(std::size(kTrimPatterns)) && material >= 1 &&
            material <= int(std::size(kTrimMaterials))) {
            nbt::Compound trim;
            trim.put("pattern", "minecraft:" + std::string(kTrimPatterns[pattern - 1]));
            trim.put("material", "minecraft:" + std::string(kTrimMaterials[material - 1].id));
            components.put("minecraft:trim", std::move(trim));
        }
    }
    if (s.contents) { // 1.20.5+ minecraft:container: [{slot: int, item: {...}}] (shulker boxes)
        const ItemContents slots = itemContents(s.contents);
        std::vector<nbt::Tag> list;
        for (int i = 0; i < int(slots.size()); ++i) {
            if (slots[size_t(i)].empty()) continue;
            nbt::Compound entry;
            entry.put("slot", int32_t{i});
            entry.put("item", itemNbt(slots[size_t(i)], -1));
            list.emplace_back(std::move(entry));
        }
        components.put("minecraft:container", nbt::listOf(nbt::TagType::Compound, std::move(list)));
    }
    if (!components.entries.empty()) c.put("components", std::move(components));
    return c;
}

ItemStack itemFromNbt(const nbt::Compound& c) {
    const std::string* id = c.string("id");
    const auto item = id ? itemRegistry().find(*id) : std::nullopt;
    if (!item) return {};
    const int maxStack = std::max<int>(1, itemRegistry().item(*item).maxStack);
    ItemStack s{*item, static_cast<uint8_t>(std::clamp<int64_t>(c.integer("count").value_or(1), 1, maxStack))};
    if (const nbt::Compound* comps = c.compound("components")) {
        s.damage = static_cast<uint16_t>(comps->integer("minecraft:damage").value_or(0));
        const ItemDef& def = itemRegistry().item(*item);
        if (const nbt::Compound* props = comps->compound("minecraft:block_state"); props && def.block) {
            BlockStateId st = blockRegistry().defaultState(def.block);
            for (const auto& p : props->entries)
                if (const std::string* v = p.value.get<std::string>())
                    st = blockRegistry().with(st, p.name, *v).value_or(st);
            if (st != blockRegistry().defaultState(def.block)) s.state = st;
        }
        for (const char* key : {"minecraft:enchantments", "minecraft:stored_enchantments"})
            if (const nbt::Compound* ench = comps->compound(key)) {
                // (pre-1.21.5 saves nest them under "levels")
                const nbt::Compound* levels = ench->compound("levels") ? ench->compound("levels") : ench;
                for (const auto& e : levels->entries) {
                    const auto kind = findEnchantment(e.name);
                    const auto lvl = levels->integer(e.name);
                    if (!kind || !lvl || !setEnchantment(s, *kind, int(std::clamp<int64_t>(*lvl, 1, 255)))) {
                        static bool logged = false; // (unknown kinds: Mending, curses...)
                        if (!logged) MC_LOG_WARN("Dropping enchantment %s (not supported yet)", e.name.c_str());
                        logged = true;
                    }
                }
            }
        s.repairCost = static_cast<uint8_t>(std::clamp<int64_t>(comps->integer("minecraft:repair_cost").value_or(0), 0, 255));
        if (const nbt::Compound* pc = comps->compound("minecraft:potion_contents"))
            if (const std::string* pid = pc->string("potion"))
                if (const auto p = findPotion(*pid)) s.potion = static_cast<uint8_t>(*p);
        if (const nbt::Compound* trim = comps->compound("minecraft:trim")) {
            const std::string* pattern = trim->string("pattern");
            const std::string* material = trim->string("material");
            const auto p = pattern ? findTrimPattern(*pattern) : std::nullopt;
            const auto m = material ? findTrimMaterial(*material) : std::nullopt;
            if (p && m) s.trim = static_cast<uint16_t>(*p << 8 | *m);
        }
        if (const nbt::List* box = comps->list("minecraft:container")) {
            ItemContents slots{};
            for (const nbt::Tag& t : box->items)
                if (const nbt::Compound* entry = t.get<nbt::Compound>()) {
                    const auto slot = entry->integer("slot").value_or(-1);
                    const nbt::Compound* inner = entry->compound("item");
                    if (inner && slot >= 0 && slot < int(slots.size())) slots[size_t(slot)] = itemFromNbt(*inner);
                }
            s.contents = addItemContents(slots);
        }
    }
    return s;
}

} // namespace

nbt::Compound itemToNbt(const ItemStack& s, int slot) { return itemNbt(s, slot); }
ItemStack itemFromNbtPublic(const nbt::Compound& c) { return itemFromNbt(c); }

nbt::Compound chunkToNbt(const ChunkSnapshot& chunk) {
    const auto& reg = blockRegistry();
    nbt::Compound root;
    root.put("DataVersion", kDataVersion);
    root.put("xPos", chunk.pos.x);
    root.put("zPos", chunk.pos.z);
    // The lowest section's Y: -4 in the Overworld, 0 in the Nether and End (vanilla).
    root.put("yPos", int32_t{chunk.height.minSection()});
    root.put("Status", std::string("minecraft:full")); // 1.21.11 (renamed "status" only in 26.4)
    root.put("LastUpdate", chunk.gameTime); // game tick of this save
    root.put("InhabitedTime", int64_t{0});
    root.put("clone_format", kCloneFormat); // our tag (vanilla ignores it): see kCloneFormat
    bool lit = true;
    for (int s = 0; s < chunk.height.sections(); ++s)
        lit = lit && chunk.light[size_t(s)] != nullptr;
    root.put("isLightOn", static_cast<int8_t>(lit ? 1 : 0));

    std::vector<nbt::Tag> sections;
    std::vector<BlockStateId> states(Section::kVolume);
    Heightmaps heights;
    std::vector<BlockStateId> all(size_t(chunk.height.sections()) * Section::kVolume); // (IO thread)
    std::vector<BlockStateId> palette;
    std::unordered_map<BlockStateId, uint32_t> paletteIndex;
    for (int s = 0; s < chunk.height.sections(); ++s) {
        nbt::Compound sec;
        sec.put("Y", static_cast<int8_t>(chunk.height.minSection() + s));
        // Block states: local palette in order of first appearance.
        chunk.sections[size_t(s)]->copyTo(states.data());
        std::copy(states.begin(), states.end(), all.begin() + std::ptrdiff_t(s) * Section::kVolume);
        palette.clear();
        paletteIndex.clear();
        for (BlockStateId st : states)
            if (paletteIndex.try_emplace(st, uint32_t(palette.size())).second) palette.push_back(st);
        nbt::Compound blockStates;
        std::vector<nbt::Tag> entries;
        for (BlockStateId st : palette)
            entries.emplace_back(paletteEntry(reg.toString(st)));
        blockStates.put("palette", nbt::listOf(nbt::TagType::Compound, std::move(entries)));
        if (palette.size() > 1) {
            const int bits = std::max(4, int(std::bit_width(palette.size() - 1)));
            const int perLong = 64 / bits;
            std::vector<int64_t> data((Section::kVolume + perLong - 1) / perLong, 0);
            for (int i = 0; i < Section::kVolume; ++i) {
                const uint64_t v = paletteIndex[states[size_t(i)]];
                data[size_t(i / perLong)] |= static_cast<int64_t>(v << ((i % perLong) * bits));
            }
            blockStates.put("data", std::move(data));
        }
        sec.put("block_states", std::move(blockStates));
        {
            const ChunkBiomes& cb = chunk.biomes ? *chunk.biomes : *Chunk::defaultBiomes();
            std::vector<Biome> bpal;
            uint8_t indexOf[256] = {};
            for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                const Biome b = cb.cells[size_t(s * ChunkBiomes::kPerSection + i)];
                if (std::find(bpal.begin(), bpal.end(), b) == bpal.end()) {
                    indexOf[static_cast<int>(b)] = static_cast<uint8_t>(bpal.size());
                    bpal.push_back(b);
                }
            }
            std::vector<nbt::Tag> names;
            for (Biome b : bpal)
                names.emplace_back(std::string(biomeInfo(b).id));
            nbt::Compound biomes;
            biomes.put("palette", nbt::listOf(nbt::TagType::String, std::move(names)));
            if (bpal.size() > 1) {
                const int bits = int(std::bit_width(bpal.size() - 1));
                const int perLong = 64 / bits;
                std::vector<int64_t> data((ChunkBiomes::kPerSection + perLong - 1) / perLong, 0);
                for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                    const uint64_t v =
                        indexOf[static_cast<int>(cb.cells[size_t(s * ChunkBiomes::kPerSection + i)])];
                    data[size_t(i / perLong)] |= static_cast<int64_t>(v << ((i % perLong) * bits));
                }
                biomes.put("data", std::move(data));
            }
            sec.put("biomes", std::move(biomes));
        }
        if (const auto& l = chunk.light[size_t(s)]) {
            // SkyLight is always written (an omitted one means "same as the section
            // above"); BlockLight is omitted when no light reaches the section.
            sec.put("SkyLight", nibbles(l->sky));
            if (!(l->block.isUniform() && l->block.uniformValue() == 0))
                sec.put("BlockLight", nibbles(l->block));
        }
        sections.emplace_back(std::move(sec));
    }
    root.put("sections", nbt::listOf(nbt::TagType::Compound, std::move(sections)));
    // Heightmaps of a full chunk (wiki: Chunk format › Heightmaps): per column the
    // height above the bottom of the first block from the top that matches, 0 if none.
    computeHeightmaps(all, chunk.height.height, heights);
    nbt::Compound maps;
    for (int k = 0; k < Heightmaps::kCount; ++k)
        maps.put(std::string(Heightmaps::kNames[k]), packHeightmap(heights.values[size_t(k)], chunk.height));
    root.put("Heightmaps", std::move(maps));
    // Empty lists vanilla always writes for full chunks.
    std::vector<nbt::Tag> post;
    for (int s = 0; s < chunk.height.sections(); ++s)
        post.emplace_back(nbt::listOf(nbt::TagType::Short, {}));
    root.put("PostProcessing", nbt::listOf(nbt::TagType::List, std::move(post)));
    nbt::Compound structures;
    structures.put("references", nbt::Compound{});
    structures.put("starts", nbt::Compound{});
    root.put("structures", std::move(structures));
    // Block entities (wiki: Chunk format › block_entities; Furnace › Block data, 1.21.1).
    std::vector<nbt::Tag> entities;
    for (const auto& f : chunk.furnaces) {
        nbt::Compound e;
        e.put("id", std::string(f.data.kind == 1 ? "minecraft:smoker" : f.data.kind == 2 ? "minecraft:blast_furnace"
                                                                                      : "minecraft:furnace"));
        e.put("x", int32_t{chunk.pos.x * 16 + f.x});
        e.put("y", int32_t{f.y});
        e.put("z", int32_t{chunk.pos.z * 16 + f.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        const ItemStack* slots[3] = {&f.data.input, &f.data.fuel, &f.data.output};
        for (int i = 0; i < 3; ++i)
            if (!slots[i]->empty()) items.emplace_back(itemNbt(*slots[i], i));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        // 1.21.4+ names (wiki: Furnace › Block data; before: BurnTime, CookTime, CookTimeTotal).
        e.put("lit_time_remaining", static_cast<int16_t>(f.data.burnLeft));
        e.put("lit_total_time", static_cast<int16_t>(f.data.burnDuration));
        e.put("cooking_time_spent", static_cast<int16_t>(f.data.cookTime));
        e.put("cooking_total_time", int16_t{200});
        // Experience stored by smelting (vanilla keeps per-recipe counts in RecipesUsed;
        // we keep the total in our own tag, which vanilla ignores).
        nbt::Compound used; // RecipesUsed: recipe id -> times used (vanilla)
        for (const FurnaceData::RecipeUse& u : f.data.recipesUsed)
            if (u.recipe != kNoRecipe) used.put(recipeIdName(u.recipe), u.count);
        e.put("RecipesUsed", std::move(used));
        entities.emplace_back(std::move(e));
    }
    for (const auto& jb : chunk.jukeboxes) { // wiki: Jukebox › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:jukebox"));
        e.put("x", int32_t{chunk.pos.x * 16 + jb.x});
        e.put("y", int32_t{jb.y});
        e.put("z", int32_t{chunk.pos.z * 16 + jb.z});
        e.put("keepPacked", int8_t{0});
        if (!jb.data.record.empty()) {
            nbt::Compound rec = itemNbt(jb.data.record, -1);
            e.put("RecordItem", std::move(rec));
            e.put("ticks_since_song_started", int64_t{jb.data.playing ? jb.data.ticks : 0});
        }
        entities.emplace_back(std::move(e));
    }
    for (const auto& bc : chunk.beacons) { // wiki: Beacon › Block data; Conduit
        nbt::Compound e;
        e.put("id", std::string(bc.data.conduit ? "minecraft:conduit" : "minecraft:beacon"));
        e.put("x", int32_t{chunk.pos.x * 16 + bc.x});
        e.put("y", int32_t{bc.y});
        e.put("z", int32_t{chunk.pos.z * 16 + bc.z});
        e.put("keepPacked", int8_t{0});
        if (!bc.data.conduit) {
            e.put("Levels", int32_t{bc.data.levels});
            if (bc.data.primary)
                e.put("primary_effect", std::string(effectInfo(static_cast<Effect>(bc.data.primary)).id));
            if (bc.data.secondary)
                e.put("secondary_effect", std::string(effectInfo(static_cast<Effect>(bc.data.secondary)).id));
        }
        entities.emplace_back(std::move(e));
    }
    for (const auto& cf : chunk.campfires) { // wiki: Campfire › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:campfire"));
        e.put("x", int32_t{chunk.pos.x * 16 + cf.x});
        e.put("y", int32_t{cf.y});
        e.put("z", int32_t{chunk.pos.z * 16 + cf.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        std::vector<int32_t> times, totals;
        for (int i = 0; i < 4; ++i) {
            if (!cf.data.items[size_t(i)].empty()) items.emplace_back(itemNbt(cf.data.items[size_t(i)], i));
            times.push_back(cf.data.cookTime[size_t(i)]);
            totals.push_back(600);
        }
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        e.put("CookingTimes", std::move(times));
        e.put("CookingTotalTimes", std::move(totals));
        entities.emplace_back(std::move(e));
    }
    for (const auto& sg : chunk.signs) { // wiki: Sign › Block data (front_text / back_text)
        nbt::Compound e;
        e.put("id", std::string(sg.data.hanging ? "minecraft:hanging_sign" : "minecraft:sign"));
        e.put("x", int32_t{chunk.pos.x * 16 + sg.x});
        e.put("y", int32_t{sg.y});
        e.put("z", int32_t{chunk.pos.z * 16 + sg.z});
        e.put("keepPacked", int8_t{0});
        auto side = [&](const SignData::Side& sd) {
            nbt::Compound t;
            std::vector<nbt::Tag> lines; // (1.21.5+: text components as plain strings)
            for (const auto& l : sd.lines)
                lines.emplace_back(std::string(l.data()));
            t.put("messages", nbt::listOf(nbt::TagType::String, std::move(lines)));
            t.put("color", std::string(kDyeColours[sd.colour & 15]));
            t.put("has_glowing_text", int8_t(sd.glowing ? 1 : 0));
            return t;
        };
        e.put("front_text", side(sg.data.front));
        e.put("back_text", side(sg.data.back));
        e.put("is_waxed", int8_t(sg.data.waxed ? 1 : 0));
        entities.emplace_back(std::move(e));
    }
    for (const auto& br : chunk.brewing) { // wiki: Brewing Stand › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:brewing_stand"));
        e.put("x", int32_t{chunk.pos.x * 16 + br.x});
        e.put("y", int32_t{br.y});
        e.put("z", int32_t{chunk.pos.z * 16 + br.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items; // Slot 0-2 bottles, 3 ingredient, 4 fuel
        for (int i = 0; i < 3; ++i)
            if (!br.data.bottles[size_t(i)].empty()) items.emplace_back(itemNbt(br.data.bottles[size_t(i)], i));
        if (!br.data.ingredient.empty()) items.emplace_back(itemNbt(br.data.ingredient, 3));
        if (!br.data.fuel.empty()) items.emplace_back(itemNbt(br.data.fuel, 4));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        e.put("BrewTime", int16_t(br.data.brewTime));
        e.put("Fuel", int8_t(br.data.fuelLeft));
        entities.emplace_back(std::move(e));
    }
    for (const auto& h : chunk.hoppers) { // wiki: Hopper › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:hopper"));
        e.put("x", int32_t{chunk.pos.x * 16 + h.x});
        e.put("y", int32_t{h.y});
        e.put("z", int32_t{chunk.pos.z * 16 + h.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        for (int i = 0; i < 5; ++i)
            if (!h.data.items[size_t(i)].empty()) items.emplace_back(itemNbt(h.data.items[size_t(i)], i));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        e.put("TransferCooldown", int32_t(h.data.cooldown));
        entities.emplace_back(std::move(e));
    }
    for (const auto& d : chunk.dispensers) { // wiki: Dispenser, Dropper › Block data
        nbt::Compound e;
        e.put("id", std::string(d.data.dropper ? "minecraft:dropper" : "minecraft:dispenser"));
        e.put("x", int32_t{chunk.pos.x * 16 + d.x});
        e.put("y", int32_t{d.y});
        e.put("z", int32_t{chunk.pos.z * 16 + d.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        for (int i = 0; i < 9; ++i)
            if (!d.data.items[size_t(i)].empty()) items.emplace_back(itemNbt(d.data.items[size_t(i)], i));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        entities.emplace_back(std::move(e));
    }
    for (const auto& cp : chunk.comparators) { // wiki: Redstone Comparator › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:comparator"));
        e.put("x", int32_t{chunk.pos.x * 16 + cp.x});
        e.put("y", int32_t{cp.y});
        e.put("z", int32_t{chunk.pos.z * 16 + cp.z});
        e.put("keepPacked", int8_t{0});
        e.put("OutputSignal", int32_t(cp.data.output));
        entities.emplace_back(std::move(e));
    }
    for (const auto& sp : chunk.spawners) { // wiki: Monster Spawner › Block data
        nbt::Compound e;
        e.put("id", std::string("minecraft:mob_spawner"));
        e.put("x", int32_t{chunk.pos.x * 16 + sp.x});
        e.put("y", int32_t{sp.y});
        e.put("z", int32_t{chunk.pos.z * 16 + sp.z});
        e.put("keepPacked", int8_t{0});
        e.put("Delay", int16_t{sp.data.delay});
        nbt::Compound entity, spawnData;
        entity.put("id", std::string(mobInfo(sp.data.mob).id));
        spawnData.put("entity", std::move(entity));
        e.put("SpawnData", std::move(spawnData));
        // Vanilla's defaults, written so vanilla keeps the same behaviour.
        e.put("MinSpawnDelay", int16_t{200});
        e.put("MaxSpawnDelay", int16_t{800});
        e.put("SpawnCount", int16_t{4});
        e.put("MaxNearbyEntities", int16_t{6});
        e.put("RequiredPlayerRange", int16_t{16});
        e.put("SpawnRange", int16_t{4});
        entities.emplace_back(std::move(e));
    }
    for (const auto& c : chunk.chests) { // wiki: Chest › Block data - Items with Slot 0..26
        nbt::Compound e;
        e.put("id", std::string(c.data.barrel    ? "minecraft:barrel"
                                : c.data.shulker ? "minecraft:shulker_box"
                                                 : "minecraft:chest"));
        e.put("x", int32_t{chunk.pos.x * 16 + c.x});
        e.put("y", int32_t{c.y});
        e.put("z", int32_t{chunk.pos.z * 16 + c.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        for (int i = 0; i < 27; ++i)
            if (!c.data.items[size_t(i)].empty()) items.emplace_back(itemNbt(c.data.items[size_t(i)], i));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        entities.emplace_back(std::move(e));
    }
    root.put("block_entities", nbt::listOf(nbt::TagType::Compound, std::move(entities)));
    // Scheduled block ticks (wiki: Chunk format › block_ticks): i block id, p
    // priority, t delay, x/y/z world position; in scheduling order.
    // Fluid ticks go to their own list (wiki: Chunk format › fluid_ticks) with the
    // fluid's id: "minecraft:water" for a source, "minecraft:flowing_water" otherwise
    // (no priority: always 0).
    std::vector<nbt::Tag> ticks, fluidTicks;
    std::vector<const Chunk::BlockTick*> ordered;
    for (const auto& t : chunk.blockTicks)
        ordered.push_back(&t);
    std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) { return a->order < b->order; });
    for (const Chunk::BlockTick* t : ordered) {
        nbt::Compound e;
        const bool fluid = t->block == blocks::Water || t->block == blocks::Lava;
        if (fluid) {
            const int s = chunk.height.sectionIndex(t->y);
            const BlockStateId st = s >= 0 && s < chunk.height.sections()
                                        ? chunk.sections[size_t(s)]->get(t->x, blockToLocal(t->y), t->z)
                                        : BlockStateId{0};
            const bool source = blockRegistry().blockOf(st) == t->block && blockRegistry().get(st, properties::level) == 0;
            const std::string name = t->block == blocks::Water ? "water" : "lava";
            e.put("i", std::string(source ? "minecraft:" : "minecraft:flowing_") + name);
        } else {
            e.put("i", blockRegistry().block(t->block).id);
        }
        e.put("p", int32_t{t->priority});
        e.put("t", static_cast<int32_t>(std::max<int64_t>(0, t->time)));
        e.put("x", int32_t{chunk.pos.x * 16 + t->x});
        e.put("y", int32_t{t->y});
        e.put("z", int32_t{chunk.pos.z * 16 + t->z});
        (fluid ? fluidTicks : ticks).emplace_back(std::move(e));
    }
    root.put("fluid_ticks", nbt::listOf(nbt::TagType::Compound, std::move(fluidTicks)));
    root.put("block_ticks", nbt::listOf(nbt::TagType::Compound, std::move(ticks)));
    return root;
}

bool chunkFromNbt(const nbt::Compound& root, Chunk& chunk, int* unknownBlocks, bool legacyWorld) {
    const auto& reg = blockRegistry();
    // A chunk of a pre-v0.17.1 world that wasn't saved since: placed leaves were stored
    // as distance=7, persistent=false (before v0.15.0) and would now decay. Generated
    // and grown leaves always carry a real distance (1..6), so distance 7 non-persistent
    // can only be placed leaves (or leaves about to decay anyway): made persistent.
    const bool upgradeLeaves = legacyWorld && !root.integer("clone_format");
    if (root.integer("xPos") != chunk.pos().x || root.integer("zPos") != chunk.pos().z)
        return false;
    const nbt::List* sections = root.list("sections");
    if (!sections) return false;
    std::vector<BlockStateId> states(Section::kVolume);
    std::vector<BlockStateId> palette;
    for (int s = 0; s < chunk.sectionCount(); ++s)
        chunk.mutableSection(s).fill(0);
    auto biomes = std::make_shared<ChunkBiomes>(*Chunk::defaultBiomes());
    for (const nbt::Tag& t : sections->items) {
        const nbt::Compound* sec = t.get<nbt::Compound>();
        if (!sec) continue;
        const auto y = sec->integer("Y");
        if (!y) continue;
        const int index = static_cast<int>(*y) - chunk.height().minSection();
        if (index < 0 || index >= chunk.sectionCount()) continue; // outside this dimension's height
        // Biomes (unknown names become plains).
        if (const nbt::Compound* bio = sec->compound("biomes")) {
            const nbt::List* bpal = bio->list("palette");
            std::vector<Biome> ids;
            if (bpal)
                for (const nbt::Tag& name : bpal->items) {
                    const std::string* n = name.get<std::string>();
                    ids.push_back(n ? findBiome(*n).value_or(Biome::Plains) : Biome::Plains);
                }
            const std::vector<int64_t>* bdata = bio->longArray("data");
            if (ids.size() == 1) {
                for (int i = 0; i < ChunkBiomes::kPerSection; ++i)
                    biomes->cells[size_t(index * ChunkBiomes::kPerSection + i)] = ids[0];
            } else if (ids.size() > 1 && bdata) {
                const int bits = int(std::bit_width(ids.size() - 1));
                const int perLong = 64 / bits;
                const uint64_t mask = (uint64_t{1} << bits) - 1;
                if (bdata->size() >= size_t((ChunkBiomes::kPerSection + perLong - 1) / perLong))
                    for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                        const uint64_t v =
                            (static_cast<uint64_t>((*bdata)[size_t(i / perLong)]) >> ((i % perLong) * bits)) & mask;
                        biomes->cells[size_t(index * ChunkBiomes::kPerSection + i)] =
                            v < ids.size() ? ids[v] : Biome::Plains;
                    }
            }
        }
        const nbt::Compound* bs = sec->compound("block_states");
        const nbt::List* pal = bs ? bs->list("palette") : nullptr;
        if (!pal || pal->items.empty()) continue;
        palette.clear();
        for (const nbt::Tag& e : pal->items) {
            const nbt::Compound* entry = e.get<nbt::Compound>();
            std::optional<BlockStateId> st;
            if (entry) {
                std::string text = paletteText(*entry);
                if (text.starts_with("minecraft:")) text.erase(0, 10);
                if (upgradeLeaves && text.ends_with("_leaves[distance=7,persistent=false]"))
                    text.replace(text.size() - 6, 5, "true");
                st = reg.parse(text);
                if (!st) { // lenient: known block, unknown property or value
                    const std::string* name = entry->string("Name");
                    std::string_view id = name ? std::string_view(*name) : std::string_view();
                    if (id.starts_with("minecraft:")) id.remove_prefix(10);
                    if (const auto block = reg.findBlock(id)) {
                        st = reg.defaultState(*block);
                        if (const nbt::Compound* props = entry->compound("Properties"))
                            for (const auto& p : props->entries)
                                if (const std::string* v = p.value.get<std::string>())
                                    st = reg.with(*st, p.name, *v).value_or(*st);
                    }
                }
            }
            if (!st && unknownBlocks) ++*unknownBlocks;
            palette.push_back(st.value_or(0));
        }
        const std::vector<int64_t>* data = bs->longArray("data");
        if (palette.size() == 1 || !data) {
            chunk.mutableSection(index).fill(palette[0]);
            continue;
        }
        const int bits = std::max(4, int(std::bit_width(palette.size() - 1)));
        const int perLong = 64 / bits;
        if (data->size() < size_t((Section::kVolume + perLong - 1) / perLong)) continue;
        const uint64_t mask = (uint64_t{1} << bits) - 1;
        for (int i = 0; i < Section::kVolume; ++i) {
            const uint64_t word = static_cast<uint64_t>((*data)[size_t(i / perLong)]);
            const uint64_t v = (word >> ((i % perLong) * bits)) & mask;
            states[size_t(i)] = v < palette.size() ? palette[v] : BlockStateId{0};
        }
        chunk.mutableSection(index).assign(states.data());
    }
    // Block entities (furnaces) inside this chunk.
    if (const nbt::List* entities = root.list("block_entities"))
        for (const nbt::Tag& t : entities->items) {
            const nbt::Compound* e = t.get<nbt::Compound>();
            const std::string* id = e ? e->string("id") : nullptr;
            if (!id || (*id != "minecraft:furnace" && *id != "minecraft:chest" && *id != "minecraft:mob_spawner" &&
                        *id != "minecraft:smoker" && *id != "minecraft:blast_furnace" && *id != "minecraft:barrel" && *id != "minecraft:shulker_box" &&
                        *id != "minecraft:brewing_stand" && *id != "minecraft:comparator" && *id != "minecraft:hopper" &&
                        *id != "minecraft:dispenser" && *id != "minecraft:dropper" && *id != "minecraft:sign" &&
                        *id != "minecraft:hanging_sign" && *id != "minecraft:campfire" &&
                        *id != "minecraft:beacon" && *id != "minecraft:conduit" && *id != "minecraft:jukebox"))
                continue;
            const int x = static_cast<int>(e->integer("x").value_or(0)) - chunk.pos().x * 16;
            const int y = static_cast<int>(e->integer("y").value_or(chunk.height().minY - 1));
            const int z = static_cast<int>(e->integer("z").value_or(0)) - chunk.pos().z * 16;
            if (x < 0 || x > 15 || z < 0 || z > 15 || !chunk.height().contains(y)) continue;
            if (*id == "minecraft:hopper") {
                if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Hopper) continue;
                HopperData& h = chunk.addHopper(x, y, z);
                if (const nbt::List* items = e->list("Items"))
                    for (const nbt::Tag& it : items->items)
                        if (const nbt::Compound* ic = it.get<nbt::Compound>())
                            if (const auto slot = ic->integer("Slot").value_or(-1); slot >= 0 && slot < 5)
                                h.items[size_t(slot)] = itemFromNbt(*ic);
                h.cooldown = static_cast<int>(std::clamp<int64_t>(e->integer("TransferCooldown").value_or(0), 0, 8));
                continue;
            }
            if (*id == "minecraft:dispenser" || *id == "minecraft:dropper") {
                const BlockId b = blockRegistry().blockOf(chunk.get(x, y, z));
                if (b != blocks::Dispenser && b != blocks::Dropper) continue;
                DispenserData& d = chunk.addDispenser(x, y, z);
                d.dropper = b == blocks::Dropper;
                if (const nbt::List* items = e->list("Items"))
                    for (const nbt::Tag& it : items->items)
                        if (const nbt::Compound* ic = it.get<nbt::Compound>())
                            if (const auto slot = ic->integer("Slot").value_or(-1); slot >= 0 && slot < 9)
                                d.items[size_t(slot)] = itemFromNbt(*ic);
                continue;
            }
            if (*id == "minecraft:comparator") {
                if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Comparator) continue;
                chunk.addComparator(x, y, z).output =
                    static_cast<int>(std::clamp<int64_t>(e->integer("OutputSignal").value_or(0), 0, 15));
                continue;
            }
            if (*id == "minecraft:jukebox") {
                if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Jukebox) continue;
                JukeboxData& jd = chunk.addJukebox(x, y, z);
                if (const nbt::Compound* rec = e->compound("RecordItem")) jd.record = itemFromNbt(*rec);
                const auto played = e->integer("ticks_since_song_started").value_or(0);
                jd.playing = !jd.record.empty() && played > 0;
                jd.ticks = int(std::clamp<int64_t>(played, 0, 1 << 20));
                continue;
            }
            if (*id == "minecraft:beacon" || *id == "minecraft:conduit") {
                const BlockId bb = blockRegistry().blockOf(chunk.get(x, y, z));
                const bool conduit = *id == "minecraft:conduit";
                if (bb != (conduit ? blocks::Conduit : blocks::Beacon)) continue;
                BeaconData& bd = chunk.addBeacon(x, y, z);
                bd.conduit = conduit;
                bd.levels = int(std::clamp<int64_t>(e->integer("Levels").value_or(0), 0, 4));
                if (const std::string* p = e->string("primary_effect"))
                    if (const auto ef = findEffect(*p)) bd.primary = static_cast<uint8_t>(*ef);
                if (const std::string* s2 = e->string("secondary_effect"))
                    if (const auto ef = findEffect(*s2)) bd.secondary = static_cast<uint8_t>(*ef);
                continue;
            }
            if (*id == "minecraft:campfire") {
                const BlockId cb = blockRegistry().blockOf(chunk.get(x, y, z));
                if (cb != blocks::Campfire && cb != blocks::SoulCampfire) continue;
                CampfireData& cf = chunk.addCampfire(x, y, z);
                if (const nbt::List* items = e->list("Items"))
                    for (const nbt::Tag& it : items->items)
                        if (const nbt::Compound* ic = it.get<nbt::Compound>()) {
                            const auto slot = ic->integer("Slot").value_or(-1);
                            if (slot >= 0 && slot < 4) cf.items[size_t(slot)] = itemFromNbt(*ic);
                        }
                if (const nbt::Tag* times = e->find("CookingTimes"))
                    if (const auto* a = times->get<std::vector<int32_t>>())
                        for (size_t i = 0; i < a->size() && i < 4; ++i)
                            cf.cookTime[i] = int16_t(std::clamp((*a)[i], 0, 600));
                continue;
            }
            if (*id == "minecraft:sign" || *id == "minecraft:hanging_sign") {
                const BlockKind k = blockRegistry().kind(blockRegistry().blockOf(chunk.get(x, y, z)));
                if (k != BlockKind::Sign && k != BlockKind::WallSign && k != BlockKind::HangingSign &&
                    k != BlockKind::WallHangingSign)
                    continue;
                SignData& sg = chunk.addSign(x, y, z);
                sg.hanging = k == BlockKind::HangingSign || k == BlockKind::WallHangingSign;
                auto readSide = [&](const char* key, SignData::Side& sd) {
                    const nbt::Compound* t = e->compound(key);
                    if (!t) return;
                    if (const nbt::List* msgs = t->list("messages"))
                        for (size_t i = 0; i < msgs->items.size() && i < size_t(SignData::kLines); ++i)
                            if (const std::string* m = msgs->items[i].get<std::string>()) {
                                std::string_view text = *m; // (older saves: a JSON string "\"...\"")
                                if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
                                    text = text.substr(1, text.size() - 2);
                                auto& line = sd.lines[i];
                                size_t n = 0;
                                for (const char c : text)
                                    if (n < size_t(SignData::kChars) && c >= 32 && c < 127) line[n++] = c;
                                line[n] = 0;
                            }
                    if (const std::string* c = t->string("color"))
                        for (int k2 = 0; k2 < 16; ++k2)
                            if (*c == kDyeColours[k2]) sd.colour = uint8_t(k2);
                    sd.glowing = t->integer("has_glowing_text").value_or(0) != 0;
                };
                readSide("front_text", sg.front);
                readSide("back_text", sg.back);
                sg.waxed = e->integer("is_waxed").value_or(0) != 0;
                continue;
            }
            if (*id == "minecraft:brewing_stand") {
                if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::BrewingStand) continue;
                BrewingData& br = chunk.addBrewing(x, y, z);
                if (const nbt::List* items = e->list("Items"))
                    for (const nbt::Tag& it : items->items)
                        if (const nbt::Compound* ic = it.get<nbt::Compound>()) {
                            const auto slot = ic->integer("Slot").value_or(-1);
                            if (slot >= 0 && slot < 3) br.bottles[size_t(slot)] = itemFromNbt(*ic);
                            else if (slot == 3) br.ingredient = itemFromNbt(*ic);
                            else if (slot == 4) br.fuel = itemFromNbt(*ic);
                        }
                br.brewTime = static_cast<int>(std::clamp<int64_t>(e->integer("BrewTime").value_or(0), 0, 400));
                br.fuelLeft = static_cast<int>(std::clamp<int64_t>(e->integer("Fuel").value_or(0), 0, 20));
                continue;
            }
            if (*id == "minecraft:mob_spawner") {
                if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Spawner) continue;
                SpawnerData& sp = chunk.addSpawner(x, y, z);
                sp.delay = static_cast<int16_t>(std::clamp<int64_t>(e->integer("Delay").value_or(20), 0, 32767));
                const nbt::Compound* data = e->compound("SpawnData");
                const nbt::Compound* entity = data ? data->compound("entity") : nullptr;
                if (const std::string* mob = entity ? entity->string("id") : nullptr)
                    for (int k = 0; k < static_cast<int>(MobType::Count); ++k)
                        if (mobInfo(static_cast<MobType>(k)).id == *mob) sp.mob = static_cast<MobType>(k);
                continue;
            }
            if (*id == "minecraft:chest" || *id == "minecraft:barrel" || *id == "minecraft:shulker_box") {
                const BlockId cb = blockRegistry().blockOf(chunk.get(x, y, z));
                const bool shulker = blockRegistry().likeOf(cb) == blocks::ShulkerBox;
                if (cb != blocks::Chest && cb != blocks::Barrel && !shulker) continue;
                ChestData& c = chunk.addChest(x, y, z);
                c.barrel = cb == blocks::Barrel;
                c.shulker = shulker;
                if (const nbt::List* items = e->list("Items"))
                    for (const nbt::Tag& it : items->items)
                        if (const nbt::Compound* ic = it.get<nbt::Compound>()) {
                            const auto slot = ic->integer("Slot").value_or(-1);
                            if (slot >= 0 && slot < 27) c.items[size_t(slot)] = itemFromNbt(*ic);
                        }
                continue;
            }
            // An entry whose block isn't a furnace (foreign or edited saves) is dropped.
            const BlockId fb = blockRegistry().blockOf(chunk.get(x, y, z));
            if (blockRegistry().likeOf(fb) != blocks::Furnace) continue;
            FurnaceData& f = chunk.addFurnace(x, y, z);
            f.kind = fb == blocks::Smoker ? 1 : fb == blocks::BlastFurnace ? 2 : 0;
            if (const nbt::List* items = e->list("Items"))
                for (const nbt::Tag& it : items->items)
                    if (const nbt::Compound* c = it.get<nbt::Compound>()) {
                        const auto slot = c->integer("Slot").value_or(-1);
                        ItemStack* dst = slot == 0 ? &f.input : slot == 1 ? &f.fuel : slot == 2 ? &f.output : nullptr;
                        if (dst) *dst = itemFromNbt(*c);
                    }
            // Current names first, then the pre-1.21.4 ones (our older saves).
            auto field = [&](const char* now, const char* before) {
                return static_cast<int>(e->integer(now).value_or(e->integer(before).value_or(0)));
            };
            f.burnLeft = field("lit_time_remaining", "BurnTime");
            f.burnDuration = static_cast<int>(e->integer("lit_total_time").value_or(f.burnLeft));
            f.cookTime = field("cooking_time_spent", "CookTime");
            f.cooking = f.input.item; // not saved (vanilla neither): progress belongs to the input
            if (const nbt::Compound* used = e->compound("RecipesUsed"))
                for (const auto& u : used->entries)
                    if (const auto n = used->integer(u.name); n && *n > 0)
                        f.countRecipe(internRecipeId(u.name), static_cast<int32_t>(std::min<int64_t>(*n, INT32_MAX)));
            // v0.17.0 kept the stored experience as a number (clone_experience): kept as
            // uses of the cobblestone -> stone recipe (0.1 each), which pays the same.
            if (const auto old = e->real("clone_experience"); old && *old > 0.0)
                f.countRecipe(internRecipeId("minecraft:stone"),
                              static_cast<int32_t>(std::lround(std::min(*old, 1.0e6) * 10.0)));
        }
    chunk.blockTicks().clear();
    uint64_t order = 0;
    for (const char* listName : {"block_ticks", "fluid_ticks"}) {
        const nbt::List* ticks = root.list(listName);
        if (!ticks) continue;
        for (const nbt::Tag& t : ticks->items) {
            const nbt::Compound* e = t.get<nbt::Compound>();
            const std::string* id = e ? e->string("i") : nullptr;
            std::optional<BlockId> block;
            if (id && (*id == "minecraft:water" || *id == "minecraft:flowing_water")) block = blocks::Water;
            else if (id && (*id == "minecraft:lava" || *id == "minecraft:flowing_lava")) block = blocks::Lava;
            else if (id) block = reg.findBlock(*id);
            if (!block) continue;
            const int64_t x = e->integer("x").value_or(INT32_MIN) - int64_t{chunk.pos().x} * 16;
            const int64_t z = e->integer("z").value_or(INT32_MIN) - int64_t{chunk.pos().z} * 16;
            const int64_t y = e->integer("y").value_or(INT32_MIN);
            if (x < 0 || x > 15 || z < 0 || z > 15 || !chunk.height().contains(static_cast<int32_t>(std::clamp<int64_t>(y, INT32_MIN, INT32_MAX))))
                continue;
            // One pending tick per block (a duplicate would run twice in one tick).
            const bool dup = std::any_of(chunk.blockTicks().begin(), chunk.blockTicks().end(), [&](const auto& t) {
                return t.x == x && t.z == z && t.y == y && t.block == *block;
            });
            if (dup) continue;
            chunk.blockTicks().push_back({static_cast<int8_t>(x), static_cast<int8_t>(z), static_cast<int16_t>(y),
                                          static_cast<int8_t>(std::clamp<int64_t>(e->integer("p").value_or(0), -3, 3)), *block,
                                          std::clamp<int64_t>(e->integer("t").value_or(0), 0, 1 << 20), order++});
        }
    }
    chunk.ticksRelative = !chunk.blockTicks().empty();
    chunk.setBiomes(std::move(biomes));
    return true;
}

nbt::Compound entitiesToNbt(const ChunkSnapshot& chunk) {
    nbt::Compound root;
    root.put("DataVersion", kDataVersion);
    root.put("Position", std::vector<int32_t>{chunk.pos.x, chunk.pos.z});
    std::vector<nbt::Tag> list;
    for (const MobData& m : chunk.mobs) {
        // Dying mobs are not saved - except the dragon, whose 10 s death ends the fight.
        if (m.health <= 0.0f && m.type != MobType::EnderDragon) continue;
        nbt::Compound e;
        e.put("id", std::string(mobInfo(m.type).id));
        e.put("Pos", nbt::listOf(nbt::TagType::Double, {m.pos.x, m.pos.y, m.pos.z}));
        e.put("Motion", nbt::listOf(nbt::TagType::Double, {m.vel.x, m.vel.y, m.vel.z}));
        e.put("Rotation", nbt::listOf(nbt::TagType::Float, {m.yaw, m.pitch}));
        e.put("Health", m.health);
        e.put("OnGround", static_cast<int8_t>(m.onGround ? 1 : 0));
        e.put("fall_distance", double(m.fallDistance)); // 1.21.5+: a double (was FallDistance, float)
        e.put("Air", int16_t{300});
        e.put("PortalCooldown", int32_t{0});
        e.put("Invulnerable", int8_t{0});
        e.put("AbsorptionAmount", 0.0f);
        // 1.21.5+ `equipment` (was HandItems/ArmorItems) is left out when nothing is worn.
        e.put("CanPickUpLoot", int8_t{0});
        e.put("LeftHanded", int8_t{0});
        if (m.type == MobType::Cow || m.type == MobType::Pig || m.type == MobType::Chicken)
            e.put("variant", std::string("minecraft:temperate")); // 1.21.5+ farm animal variants
        if (!mobInfo(m.type).hostile) { // animals (wiki: Entity format › Animal)
            e.put("Age", int32_t(m.age));
            e.put("ForcedAge", int32_t{0});
            e.put("InLove", int32_t(m.loveTicks));
        }
        if (m.type == MobType::Sheep) {
            e.put("Color", int8_t(m.woolColour));
            e.put("Sheared", int8_t(m.sheared ? 1 : 0));
        }
        if (m.type == MobType::Creeper) { // wiki: Creeper › Entity data
            e.put("Fuse", int16_t{30});
            e.put("ExplosionRadius", int8_t{3});
            e.put("ignited", int8_t{0});
            e.put("powered", int8_t(m.powered ? 1 : 0));
        }
        if (m.type == MobType::Enderman && m.carried) // carriedBlockState {Name, Properties}
            e.put("carriedBlockState", paletteEntry(blockRegistry().toString(m.carried)));
        if (m.type == MobType::Chicken) {
            e.put("EggLayTime", int32_t(m.eggTicks));
            e.put("IsChickenJockey", int8_t{0});
        }
        if (m.type == MobType::EndCrystal) e.put("ShowBottom", int8_t(m.showBottom ? 1 : 0));
        if (m.type == MobType::EnderDragon) e.put("DragonPhase", int32_t(m.phase)); // (vanilla's numbers)
        if (m.type == MobType::Shulker) {
            e.put("AttachFace", int8_t{0}); // (ours always sit on a floor: down)
            e.put("Peek", int8_t(m.peek));
            e.put("Color", int8_t{16}); // (no colour)
        }
        if (m.type == MobType::MagmaCube || m.type == MobType::Slime) e.put("Size", int32_t(m.size == 4 ? 3 : m.size - 1)); // vanilla: size - 1
        if (m.type == MobType::ZombifiedPiglin) e.put("AngerTime", int32_t(m.angry ? m.angerTicks : 0));
        if (m.type == MobType::ZombieVillager) e.put("ConversionTime", int32_t(m.convertTicks > 0 ? m.convertTicks : -1));
        if (m.type == MobType::WanderingTrader) e.put("DespawnDelay", int32_t(m.despawnDelay));
        if (m.type == MobType::IronGolem) e.put("PlayerCreated", int8_t(m.playerCreated ? 1 : 0));
        if (m.type == MobType::Pillager) { // wiki: Raider › Entity data
            e.put("PatrolLeader", int8_t(m.captain ? 1 : 0));
            e.put("Patrolling", int8_t{0});
            e.put("CanJoinRaid", int8_t{1});
        }
        if (m.raidId) { // (M24.5: in a raid)
            e.put("Wave", int32_t(1));
            e.put("RaidId", int32_t(m.raidId));
        }
        if (m.type == MobType::Villager || m.type == MobType::ZombieVillager ||
            m.type == MobType::WanderingTrader) { // wiki: Villager › Entity data (the trader: Offers)
            nbt::Compound data;
            data.put("type", std::string(villagerTypeId(static_cast<VillagerType>(m.villagerType))));
            data.put("profession", std::string(professionInfo(static_cast<Profession>(m.profession)).id));
            data.put("level", int32_t(m.villagerLevel));
            e.put("VillagerData", std::move(data));
            e.put("Xp", int32_t(m.villagerXp));
            { // its food (M24.3) as vanilla's Inventory: up to 8 stacks
                static constexpr const char* kFood[6] = {"bread", "carrot", "potato", "beetroot", "wheat", "wheat_seeds"};
                std::vector<nbt::Tag> inv;
                for (int i = 0; i < 6; ++i)
                    if (m.food[size_t(i)] > 0)
                        if (const auto id = itemRegistry().find(kFood[i]))
                            inv.emplace_back(itemNbt({*id, m.food[size_t(i)]}, -1));
                e.put("Inventory", nbt::listOf(nbt::TagType::Compound, std::move(inv)));
            }
            e.put("LastRestock", int64_t(m.lastRestockDay));
            e.put("RestocksToday", int32_t(m.restocksToday));
            nbt::Compound memories; // Brain.memories: home / job_site / meeting_point {value: {pos, dimension}}
            auto memory = [&](const char* key, const glm::ivec3& p) {
                if (p.y == kNoPoint) return;
                nbt::Compound value, wrap;
                value.put("pos", std::vector<int32_t>{p.x, p.y, p.z});
                value.put("dimension", std::string("minecraft:overworld"));
                wrap.put("value", std::move(value));
                memories.put(key, std::move(wrap));
            };
            memory("minecraft:home", m.home);
            memory("minecraft:job_site", m.jobSite);
            memory("minecraft:meeting_point", m.meetingPoint);
            nbt::Compound brain;
            brain.put("memories", std::move(memories));
            e.put("Brain", std::move(brain));
            if (m.offerCount > 0) { // Offers {Recipes: [{buy, buyB, sell, uses, maxUses, xp, ...}]}
                std::vector<nbt::Tag> recipes;
                for (int i = 0; i < m.offerCount; ++i) {
                    const TradeOffer& o = m.offers[size_t(i)];
                    nbt::Compound r;
                    r.put("buy", itemNbt({o.buyA, o.buyACount}, -1));
                    if (o.buyB) r.put("buyB", itemNbt({o.buyB, o.buyBCount}, -1));
                    ItemStack sold{o.sell, o.sellCount};
                    if (o.sellEnchant) setEnchantment(sold, static_cast<Enchantment>(o.sellEnchant >> 8), o.sellEnchant & 0xFF);
                    r.put("sell", itemNbt(sold, -1));
                    r.put("uses", int32_t(o.uses));
                    r.put("maxUses", int32_t(o.maxUses));
                    r.put("rewardExp", int8_t{1});
                    r.put("xp", int32_t(o.xp));
                    r.put("priceMultiplier", o.priceMultiplier);
                    r.put("specialPrice", int32_t(o.specialPrice));
                    r.put("demand", int32_t(o.demand));
                    recipes.emplace_back(std::move(r));
                }
                nbt::Compound offers;
                offers.put("Recipes", nbt::listOf(nbt::TagType::Compound, std::move(recipes)));
                e.put("Offers", std::move(offers));
            }
        }
        if (m.type == MobType::Zombie) {
            e.put("IsBaby", int8_t{0});
            e.put("CanBreakDoors", int8_t{0});
            e.put("DrownedConversionTime", int32_t{-1});
            e.put("InWaterTime", int32_t{-1});
        }
        e.put("Fire", static_cast<int16_t>(m.fireTicks > 0 ? m.fireTicks : -20)); // -20: not burning (wiki)
        e.put("HurtTime", static_cast<int16_t>(m.hurtTime));
        e.put("DeathTime", static_cast<int16_t>(m.type == MobType::EnderDragon ? m.deathTime : 0));
        e.put("PersistenceRequired", static_cast<int8_t>(m.persistent ? 1 : 0));
        e.put("UUID", std::vector<int32_t>{int32_t(m.uuidHi >> 32), int32_t(m.uuidHi), int32_t(m.uuidLo >> 32),
                                           int32_t(m.uuidLo)});
        list.emplace_back(std::move(e));
    }
    root.put("Entities", nbt::listOf(nbt::TagType::Compound, std::move(list)));
    return root;
}

void entitiesFromNbt(const nbt::Compound& root, Chunk& chunk) {
    chunk.mobs().clear();
    const nbt::List* list = root.list("Entities");
    if (!list) return;
    for (const nbt::Tag& t : list->items) {
        const nbt::Compound* e = t.get<nbt::Compound>();
        const std::string* id = e ? e->string("id") : nullptr;
        if (!id) continue;
        MobData m;
        bool known = false;
        for (int k = 0; k < static_cast<int>(MobType::Count); ++k)
            if (mobInfo(static_cast<MobType>(k)).id == *id) {
                m.type = static_cast<MobType>(k);
                known = true;
            }
        if (!known) continue; // entity types we don't have yet are skipped
        auto vec3 = [&](const char* key, glm::dvec3& out) {
            if (const nbt::List* l = e->list(key); l && l->items.size() == 3)
                for (int i = 0; i < 3; ++i)
                    if (auto d = l->items[size_t(i)].get<double>()) out[i] = *d;
        };
        vec3("Pos", m.pos);
        vec3("Motion", m.vel);
        // Corrupt or hand-edited files: skip mobs outside the world, clamp motion
        // (vanilla clamps each component to +-10) and health.
        if (!isValidMobPosition(m.pos)) continue;
        for (int i = 0; i < 3; ++i)
            m.vel[i] = std::isfinite(m.vel[i]) ? std::clamp(m.vel[i], -10.0, 10.0) : 0.0;
        m.prevPos = m.goal = m.pos;
        if (const nbt::List* r = e->list("Rotation"); r && r->items.size() == 2) {
            if (auto v = r->items[0].get<float>()) m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = *v;
            if (auto v = r->items[1].get<float>()) m.pitch = m.prevPitch = *v;
        }
        m.health = static_cast<float>(e->real("Health").value_or(mobInfo(m.type).maxHealth));
        m.health = std::isfinite(m.health) ? std::min(m.health, mobInfo(m.type).maxHealth) : 0.0f;
        m.onGround = e->integer("OnGround").value_or(0) != 0;
        if (const auto f = e->real("fall_distance").value_or(e->real("FallDistance").value_or(0.0)); std::isfinite(f))
            m.fallDistance = static_cast<float>(std::clamp(f, 0.0, 1.0e6));
        m.fireTicks = static_cast<int16_t>(e->integer("Fire").value_or(0));
        m.persistent = e->integer("PersistenceRequired").value_or(0) != 0;
        m.age = static_cast<int>(std::clamp<int64_t>(e->integer("Age").value_or(0), -24000, 24000));
        m.loveTicks = static_cast<int>(std::clamp<int64_t>(e->integer("InLove").value_or(0), 0, 600));
        m.woolColour = static_cast<uint8_t>(std::clamp<int64_t>(e->integer("Color").value_or(0), 0, 15));
        m.sheared = e->integer("Sheared").value_or(0) != 0;
        m.powered = m.type == MobType::Creeper && e->integer("powered").value_or(0) != 0;
        m.showBottom = e->integer("ShowBottom").value_or(1) != 0;
        if (m.type == MobType::Shulker) m.peek = static_cast<uint8_t>(std::clamp<int64_t>(e->integer("Peek").value_or(0), 0, 100));
        if (m.type == MobType::EnderDragon) {
            m.phase = static_cast<uint8_t>(std::clamp<int64_t>(e->integer("DragonPhase").value_or(0), 0, 10));
            m.deathTime = static_cast<int16_t>(std::clamp<int64_t>(e->integer("DeathTime").value_or(0), 0, 199));
            m.lastHealth = m.health;
        }
        if (m.type == MobType::MagmaCube || m.type == MobType::Slime) {
            const int64_t sz = std::clamp<int64_t>(e->integer("Size").value_or(3), 0, 3);
            m.size = uint8_t(sz >= 3 ? 4 : sz + 1);
        }
        if (m.type == MobType::ZombifiedPiglin) {
            m.angerTicks = static_cast<int16_t>(std::clamp<int64_t>(e->integer("AngerTime").value_or(0), 0, 30000));
            m.angry = m.angerTicks > 0;
        }
        m.eggTicks = static_cast<int>(std::clamp<int64_t>(e->integer("EggLayTime").value_or(6000), 0, 12000));
        if (m.type == MobType::ZombieVillager)
            m.convertTicks = int16_t(std::clamp<int64_t>(e->integer("ConversionTime").value_or(-1), 0, 6000));
        if (m.type == MobType::WanderingTrader)
            m.despawnDelay = int(std::clamp<int64_t>(e->integer("DespawnDelay").value_or(48000), 1, 48000));
        m.captain = m.type == MobType::Pillager && e->integer("PatrolLeader").value_or(0) != 0;
        m.playerCreated = m.type == MobType::IronGolem && e->integer("PlayerCreated").value_or(0) != 0;
        m.raidId = isRaider(m.type) ? int32_t(std::clamp<int64_t>(e->integer("RaidId").value_or(0), 0, 1 << 30)) : 0;
        if (m.type == MobType::Villager || m.type == MobType::ZombieVillager || m.type == MobType::WanderingTrader) {
            if (const nbt::Compound* data = e->compound("VillagerData")) {
                if (const std::string* vt = data->string("type"))
                    m.villagerType = uint8_t(findVillagerType(*vt).value_or(VillagerType::Plains));
                if (const std::string* p = data->string("profession"))
                    m.profession = uint8_t(findProfession(*p).value_or(Profession::None));
                m.villagerLevel = uint8_t(std::clamp<int64_t>(data->integer("level").value_or(1), 1, 5));
                m.poiSearch = int16_t(std::abs(int(std::floor(m.pos.x * 7.0 + m.pos.z * 13.0))) % 200); // (loaded villagers don't all search on one tick)
            }
            m.villagerXp = int(std::clamp<int64_t>(e->integer("Xp").value_or(0), 0, 1000000));
            if (const nbt::List* inv = e->list("Inventory")) {
                static constexpr const char* kFood[6] = {"bread", "carrot", "potato", "beetroot", "wheat", "wheat_seeds"};
                for (const nbt::Tag& it : inv->items)
                    if (const nbt::Compound* ic = it.get<nbt::Compound>()) {
                        const ItemStack st = itemFromNbt(*ic);
                        for (int i = 0; i < 6; ++i)
                            if (!st.empty() && itemRegistry().find(kFood[i]) == st.item)
                                m.food[size_t(i)] = uint8_t(std::min(64, m.food[size_t(i)] + st.count));
                    }
            }
            m.lastRestockDay = e->integer("LastRestock").value_or(-1);
            m.restocksToday = uint8_t(std::clamp<int64_t>(e->integer("RestocksToday").value_or(0), 0, 2));
            if (const nbt::Compound* brain = e->compound("Brain"))
                if (const nbt::Compound* mem = brain->compound("memories")) {
                    auto memory = [&](const char* key, glm::ivec3& out) {
                        const nbt::Compound* wrap = mem->compound(key);
                        const nbt::Compound* value = wrap ? wrap->compound("value") : nullptr;
                        const nbt::Tag* pos = value ? value->find("pos") : nullptr;
                        if (const auto* a = pos ? pos->get<std::vector<int32_t>>() : nullptr; a && a->size() == 3)
                            out = {(*a)[0], (*a)[1], (*a)[2]};
                    };
                    memory("minecraft:home", m.home);
                    memory("minecraft:job_site", m.jobSite);
                    memory("minecraft:meeting_point", m.meetingPoint);
                }
            if (const nbt::Compound* offers = e->compound("Offers"))
                if (const nbt::List* recipes = offers->list("Recipes"))
                    for (const nbt::Tag& rt : recipes->items) {
                        const nbt::Compound* r = rt.get<nbt::Compound>();
                        if (!r || m.offerCount >= kMaxOffers) continue;
                        const nbt::Compound* buyTag = r->compound("buy");
                        const nbt::Compound* sellTag = r->compound("sell");
                        if (!buyTag || !sellTag) continue;
                        const ItemStack a = itemFromNbt(*buyTag), s = itemFromNbt(*sellTag);
                        if (a.empty() || s.empty()) continue; // (items we don't have yet)
                        TradeOffer o;
                        o.buyA = a.item;
                        o.buyACount = a.count;
                        if (const nbt::Compound* b = r->compound("buyB")) {
                            const ItemStack bs = itemFromNbt(*b);
                            o.buyB = bs.item;
                            o.buyBCount = bs.count;
                        }
                        o.sell = s.item;
                        o.sellCount = s.count;
                        o.sellEnchant = s.enchantments[0];
                        o.uses = uint8_t(std::clamp<int64_t>(r->integer("uses").value_or(0), 0, 255));
                        o.maxUses = uint8_t(std::clamp<int64_t>(r->integer("maxUses").value_or(12), 1, 255));
                        o.xp = uint8_t(std::clamp<int64_t>(r->integer("xp").value_or(1), 0, 255));
                        o.priceMultiplier = float(r->real("priceMultiplier").value_or(0.05));
                        o.specialPrice = int16_t(std::clamp<int64_t>(r->integer("specialPrice").value_or(0), -64, 64));
                        o.demand = int8_t(std::clamp<int64_t>(r->integer("demand").value_or(0), 0, 100));
                        m.offers[m.offerCount++] = o;
                    }
            m.persistent = true;
        }
        if (const nbt::Compound* carried = e->compound("carriedBlockState"))
            if (const auto s = blockRegistry().parse(paletteText(*carried))) m.carried = *s;
        if (const nbt::Tag* u = e->find("UUID"))
            if (const auto* a = u->get<std::vector<int32_t>>(); a && a->size() == 4) {
                m.uuidHi = (uint64_t(uint32_t((*a)[0])) << 32) | uint32_t((*a)[1]);
                m.uuidLo = (uint64_t(uint32_t((*a)[2])) << 32) | uint32_t((*a)[3]);
            }
        if (m.health > 0.0f || (m.type == MobType::EnderDragon && m.deathTime > 0)) chunk.mobs().push_back(m);
    }
}

} // namespace mc::world
