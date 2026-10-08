#include "world/LevelData.h"

#include "world/Dimension.h"

#include "core/Compression.h"
#include "core/Files.h"
#include "core/Log.h"
#include "core/Nbt.h"
#include "world/ChunkSerializer.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace mc::world {

using namespace mc::nbt;

bool LevelData::save(const std::filesystem::path& dir) const {
    Compound data;
    data.put("DataVersion", kDataVersion);
    data.put("version", int32_t{19133}); // Anvil
    data.put("LevelName", name);
    data.put("DayTime", dayTime);
    data.put("Time", gameTime);
    data.put("GameType", int32_t{gameMode}); // (M28.1: 0 survival, 1 creative, 2 adventure, 3 spectator)
    data.put("allowCommands", int8_t{1});
    data.put("initialized", int8_t{1});
    // World spawn: since 1.21.9 a compound that names its dimension (was SpawnX/Y/Z,
    // SpawnAngle; wiki: Java Edition level format).
    Compound spawnTag;
    spawnTag.put("dimension", std::string("minecraft:overworld"));
    spawnTag.put("pos", std::vector<int32_t>{spawn[0], spawn[1], spawn[2]});
    spawnTag.put("yaw", 0.0f);
    spawnTag.put("pitch", 0.0f);
    data.put("spawn", std::move(spawnTag));
    data.put("Difficulty", int8_t(difficulty)); // (M28.1)
    data.put("DifficultyLocked", int8_t{0});
    data.put("hardcore", int8_t{0});
    data.put("raining", int8_t{raining ? 1 : 0});
    data.put("thundering", int8_t{thundering ? 1 : 0});
    data.put("rainTime", rainTime);
    data.put("thunderTime", thunderTime);
    data.put("clearWeatherTime", clearWeatherTime);
    data.put("WasModded", int8_t{1}); // not written by vanilla's own server
    data.put("ServerBrands", listOf(TagType::String, {std::string("minecraftclone")}));
    // Game rules: 1.21.11 ids (namespaced snake_case; wiki: Game rule). Only the ones
    // our game follows (M28.1: world/GameRules); vanilla fills in the rest with defaults.
    Compound gameRules;
    for (int i = 0; i < GameRules::kCount; ++i)
        gameRules.put(std::string(GameRules::id(i)), *rules.get(GameRules::id(i)));
    data.put("GameRules", std::move(gameRules));
    Compound packs;
    packs.put("Enabled", listOf(TagType::String, {std::string("vanilla")}));
    packs.put("Disabled", listOf(TagType::String, {}));
    data.put("DataPacks", std::move(packs));
    data.put("LastPlayed", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     std::chrono::system_clock::now().time_since_epoch())
                                                     .count()));
    Compound version;
    version.put("Id", kDataVersion);
    version.put("Name", std::string("1.21.11"));
    version.put("Series", std::string("main"));
    version.put("Snapshot", int8_t{0});
    data.put("Version", std::move(version));
    Compound gen;
    gen.put("seed", static_cast<int64_t>(seed));
    gen.put("generate_features", int8_t{1});
    gen.put("bonus_chest", int8_t{0});
    // The three vanilla dimensions with vanilla's generators (vanilla generates its own
    // terrain for chunks we never saved; ours are kept as saved).
    auto dimensionEntry = [](const char* type, const char* settings, Compound biomeSource) {
        Compound g;
        g.put("type", std::string("minecraft:noise"));
        g.put("settings", std::string(settings));
        g.put("biome_source", std::move(biomeSource));
        Compound d;
        d.put("type", std::string(type));
        d.put("generator", std::move(g));
        return d;
    };
    Compound multiNoise, netherNoise, endSource;
    multiNoise.put("type", std::string("minecraft:multi_noise"));
    multiNoise.put("preset", std::string("minecraft:overworld"));
    netherNoise.put("type", std::string("minecraft:multi_noise"));
    netherNoise.put("preset", std::string("minecraft:nether"));
    endSource.put("type", std::string("minecraft:the_end"));
    Compound dims;
    dims.put("minecraft:overworld", dimensionEntry("minecraft:overworld", "minecraft:overworld", std::move(multiNoise)));
    dims.put("minecraft:the_nether", dimensionEntry("minecraft:the_nether", "minecraft:nether", std::move(netherNoise)));
    dims.put("minecraft:the_end", dimensionEntry("minecraft:the_end", "minecraft:end", std::move(endSource)));
    gen.put("dimensions", std::move(dims));
    data.put("WorldGenSettings", std::move(gen));

    Compound player;
    player.put("Pos", listOf(TagType::Double, {pos[0], pos[1], pos[2]}));
    player.put("Rotation", listOf(TagType::Float, {yaw, pitch}));
    player.put("Dimension", dimension);
    player.put("DataVersion", kDataVersion);
    player.put("Motion", listOf(TagType::Double, {0.0, 0.0, 0.0}));
    player.put("OnGround", int8_t{1});
    player.put("fall_distance", 0.0);
    player.put("Air", static_cast<int16_t>(air));
    if (hasLastDeath) { // LastDeathLocation {dimension, pos: [I; x, y, z]} (wiki: Player.dat)
        Compound d;
        d.put("dimension", std::string(dimensionInfo(Dimension(std::clamp(lastDeathDimension, 0, 2))).id));
        d.put("pos", std::vector<int32_t>{lastDeath[0], lastDeath[1], lastDeath[2]});
        player.put("LastDeathLocation", std::move(d));
    }
    if (hasRespawn) { // 1.21.5+: respawn {pos, dimension, yaw, pitch, forced} (wiki: Player.dat)
        Compound r;
        r.put("pos", std::vector<int32_t>{respawn[0], respawn[1], respawn[2]});
        r.put("dimension", std::string("minecraft:overworld"));
        r.put("yaw", 0.0f);
        r.put("pitch", 0.0f);
        r.put("forced", int8_t{0});
        player.put("respawn", std::move(r));
    }
    player.put("Fire", static_cast<int16_t>(fire > 0 ? fire : -20));
    player.put("XpLevel", int32_t{xpLevel});
    player.put("XpP", xpProgress);
    player.put("XpTotal", int32_t{xpTotal});
    player.put("XpSeed", int32_t{xpSeed});
    player.put("TimeSinceRest", int32_t{timeSinceRest});
    {
        Compound tracker; // (M27.3)
        tracker.put("warning_level", int32_t{wardenLevel});
        tracker.put("ticks_since_last_warning", int32_t{wardenTicks});
        tracker.put("cooldown_ticks", int32_t{wardenCooldown});
        player.put("warden_spawn_tracker", std::move(tracker));
    }
    if (!effects.empty()) {
        std::vector<Tag> list;
        for (const SavedEffect& e : effects) {
            Compound c;
            c.put("id", e.id);
            c.put("amplifier", int8_t(e.amplifier));
            c.put("duration", int32_t{e.duration});
            c.put("ambient", int8_t{0});
            c.put("show_particles", int8_t{1});
            c.put("show_icon", int8_t{1});
            list.emplace_back(std::move(c));
        }
        player.put("active_effects", listOf(TagType::Compound, std::move(list)));
    }
    player.put("Score", int32_t{0});
    Compound abilities;
    abilities.put("flying", static_cast<int8_t>(flying ? 1 : 0));
    abilities.put("mayfly", static_cast<int8_t>(gameMode == 1 || gameMode == 3 ? 1 : 0));
    abilities.put("instabuild", static_cast<int8_t>(gameMode == 1 ? 1 : 0));
    abilities.put("invulnerable", static_cast<int8_t>(gameMode == 1 || gameMode == 3 ? 1 : 0));
    abilities.put("mayBuild", static_cast<int8_t>(gameMode <= 1 ? 1 : 0));
    abilities.put("flySpeed", 0.05f);
    abilities.put("walkSpeed", 0.1f);
    player.put("abilities", std::move(abilities));
    player.put("playerGameType", int32_t{gameMode});
    player.put("Health", health);
    player.put("foodLevel", int32_t{food});
    player.put("foodSaturationLevel", saturation);
    player.put("foodExhaustionLevel", exhaustion);
    player.put("foodTickTimer", int32_t{foodTimer});
    player.put("SelectedItemSlot", int32_t{selectedSlot});
    std::vector<Tag> items; // vanilla's Inventory list
    std::vector<Tag> enderItems; // vanilla's EnderItems list (Slot 0..26)
    Compound equipment;     // 1.21.5+: worn armor and the offhand
    for (const SavedItem& it : inventory) {
        Compound item;
        const std::string& s = it.state;
        item.put("id", it.id);
        item.put("count", int32_t{it.count});
        item.put("Slot", static_cast<int8_t>(it.slot));
        Compound components;
        if (it.id == "minecraft:filled_map") components.put("minecraft:map_id", int32_t{it.damage}); // (M28.2b)
        else if (it.damage > 0) components.put("minecraft:damage", int32_t{it.damage});
        // The exact state (e.g. log axis) as vanilla's block_state item component.
        if (const size_t open = s.find('['); open != std::string::npos) {
            Compound props;
            std::string_view list(s.data() + open + 1, s.size() - open - 2);
            while (!list.empty()) {
                const size_t comma = std::min(list.find(','), list.size());
                const std::string_view kv = list.substr(0, comma);
                if (const size_t eq = kv.find('='); eq != std::string_view::npos)
                    props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
                list.remove_prefix(std::min(comma + 1, list.size()));
            }
            components.put("minecraft:block_state", std::move(props));
        }
        if (!it.enchantments.empty()) {
            Compound ench;
            for (const auto& [id, lvl] : it.enchantments)
                ench.put(id, int32_t{lvl});
            components.put(it.storedEnchantments ? "minecraft:stored_enchantments" : "minecraft:enchantments",
                           std::move(ench));
        }
        if (it.repairCost) components.put("minecraft:repair_cost", int32_t{it.repairCost});
        if (!it.potion.empty()) {
            Compound contents;
            contents.put("potion", "minecraft:" + it.potion);
            components.put("minecraft:potion_contents", std::move(contents));
        }
        if (it.contents || it.trim || it.extra) { // (the item writer knows these components' layouts)
            ItemStack carrier{};
            carrier.item = 1; // (any item: only these components are used)
            carrier.count = 1;
            carrier.contents = it.contents;
            carrier.trim = it.trim;
            carrier.extra = it.extra;
            const nbt::Compound full = itemToNbt(carrier, -1);
            if (const Compound* fc = full.compound("components"))
                for (const char* key : {"minecraft:container", "minecraft:trim", "minecraft:lodestone_tracker",
                                        "minecraft:writable_book_content", "minecraft:written_book_content"})
                    if (const Tag* t = fc->find(key)) components.put(key, *t);
        }
        if (!components.entries.empty()) item.put("components", std::move(components));
        if (it.slot >= 200) { // the ender chest
            item.put("Slot", static_cast<int8_t>(it.slot - 200));
            enderItems.emplace_back(std::move(item));
            continue;
        }
        if (it.slot >= 100) { // equipment: no Slot field
            static constexpr const char* kKeys[4] = {"feet", "legs", "chest", "head"};
            item.entries.erase(std::remove_if(item.entries.begin(), item.entries.end(),
                                              [](const auto& e) { return e.name == "Slot"; }),
                               item.entries.end());
            equipment.put(it.slot == 150 ? "offhand" : kKeys[it.slot - 100], std::move(item));
            continue;
        }
        items.emplace_back(std::move(item));
    }
    player.put("Inventory", listOf(TagType::Compound, std::move(items)));
    if (playerUuidHi || playerUuidLo)
        player.put("UUID", std::vector<int32_t>{int32_t(playerUuidHi >> 32), int32_t(playerUuidHi), int32_t(playerUuidLo >> 32),
                                                int32_t(playerUuidLo)});
    if (!equipment.entries.empty()) player.put("equipment", std::move(equipment));
    player.put("EnderItems", listOf(TagType::Compound, std::move(enderItems)));
    data.put("Player", std::move(player));
    data.put("WanderingTraderSpawnDelay", int32_t{traderSpawnDelay});
    data.put("WanderingTraderSpawnChance", int32_t{traderSpawnChance});
    if (raidActive || raidPendingTicks > 0 || raidNextId > 1) {
        Compound raid;
        raid.put("Active", int8_t(raidActive ? 1 : 0));
        raid.put("Id", int32_t{raidId});
        raid.put("NextAvailableID", int32_t{raidNextId});
        raid.put("OmenTicks", int32_t{raidPendingTicks});
        raid.put("OmenLevel", int32_t{raidPendingLevel});
        raid.put("OmenCenter", std::vector<int32_t>{raidPendingCentre[0], raidPendingCentre[1], raidPendingCentre[2]});
        raid.put("Center", std::vector<int32_t>{raidCentre[0], raidCentre[1], raidCentre[2]});
        raid.put("Wave", int32_t{raidWave});
        raid.put("NumGroups", int32_t{raidWaves});
        raid.put("BadOmenLevel", int32_t{raidLevel});
        raid.put("TicksActive", int32_t{raidTicks});
        raid.put("PreRaidTicks", int32_t{raidCooldown});
        raid.put("TotalHealth", raidWaveHealth);
        data.put("Raid", std::move(raid));
    }
    {
        Compound fight; // (vanilla's DragonFight tag)
        fight.put("DragonKilled", int8_t(dragonKilled ? 1 : 0));
        fight.put("PreviouslyKilled", int8_t(dragonPreviouslyKilled ? 1 : 0));
        fight.put("NeedsStateScanning", int8_t{0});
        if (dragonUuidHi != 0 || dragonUuidLo != 0)
            fight.put("Dragon", std::vector<int32_t>{int32_t(dragonUuidHi >> 32), int32_t(dragonUuidHi),
                                                     int32_t(dragonUuidLo >> 32), int32_t(dragonUuidLo)});
        if (hasGateways) fight.put("Gateways", gateways);
        data.put("DragonFight", std::move(fight));
    }

    Compound ours;
    ours.put("generator", flat ? std::string("flat") : generator);
    ours.put("format", cloneFormat);
    ours.put("nether_generator", netherGenerator);
    ours.put("end_generator", endGenerator);
    std::vector<Tag> portalTags;
    for (const Portal& p : portals) {
        Compound c;
        c.put("dimension", p.dimension);
        c.put("x", p.x);
        c.put("y", p.y);
        c.put("z", p.z);
        portalTags.emplace_back(std::move(c));
    }
    ours.put("portals", listOf(TagType::Compound, std::move(portalTags)));
    data.put("MinecraftClone", std::move(ours));

    Compound root;
    root.put("Data", std::move(data));
    const auto bytes = gzipCompress(write(root));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto tmp = dir / "level.dat_new";
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        f.close(); // a failed flush (disk full) shows up here
        if (!f) return false;
    }
    // Backup by copy, so level.dat exists at every moment; then one replacing rename.
    if (std::filesystem::exists(dir / "level.dat", ec))
        std::filesystem::copy_file(dir / "level.dat", dir / "level.dat_old",
                                   std::filesystem::copy_options::overwrite_existing, ec);
    ec.clear();
    std::filesystem::rename(tmp, dir / "level.dat", ec);
    return !ec;
}

namespace {

std::optional<Compound> readLevelFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto raw = gzipDecompress(gz);
    auto root = raw ? read(*raw) : std::nullopt;
    if (!root || !root->compound("Data")) return std::nullopt;
    return root;
}

} // namespace

std::optional<LevelData> LevelData::load(const std::filesystem::path& dir) {
    std::optional<Compound> root;
    for (const char* name : {"level.dat", "level.dat_old", "level.dat_new"}) {
        root = readLevelFile(dir / name);
        if (root) {
            if (std::string_view(name) != "level.dat")
                MC_LOG_WARN("level.dat in %s is missing or unreadable; using %s",
                            dir.string().c_str(), name);
            break;
        }
    }
    if (!root) return std::nullopt;
    const Compound* data = root->compound("Data");
    LevelData l;
    if (auto s = data->string("LevelName")) l.name = *s;
    l.dayTime = data->integer("DayTime").value_or(0);
    l.difficulty = int(std::clamp<int64_t>(data->integer("Difficulty").value_or(2), 0, 3)); // (M28.1)
    if (const Compound* gr = data->compound("GameRules")) // (string values, as vanilla writes them)
        for (int i = 0; i < GameRules::kCount; ++i)
            if (const std::string* v = gr->string(GameRules::id(i))) l.rules.set(GameRules::id(i), *v);
    l.gameTime = data->integer("Time").value_or(0);
    l.lastPlayed = data->integer("LastPlayed").value_or(0);
    l.raining = data->integer("raining").value_or(0) != 0;
    l.thundering = data->integer("thundering").value_or(0) != 0;
    l.rainTime = static_cast<int32_t>(data->integer("rainTime").value_or(0));
    l.thunderTime = static_cast<int32_t>(data->integer("thunderTime").value_or(0));
    l.clearWeatherTime = static_cast<int32_t>(data->integer("clearWeatherTime").value_or(0));
    // World spawn: the 1.21.9+ compound, else the older SpawnX/Y/Z (our earlier saves).
    l.spawn[0] = static_cast<int32_t>(data->integer("SpawnX").value_or(0));
    l.spawn[1] = static_cast<int32_t>(data->integer("SpawnY").value_or(64));
    l.spawn[2] = static_cast<int32_t>(data->integer("SpawnZ").value_or(0));
    if (const Compound* sp = data->compound("spawn"))
        if (const Tag* pos = sp->find("pos"))
            if (const auto* a = pos->get<std::vector<int32_t>>(); a && a->size() == 3)
                for (int i = 0; i < 3; ++i)
                    l.spawn[i] = (*a)[size_t(i)];
    if (const Compound* gen = data->compound("WorldGenSettings"))
        l.seed = static_cast<uint64_t>(gen->integer("seed").value_or(0));
    if (const Compound* ours = data->compound("MinecraftClone")) {
        l.cloneFormat = static_cast<int32_t>(ours->integer("format").value_or(0)); // missing: before v0.17.1
        if (auto g = ours->string("generator")) {
            l.flat = *g == "flat";
            if (!l.flat) l.generator = *g;
        }
        if (auto g = ours->string("nether_generator")) l.netherGenerator = *g; // (unknown kinds: main refuses)
        if (auto g = ours->string("end_generator")) l.endGenerator = *g;
        if (const List* portals = ours->list("portals"))
            for (const Tag& t : portals->items)
                if (const Compound* c = t.get<Compound>(); c && c->string("dimension"))
                    l.portals.push_back({*c->string("dimension"), static_cast<int32_t>(c->integer("x").value_or(0)),
                                         static_cast<int32_t>(c->integer("y").value_or(0)),
                                         static_cast<int32_t>(c->integer("z").value_or(0))});
    }
    if (const Compound* p = data->compound("Player")) {
        if (const Tag* u = p->find("UUID"))
            if (const auto* a = u->get<std::vector<int32_t>>(); a && a->size() == 4) {
                l.playerUuidHi = (uint64_t(uint32_t((*a)[0])) << 32) | uint32_t((*a)[1]);
                l.playerUuidLo = (uint64_t(uint32_t((*a)[2])) << 32) | uint32_t((*a)[3]);
            }
        if (const List* pos = p->list("Pos"); pos && pos->items.size() == 3)
            for (int i = 0; i < 3; ++i)
                if (auto d = pos->items[size_t(i)].get<double>()) l.pos[i] = *d;
        if (const List* rot = p->list("Rotation"); rot && rot->items.size() == 2) {
            if (auto v = rot->items[0].get<float>()) l.yaw = *v;
            if (auto v = rot->items[1].get<float>()) l.pitch = *v;
        }
        if (auto d = p->string("Dimension")) l.dimension = *d;
        l.air = static_cast<int>(std::clamp<int64_t>(p->integer("Air").value_or(300), -20, 300));
        l.fire = static_cast<int>(std::clamp<int64_t>(p->integer("Fire").value_or(-20), -20, 32767));
        l.xpLevel = static_cast<int>(std::clamp<int64_t>(p->integer("XpLevel").value_or(0), 0, 21863));
        l.xpProgress = std::clamp(float(p->real("XpP").value_or(0.0)), 0.0f, 1.0f);
        l.xpTotal = static_cast<int>(std::max<int64_t>(0, p->integer("XpTotal").value_or(0)));
        l.xpSeed = static_cast<int32_t>(p->integer("XpSeed").value_or(0));
        l.timeSinceRest = static_cast<int32_t>(std::clamp<int64_t>(p->integer("TimeSinceRest").value_or(0), 0, 1 << 30));
        if (const Compound* t = p->compound("warden_spawn_tracker")) {
            l.wardenLevel = int32_t(std::clamp<int64_t>(t->integer("warning_level").value_or(0), 0, 4));
            l.wardenTicks = int32_t(std::clamp<int64_t>(t->integer("ticks_since_last_warning").value_or(0), 0, 1 << 30));
            l.wardenCooldown = int32_t(std::clamp<int64_t>(t->integer("cooldown_ticks").value_or(0), 0, 1 << 30));
        }
        if (const List* effects = p->list("active_effects"))
            for (const Tag& t : effects->items)
                if (const Compound* c = t.get<Compound>(); c && c->string("id"))
                    l.effects.push_back({*c->string("id"),
                                         static_cast<int>(std::clamp<int64_t>(c->integer("amplifier").value_or(0), 0, 255)),
                                         static_cast<int>(std::clamp<int64_t>(c->integer("duration").value_or(0), 0, 1 << 30))});
        l.traderSpawnDelay = int(std::clamp<int64_t>(data->integer("WanderingTraderSpawnDelay").value_or(24000), 1, 24000));
        l.traderSpawnChance = int(std::clamp<int64_t>(data->integer("WanderingTraderSpawnChance").value_or(25), 25, 75));
        if (const Compound* raid = data->compound("Raid")) {
            const Tag* centre = raid->find("Center");
            if (const auto* c = centre ? centre->get<std::vector<int32_t>>() : nullptr; c && c->size() == 3) {
                l.raidActive = raid->integer("Active").value_or(1) != 0;
                for (int i = 0; i < 3; ++i) l.raidCentre[i] = (*c)[size_t(i)];
            }
            const Tag* omen = raid->find("OmenCenter");
            if (const auto* c = omen ? omen->get<std::vector<int32_t>>() : nullptr; c && c->size() == 3)
                for (int i = 0; i < 3; ++i) l.raidPendingCentre[i] = (*c)[size_t(i)];
            l.raidId = int32_t(std::clamp<int64_t>(raid->integer("Id").value_or(0), 0, 1 << 30));
            l.raidNextId = int32_t(std::clamp<int64_t>(raid->integer("NextAvailableID").value_or(1), 1, 1 << 30));
            l.raidPendingTicks = int(std::clamp<int64_t>(raid->integer("OmenTicks").value_or(0), 0, 600));
            l.raidPendingLevel = int(std::clamp<int64_t>(raid->integer("OmenLevel").value_or(1), 1, 5));
            l.raidWave = int(std::clamp<int64_t>(raid->integer("Wave").value_or(0), 0, 7));
            l.raidWaves = int(std::clamp<int64_t>(raid->integer("NumGroups").value_or(5), 1, 7));
            l.raidLevel = int(std::clamp<int64_t>(raid->integer("BadOmenLevel").value_or(1), 1, 5));
            l.raidTicks = int(std::clamp<int64_t>(raid->integer("TicksActive").value_or(0), 0, 48000));
            l.raidCooldown = int(std::clamp<int64_t>(raid->integer("PreRaidTicks").value_or(300), 0, 300));
            l.raidWaveHealth = std::max(0.0f, float(raid->real("TotalHealth").value_or(0.0)));
        }
        if (const Compound* f = data->compound("DragonFight")) {
            l.dragonKilled = f->integer("DragonKilled").value_or(0) != 0;
            l.dragonPreviouslyKilled = f->integer("PreviouslyKilled").value_or(0) != 0;
            if (const Tag* u = f->find("Dragon"))
                if (const auto* a = u->get<std::vector<int32_t>>(); a && a->size() == 4) {
                    l.dragonUuidHi = uint64_t(uint32_t((*a)[0])) << 32 | uint32_t((*a)[1]);
                    l.dragonUuidLo = uint64_t(uint32_t((*a)[2])) << 32 | uint32_t((*a)[3]);
                }
            if (const Tag* g = f->find("Gateways"))
                if (const auto* a = g->get<std::vector<int32_t>>()) {
                    l.gateways = *a;
                    l.hasGateways = true;
                }
        }
        if (const Compound* d = p->compound("LastDeathLocation"))
            if (const Tag* pos = d->find("pos"))
                if (const auto* a = pos->get<std::vector<int32_t>>(); a && a->size() == 3) {
                    const std::string* dim = d->string("dimension");
                    const auto dd = dim ? findDimension(*dim) : std::nullopt;
                    l.hasLastDeath = true;
                    l.lastDeathDimension = dd ? int(*dd) : 0;
                    for (int i = 0; i < 3; ++i)
                        l.lastDeath[i] = (*a)[size_t(i)];
                }
        if (const Compound* r = p->compound("respawn"))
            if (const Tag* pos = r->find("pos"))
                if (const auto* a = pos->get<std::vector<int32_t>>(); a && a->size() == 3) {
                    l.hasRespawn = true;
                    for (int i = 0; i < 3; ++i)
                        l.respawn[i] = (*a)[size_t(i)];
                }
        if (const Compound* a = p->compound("abilities")) l.flying = a->integer("flying").value_or(0) != 0;
        l.gameMode = int(std::clamp<int64_t>(p->integer("playerGameType").value_or(data->integer("GameType").value_or(1)), 0, 3));
        l.survival = l.gameMode == 0 || l.gameMode == 2;
        if (auto h = p->real("Health")) l.health = static_cast<float>(*h);
        l.food = static_cast<int>(p->integer("foodLevel").value_or(20));
        if (auto v = p->real("foodSaturationLevel")) l.saturation = static_cast<float>(*v);
        if (auto v = p->real("foodExhaustionLevel")) l.exhaustion = static_cast<float>(*v);
        l.foodTimer = static_cast<int>(p->integer("foodTickTimer").value_or(0));
        l.selectedSlot = static_cast<int>(p->integer("SelectedItemSlot").value_or(0)) % 9;
        auto readItem = [&](const Compound& item, int slot) {
            const std::string* id = item.string("id");
            if (!id) return;
            SavedItem saved;
            saved.slot = slot;
            saved.id = *id;
            saved.count = static_cast<int>(item.integer("count").value_or(1));
            std::string st = *id;
            const Compound* comps = item.compound("components");
            if (comps) {
                saved.damage = static_cast<int>(comps->integer("minecraft:damage").value_or(0));
                if (*id == "minecraft:filled_map") saved.damage = static_cast<int>(comps->integer("minecraft:map_id").value_or(0));
                saved.repairCost = static_cast<int>(comps->integer("minecraft:repair_cost").value_or(0));
                if (const Compound* pc = comps->compound("minecraft:potion_contents"))
                    if (const std::string* pid = pc->string("potion"))
                        saved.potion = pid->starts_with("minecraft:") ? pid->substr(10) : *pid;
                for (const char* key : {"minecraft:enchantments", "minecraft:stored_enchantments"})
                    if (const Compound* ench = comps->compound(key)) {
                        const Compound* levels = ench->compound("levels") ? ench->compound("levels") : ench;
                        saved.storedEnchantments = std::string_view(key) == "minecraft:stored_enchantments";
                        for (const auto& e : levels->entries)
                            if (const auto lvl = levels->integer(e.name)) saved.enchantments.emplace_back(e.name, int(*lvl));
                    }
                if (comps->list("minecraft:container") || comps->compound("minecraft:trim") ||
                    comps->compound("minecraft:lodestone_tracker") || comps->compound("minecraft:writable_book_content") ||
                    comps->compound("minecraft:written_book_content")) {
                    const ItemStack parsed = itemFromNbtPublic(item);
                    saved.contents = parsed.contents;
                    saved.trim = parsed.trim;
                    saved.extra = parsed.extra;
                }
            }
            const Compound* props = comps ? comps->compound("minecraft:block_state") : nullptr;
            if (props && !props->entries.empty()) {
                st += '[';
                for (size_t i = 0; i < props->entries.size(); ++i) {
                    const std::string* v = props->entries[i].value.get<std::string>();
                    if (i) st += ',';
                    st += props->entries[i].name + '=' + (v ? *v : std::string());
                }
                st += ']';
                saved.state = std::move(st);
            }
            l.inventory.push_back(std::move(saved));
        };
        if (const List* inv = p->list("Inventory"))
            for (const Tag& t : inv->items) {
                const Compound* item = t.get<Compound>();
                if (!item) continue;
                const auto slot = item->integer("Slot").value_or(-1);
                // 0..35; before 1.21.5 armor sat at 100..103 and the offhand at -106.
                if ((slot >= 0 && slot <= 35) || (slot >= 100 && slot <= 103)) readItem(*item, int(slot));
                else if (slot == -106) readItem(*item, 150);
            }
        if (const List* ender = p->list("EnderItems"))
            for (const Tag& t : ender->items)
                if (const Compound* item = t.get<Compound>()) {
                    const auto slot = item->integer("Slot").value_or(-1);
                    if (slot >= 0 && slot < 27) readItem(*item, 200 + int(slot));
                }
        if (const Compound* eq = p->compound("equipment")) {
            static constexpr std::pair<const char*, int> kKeys[5] = {
                {"feet", 100}, {"legs", 101}, {"chest", 102}, {"head", 103}, {"offhand", 150}};
            for (const auto& [key, slot] : kKeys)
                if (const Compound* item = eq->compound(key)) readItem(*item, slot);
        }
    }
    return l;
}

} // namespace mc::world
