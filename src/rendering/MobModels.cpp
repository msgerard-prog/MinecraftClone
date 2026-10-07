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

} // namespace

std::span<const MobPart> mobModel(world::MobType type) {
    if (type == world::MobType::Zombie) return kZombie;
    return kCow;
}

} // namespace mc::gfx
