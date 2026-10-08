#pragma once

#include "world/Villagers.h"

#include <glm/glm.hpp>

#include <array>
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

// Fish, squid (M25.2): water creatures.
inline bool isFish(MobType t) {
    return t == MobType::Cod || t == MobType::Salmon || t == MobType::TropicalFish || t == MobType::Pufferfish;
}
const MobInfo& mobInfo(MobType t);
// Zombies and zombie villagers share their behaviour (targets, burning, drops).
inline bool isZombie(MobType t) { return t == MobType::Zombie || t == MobType::ZombieVillager || t == MobType::Drowned; }
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
    bool sheared = false;
    bool powered = false; // creeper struck by lightning: a charged creeper (twice the blast)
    int16_t ambientTime = 0; // ambient sound clock (not saved; vanilla ambientSoundTime)
    bool showBottom = true; // end crystals: drawn on a bedrock base (ShowBottom)
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
