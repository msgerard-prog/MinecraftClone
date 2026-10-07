#include "rendering/MobModels.h"

#include <array>

namespace mc::gfx {

namespace {

using A = MobPart::Anim;

// Humanoid (zombie): the skin layout's boxes; arms reach forward.
constexpr std::array<MobPart, 6> kZombie = {{
    {{-4, 24, -4}, {4, 32, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-4, 12, -2}, {4, 24, 2}, {0, 24, 0}, 16, 16, A::None},
    {{-8, 12, -2}, {-4, 24, 2}, {-6, 22, 0}, 40, 16, A::ArmForward},
    {{4, 12, -2}, {8, 24, 2}, {6, 22, 0}, 32, 48, A::ArmForward},
    {{-4, 0, -2}, {0, 12, 2}, {-2, 12, 0}, 0, 16, A::LegA},
    {{0, 0, -2}, {4, 12, 2}, {2, 12, 0}, 16, 48, A::LegB},
}};

// Cow: long body, four legs, head with horns in front (+Z).
constexpr std::array<MobPart, 8> kCow = {{
    {{-4, 16, 9}, {4, 24, 15}, {0, 20, 9}, 0, 0, A::Head},
    {{-5, 22, 11}, {-4, 25, 12}, {0, 20, 9}, 22, 0, A::Head},
    {{4, 22, 11}, {5, 25, 12}, {0, 20, 9}, 22, 0, A::Head},
    {{-5, 12, -9}, {5, 22, 9}, {0, 12, 0}, 0, 16, A::None},
    {{-5, 0, 5}, {-1, 12, 9}, {-3, 12, 7}, 0, 48, A::LegA},
    {{1, 0, 5}, {5, 12, 9}, {3, 12, 7}, 0, 48, A::LegB},
    {{-5, 0, -9}, {-1, 12, -5}, {-3, 12, -7}, 0, 48, A::LegB},
    {{1, 0, -9}, {5, 12, -5}, {3, 12, -7}, 0, 48, A::LegA},
}};

// Sheep: skin body/head/legs plus a wool layer over them (same UVs, wool texture).
constexpr std::array<MobPart, 12> kSheep = {{
    {{-3, 16, 8}, {3, 22, 16}, {0, 18, 8}, 0, 0, A::Head},
    {{-4, 12, -8}, {4, 20, 8}, {0, 12, 0}, 0, 16, A::None},
    {{-5, 0, 4}, {-1, 12, 8}, {-3, 12, 6}, 0, 40, A::LegA},
    {{1, 0, 4}, {5, 12, 8}, {3, 12, 6}, 0, 40, A::LegB},
    {{-5, 0, -8}, {-1, 12, -4}, {-3, 12, -6}, 0, 40, A::LegB},
    {{1, 0, -8}, {5, 12, -4}, {3, 12, -6}, 0, 40, A::LegA},
    {{-3, 16, 8}, {3, 22, 16}, {0, 18, 8}, 0, 0, A::Head, 1, 0.6f},
    {{-4, 12, -8}, {4, 20, 8}, {0, 12, 0}, 0, 16, A::None, 1, 1.75f},
    {{-5, 6, 4}, {-1, 12, 8}, {-3, 12, 6}, 0, 40, A::LegA, 1, 0.5f},
    {{1, 6, 4}, {5, 12, 8}, {3, 12, 6}, 0, 40, A::LegB, 1, 0.5f},
    {{-5, 6, -8}, {-1, 12, -4}, {-3, 12, -6}, 0, 40, A::LegB, 1, 0.5f},
    {{1, 6, -8}, {5, 12, -4}, {3, 12, -6}, 0, 40, A::LegA, 1, 0.5f},
}};

// Pig: wide body on short legs, square head with a snout.
constexpr std::array<MobPart, 7> kPig = {{
    {{-4, 8, 8}, {4, 16, 16}, {0, 12, 8}, 0, 0, A::Head},
    {{-2, 9, 16}, {2, 12, 17}, {0, 12, 8}, 32, 0, A::Head},
    {{-5, 6, -8}, {5, 14, 8}, {0, 6, 0}, 0, 16, A::None},
    {{-5, 0, 4}, {-1, 6, 8}, {-3, 6, 6}, 0, 40, A::LegA},
    {{1, 0, 4}, {5, 6, 8}, {3, 6, 6}, 0, 40, A::LegB},
    {{-5, 0, -8}, {-1, 6, -4}, {-3, 6, -6}, 0, 40, A::LegB},
    {{1, 0, -8}, {5, 6, -4}, {3, 6, -6}, 0, 40, A::LegA},
}};

// Chicken: round body, small head with beak and wattle, wings, two thin legs.
constexpr std::array<MobPart, 8> kChicken = {{
    {{-2, 9, 3}, {2, 15, 6}, {0, 9, 4}, 28, 0, A::Head},
    {{-2, 11, 6}, {2, 13, 8}, {0, 9, 4}, 42, 0, A::Head},
    {{-1, 9, 6}, {1, 11, 7}, {0, 9, 4}, 42, 4, A::Head},
    {{-3, 4, -4}, {3, 10, 4}, {0, 4, 0}, 0, 0, A::None},
    {{-4, 5, -3}, {-3, 9, 3}, {-3, 9, 0}, 0, 16, A::None},
    {{3, 5, -3}, {4, 9, 3}, {3, 9, 0}, 0, 16, A::None},
    {{-2, 0, -1}, {-1, 4, 1}, {-1.5f, 4, 0}, 16, 16, A::LegA},
    {{1, 0, -1}, {2, 4, 1}, {1.5f, 4, 0}, 16, 16, A::LegB},
}};

} // namespace

std::span<const MobPart> mobModel(world::MobType type) {
    switch (type) {
    case world::MobType::Zombie: return kZombie;
    case world::MobType::Sheep: return kSheep;
    case world::MobType::Pig: return kPig;
    case world::MobType::Chicken: return kChicken;
    default: return kCow;
    }
}

} // namespace mc::gfx
