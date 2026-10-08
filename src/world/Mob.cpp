#include "world/Mob.h"

namespace mc::world {

const MobInfo& mobInfo(MobType t) {
    // wiki: Zombie (20 HP, 0.6 x 1.95, speed 0.23, 3 damage on normal), Cow (10 HP,
    // 0.9 x 1.4, speed 0.2, passive).
    static constexpr MobInfo kInfo[] = {
        {"minecraft:zombie", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        {"minecraft:cow", 10.0f, 0.9, 1.4, 0.2, 0.0f, false},
        // wiki: Sheep (8 HP, 0.9 x 1.3, 0.23), Pig (10 HP, 0.9 x 0.9, 0.25), Chicken
        // (4 HP, 0.4 x 0.7, 0.25).
        {"minecraft:sheep", 8.0f, 0.9, 1.3, 0.23, 0.0f, false},
        {"minecraft:pig", 10.0f, 0.9, 0.9, 0.25, 0.0f, false},
        {"minecraft:chicken", 4.0f, 0.4, 0.7, 0.25, 0.0f, false},
        // wiki: Skeleton (20, 0.6 x 1.99, 0.25, arrows), Creeper (20, 0.6 x 1.7, 0.25,
        // explodes), Spider (16, 1.4 x 0.9, 0.3, 2), Enderman (40, 0.6 x 2.9, 0.3, 7).
        {"minecraft:skeleton", 20.0f, 0.6, 1.99, 0.25, 0.0f, true},
        {"minecraft:creeper", 20.0f, 0.6, 1.7, 0.25, 0.0f, true},
        {"minecraft:spider", 16.0f, 1.4, 0.9, 0.3, 2.0f, true},
        {"minecraft:enderman", 40.0f, 0.6, 2.9, 0.3, 7.0f, true},
        // wiki: Ghast (10, 4 x 4, fireballs, flies), Blaze (20, 0.6 x 1.8, 6 melee,
        // fireballs, hovers), Magma Cube (large: 16, 2.08 x 2.08, 6; smaller sizes scale
        // in Mobs::box), Zombified Piglin (20, 0.6 x 1.95, 8 with its golden sword) -
        // all immune to fire and lava.
        {"minecraft:ghast", 10.0f, 4.0, 4.0, 0.7, 0.0f, true, true, true, 4.5f},
        {"minecraft:blaze", 20.0f, 0.6, 1.8, 0.23, 6.0f, true, true, true},
        {"minecraft:magma_cube", 16.0f, 2.08, 2.08, 0.2, 6.0f, true, true},
        {"minecraft:zombified_piglin", 20.0f, 0.6, 1.95, 0.23, 8.0f, true, true},
        // wiki: Piglin (16, 0.6 x 1.95, 0.35, 8 with a golden sword on normal), Hoglin
        // (40, 1.3965 x 1.4, 0.3, 3-8: ours 6), Strider (20, 0.9 x 1.7, 0.175, passive,
        // fire immune).
        {"minecraft:piglin", 16.0f, 0.6, 1.95, 0.35, 8.0f, true},
        {"minecraft:hoglin", 40.0f, 1.3965, 1.4, 0.3, 6.0f, true, false, false, 2.0f},
        {"minecraft:strider", 20.0f, 0.9, 1.7, 0.175, 0.0f, false, true},
        // wiki: End Crystal - a 2x2x2 entity that any damage destroys (an explosion of
        // power 6); no gravity; fire doesn't harm it.
        {"minecraft:end_crystal", 1.0f, 2.0, 2.0, 0.0, 0.0f, false, true, true, 1.6f},
        // wiki: Ender Dragon - 200 health, 10 damage from its head (Normal); flies
        // through blocks; drawn 4x its model (about 14 blocks long, 12 across the wings).
        // Our box is its body (vanilla: several part boxes, 16 x 8 overall).
        {"minecraft:ender_dragon", 200.0f, 6.0, 3.0, 0.0, 10.0f, true, true, true, 4.0f},
        // wiki: Shulker - 30 health, a 1x1x1 box stuck to a block, its bullets hit for
        // 4 and levitate; armour 20 while closed (our: 80% less damage).
        {"minecraft:shulker", 30.0f, 1.0, 1.0, 0.0, 4.0f, true, true, true},
        // wiki: Minecart - 0.98 x 0.7; a couple of hits break it (ours: 2 health).
        {"minecraft:minecart", 2.0f, 0.98, 0.7, 0.0, 0.0f, false, false, true},
        // wiki: Slime (large: 16, 2.08 x 2.08, 4; sizes as the magma cube's)
        {"minecraft:slime", 16.0f, 2.08, 2.08, 0.2, 4.0f, true},
        // wiki: Villager - 20 health, 0.6 x 1.95, movement speed 0.5 (strolls at 0.6 of it).
        {"minecraft:villager", 20.0f, 0.6, 1.95, 0.5, 0.0f, false},
        // wiki: Zombie Villager - as a zombie (20, 0.6 x 1.95, 0.23, 3 on normal).
        {"minecraft:zombie_villager", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        // wiki: Iron Golem - 100 health, 1.4 x 2.7, speed 0.25, hits for 7.5-21.5 (ours:
        // the attack adds up to 15 at random), never hostile on its own.
        {"minecraft:iron_golem", 100.0f, 1.4, 2.7, 0.25, 7.5f, false},
        // wiki: Witch - 26 health, 0.6 x 1.95, speed 0.25, throws potions.
        {"minecraft:witch", 26.0f, 0.6, 1.95, 0.25, 0.0f, true},
    };
    return kInfo[static_cast<int>(t)];
}

} // namespace mc::world
