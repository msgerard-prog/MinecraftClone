#pragma once

#include "world/Blocks.h"
#include "world/Mob.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mc::world {

// Sound events (M22.4; wiki: Sounds.json - vanilla's event ids). Gameplay code queues
// them with World::playSound; main plays them through audio/SoundEngine. Each event
// picks one of its files at random (our own WAVs, assets/minecraft/sounds/, ADR 0008)
// and a pitch in its range.

// Block sound groups (vanilla SoundType): what a block sounds like when broken,
// placed, stepped on and mined.
enum class SoundType : uint8_t { Stone, Wood, Gravel, Grass, Sand, Wool, Snow, Metal, Glass, Count };
SoundType soundTypeOf(BlockStateId state);
enum class BlockSound : uint8_t { Break, Step, Place, Hit };
enum class MobSound : uint8_t { Ambient, Hurt, Death };

enum class Sound : uint16_t {
    // The rest of the ids are computed: blockSound() and mobSound().
    Explode,
    ItemPickup,  // entity.item.pickup
    OrbPickup,   // entity.experience_orb.pickup
    LevelUp,     // entity.player.levelup
    BowShoot,    // entity.arrow.shoot
    ArrowHit,    // entity.arrow.hit
    DoorOpen,    // block.wooden_door.open (doors, trapdoors, gates)
    DoorClose,
    ChestOpen,
    ChestClose,
    Click,       // block.lever.click / stone button
    WoodClick,   // wooden button, pressure plate
    Fuse,        // entity.tnt.primed / entity.creeper.primed
    Fizz,        // block.fire.extinguish, lava meeting water
    Eat,
    Drink,
    Burp,
    Splash,      // entity.player.splash
    Swim,
    PlayerHurt,  // entity.player.hurt
    GlassBreak,  // block.glass.break
    AnvilLand,
    AnvilUse,
    Enchant,
    PistonOut,
    PistonIn,
    FireAmbient,
    LavaPop,
    Rain,        // weather.rain
    Thunder,     // entity.lightning_bolt.thunder
    PortalAmbient,
    Teleport,    // entity.enderman.teleport
    Crit,        // entity.player.attack.crit
    AttackHit,   // entity.player.attack.strong
    ToolBreak,   // entity.item.break
    Minecart,
    SuccessfulHit,
    kMiscCount,
};
inline constexpr int kBlockSoundBase = int(Sound::kMiscCount);
inline constexpr int kMobSoundBase = kBlockSoundBase + int(SoundType::Count) * 4;
inline constexpr int kSoundCount = kMobSoundBase + int(MobType::Count) * 3;
inline Sound blockSound(SoundType type, BlockSound kind) {
    return static_cast<Sound>(kBlockSoundBase + int(type) * 4 + int(kind));
}
inline Sound mobSound(MobType type, MobSound kind) {
    return static_cast<Sound>(kMobSoundBase + int(type) * 3 + int(kind));
}
// A block's sound for this action (glass breaks with its own sound).
Sound blockSoundOf(BlockStateId state, BlockSound kind);

struct SoundInfo {
    std::string id;                 // vanilla event id ("block.stone.break")
    std::vector<std::string> files; // under assets/minecraft/sounds/, no extension
    float volume = 1.0f;            // >1 reaches farther (16 blocks x volume)
    float pitchMin = 1.0f, pitchMax = 1.0f;
};
const SoundInfo& soundInfo(Sound s); // built once on first use (load time)

// One queued sound (World::playSound).
struct SoundEvent {
    Sound sound;
    double x, y, z;
    float volume = 1.0f, pitch = 1.0f; // multiplied with the event's own
};

} // namespace mc::world
