#include "world/Sounds.h"

#include <array>

namespace mc::world {

namespace {

constexpr std::string_view kMaterialNames[] = {"stone", "wood", "gravel", "grass", "sand",
                                               "wool",  "snow", "metal",  "glass"};

bool contains(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

// Block -> sound group from its id (wiki: each block's Sounds table; our grouping by
// name, built once).
SoundType groupFor(std::string_view n) {
    if (contains(n, "glass") || contains(n, "ice") || n == "glowstone" || contains(n, "sea_lantern") ||
        contains(n, "beacon") || contains(n, "end_portal_frame"))
        return SoundType::Glass;
    if (contains(n, "wool") || contains(n, "carpet") || n == "cactus" || contains(n, "cake") || n == "sponge")
        return SoundType::Wool;
    if (contains(n, "snow") || n == "powder_snow") return SoundType::Snow;
    if ((contains(n, "sand") && !contains(n, "sandstone")) || contains(n, "concrete_powder")) return SoundType::Sand;
    if (n == "soul_soil" || n == "clay" || n == "red_sand") return SoundType::Sand;
    if (n == "gravel" || n == "dirt" || n == "coarse_dirt" || n == "farmland" || n == "dirt_path" || n == "rooted_dirt" ||
        n == "mud")
        return SoundType::Gravel;
    if (contains(n, "grass") || contains(n, "leaves") || contains(n, "sapling") || contains(n, "flower") ||
        contains(n, "tulip") || contains(n, "fern") || contains(n, "vine") || contains(n, "mushroom") ||
        contains(n, "podzol") || contains(n, "mycelium") || contains(n, "hay") || contains(n, "sugar_cane") ||
        contains(n, "wheat") || contains(n, "carrots") || contains(n, "potatoes") || contains(n, "beetroots") ||
        contains(n, "dandelion") || contains(n, "poppy") || contains(n, "orchid") || contains(n, "allium") ||
        contains(n, "azure") || contains(n, "daisy") || contains(n, "cornflower") || contains(n, "lily") ||
        contains(n, "bush") || contains(n, "pumpkin") || contains(n, "melon") || contains(n, "kelp") ||
        contains(n, "sponge") || contains(n, "nylium") || contains(n, "fungus") || contains(n, "roots") ||
        contains(n, "wart") || contains(n, "chorus") || n == "tnt" || n == "slime_block")
        return SoundType::Grass;
    if (contains(n, "planks") || contains(n, "log") || contains(n, "wood") || contains(n, "stem") ||
        contains(n, "hyphae") || contains(n, "fence") || contains(n, "chest") || contains(n, "crafting") ||
        contains(n, "bookshelf") || contains(n, "door") || contains(n, "sign") || n == "ladder" ||
        n == "note_block" || n == "jukebox" || contains(n, "barrel") || contains(n, "lectern") ||
        contains(n, "bed") || contains(n, "torch") || contains(n, "lever") || contains(n, "composter"))
        return SoundType::Wood;
    if (contains(n, "iron") || contains(n, "gold") || contains(n, "copper") || contains(n, "anvil") ||
        n == "hopper" || contains(n, "rail") || n == "diamond_block" || n == "emerald_block" ||
        n == "netherite_block" || n == "lapis_block" || n == "redstone_block" || contains(n, "cauldron") ||
        contains(n, "chain") || contains(n, "lantern") || n == "bell" || n == "brewing_stand")
        return SoundType::Metal;
    return SoundType::Stone;
}

std::vector<SoundInfo> buildTable() {
    std::vector<SoundInfo> t(static_cast<size_t>(kSoundCount));
    auto set = [&](Sound s, std::string id, std::vector<std::string> files, float volume, float p0, float p1) {
        t[size_t(s)] = {std::move(id), std::move(files), volume, p0, p1};
    };
    auto variants = [](const std::string& base, int n) {
        std::vector<std::string> v;
        for (int i = 1; i <= n; ++i)
            v.push_back(base + std::to_string(i));
        return v;
    };
    // Vanilla's pitch spreads: most sounds 0.8-1.2 or so; explosions lower.
    set(Sound::Explode, "entity.generic.explode", variants("random/explode", 3), 4.0f, 0.56f, 0.84f);
    set(Sound::ItemPickup, "entity.item.pickup", {"random/pop"}, 0.2f, 1.6f, 3.4f); // (wiki: ((r - r) x 0.7 + 1) x 2)
    set(Sound::OrbPickup, "entity.experience_orb.pickup", {"random/orb"}, 0.1f, 0.55f, 1.25f);
    set(Sound::LevelUp, "entity.player.levelup", {"random/levelup"}, 0.75f, 1.0f, 1.0f);
    set(Sound::BowShoot, "entity.arrow.shoot", {"random/bow"}, 1.0f, 0.8f, 1.2f);
    set(Sound::ArrowHit, "entity.arrow.hit", variants("random/bowhit", 2), 1.0f, 1.0f, 1.3f);
    set(Sound::DoorOpen, "block.wooden_door.open", {"random/door_open"}, 1.0f, 0.9f, 1.0f);
    set(Sound::DoorClose, "block.wooden_door.close", {"random/door_close"}, 1.0f, 0.9f, 1.0f);
    set(Sound::ChestOpen, "block.chest.open", {"random/chestopen"}, 0.5f, 0.9f, 1.0f);
    { // note blocks: a fixed pitch per sample, shifted by the note (M23.6)
        static constexpr const char* kInstruments[16][2] = {
            {"harp", "harp"}, {"basedrum", "bd"}, {"snare", "snare"}, {"hat", "hat"}, {"bass", "bass"},
            {"flute", "flute"}, {"bell", "bell"}, {"guitar", "guitar"}, {"chime", "chime"},
            {"xylophone", "xylophone"}, {"iron_xylophone", "iron_xylophone"}, {"cow_bell", "cow_bell"},
            {"didgeridoo", "didgeridoo"}, {"bit", "bit"}, {"banjo", "banjo"}, {"pling", "pling"}};
        for (int i = 0; i < 16; ++i)
            set(static_cast<Sound>(int(Sound::NoteHarp) + i), std::string("block.note_block.") + kInstruments[i][0],
                {std::string("note/") + kInstruments[i][1]}, 3.0f, 1.0f, 1.0f);
    }
    set(Sound::ChestClose, "block.chest.close", {"random/chestclosed"}, 0.5f, 0.9f, 1.0f);
    set(Sound::Click, "block.lever.click", {"random/click"}, 0.3f, 0.6f, 0.6f); // (0.5 switching off)
    set(Sound::WoodClick, "block.wooden_button.click_on", {"random/wood_click"}, 0.3f, 0.6f, 0.6f);
    set(Sound::Fuse, "entity.tnt.primed", {"random/fuse"}, 1.0f, 1.0f, 1.0f);
    set(Sound::Fizz, "block.lava.extinguish", {"random/fizz"}, 0.5f, 1.8f, 3.4f);
    set(Sound::Eat, "entity.generic.eat", variants("random/eat", 2), 0.5f, 0.8f, 1.2f);
    set(Sound::Drink, "entity.generic.drink", {"random/drink"}, 0.5f, 0.9f, 1.0f);
    set(Sound::Burp, "entity.player.burp", {"random/burp"}, 0.5f, 0.9f, 1.0f);
    set(Sound::Splash, "entity.player.splash", {"random/splash"}, 0.3f, 0.6f, 1.4f);
    set(Sound::Swim, "entity.player.swim", variants("liquid/swim", 2), 0.2f, 0.6f, 1.4f);
    set(Sound::PlayerHurt, "entity.player.hurt", variants("mob/player/hurt", 3), 1.0f, 0.8f, 1.2f);
    set(Sound::GlassBreak, "block.glass.break", variants("random/glass", 2), 1.0f, 0.8f, 1.0f);
    set(Sound::AnvilLand, "block.anvil.land", {"random/anvil_land"}, 0.3f, 0.9f, 1.1f);
    set(Sound::AnvilUse, "block.anvil.use", {"random/anvil_use"}, 1.0f, 0.9f, 1.1f);
    set(Sound::Enchant, "block.enchantment_table.use", {"random/enchant"}, 1.0f, 0.9f, 1.1f);
    set(Sound::PistonOut, "block.piston.extend", {"tile/piston/out"}, 0.5f, 0.6f, 0.85f);
    set(Sound::PistonIn, "block.piston.contract", {"tile/piston/in"}, 0.5f, 0.6f, 0.75f);
    set(Sound::FireAmbient, "block.fire.ambient", {"fire/fire"}, 1.0f, 0.3f, 1.0f);
    set(Sound::LavaPop, "block.lava.pop", {"liquid/lavapop"}, 0.2f, 0.9f, 1.05f);
    set(Sound::Rain, "weather.rain", variants("ambient/weather/rain", 2), 0.2f, 1.0f, 1.0f); // (above: 0.1, pitch 0.5)
    set(Sound::Thunder, "entity.lightning_bolt.thunder", variants("ambient/weather/thunder", 2), 10000.0f, 0.8f, 1.0f);
    set(Sound::PortalAmbient, "block.portal.ambient", {"portal/portal"}, 0.5f, 0.8f, 1.2f);
    set(Sound::Teleport, "entity.enderman.teleport", {"mob/endermen/portal"}, 1.0f, 1.0f, 1.0f);
    set(Sound::Crit, "entity.player.attack.crit", {"damage/crit"}, 1.0f, 1.0f, 1.0f);
    set(Sound::AttackHit, "entity.player.attack.strong", {"damage/hit"}, 1.0f, 0.9f, 1.1f);
    set(Sound::ToolBreak, "entity.item.break", {"random/break"}, 0.8f, 0.8f, 1.2f);
    set(Sound::Minecart, "entity.minecart.riding", {"random/minecart"}, 0.4f, 1.0f, 1.0f);
    set(Sound::SuccessfulHit, "entity.arrow.hit_player", {"random/successful_hit"}, 0.18f, 0.45f, 0.45f);
    // Blocks (vanilla SoundType: break and place at (volume + 1) / 2 and pitch 0.8,
    // steps at 0.15, mining hits at 1/8 volume and pitch 0.5).
    for (int m = 0; m < int(SoundType::Count); ++m) {
        const std::string name(kMaterialNames[m]);
        const auto type = static_cast<SoundType>(m);
        const bool glass = type == SoundType::Glass;
        set(blockSound(type, BlockSound::Break), "block." + name + ".break",
            glass ? variants("random/glass", 2) : variants("dig/" + name, 3), 1.0f, 0.8f, 0.8f);
        set(blockSound(type, BlockSound::Step), "block." + name + ".step", variants("step/" + name, 3), 0.15f, 1.0f,
            1.0f);
        set(blockSound(type, BlockSound::Place), "block." + name + ".place", variants("dig/" + name, 3), 1.0f, 0.8f,
            0.8f);
        set(blockSound(type, BlockSound::Hit), "block." + name + ".hit", variants("step/" + name, 3), 0.25f, 0.5f,
            0.5f);
    }
    // Mobs: say1.., hurt1.., death (missing ones fall back to hurt, or stay silent).
    // Vanilla mob pitch: 1 + (random - random) x 0.2 (babies +0.5, added by the caller).
    static constexpr int kSays[] = {3, 3, 3, 3, 3, 3, 0, 2, 3, 3, 2, 0, 3, 3, 3, 2, 0, 2, 2, 0, 0, 3, 3, 0, 3, 3, 3, 3, 3, 3, 3,
                                    0, 0, 0, 0, 3, 3};
    static constexpr int kHurts[] = {2, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                     2, 2, 2, 2, 2, 2};
    static constexpr bool kDeath[] = {true, false, false, true, false, true, true, true, true, true, true,
                                      true, true, true, true, true, false, true, true, false, true, true, true, true, true, true, true, true, true, true, true,
                                      true, true, true, true, true, true};
    static_assert(std::size(kSays) == size_t(MobType::Count) && std::size(kHurts) == size_t(MobType::Count) &&
                  std::size(kDeath) == size_t(MobType::Count));
    for (int i = 0; i < int(MobType::Count); ++i) {
        const auto type = static_cast<MobType>(i);
        std::string name(mobInfo(type).id.substr(10)); // "minecraft:zombie" -> "zombie"
        const std::string dir = "mob/" + name + "/";
        const std::vector<std::string> hurt = variants(dir + "hurt", kHurts[i]);
        set(mobSound(type, MobSound::Ambient), "entity." + name + ".ambient", variants(dir + "say", kSays[i]),
            type == MobType::EnderDragon ? 5.0f : type == MobType::Ghast ? 5.0f : 1.0f, 0.8f, 1.2f);
        set(mobSound(type, MobSound::Hurt), "entity." + name + ".hurt", hurt, 1.0f, 0.8f, 1.2f);
        set(mobSound(type, MobSound::Death), "entity." + name + ".death",
            kDeath[i] ? std::vector<std::string>{dir + "death"} : hurt, 1.0f, 0.8f, 1.2f);
    }
    return t;
}

} // namespace

SoundType soundTypeOf(BlockStateId state) {
    static const std::vector<SoundType> table = [] {
        const auto& r = blockRegistry();
        std::vector<SoundType> v(r.blockCount());
        for (size_t b = 0; b < v.size(); ++b) {
            const BlockDef& def = r.block(BlockId(b));
            // Slabs, stairs and walls sound like the block they're cut from.
            const BlockId from = def.settings.kind != BlockKind::Plain ? def.settings.base : BlockId(b);
            v[b] = groupFor(std::string_view(r.block(from).id).substr(10)); // (after "minecraft:")
        }
        return v;
    }();
    const BlockId b = blockRegistry().blockOf(state);
    return size_t(b) < table.size() ? table[size_t(b)] : SoundType::Stone;
}

Sound blockSoundOf(BlockStateId state, BlockSound kind) { return blockSound(soundTypeOf(state), kind); }

const SoundInfo& soundInfo(Sound s) {
    static const std::vector<SoundInfo> table = buildTable();
    return table[size_t(s) < table.size() ? size_t(s) : 0];
}

} // namespace mc::world
