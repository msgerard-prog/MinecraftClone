#include "world/Mob.h"

namespace mc::world {

const MobInfo& mobInfo(MobType t) {
    // wiki: Zombie (20 HP, 0.6 x 1.95, speed 0.23, 3 damage on normal), Cow (10 HP,
    // 0.9 x 1.4, speed 0.2, passive).
    static constexpr MobInfo kInfo[] = {
        {"minecraft:zombie", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        {"minecraft:cow", 10.0f, 0.9, 1.4, 0.2, 0.0f, false},
    };
    return kInfo[static_cast<int>(t)];
}

} // namespace mc::world
