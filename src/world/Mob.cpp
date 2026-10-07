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
    };
    return kInfo[static_cast<int>(t)];
}

} // namespace mc::world
