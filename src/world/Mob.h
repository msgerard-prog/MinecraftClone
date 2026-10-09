#pragma once

#include "world/Villagers.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace mc::world {

enum class MobType : uint8_t {
    Zombie,
    Cow,
    Sheep,
    Pig,
    Chicken,
    Skeleton,
    Creeper,
    Spider,
    Enderman,
    // Nether (M19.2).
    Ghast,
    Blaze,
    MagmaCube,
    ZombifiedPiglin,
    Piglin,
    Hoglin,
    Strider,
    // The End (M20).
    EndCrystal, // not a mob in vanilla but an entity; it lives with the mobs here
    EnderDragon,
    Shulker,
    Minecart, // (M21.4: a vehicle, kept with the mobs)
    Slime,    // (M21.5)
    Villager, // (M24.1)
    ZombieVillager, // (M24.3)
    IronGolem,      // (M24.3)
    Witch,          // (M24.4)
    WanderingTrader, // (M24.4)
    Pillager,        // (M24.4)
    Vindicator,      // (M24.5)
    Evoker,
    Vex,
    Ravager,
    Cod,          // (M25.2: water mobs)
    Salmon,
    TropicalFish, // pattern in `size`, colours in `woolColour` (base) and `color2`
    Pufferfish,   // puff state 0..2 in `size`
    Squid,
    GlowSquid,
    Boat, // (M25.2b) its wood in `woolColour` (kBoatWoods); saved as "<wood>_boat" / "bamboo_raft"
    Drowned, // (M25.3) a zombie of the seas: swims, throws a trident if it holds one
    Dolphin, // (M25.3b) swims fast, gives swimming players Dolphin's Grace
    Turtle,  // (M25.3b) walks and swims; lays eggs on its home beach (`home`)
    Guardian,      // (M25.5) ocean monument guard: a charging laser
    ElderGuardian, // (M25.5) three per monument: a stronger laser, Mining Fatigue nearby
    Wolf,    // (M26.1) variant in woolColour (kWolfVariants), collar dye in color2
    Cat,     // (M26.1) variant in woolColour (kCatVariants), collar dye in color2
    Ocelot,  // (M26.1) trusting (fed fish) in `tamed`
    Parrot,  // (M26.1) variant 0-4 in woolColour
    Horse,       // (M26.2) coat colour in woolColour (kHorseColours), markings in color2
    Donkey,      // (M26.2) carries a chest (15 slots)
    Mule,        // (M26.2) a horse and a donkey's foal; carries a chest, never breeds
    Llama,       // (M26.2) variant in woolColour (kLlamaVariants); strength 1-5 (3 slots each)
    TraderLlama, // (M26.2) walks with a wandering trader (vanilla: on its lead)
    Camel,       // (M26.2) needs no taming; dashes, sits
    Rabbit,      // (M26.3) kind in woolColour (kRabbitKinds); hops
    Fox,         // (M26.3) red / snow in woolColour; sleeps by day, carries an item in its mouth
    PolarBear,   // (M26.3) neutral; fights for its cubs
    Panda,       // (M26.3) main / hidden gene in woolColour / color2 (kPandaGenes)
    Goat,        // (M26.3) rams; horns (bits 1 left, 2 right) in `horns`; screaming in `powered`
    Armadillo,   // (M26.3) rolls up when scared (`sitting`); sheds scutes
    Bee,         // (M26.3b) home: its hive (`home`); pollen in `nectar`
    Frog,        // (M26.3c) variant in woolColour (0 temperate, 1 warm, 2 cold); carrying spawn: `hasEgg`
    Tadpole,     // (M26.3c) grows into a frog (age counts up from -24000)
    Axolotl,     // (M26.3c) colour in woolColour (kAxolotlColours); plays dead (`spellTicks`)
    CaveSpider,     // (M26.4a) a small spider that poisons; from mineshaft spawners
    Silverfish,     // (M26.4a) hides in infested stone, calls the others out when hurt
    WitherSkeleton, // (M26.4a) Nether fortresses: its hits wither
    Phantom,        // (M26.4a) swoops on players who haven't slept for 3 days
    Wither,         // (M26.4b) the boss built of soul sand and wither skeleton skulls
    Breeze,         // (M26.4c) leaps about and shoots wind charges (trial chambers: M27)
    Allay,          // (M26.5a) holds a liked item (`mouthItem`), gathers more of it (`allayCount`)
    Nautilus,       // (M26.5a) tamed with pufferfish, ridden under water with a saddle
    HappyGhast,     // (M26.5b) grown from a dried ghast; worn harness colour in `decor`
    CopperGolem,    // (M26.5b) sorts items out of copper chests; oxidation in `woolColour`, waxed in `sheared`
    Creaking,       // (M27.1c) a creaking heart's guardian: frozen while watched; its heart at `home`
    Warden,         // (M27.3c) blind, hears vibrations, smells players; `phase` emerging/active/digging, anger in `angerTicks`
    Sniffer,        // (M27.5c) digs up ancient seeds now and then (`eggTicks` to the next dig, `phase` 1 digging)
    // Hanging entities (M28.3a; wiki: Item Frame, Painting): fixed to the block at `home`
    // on its face `phase` (world::Direction), centred at `pos`; an item frame's item is in
    // its chunk's mob store (slot 0), turned `node` x 45 degrees; a painting's variant
    // (world::kPaintings) is `woolColour`. Not mobs in vanilla either.
    ItemFrame,
    GlowItemFrame,
    Painting,
    // (M28.3b; wiki: Armor Stand) wears armor (its chunk's mob store, slots 0 head .. 3
    // feet; `worn` keeps each piece's material for drawing); two quick hits break it.
    ArmorStand,
    // (M28.3c; wiki: Lead) a lead tied to the fence at `home`; mobs tied there have
    // `leash` 2 and `leashPos` = that fence.
    LeashKnot,
    // Variants (M29.1a; wiki: Husk, Stray, Bogged, Parched): a desert zombie that doesn't
    // burn and starves what it hits; skeletons whose arrows slow, poison or weaken.
    Husk,
    Stray,
    Bogged,
    Parched,
    // Undead mounts (M29.1b; wiki: Skeleton Horse, Zombie Horse, Camel Husk, Zombie
    // Nautilus): ridden by monsters as jockeys; none of them breed.
    SkeletonHorse,
    ZombieHorse,
    CamelHusk,
    ZombieNautilus,
    // The rest of vanilla's roster (M29.1c; wiki pages of each).
    Bat,         // roosts upside down on ceilings (`sitting`)
    Endermite,   // sometimes left by a thrown ender pearl; gone after 2 minutes
    Mooshroom,   // red (woolColour 0) or brown (1); sheared into a cow and mushrooms
    SnowGolem,   // built of snow; throws snowballs at monsters; `sheared`: no pumpkin
    PiglinBrute, // bastion guard with a golden axe, always hostile
    Zoglin,      // a hoglin zombified outside the Nether
    Illusioner,  // a bow-shooting illager (not natural: commands only, as vanilla)
    Count
};

// Static facts per mob type (wiki: Zombie, Cow - health, hitbox, speed, damage).
struct MobInfo {
    std::string_view id;  // "minecraft:zombie"
    float maxHealth;
    double width, height; // hitbox
    double speed;         // movement_speed attribute (blocks/tick scale)
    float attackDamage;   // normal difficulty; 0 = passive
    bool hostile;
    bool fireImmune = false; // fire and lava don't hurt it (Nether mobs)
    bool flies = false;      // no gravity: ghasts, blazes hover
    float modelScale = 1.0f; // drawn this much larger than its model (ghast 4.5)
    bool swims = false;      // lives in water (M25.2): swims in 3D there, flops and suffocates on land
};
// Boat woods (M25.2b; wiki: Boat): the item and entity names, and a colour for our
// tinted model.
struct BoatWood {
    const char* name;  // "oak" (item "oak_boat"; bamboo makes "bamboo_raft")
    uint32_t colour;   // 0xRRGGBB, the planks' look
};
inline constexpr BoatWood kBoatWoods[10] = {{"oak", 0xB8945F},     {"spruce", 0x7A5A34},  {"birch", 0xD7C185},
                                            {"jungle", 0xB88764},  {"acacia", 0xBA6337},  {"dark_oak", 0x4F3218},
                                            {"mangrove", 0x773636}, {"cherry", 0xE7B7AE}, {"pale_oak", 0xE5DACD},
                                            {"bamboo", 0xC9B758}};
inline std::string boatId(int wood) { // entity and item id
    return std::string("minecraft:") + kBoatWoods[wood % 10].name + (wood % 10 == 9 ? "_raft" : "_boat");
}
inline std::string chestBoatId(int wood) { // (M26.2) a boat with a chest
    return std::string("minecraft:") + kBoatWoods[wood % 10].name + (wood % 10 == 9 ? "_chest_raft" : "_chest_boat");
}

// Pets (M26.1): what tames them and what they look like (wiki: Wolf, Cat, Parrot).
inline bool isPet(MobType t) {
    return t == MobType::Wolf || t == MobType::Cat || t == MobType::Parrot;
}
// Wolf variants (1.20.5, by biome) and their fur colour for our tinted model.
struct NamedColour {
    const char* name;
    uint32_t colour;
};
inline constexpr NamedColour kWolfVariants[9] = {{"pale", 0xD8D4CE},     {"woods", 0x8C6E50},  {"ashen", 0x9A9CA4},
                                                 {"black", 0x3A3634},    {"chestnut", 0x9A6A4A}, {"rusty", 0xB8703C},
                                                 {"spotted", 0xC8A880},  {"striped", 0xB89060}, {"snowy", 0xF0F0F0}};
inline constexpr NamedColour kCatVariants[11] = {{"tabby", 0x9C7A54},      {"black", 0x2E2A2A},   {"red", 0xD2783A},
                                                 {"siamese", 0xE8DCC4},    {"british_shorthair", 0x8A8E94},
                                                 {"calico", 0xD8B890},     {"persian", 0xE8C89A}, {"ragdoll", 0xEEE6DA},
                                                 {"white", 0xF4F4F0},      {"jellie", 0x5A5A60},  {"all_black", 0x1E1C1C}};
inline constexpr uint32_t kParrotColours[5] = {0xD02A20, 0x2850D8, 0x50C830, 0x30C8D8, 0xA8A8A8}; // red blue green cyan grey

// Mounts (M26.2; wiki: Horse, Donkey, Mule, Llama, Camel): ridden by the player.
inline bool isMount(MobType t) {
    return t == MobType::Horse || t == MobType::Donkey || t == MobType::Mule || t == MobType::Llama ||
           t == MobType::TraderLlama || t == MobType::Camel || t == MobType::Nautilus || t == MobType::HappyGhast ||
           t == MobType::SkeletonHorse || t == MobType::ZombieHorse || t == MobType::CamelHusk ||
           t == MobType::ZombieNautilus;
}
// Horses for riding, jumping and taming (M29.1b: the undead ones too).
inline bool isHorseKind(MobType t) {
    return t == MobType::Horse || t == MobType::Donkey || t == MobType::Mule || t == MobType::SkeletonHorse ||
           t == MobType::ZombieHorse;
}
inline bool isCamel(MobType t) { return t == MobType::Camel || t == MobType::CamelHusk; }
// (M29.3d) saddled and steered with a carrot / warped fungus on a stick.
inline bool isStickRidden(MobType t) { return t == MobType::Pig || t == MobType::Strider; }
// What a mob holds in its main hand for drawing (M29.1f): its saved item, a drowned's
// trident, else its kind's usual weapon (skeletons bows, pillagers crossbows...). 0: none.
uint16_t heldItemOf(const struct MobData& m);
// 1.21.5 farm animal variants (M29.1d), kept in `woolColour`: their vanilla names.
inline constexpr const char* kFarmVariants[3] = {"temperate", "warm", "cold"};
inline bool isNautilus(MobType t) { return t == MobType::Nautilus || t == MobType::ZombieNautilus; }
// The undead mounts never breed (wiki).
inline bool isUndeadMount(MobType t) {
    return t == MobType::SkeletonHorse || t == MobType::ZombieHorse || t == MobType::CamelHusk ||
           t == MobType::ZombieNautilus;
}
inline bool isLlama(MobType t) { return t == MobType::Llama || t == MobType::TraderLlama; }
// Takes a chest (donkeys, mules, llamas; chest boats are boats with `hasChest`).
inline bool canCarryChest(MobType t) { return t == MobType::Donkey || t == MobType::Mule || isLlama(t); }
// The 7 horse coat colours and 5 markings (wiki: Horse › Appearance; saved as Variant =
// colour | markings << 8).
inline constexpr NamedColour kHorseColours[7] = {{"white", 0xE8E4DC},  {"creamy", 0xC8A878},   {"chestnut", 0xA0603A},
                                                 {"brown", 0x6E4A2C},  {"black", 0x2C2624},    {"gray", 0x6E6A68},
                                                 {"dark_brown", 0x3E2A1C}};
inline constexpr const char* kHorseMarkings[5] = {"none", "white", "white_field", "white_dots", "black_dots"};
inline constexpr NamedColour kLlamaVariants[4] = {{"creamy", 0xD8C8A0}, {"white", 0xEEEAE2}, {"brown", 0x7A5A3C},
                                                  {"gray", 0x8A8682}};
// Horse armor (wiki: Horse Armor - leather 3, iron 5, golden 7, diamond 11 armor points),
// indexed by MobData::horseArmor (0: none).
inline constexpr const char* kHorseArmorItems[7] = {"", "leather_horse_armor", "iron_horse_armor",
                                                    "golden_horse_armor", "diamond_horse_armor",
                                                    "copper_horse_armor", "netherite_horse_armor"}; // (M29.3a)
inline constexpr int kHorseArmorPoints[7] = {0, 3, 5, 7, 11, 4, 19}; // (M29.3a: copper 4, netherite 19)
// Nautilus armor (M29.3a; wiki: Nautilus Armor - copper 4, iron 5, golden 7, diamond 11 (+2
// toughness), netherite 19 (+3)): a nautilus's or zombie nautilus's `horseArmor`.
inline constexpr const char* kNautilusArmorItems[6] = {"", "copper_nautilus_armor", "iron_nautilus_armor",
                                                       "golden_nautilus_armor", "diamond_nautilus_armor",
                                                       "netherite_nautilus_armor"};
inline constexpr int kNautilusArmorPoints[6] = {0, 4, 5, 7, 11, 19};
// The body armor items a mount takes (index = MobData::horseArmor).
inline std::span<const char* const> bodyArmorItems(MobType t) {
    if (t == MobType::Nautilus || t == MobType::ZombieNautilus) return kNautilusArmorItems;
    return kHorseArmorItems;
}
// Chest slots a mount (or chest boat) carries: donkeys and mules 15, llamas 3 per
// strength, chest boats 27 (wiki).
inline int chestSlots(MobType t, int strength) { // (minecarts: `strength` holds their slots - M29.3e)
    return t == MobType::Boat       ? 27
           : t == MobType::Minecart ? std::clamp(strength, 0, 27)
           : isLlama(t)             ? 3 * std::clamp(strength, 1, 5)
           : canCarryChest(t)       ? 15
                                    : 0;
}
// Minecart kinds (M29.3e; wiki: each minecart), kept in MobData::decor: their item and
// entity ids.
inline constexpr const char* kCartKinds[6] = {"minecart",        "chest_minecart", "furnace_minecart",
                                              "hopper_minecart", "tnt_minecart",   "command_block_minecart"};
inline int cartSlotsOf(int kind) { return kind == 1 ? 27 : kind == 3 ? 5 : 0; }

// Wildlife (M26.3; wiki: Rabbit, Fox, Panda, Goat, Armadillo).
inline constexpr NamedColour kRabbitKinds[6] = {{"brown", 0x8A6A4A},        {"white", 0xF2F2F2}, {"black", 0x2E2A2A},
                                                {"white_splotched", 0xD0D0D0}, {"gold", 0xE0C070}, {"salt", 0xB09A7A}};
inline constexpr const char* kPandaGenes[7] = {"normal", "lazy", "worried", "playful", "brown", "weak", "aggressive"};
// Brown and weak are recessive: they show only when both genes carry them.
inline int pandaPersonality(int main, int hidden) {
    const bool recessive = main == 4 || main == 5;
    return recessive && main != hidden ? 0 : main;
}
// Frog variants (wiki: Frog): temperate (orange), warm (white), cold (green) - and the
// froglight each makes of a magma cube: ochre, pearlescent, verdant.
inline constexpr NamedColour kFrogVariants[3] = {{"temperate", 0xC8783A}, {"warm", 0xE8E0D0}, {"cold", 0x5E9A4A}};
inline constexpr NamedColour kAxolotlColours[5] = {{"lucy", 0xF4A8C8}, {"wild", 0x8A6A4A}, {"gold", 0xF0C850},
                                                  {"cyan", 0xC8F0F0}, {"blue", 0x5A6AE0}};
inline bool isWildlife(MobType t) { // (M26.3: Wildlife.cpp's goals and ticks)
    return (t >= MobType::Rabbit && t <= MobType::Armadillo) || t == MobType::Frog;
}
// Goat horn instruments (wiki: Goat Horn), stored in the item's `damage`.
inline constexpr const char* kGoatHorns[8] = {"ponder", "sing", "seek", "feel", "admire", "call", "yearn", "dream"};

// Fish, squid (M25.2): water creatures.
inline bool isFish(MobType t) {
    return t == MobType::Cod || t == MobType::Salmon || t == MobType::TropicalFish || t == MobType::Pufferfish;
}
const MobInfo& mobInfo(MobType t);
struct MobData;
// A mob's top health: its own (mounts), a tamed wolf's 40, else its type's.
float maxHealthOf(const MobData& m);
// Spiders and cave spiders share their behaviour (climbing, neutral in the light, leaps).
inline bool isSpider(MobType t) { return t == MobType::Spider || t == MobType::CaveSpider; }
// Zombies and zombie villagers share their behaviour (targets, burning, drops).
inline bool isZombie(MobType t) {
    return t == MobType::Zombie || t == MobType::ZombieVillager || t == MobType::Drowned || t == MobType::Husk;
}
// Bow skeletons (M29.1a): the skeleton and its variants share its AI.
inline bool isSkeleton(MobType t) {
    return t == MobType::Skeleton || t == MobType::Stray || t == MobType::Bogged || t == MobType::Parched;
}
// Undead that burn under the open sky by day (wiki: Undead) - husks and parched don't.
inline bool burnsInDaylight(MobType t) {
    return ((isZombie(t) || isSkeleton(t)) && t != MobType::Husk && t != MobType::Parched) ||
           t == MobType::ZombieHorse || t == MobType::ZombieNautilus; // (unless armored)
}
// The undead (wiki: Undead): Smite hits them harder, the Wither leaves them alone.
// Hostile mobs that vanish on Peaceful (wiki: Difficulty; the ender dragon, shulkers
// and hoglins stay).
inline bool despawnsInPeaceful(MobType t) {
    return mobInfo(t).hostile && t != MobType::EnderDragon && t != MobType::Shulker && t != MobType::Hoglin;
}
inline bool isHanging(MobType t) {
    return t == MobType::ItemFrame || t == MobType::GlowItemFrame || t == MobType::Painting;
}
// (M28.3b) how a worn item is drawn on an armor stand: 1 leather, 2 chainmail, 3 iron,
// 4 gold, 5 diamond, 6 netherite, 7 turtle, 8 copper, 9 anything else (a block, a head).
inline uint8_t armorMaterial(std::string_view id) {
    if (id.empty()) return 0;
    static constexpr std::string_view kNames[8] = {"leather", "chainmail", "iron", "golden", "diamond", "netherite", "turtle", "copper"};
    for (int i = 0; i < 8; ++i)
        if (id.find(kNames[i]) != std::string_view::npos) return uint8_t(i + 1);
    return 9;
}
// (M28.3c; wiki: Lead › Usage) mobs a lead can hold: animals, golems, mounts, pets,
// squid... not villagers, traders, monsters (hoglins and zoglins are) or things.
inline bool isLeashable(MobType t) {
    switch (t) {
    case MobType::Villager: case MobType::WanderingTrader: case MobType::EndCrystal: case MobType::Minecart:
    case MobType::Boat: case MobType::ItemFrame: case MobType::GlowItemFrame: case MobType::Painting:
    case MobType::ArmorStand: case MobType::LeashKnot: case MobType::EnderDragon: case MobType::Wither:
    case MobType::Turtle: case MobType::Cod: case MobType::Salmon: case MobType::TropicalFish:
    case MobType::Pufferfish: case MobType::Tadpole: case MobType::Allay:
        return false;
    case MobType::Hoglin: return true;
    default: return !mobInfo(t).hostile;
    }
}
inline bool isUndead(MobType t) {
    return isZombie(t) || isSkeleton(t) || isUndeadMount(t) || t == MobType::WitherSkeleton || t == MobType::ZombifiedPiglin ||
           t == MobType::Phantom || t == MobType::Wither;
}
// Raid mobs (M24.5): they go after villagers, iron golems and wandering traders too.
inline bool isRaider(MobType t) {
    return t == MobType::Pillager || t == MobType::Vindicator || t == MobType::Evoker || t == MobType::Ravager ||
           t == MobType::Witch;
}

// Inside the world bounds vanilla accepts for entities (+-30,000,000 horizontally,
// +-20,000,000 vertically; wiki: World boundary) and finite.
inline bool isValidMobPosition(const glm::dvec3& p) {
    return std::abs(p.x) < 3.0e7 && std::abs(p.z) < 3.0e7 && std::abs(p.y) < 2.0e7; // false for NaN
}

// A mob's state: saved fields first, then AI / animation state (transient, but
// kept here so a mob is one plain value that moves between chunks).
struct MobData {
    MobType type = MobType::Cow;
    uint64_t uuidHi = 0, uuidLo = 0; // vanilla UUID (saved as 4 ints)
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    float yaw = 0.0f, prevYaw = 0.0f; // body
    float headYaw = 0.0f, pitch = 0.0f;
    float prevHeadYaw = 0.0f, prevPitch = 0.0f;
    float health = 10.0f;
    float fallDistance = 0.0f;
    int16_t hurtTime = 0;  // red tint ticks after damage
    int16_t deathTime = 0; // death animation ticks (removed at 20)
    int16_t fireTicks = 0;
    bool onGround = false;
    bool persistent = false; // never despawns (named, picked up items...)
    int noPlayerTicks = 0;   // despawn clock (wiki: Spawn › Despawning)
    // Animals (M16.3; wiki: Breeding, Sheep, Chicken). Saved as Age, InLove, Color,
    // Sheared, EggLayTime.
    int age = 0;            // < 0: a baby growing up (-24000 at birth); > 0: breeding cooldown
    int loveTicks = 0;      // in love mode after being fed (600)
    uint8_t woolColour = 0; // sheep: dye index (0 white .. 15 black); tropical fish: base colour
    uint8_t color2 = 0;     // tropical fish: pattern colour (dye index, M25.2)
    bool fromBucket = false; // (M25.2) a fish let out of a bucket: never despawns (FromBucket)
    int16_t airTicks = 300;  // (M25.2) water mobs' air out of water (Air)
    int8_t paddleForward = 0, paddleTurn = 0; // (M25.2b) a boat's rider input this tick (-1, 0, 1)
    float yawVel = 0.0f;                      // (M25.2b) a boat's turning momentum (degrees a tick)
    bool heldTrident = false;                 // (M25.3) a drowned holding a trident (equipment.mainhand)
    bool hasEgg = false;                      // (M25.3b) a turtle carrying eggs home (HasEgg)
    bool tamed = false;   // (M26.1) a pet of the player (Owner: the player's UUID); ocelots: trusting
    bool sitting = false; // (M26.1) ordered to sit (Sitting); camels: resting on the ground
    // Mounts (M26.2): taming temper (Temper), equipment, chests and their own stats
    // (vanilla attributes: max_health, movement_speed, jump_strength).
    int16_t temper = 0;
    int16_t tameCheck = 0;   // ticks until a wild mount ridden decides: tamed, or the rider thrown
    bool saddled = false;
    bool hasChest = false;   // donkeys, mules, llamas (ChestedHorse) and chest boats
    uint8_t horseArmor = 0;  // kHorseArmorItems index
    uint8_t decor = 0;       // a llama's carpet: dye colour + 1 (0: none)
    uint8_t strength = 3;    // llama: 1-5 (Strength)
    float maxHealth = 0.0f;  // its own top health (0: the type's)
    float moveSpeed = 0.0f;  // its own movement speed (0: the type's)
    float jumpStrength = 0.0f;
    int8_t riderJump = 0;    // the rider's jump: charge 1..100 released this tick (camels: dash)
    int16_t dashCooldown = 0; // camel: 55 ticks between dashes
    // Wildlife (M26.3).
    uint8_t horns = 3;        // goat: bit 0 left horn, bit 1 right horn
    ItemId mouthItem = 0;     // fox: what it carries (one item)
    int16_t armorWear = 0;    // a wolf's armor: damage it has taken (breaks at 64)
    bool nectar = false;      // (M26.3b) a bee carrying pollen home (HasNectar)
    bool stung = false;       // a bee that stung: it dies soon (HasStung)
    bool vanish = false;      // (not saved) gone without a death: a bee entering its hive
    uint8_t allayCount = 0;   // (M26.5a) how many of its liked item an allay carries
    int8_t chargedBlast = 0;  // (M26.4b) ticks left in which a death counts as a charged creeper's kill (a head)
    bool sheared = false;
    bool powered = false; // creeper struck by lightning: a charged creeper (twice the blast)
    int16_t ambientTime = 0; // ambient sound clock (not saved; vanilla ambientSoundTime)
    bool showBottom = true; // end crystals: drawn on a bedrock base (ShowBottom)
    std::array<uint8_t, 4> worn{}; // (M28.3b) an armor stand's armor materials, head..feet (armorMaterial; 0 none)
    uint8_t leash = 0;             // (M28.3c) on a lead: 0 no, 1 held by the player, 2 tied to the fence at `leashPos`
    glm::ivec3 leashPos{0};
    uint64_t caravanHead = 0;      // (M28.3c) a llama following another in a caravan (its UUID high half)
    // Jockeys (M29.1b): the mob this one rides (its UUID high half; saved as our
    // clone:Vehicle - vanilla nests Passengers); a mount whose rider chases moves to the
    // rider's goal (`jockeyChase`, refreshed each tick by Mobs::tick's riding pass).
    uint64_t vehicle = 0;
    bool jockeyChase = false;
    bool mobRidden = false; // (a monster sits on it: the player can't get on)
    bool skeletonTrap = false;   // (M29.1b) a skeleton trap horse (vanilla SkeletonTrap)
    uint16_t heldItem = 0;       // (M29.1b) its main-hand item other than a trident (ItemId; equipment.mainhand)
    int16_t zombifyTicks = 0;    // (M29.1c) a piglin or hoglin outside the Nether (vanilla TimeInOverworld)
    uint32_t nameId = 0;         // (M29.3b) a name-tag name (world::nameText; vanilla CustomName)
    // (M29.2c) lasting effects on the mob (world::Effect ids; vanilla active_effects): up to 4.
    struct ActiveEffect {
        uint8_t type = 0, amplifier = 0;
        int16_t ticks = 0;
    };
    std::array<ActiveEffect, 4> effects{};
    int effectLevel(uint8_t effect) const { // (0: none, else amplifier + 1)
        for (const ActiveEffect& e : effects)
            if (e.type == effect && e.ticks > 0) return e.amplifier + 1;
        return 0;
    }
    int eggTicks = 6000;    // chicken: ticks until the next egg
    int eatTicks = 0;       // sheep: eating-grass animation (40)
    int16_t breedTicks = 0; // time spent next to a partner in love
    // Hostiles 2 (M16.5; wiki: Creeper, Skeleton, Spider, Enderman).
    int16_t fuse = 0;           // creeper: swelling ticks (explodes at 30)
    int16_t shootTicks = 0;     // skeleton: drawing the bow
    bool angry = false;         // enderman stared at / spider or enderman hit
    uint16_t carried = 0;       // enderman: the block state it holds (0 = none)
    bool wantsTeleport = false; // enderman: hit by an arrow / in water - teleport away
    bool climbing = false;      // spider: against a wall last tick
    bool isBaby() const { return age < 0; }
    // AI
    glm::dvec3 goal{0.0};    // wander / chase target
    int goalTicks = 0;       // time spent on the current goal
    int panicTicks = 0;
    bool targeting = false; // a hostile chasing the player (seen it)
    uint8_t sightCheck = 0; // ticks since the last line-of-sight check
    int attackCooldown = 0;
    // Path (M16.2): block cells (feet) to walk through, from the pathfinder; not saved.
    static constexpr int kMaxPath = 32;
    std::array<glm::ivec3, kMaxPath> path{};
    uint8_t pathLength = 0, pathIndex = 0;
    int16_t repathTicks = 0;
    glm::ivec3 pathRequest{0, -100000, 0}; // the goal cell the current path was asked for
    bool lastHurtByPlayer = false;          // (spider eyes drop only for player kills)
    bool lastHurtBySkeleton = false;        // (M23.6: creepers shot by skeletons drop a disc)
    uint8_t looting = 0;                    // Looting level of the player's last hit
    int16_t stareTicks = 0;                 // enderman: ticks the player has looked at it
    int16_t angerTicks = 0;                 // enderman: anger left (calms down at 0)
    // Nether mobs (M19.2).
    uint8_t size = 1;       // magma cube: 1, 2 or 4 (saved as Size 0, 1, 3)
    int16_t chargeTicks = 0; // ghast/blaze: charging a shot; blaze: volley timing
    uint8_t volley = 0;      // blaze: fireballs left in this volley
    int16_t jumpTicks = 0;   // magma cube: ticks to its next jump
    bool angerAlert = false; // zombified piglin: just hit - the ones around join in
    int16_t admireTicks = 0; // piglin: inspecting a gold ingot (barters at the end)
    // The ender dragon (M20.2): its phase uses vanilla's DragonPhase numbers (saved).
    uint8_t phase = 0;
    int16_t phaseTicks = 0;
    uint8_t node = 0;         // holding pattern: the ring node it flies to
    int8_t nodeStep = 1;      // around the ring clockwise or not
    float lastHealth = 0.0f;  // health at the last tick (damage taken while perched)
    float perchDamage = 0.0f; // damage taken since it landed (takes off at 50)
    bool hasBeam = false;     // an end crystal heals it: the beam starts at `beam`
    uint8_t peek = 0;         // shulker: how far its lid is open, 0..100 (saved as Peek)
    bool ridden = false;      // minecart: the player sits in it (not saved: vanilla saves passengers)
    glm::dvec3 beam{0.0};
    // Villagers (M24; saved as VillagerData {type, profession, level}, Xp, Offers and
    // the Brain's home / job_site / meeting_point memories).
    uint8_t profession = 0;   // world::Profession
    uint8_t villagerType = 0; // world::VillagerType
    uint8_t villagerLevel = 1; // 1 novice .. 5 master
    int villagerXp = 0;
    glm::ivec3 home{0, kNoPoint, 0}, jobSite{0, kNoPoint, 0}, meetingPoint{0, kNoPoint, 0};
    bool sleeping = false;    // in its bed at night (drawn lying down)
    int16_t poiSearch = 0;    // ticks to the next look for a bed / job site
    uint8_t restocksToday = 0; // (M24.2) two restocks a day at the job site
    int64_t lastRestockDay = -1;
    uint8_t offerCount = 0;
    int16_t tradingTicks = 0; // (not saved) a player has its trading screen open: it stands still
    // Zombie villagers (M24.3): Weakness from a splash potion (a golden apple then cures),
    // the cure's countdown (saved as ConversionTime); a zombie's villager target.
    int16_t weaknessTicks = 0;
    int16_t convertTicks = 0;
    uint64_t targetUuid = 0;
    // A villager's food (M24.3; vanilla keeps an 8-slot Inventory): bread, carrots,
    // potatoes, beetroots, wheat and wheat seeds - saved as Inventory.
    std::array<uint8_t, 6> food{};
    int16_t breedTogether = 0; // ticks next to a willing partner
    glm::ivec3 workTarget{0, kNoPoint, 0}; // a farmer's ripe crop
    // Witches (M24.4): the potion being drunk and its countdown; fire resistance left.
    uint8_t drinking = 0;
    int16_t drinkTicks = 0;
    int16_t fireResistTicks = 0;
    int despawnDelay = 0; // wandering trader: ticks until it leaves (saved as DespawnDelay)
    bool playerCreated = false; // an iron golem the player built: never turns on the player (vanilla PlayerCreated)
    bool captain = false; // a patrol / raid captain (M24.4; drops an ominous bottle)
    int32_t raidId = 0;   // (M24.5) the raid it belongs to, 0: none (vanilla RaidId)
    int16_t spellTicks = 0; // evoker: casting (fangs, vexes); vex: life left
    std::array<TradeOffer, kMaxOffers> offers{};
    float limbSwing = 0.0f, limbSwingAmount = 0.0f; // walk animation
};

} // namespace mc::world
