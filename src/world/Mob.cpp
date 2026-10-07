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
    };
    return kInfo[static_cast<int>(t)];
}

} // namespace mc::world
