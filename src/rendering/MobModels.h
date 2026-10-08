#pragma once

#include "world/Mob.h"

#include <iterator>
#include <span>

namespace mc::gfx {

// Cuboid mob models (our own, matching tools/textures/gen_entities.py's box-UV
// layouts). Units: pixels (1/16 block), origin at the feet centre, +Y up, the mob
// facing +Z. Parts rotate about their pivot for animation.
struct MobPart {
    // LegA/B swing opposite; Lift: raised by a shulker's Peek (up to 8 px).
    enum class Anim : uint8_t { None, Head, LegA, LegB, ArmForward, WingL, WingR, Lift, Tail };
    float from[3], to[3];
    float pivot[3];
    int u, v; // box-UV origin in the mob's 64x64 texture
    Anim anim;
    // 1 = sheep wool: drawn from the wool texture, grown by `inflate` pixels (same
    // UV), tinted by the wool colour, hidden once sheared (vanilla's wool layer).
    // 2 = an end crystal's base, hidden without ShowBottom.
    // 3 = a villager's profession robe: drawn from the apron texture, tinted.
    // 4, 5 = a tropical fish's body and pattern, tinted by its two colours (M25.2).
    // 6 = a boat's planks, tinted by its wood (M25.2b).
    // 7 = a pet's collar, shown when tamed, tinted by its dye (M26.1).
    // 8 = fur/feathers tinted by the variant (wolves, cats, parrots - M26.1; horses, llamas).
    // 9-12 = mount gear from the mount-gear texture (M26.2): 9 saddle (when saddled),
    // 10 horse armor (tinted by its material; a wolf's armor too), 11 chest packs (with a
    // chest), 12 a llama's carpet (tinted by its dye).
    // 13, 14 = a goat's left / right horn (shown while it has it - M26.3).
    uint8_t layer = 0;
    float inflate = 0.0f;
};

std::span<const MobPart> mobModel(world::MobType type);
std::span<const MobPart> chestBoatModel(); // (M26.2: a boat with a chest)
// Rows of the stacked mob atlas (64 x 64 each): one per mob type, then sheep wool,
// then projectiles (the arrow: 16 x 5 at (0,0), tip at +x).
inline constexpr int kMobTextureRows = static_cast<int>(world::MobType::Count) + 4;
inline constexpr int kSheepWoolRow = static_cast<int>(world::MobType::Count);
inline constexpr int kProjectileRow = static_cast<int>(world::MobType::Count) + 1;
inline constexpr int kVillagerApronRow = static_cast<int>(world::MobType::Count) + 2; // (M24.1)
inline constexpr int kMountGearRow = static_cast<int>(world::MobType::Count) + 3;     // (M26.2)
inline int mobTextureRow(world::MobType type) { return static_cast<int>(type); }
inline const char* mobTexturePath(int row) {
    static constexpr const char* kPaths[] = {
        "assets/minecraft/textures/entity/clone/zombie.png", "assets/minecraft/textures/entity/clone/cow.png",
        "assets/minecraft/textures/entity/clone/sheep.png",  "assets/minecraft/textures/entity/clone/pig.png",
        "assets/minecraft/textures/entity/clone/chicken.png", "assets/minecraft/textures/entity/clone/skeleton.png",
        "assets/minecraft/textures/entity/clone/creeper.png", "assets/minecraft/textures/entity/clone/spider.png",
        "assets/minecraft/textures/entity/clone/enderman.png", "assets/minecraft/textures/entity/clone/ghast.png",
        "assets/minecraft/textures/entity/clone/blaze.png",  "assets/minecraft/textures/entity/clone/magma_cube.png",
        "assets/minecraft/textures/entity/clone/zombified_piglin.png",
        "assets/minecraft/textures/entity/clone/piglin.png",  "assets/minecraft/textures/entity/clone/hoglin.png",
        "assets/minecraft/textures/entity/clone/strider.png",
        "assets/minecraft/textures/entity/clone/end_crystal.png",
        "assets/minecraft/textures/entity/clone/ender_dragon.png",
        "assets/minecraft/textures/entity/clone/shulker.png",
        "assets/minecraft/textures/entity/clone/minecart.png",
        "assets/minecraft/textures/entity/clone/slime.png",
        "assets/minecraft/textures/entity/clone/villager.png",
        "assets/minecraft/textures/entity/clone/zombie_villager.png",
        "assets/minecraft/textures/entity/clone/iron_golem.png",
        "assets/minecraft/textures/entity/clone/witch.png",
        "assets/minecraft/textures/entity/clone/wandering_trader.png",
        "assets/minecraft/textures/entity/clone/pillager.png",
        "assets/minecraft/textures/entity/clone/vindicator.png",
        "assets/minecraft/textures/entity/clone/evoker.png",
        "assets/minecraft/textures/entity/clone/vex.png",
        "assets/minecraft/textures/entity/clone/ravager.png",
        "assets/minecraft/textures/entity/clone/cod.png",
        "assets/minecraft/textures/entity/clone/salmon.png",
        "assets/minecraft/textures/entity/clone/tropical_fish.png",
        "assets/minecraft/textures/entity/clone/pufferfish.png",
        "assets/minecraft/textures/entity/clone/squid.png",
        "assets/minecraft/textures/entity/clone/glow_squid.png",
        "assets/minecraft/textures/entity/clone/boat.png",
        "assets/minecraft/textures/entity/clone/drowned.png",
        "assets/minecraft/textures/entity/clone/dolphin.png",
        "assets/minecraft/textures/entity/clone/turtle.png",
        "assets/minecraft/textures/entity/clone/guardian.png",
        "assets/minecraft/textures/entity/clone/elder_guardian.png",
        "assets/minecraft/textures/entity/clone/wolf.png",
        "assets/minecraft/textures/entity/clone/cat.png",
        "assets/minecraft/textures/entity/clone/ocelot.png",
        "assets/minecraft/textures/entity/clone/parrot.png",
        "assets/minecraft/textures/entity/clone/horse.png",
        "assets/minecraft/textures/entity/clone/donkey.png",
        "assets/minecraft/textures/entity/clone/mule.png",
        "assets/minecraft/textures/entity/clone/llama.png",
        "assets/minecraft/textures/entity/clone/trader_llama.png",
        "assets/minecraft/textures/entity/clone/camel.png",
        "assets/minecraft/textures/entity/clone/rabbit.png",
        "assets/minecraft/textures/entity/clone/fox.png",
        "assets/minecraft/textures/entity/clone/polar_bear.png",
        "assets/minecraft/textures/entity/clone/panda.png",
        "assets/minecraft/textures/entity/clone/goat.png",
        "assets/minecraft/textures/entity/clone/armadillo.png",
        "assets/minecraft/textures/entity/clone/sheep_wool.png",
        "assets/minecraft/textures/entity/clone/projectiles.png",
        "assets/minecraft/textures/entity/clone/villager_apron.png",
        "assets/minecraft/textures/entity/clone/mount_gear.png"};
    static_assert(std::size(kPaths) == size_t(kMobTextureRows)); // (one skin per row)
    return kPaths[row];
}
// Wool colours by dye index (wiki: Dye - the colours of the 16 dyes).
inline constexpr uint32_t kWoolColours[16] = {0xF9FFFE, 0xF9801D, 0xC74EBD, 0x3AB3DA, 0xFED83D, 0x80C71F,
                                              0xF38BAA, 0x474F52, 0x9D9D97, 0x169C9C, 0x8932B8, 0x3C44AA,
                                              0x835432, 0x5E7C16, 0xB02E26, 0x1D1D21};

} // namespace mc::gfx
