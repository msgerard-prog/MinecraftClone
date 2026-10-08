#include "world/Beehives.h"

#include "world/Blocks.h"
#include "world/World.h"

namespace mc::world {

MobData beeFromHive(const HiveBee& b, const glm::dvec3& at, const BlockPos& hive) {
    MobData m;
    m.type = MobType::Bee;
    m.uuidHi = b.uuidHi;
    m.uuidLo = b.uuidLo;
    m.pos = m.prevPos = m.goal = at;
    m.health = b.health;
    m.age = b.age;
    m.nectar = b.nectar;
    m.home = {hive.x, hive.y, hive.z};
    m.persistent = true; // (bees never despawn)
    return m;
}

void releaseBees(Chunk& chunk, const BlockPos& hive, BeehiveData& data, bool angry) {
    for (int i = 0; i < data.count && i < 3; ++i) {
        const glm::dvec3 at(hive.x + 0.5 + (i - 1) * 0.3, hive.y + 1.1, hive.z + 0.5);
        MobData m = beeFromHive(data.bees[size_t(i)], at, hive);
        if (angry) {
            m.angry = true;
            m.angerTicks = int16_t(400 + i * 37);
        }
        chunk.mobs().push_back(m);
    }
    data.count = 0;
    chunk.markDirty();
}

bool hiveSmoked(const World& world, const BlockPos& hive) {
    const auto& r = blockRegistry();
    for (int dy = 1; dy <= 5; ++dy) {
        const BlockStateId s = world.getBlock({hive.x, hive.y - dy, hive.z});
        const BlockId b = r.blockOf(s);
        if ((b == blocks::Campfire || b == blocks::SoulCampfire) && r.get(s, properties::lit) == 0) return true;
        if (r.collides(s) && b != blocks::Campfire && b != blocks::SoulCampfire) return false; // (smoke blocked)
    }
    return false;
}

} // namespace mc::world
