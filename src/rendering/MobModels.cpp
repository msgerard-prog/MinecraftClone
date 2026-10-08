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

// Skeleton: the humanoid shape with thin 2x12x2 limbs; arms forward (holding a bow).
constexpr std::array<MobPart, 6> kSkeleton = {{
    {{-4, 24, -4}, {4, 32, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-4, 12, -2}, {4, 24, 2}, {0, 24, 0}, 16, 16, A::None},
    {{-6, 12, -1}, {-4, 24, 1}, {-5, 22, 0}, 40, 16, A::ArmForward},
    {{4, 12, -1}, {6, 24, 1}, {5, 22, 0}, 40, 16, A::ArmForward},
    {{-3, 0, -1}, {-1, 12, 1}, {-2, 12, 0}, 0, 16, A::LegA},
    {{1, 0, -1}, {3, 12, 1}, {2, 12, 0}, 0, 16, A::LegB},
}};

// Creeper: head on a tall body, four short legs.
constexpr std::array<MobPart, 6> kCreeper = {{
    {{-4, 18, -4}, {4, 26, 4}, {0, 18, 0}, 0, 0, A::Head},
    {{-4, 6, -2}, {4, 18, 2}, {0, 6, 0}, 16, 16, A::None},
    {{-4, 0, 2}, {0, 6, 6}, {-2, 6, 4}, 0, 16, A::LegA},
    {{0, 0, 2}, {4, 6, 6}, {2, 6, 4}, 0, 16, A::LegB},
    {{-4, 0, -6}, {0, 6, -2}, {-2, 6, -4}, 0, 16, A::LegB},
    {{0, 0, -6}, {4, 6, -2}, {2, 6, -4}, 0, 16, A::LegA},
}};

// Spider: head, thorax and a big abdomen low to the ground; eight legs splayed sideways.
constexpr std::array<MobPart, 11> kSpider = {{
    {{-4, 5, 3}, {4, 13, 11}, {0, 9, 3}, 0, 0, A::Head},
    {{-3, 6, -3}, {3, 12, 3}, {0, 9, 0}, 32, 0, A::None},
    {{-5, 5, -15}, {5, 13, -3}, {0, 9, -3}, 0, 16, A::None},
    {{3, 8, 1}, {19, 10, 3}, {3, 9, 2}, 0, 40, A::None},
    {{3, 8, -1}, {19, 10, 1}, {3, 9, 0}, 0, 40, A::None},
    {{3, 8, -3}, {19, 10, -1}, {3, 9, -2}, 0, 40, A::None},
    {{3, 8, -5}, {19, 10, -3}, {3, 9, -4}, 0, 40, A::None},
    {{-19, 8, 1}, {-3, 10, 3}, {-3, 9, 2}, 0, 40, A::None},
    {{-19, 8, -1}, {-3, 10, 1}, {-3, 9, 0}, 0, 40, A::None},
    {{-19, 8, -3}, {-3, 10, -1}, {-3, 9, -2}, 0, 40, A::None},
    {{-19, 8, -5}, {-3, 10, -3}, {-3, 9, -4}, 0, 40, A::None},
}};

// Enderman: tall and thin - 30-pixel legs and arms, small body and head on top.
constexpr std::array<MobPart, 6> kEnderman = {{
    {{-4, 42, -4}, {4, 50, 4}, {0, 42, 0}, 0, 0, A::Head},
    {{-4, 30, -2}, {4, 42, 2}, {0, 30, 0}, 16, 16, A::None},
    {{-6, 12, -1}, {-4, 42, 1}, {-5, 40, 0}, 0, 16, A::LegB},
    {{4, 12, -1}, {6, 42, 1}, {5, 40, 0}, 0, 16, A::LegA},
    {{-3, 0, -1}, {-1, 30, 1}, {-2, 30, 0}, 0, 16, A::LegA},
    {{1, 0, -1}, {3, 30, 1}, {2, 30, 0}, 0, 16, A::LegB},
}};

// Ghast (M19.2): a 16-pixel cube (drawn 4.5x: MobInfo::modelScale) with nine
// tentacles hanging below it in a 3x3 grid.
constexpr std::array<MobPart, 10> kGhast = {{
    {{-8, 0, -8}, {8, 16, 8}, {0, 8, 0}, 0, 0, A::None},
    {{-6, -9, -6}, {-4, 0, -4}, {-5, 0, -5}, 0, 32, A::LegA},
    {{-1, -9, -6}, {1, 0, -4}, {0, 0, -5}, 0, 32, A::LegB},
    {{4, -9, -6}, {6, 0, -4}, {5, 0, -5}, 0, 32, A::LegA},
    {{-6, -9, -1}, {-4, 0, 1}, {-5, 0, 0}, 0, 32, A::LegB},
    {{-1, -9, -1}, {1, 0, 1}, {0, 0, 0}, 0, 32, A::LegA},
    {{4, -9, -1}, {6, 0, 1}, {5, 0, 0}, 0, 32, A::LegB},
    {{-6, -9, 4}, {-4, 0, 6}, {-5, 0, 5}, 0, 32, A::LegA},
    {{-1, -9, 4}, {1, 0, 6}, {0, 0, 5}, 0, 32, A::LegB},
    {{4, -9, 4}, {6, 0, 6}, {5, 0, 5}, 0, 32, A::LegA},
}};

// Blaze: a head over three rings of four rods (vanilla spins them; ours stand still).
constexpr std::array<MobPart, 13> kBlaze = {{
    {{-4, 20, -4}, {4, 28, 4}, {0, 20, 0}, 0, 0, A::Head},
    {{-9, 13, -1}, {-7, 21, 1}, {0, 0, 0}, 0, 16, A::None},
    {{7, 13, -1}, {9, 21, 1}, {0, 0, 0}, 0, 16, A::None},
    {{-1, 13, -9}, {1, 21, -7}, {0, 0, 0}, 0, 16, A::None},
    {{-1, 13, 7}, {1, 21, 9}, {0, 0, 0}, 0, 16, A::None},
    {{-6, 6, -6}, {-4, 14, -4}, {0, 0, 0}, 0, 16, A::None},
    {{4, 6, 4}, {6, 14, 6}, {0, 0, 0}, 0, 16, A::None},
    {{4, 6, -6}, {6, 14, -4}, {0, 0, 0}, 0, 16, A::None},
    {{-6, 6, 4}, {-4, 14, 6}, {0, 0, 0}, 0, 16, A::None},
    {{-4, 0, -1}, {-2, 8, 1}, {0, 0, 0}, 0, 16, A::None},
    {{2, 0, -1}, {4, 8, 1}, {0, 0, 0}, 0, 16, A::None},
    {{-1, 0, -4}, {1, 8, -2}, {0, 0, 0}, 0, 16, A::None},
    {{-1, 0, 2}, {1, 8, 4}, {0, 0, 0}, 0, 16, A::None},
}};

// Magma cube: one 8-pixel cube, drawn at its size (1, 2 or 4 times).
constexpr std::array<MobPart, 1> kMagmaCube = {{
    {{-4, 0, -4}, {4, 8, 4}, {0, 0, 0}, 0, 0, A::None},
}};

// Zombified piglin: the humanoid with a wide pig head, snout and ears; one arm forward.
constexpr std::array<MobPart, 9> kZombifiedPiglin = {{
    {{-5, 24, -4}, {5, 32, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-2, 24, 4}, {2, 27, 5}, {0, 24, 0}, 40, 0, A::Head},
    {{-6, 26, -2}, {-5, 31, 2}, {0, 24, 0}, 52, 0, A::Head},
    {{5, 26, -2}, {6, 31, 2}, {0, 24, 0}, 52, 0, A::Head},
    {{-4, 12, -2}, {4, 24, 2}, {0, 24, 0}, 16, 16, A::None},
    {{-8, 12, -2}, {-4, 24, 2}, {-6, 22, 0}, 40, 16, A::ArmForward},
    {{4, 12, -2}, {8, 24, 2}, {6, 22, 0}, 32, 48, A::LegA},
    {{-4, 0, -2}, {0, 12, 2}, {-2, 12, 0}, 0, 16, A::LegA},
    {{0, 0, -2}, {4, 12, 2}, {2, 12, 0}, 16, 48, A::LegB},
}};

// Hoglin: a boar at half size (drawn 2x): head with tusks low in front, a long body,
// four short legs.
constexpr std::array<MobPart, 8> kHoglin = {{
    {{-3.5f, 4, 5}, {3.5f, 10, 14}, {0, 9, 5}, 0, 0, A::Head},
    {{-4.5f, 5, 13}, {-3.5f, 8, 14}, {0, 9, 5}, 40, 0, A::Head},
    {{3.5f, 5, 13}, {4.5f, 8, 14}, {0, 9, 5}, 40, 0, A::Head},
    {{-4, 6, -6}, {4, 13, 6}, {0, 6, 0}, 0, 16, A::None},
    {{-4, 0, 2}, {-1, 6, 5}, {-2.5f, 6, 3.5f}, 0, 40, A::LegA},
    {{1, 0, 2}, {4, 6, 5}, {2.5f, 6, 3.5f}, 0, 40, A::LegB},
    {{-4, 0, -5}, {-1, 6, -2}, {-2.5f, 6, -3.5f}, 0, 40, A::LegB},
    {{1, 0, -5}, {4, 6, -2}, {2.5f, 6, -3.5f}, 0, 40, A::LegA},
}};

// Strider: a big body on two long legs.
constexpr std::array<MobPart, 3> kStrider = {{
    {{-8, 14, -8}, {8, 28, 8}, {0, 14, 0}, 0, 0, A::None},
    {{-6, 0, -2}, {-2, 14, 2}, {-4, 14, 0}, 0, 32, A::LegA},
    {{2, 0, -2}, {6, 14, 2}, {4, 14, 0}, 0, 32, A::LegB},
}};

// End crystal: a bedrock-like base, a glass cube around a pink core turning about its
// middle (vanilla: two glass cubes tumbling on tilted axes, bobbing up and down).
constexpr std::array<MobPart, 3> kEndCrystal = {{
    {{-6, 0, -6}, {6, 4, 6}, {0, 0, 0}, 0, 40, A::None, 2},
    {{-4, 10, -4}, {4, 18, 4}, {0, 14, 0}, 0, 16, A::Head},
    {{-3, 11, -3}, {3, 17, 3}, {0, 14, 0}, 0, 0, A::Head},
}};

// Ender dragon (drawn 4x): body, neck and head forward (+Z), a three-piece tail,
// thin wings that beat (vanilla: a far more detailed model with legs and jaws).
constexpr std::array<MobPart, 8> kEnderDragon = {{
    {{-4, 0, -8}, {4, 6, 8}, {0, 3, 0}, 0, 0, A::None},
    {{-2, 1, 8}, {2, 5, 16}, {0, 3, 8}, 0, 22, A::None},
    {{-3, 0.5f, 16}, {3, 5.5f, 24}, {0, 3, 16}, 24, 22, A::Head},
    {{-1.5f, 1.5f, -16}, {1.5f, 4.5f, -8}, {0, 3, -8}, 0, 35, A::None},
    {{-1.5f, 1.5f, -24}, {1.5f, 4.5f, -16}, {0, 3, -16}, 0, 35, A::None},
    {{-1.5f, 1.5f, -32}, {1.5f, 4.5f, -24}, {0, 3, -24}, 0, 35, A::None},
    {{4, 5, -5}, {24, 6, 5}, {4, 5.5f, 0}, 0, 46, A::WingL},
    {{-24, 5, -5}, {-4, 6, 5}, {-4, 5.5f, 0}, 0, 46, A::WingR},
}};

// Shulker: a shell base, a lid that lifts as it opens, and the head inside.
constexpr std::array<MobPart, 3> kShulker = {{
    {{-7.9f, 0, -7.9f}, {7.9f, 8, 7.9f}, {0, 0, 0}, 0, 28, A::None},
    {{-8, 4, -8}, {8, 16, 8}, {0, 0, 0}, 0, 0, A::Lift},
    {{-3, 5, -3}, {3, 11, 3}, {0, 0, 0}, 0, 52, A::Lift},
}};

// Minecart: an open iron box, long along its travel (+Z).
constexpr std::array<MobPart, 5> kMinecart = {{
    {{-7, 1, -9}, {7, 3, 9}, {0, 0, 0}, 0, 0, A::None},
    {{-7, 3, -9}, {-5, 9, 9}, {0, 0, 0}, 0, 20, A::None},
    {{5, 3, -9}, {7, 9, 9}, {0, 0, 0}, 0, 20, A::None},
    {{-5, 3, -9}, {5, 9, -7}, {0, 0, 0}, 40, 20, A::None},
    {{-5, 3, 7}, {5, 9, 9}, {0, 0, 0}, 40, 20, A::None},
}};

// Villager (M24.1): a long head with a nose, a body under its profession robe, arms
// crossed in front, two legs.
constexpr std::array<MobPart, 7> kVillager = {{
    {{-4, 24, -4}, {4, 34, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-1, 23, 4}, {1, 27, 6}, {0, 24, 0}, 24, 0, A::Head},
    {{-4, 12, -3}, {4, 24, 3}, {0, 24, 0}, 16, 20, A::None},
    {{-4, 16, 3}, {4, 20, 7}, {0, 18, 0}, 0, 40, A::None},
    {{-4, 0, -2}, {0, 12, 2}, {-2, 12, 0}, 0, 48, A::LegA},
    {{0, 0, -2}, {4, 12, 2}, {2, 12, 0}, 0, 48, A::LegB},
    {{-4, 6, -3}, {4, 24, 3}, {0, 24, 0}, 16, 20, A::None, 3, 0.5f},
}};

// Witch (M24.4): the villager's build (robe in its own skin) under a pointed hat.
constexpr std::array<MobPart, 9> kWitch = {{
    {{-4, 24, -4}, {4, 34, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-1, 23, 4}, {1, 27, 6}, {0, 24, 0}, 24, 0, A::Head},
    {{-4, 6, -3}, {4, 24, 3}, {0, 24, 0}, 16, 20, A::None},
    {{-4, 16, 3}, {4, 20, 7}, {0, 18, 0}, 0, 40, A::None},
    {{-4, 0, -2}, {0, 12, 2}, {-2, 12, 0}, 0, 48, A::LegA},
    {{0, 0, -2}, {4, 12, 2}, {2, 12, 0}, 0, 48, A::LegB},
    {{-5, 33, -5}, {5, 34, 5}, {0, 24, 0}, 24, 40, A::Head},
    {{-3.5f, 34, -3.5f}, {3.5f, 38, 3.5f}, {0, 24, 0}, 24, 51, A::Head},
    {{-2, 38, -2}, {2, 41, 2}, {0, 24, 0}, 40, 0, A::Head},
}};

// Illagers (M24.4 pillager): the villager's head and body, arms held forward (a
// crossbow), legs.
constexpr std::array<MobPart, 7> kIllager = {{
    {{-4, 24, -4}, {4, 34, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-1, 23, 4}, {1, 27, 6}, {0, 24, 0}, 24, 0, A::Head},
    {{-4, 12, -3}, {4, 24, 3}, {0, 24, 0}, 16, 20, A::None},
    {{-8, 12, -2}, {-4, 24, 2}, {-6, 22, 0}, 40, 16, A::ArmForward},
    {{4, 12, -2}, {8, 24, 2}, {6, 22, 0}, 40, 16, A::ArmForward},
    {{-4, 0, -2}, {0, 12, 2}, {-2, 12, 0}, 0, 48, A::LegA},
    {{0, 0, -2}, {4, 12, 2}, {2, 12, 0}, 0, 48, A::LegB},
}};

// Iron golem (M24.3): a big body on a narrow waist, long arms swinging with the legs.
constexpr std::array<MobPart, 8> kIronGolem = {{
    {{-4, 33, -4}, {4, 43, 4}, {0, 33, 0}, 0, 0, A::Head},
    {{-1, 35, 4}, {1, 39, 6}, {0, 33, 0}, 32, 0, A::Head},
    {{-9, 21, -5.5f}, {9, 33, 5.5f}, {0, 21, 0}, 0, 41, A::None},
    {{-4.5f, 16, -3}, {4.5f, 21, 3}, {0, 16, 0}, 0, 41, A::None},
    {{-13, 10, -3}, {-9, 40, 3}, {-11, 38, 0}, 40, 0, A::LegB},
    {{9, 10, -3}, {13, 40, 3}, {11, 38, 0}, 40, 0, A::LegA},
    {{-7, 0, -2.5f}, {-1, 16, 2.5f}, {-4, 16, 0}, 0, 18, A::LegA},
    {{1, 0, -2.5f}, {7, 16, 2.5f}, {4, 16, 0}, 0, 18, A::LegB},
}};

} // namespace

std::span<const MobPart> mobModel(world::MobType type) {
    switch (type) {
    case world::MobType::Zombie: return kZombie;
    case world::MobType::Sheep: return kSheep;
    case world::MobType::Pig: return kPig;
    case world::MobType::Chicken: return kChicken;
    case world::MobType::Skeleton: return kSkeleton;
    case world::MobType::Creeper: return kCreeper;
    case world::MobType::Spider: return kSpider;
    case world::MobType::Enderman: return kEnderman;
    case world::MobType::Ghast: return kGhast;
    case world::MobType::Blaze: return kBlaze;
    case world::MobType::MagmaCube: return kMagmaCube;
    case world::MobType::ZombifiedPiglin:
    case world::MobType::Piglin: return kZombifiedPiglin; // (the same build)
    case world::MobType::Hoglin: return kHoglin;
    case world::MobType::Strider: return kStrider;
    case world::MobType::EndCrystal: return kEndCrystal;
    case world::MobType::EnderDragon: return kEnderDragon;
    case world::MobType::Shulker: return kShulker;
    case world::MobType::Minecart: return kMinecart;
    case world::MobType::Slime: return kMagmaCube; // (the same cube, its own skin)
    case world::MobType::Villager:
    case world::MobType::ZombieVillager: return kVillager; // (its own skin)
    case world::MobType::IronGolem: return kIronGolem;
    case world::MobType::Witch: return kWitch;
    case world::MobType::WanderingTrader: return kVillager; // (its blue robe in its skin; no apron)
    case world::MobType::Pillager: return kIllager;
    default: return kCow;
    }
}

} // namespace mc::gfx
