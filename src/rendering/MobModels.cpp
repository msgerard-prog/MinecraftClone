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

// Armor stand (M28.3b): a base plate, two leg sticks, a hip bar, a pole, a shoulder bar
// and a neck; the armor it wears as layers 15-18 (vanilla: a player-shaped stand).
constexpr std::array<MobPart, 10> kArmorStand = {{
    {{-6, 0, -6}, {6, 1, 6}, {0, 0, 0}, 0, 51, A::None},
    {{-3, 1, -1}, {-1, 12, 1}, {0, 0, 0}, 0, 0, A::None},
    {{1, 1, -1}, {3, 12, 1}, {0, 0, 0}, 8, 0, A::None},
    {{-4, 12, -1}, {4, 14, 1}, {0, 0, 0}, 16, 0, A::None},
    {{-1, 14, -1}, {1, 24, 1}, {0, 0, 0}, 36, 0, A::None},
    {{-6, 24, -1.5f}, {6, 27, 1.5f}, {0, 0, 0}, 0, 14, A::None},
    {{-4, 24, -4}, {4, 32, 4}, {0, 0, 0}, 32, 14, A::None, 15, 0.5f},
    {{-4, 12, -2}, {4, 24, 2}, {0, 0, 0}, 0, 22, A::None, 16, 0.6f},
    {{-4, 5, -2}, {4, 11, 2}, {0, 0, 0}, 24, 32, A::None, 17, 0.4f},
    {{-4, 1, -2}, {4, 5, 2}, {0, 0, 0}, 0, 40, A::None, 18, 0.5f},
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

// Evoker (M24.5): a robed illager, arms crossed.
constexpr std::array<MobPart, 6> kEvoker = {{
    {{-4, 24, -4}, {4, 34, 4}, {0, 24, 0}, 0, 0, A::Head},
    {{-1, 23, 4}, {1, 27, 6}, {0, 24, 0}, 24, 0, A::Head},
    {{-4, 6, -3}, {4, 24, 3}, {0, 24, 0}, 16, 20, A::None},
    {{-4, 16, 3}, {4, 20, 7}, {0, 18, 0}, 0, 40, A::None},
    {{-4, 0, -2}, {0, 6, 2}, {-2, 6, 0}, 0, 48, A::LegA},
    {{0, 0, -2}, {4, 6, 2}, {2, 6, 0}, 0, 48, A::LegB},
}};

// Vex (M24.5): a small floating spirit with flapping wings and a tail.
constexpr std::array<MobPart, 7> kVex = {{
    {{-2.5f, 8, -2.5f}, {2.5f, 13, 2.5f}, {0, 8, 0}, 0, 0, A::Head},
    {{-1.5f, 3, -1}, {1.5f, 8, 1}, {0, 8, 0}, 0, 10, A::None},
    {{-2.5f, 3, -0.5f}, {-1.5f, 8, 0.5f}, {-2, 8, 0}, 20, 0, A::ArmForward},
    {{1.5f, 3, -0.5f}, {2.5f, 8, 0.5f}, {2, 8, 0}, 20, 0, A::ArmForward},
    {{-1, 0, -1}, {1, 3, 1}, {0, 3, 0}, 0, 17, A::None},
    {{0.5f, 4, -1.5f}, {6.5f, 9, -0.5f}, {0.5f, 6.5f, -1}, 24, 10, A::WingL},
    {{-6.5f, 4, -1.5f}, {-0.5f, 9, -0.5f}, {-0.5f, 6.5f, -1}, 24, 10, A::WingR},
}};

// Ravager (M24.5): a heavy body on four legs, a big horned head carried low in front.
constexpr std::array<MobPart, 8> kRavager = {{
    {{-6, 12, -9}, {6, 26, 9}, {0, 12, 0}, 0, 0, A::None},
    {{-5, 14, 9}, {5, 24, 19}, {0, 20, 9}, 0, 32, A::Head},
    {{-7, 22, 13}, {-5, 28, 15}, {0, 20, 9}, 40, 32, A::Head},
    {{5, 22, 13}, {7, 28, 15}, {0, 20, 9}, 40, 32, A::Head},
    {{-6, 0, 3}, {0, 12, 9}, {-3, 12, 6}, 40, 40, A::LegA},
    {{0, 0, 3}, {6, 12, 9}, {3, 12, 6}, 40, 40, A::LegB},
    {{-6, 0, -9}, {0, 12, -3}, {-3, 12, -6}, 40, 40, A::LegB},
    {{0, 0, -9}, {6, 12, -3}, {3, 12, -6}, 40, 40, A::LegA},
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

// Fish (M25.2): a body with a head in front, a tail fin that wags (Tail) and fins.
// Cod: 2 x 4 x 7 body (wiki: Cod), a 2 x 3 head, a 4-tall tail.
constexpr std::array<MobPart, 4> kCod = {{
    {{-1, 0, -3}, {1, 4, 4}, {0, 2, 0}, 0, 0, A::None},
    {{-1, 0, 4}, {1, 3, 7}, {0, 2, 0}, 0, 12, A::None},
    {{-0.5f, 0, -8}, {0.5f, 4, -3}, {0, 2, -3}, 0, 20, A::Tail},
    {{-0.5f, 4, -1}, {0.5f, 5, 3}, {0, 2, 0}, 20, 0, A::None},
}};
// Salmon: longer, 3 wide, a hooked head.
constexpr std::array<MobPart, 5> kSalmon = {{
    {{-1.5f, 0, -4}, {1.5f, 5, 5}, {0, 2.5f, 0}, 0, 0, A::None},
    {{-1, 0, 5}, {1, 4, 8}, {0, 2.5f, 0}, 0, 16, A::None},
    {{-0.5f, 0, -10}, {0.5f, 5, -4}, {0, 2.5f, -4}, 0, 24, A::Tail},
    {{-0.5f, 5, -2}, {0.5f, 6, 3}, {0, 2.5f, 0}, 24, 0, A::None},
    {{-0.5f, -1, -1}, {0.5f, 0, 2}, {0, 2.5f, 0}, 24, 8, A::None},
}};
// Tropical fish: a short tall body, tinted by its base colour (layer 4), a pattern
// over it tinted by the pattern colour (layer 5), and a tail.
constexpr std::array<MobPart, 4> kTropicalFish = {{
    {{-1, 0, -3}, {1, 5, 3}, {0, 2.5f, 0}, 0, 0, A::None, 4},
    {{-1, 0, -3}, {1, 5, 3}, {0, 2.5f, 0}, 0, 16, A::None, 5, 0.05f},
    {{-0.5f, 0, -7}, {0.5f, 5, -3}, {0, 2.5f, -3}, 0, 32, A::Tail, 4},
    {{-0.5f, 5, -2}, {0.5f, 7, 2}, {0, 2.5f, 0}, 16, 32, A::None, 5},
}};
// Pufferfish: a round body (drawn larger as it puffs up), with spines on its sides.
constexpr std::array<MobPart, 5> kPufferfish = {{
    {{-4, 0, -4}, {4, 8, 4}, {0, 4, 0}, 0, 0, A::None},
    {{-0.5f, 2, -7}, {0.5f, 6, -4}, {0, 4, -4}, 0, 20, A::Tail},
    {{-5, 3, -1}, {-4, 5, 1}, {0, 4, 0}, 32, 0, A::None},
    {{4, 3, -1}, {5, 5, 1}, {0, 4, 0}, 32, 0, A::None},
    {{-1, 8, -1}, {1, 9, 1}, {0, 4, 0}, 32, 4, A::None},
}};
// Squid (wiki: Squid): a 12 x 16 x 12 body over eight 2 x 18 x 2 tentacles that swing.
constexpr std::array<MobPart, 9> kSquid = {{
    {{-6, 10, -6}, {6, 26, 6}, {0, 10, 0}, 0, 0, A::None},
    {{-5, 0, -5}, {-3, 10, -3}, {-4, 10, -4}, 48, 0, A::LegA},
    {{-1, 0, -6}, {1, 10, -4}, {0, 10, -5}, 48, 0, A::LegB},
    {{3, 0, -5}, {5, 10, -3}, {4, 10, -4}, 48, 0, A::LegA},
    {{4, 0, -1}, {6, 10, 1}, {5, 10, 0}, 48, 0, A::LegB},
    {{3, 0, 3}, {5, 10, 5}, {4, 10, 4}, 48, 0, A::LegA},
    {{-1, 0, 4}, {1, 10, 6}, {0, 10, 5}, 48, 0, A::LegB},
    {{-5, 0, 3}, {-3, 10, 5}, {-4, 10, 4}, 48, 0, A::LegA},
    {{-6, 0, -1}, {-4, 10, 1}, {-5, 10, 0}, 48, 0, A::LegB},
}};

// Boat (M25.2b; wiki: Boat): a flat bottom and four low sides, at half size (drawn at
// modelScale 2: 20 x 28 pixels overall; vanilla's are 28 x 16 x 3 plates), tinted by the wood.
constexpr std::array<MobPart, 5> kBoat = {{
    {{-5, 0, -7}, {5, 1, 7}, {0, 0, 0}, 0, 0, A::None, 6},
    {{-5, 1, -7}, {-4, 4, 7}, {0, 0, 0}, 0, 16, A::None, 6},
    {{4, 1, -7}, {5, 4, 7}, {0, 0, 0}, 0, 16, A::None, 6},
    {{-4, 1, 6}, {4, 4, 7}, {0, 0, 0}, 0, 36, A::None, 6},
    {{-4, 1, -7}, {4, 4, -6}, {0, 0, 0}, 0, 36, A::None, 6},
}};

// Dolphin (M25.3b): a long body, a head with a snout, a back fin, flippers, a tail
// that beats (drawn 1.4x).
constexpr std::array<MobPart, 7> kDolphin = {{
    {{-4, 0, -6}, {4, 7, 7}, {0, 3, 0}, 0, 0, A::None},
    {{-3, 1, 7}, {3, 6, 12}, {0, 3, 7}, 0, 22, A::None},
    {{-1, 1, 12}, {1, 3, 16}, {0, 3, 7}, 24, 22, A::None},
    {{-0.5f, 7, -1}, {0.5f, 11, 3}, {0, 3, 0}, 42, 0, A::None},
    {{-5, 2, -12}, {5, 3, -6}, {0, 3, -6}, 0, 34, A::Tail},
    {{-7, 1, 2}, {-4, 2, 5}, {-4, 1, 3}, 32, 34, A::WingL},
    {{4, 1, 2}, {7, 2, 5}, {4, 1, 3}, 32, 34, A::WingR},
}};
// Turtle (M25.3b): a domed shell over a belly plate, a head, four flippers (drawn 1.5x).
constexpr std::array<MobPart, 7> kTurtle = {{
    {{-6, 2, -7}, {6, 7, 7}, {0, 2, 0}, 0, 0, A::None},
    {{-5, 1, -6}, {5, 2, 6}, {0, 2, 0}, 0, 20, A::None},
    {{-2, 2, 7}, {2, 5, 11}, {0, 3, 7}, 0, 34, A::Head},
    {{-9, 2, 3}, {-6, 3, 6}, {-6, 2, 4}, 16, 34, A::LegA},
    {{6, 2, 3}, {9, 3, 6}, {6, 2, 4}, 16, 34, A::LegB},
    {{-6, 2, -8}, {-3, 3, -6}, {-4, 2, -7}, 28, 34, A::LegB},
    {{3, 2, -8}, {6, 3, -6}, {4, 2, -7}, 28, 34, A::LegA},
}};

// Guardian (M25.5): a spiky 12 x 12 x 16 body with one big eye in front and a tail of
// three shrinking segments that wags (the elder is the same, drawn larger).
constexpr std::array<MobPart, 10> kGuardian = {{
    {{-6, 0, -8}, {6, 12, 8}, {0, 6, 0}, 0, 0, A::None},
    {{-1, 5, 8}, {1, 7, 9}, {0, 6, 0}, 56, 0, A::None},
    {{-2, 4, -16}, {2, 8, -8}, {0, 6, -8}, 0, 30, A::Tail},
    {{-1.5f, 4.5f, -23}, {1.5f, 7.5f, -16}, {0, 6, -16}, 24, 30, A::Tail},
    {{-1, 5, -29}, {1, 7, -23}, {0, 6, -23}, 44, 30, A::Tail},
    {{-0.5f, 12, -0.5f}, {0.5f, 16, 0.5f}, {0, 6, 0}, 56, 4, A::None},
    {{-0.5f, -4, -0.5f}, {0.5f, 0, 0.5f}, {0, 6, 0}, 56, 4, A::None},
    {{-10, 5.5f, -0.5f}, {-6, 6.5f, 0.5f}, {0, 6, 0}, 56, 10, A::None},
    {{6, 5.5f, -0.5f}, {10, 6.5f, 0.5f}, {0, 6, 0}, 56, 10, A::None},
    {{-0.5f, 5.5f, -12}, {0.5f, 6.5f, -8}, {0, 6, 0}, 56, 10, A::None},
}};

// Wolf (M26.1): body, head with snout and ears, four legs, a tail that wags; a collar
// when tamed. Box UVs: body 6x6x9 @ (0,0), head 6x6x4 @ (32,0), snout 3x3x4 @ (0,16),
// ear 2x2x1 @ (16,16), leg 2x8x2 @ (0,24), tail 2x2x6 @ (10,24), collar @ (32,12).
constexpr std::array<MobPart, 12> kWolf = {{
    {{-3, 8, -5}, {3, 14, 4}, {0, 8, 0}, 0, 0, A::None, 8},
    {{-3, 9, 4}, {3, 15, 8}, {0, 12, 4}, 32, 0, A::Head, 8},
    {{-1.5f, 9, 8}, {1.5f, 12, 12}, {0, 12, 4}, 0, 16, A::Head, 8},
    {{-3, 15, 6}, {-1, 17, 7}, {0, 12, 4}, 16, 16, A::Head, 8},
    {{1, 15, 6}, {3, 17, 7}, {0, 12, 4}, 16, 16, A::Head, 8},
    {{-3, 0, 1}, {-1, 8, 3}, {-2, 8, 2}, 0, 24, A::LegA, 8},
    {{1, 0, 1}, {3, 8, 3}, {2, 8, 2}, 0, 24, A::LegB, 8},
    {{-3, 0, -4}, {-1, 8, -2}, {-2, 8, -3}, 0, 24, A::LegB, 8},
    {{1, 0, -4}, {3, 8, -2}, {2, 8, -3}, 0, 24, A::LegA, 8},
    {{-1, 10, -11}, {1, 12, -5}, {0, 11, -5}, 10, 24, A::Tail, 8},
    {{-3.3f, 9, 3}, {3.3f, 15, 4.2f}, {0, 12, 4}, 32, 12, A::Head, 7, 0.1f},
    {{-3, 8, -5}, {3, 14, 4}, {0, 8, 0}, 0, 16, A::None, 10, 0.4f}, // (M26.3: wolf armor)
}};
// Cat and ocelot (M26.1): a slim body, a round head with ears and a nose, thin legs and
// a long tail; cats wear a collar when tamed. Body 4x4x13 @ (0,0), head 5x4x4 @ (36,0),
// nose 3x2x1 @ (0,18), ear 1x1x2 @ (10,18), leg 2x6x2 @ (0,22), tail 1x1x8 @ (10,22),
// collar @ (36,10).
constexpr std::array<MobPart, 12> kCat = {{
    {{-2, 6, -6}, {2, 10, 7}, {0, 6, 0}, 0, 0, A::None, 8},
    {{-2.5f, 7, 7}, {2.5f, 11, 11}, {0, 9, 7}, 36, 0, A::Head, 8},
    {{-1.5f, 7, 11}, {1.5f, 9, 12}, {0, 9, 7}, 0, 18, A::Head, 8},
    {{-2, 11, 8}, {-1, 12, 10}, {0, 9, 7}, 10, 18, A::Head, 8},
    {{1, 11, 8}, {2, 12, 10}, {0, 9, 7}, 10, 18, A::Head, 8},
    {{-2, 0, 4}, {0, 6, 6}, {-1, 6, 5}, 0, 22, A::LegA, 8},
    {{0, 0, 4}, {2, 6, 6}, {1, 6, 5}, 0, 22, A::LegB, 8},
    {{-2, 0, -5}, {0, 6, -3}, {-1, 6, -4}, 0, 22, A::LegB, 8},
    {{0, 0, -5}, {2, 6, -3}, {1, 6, -4}, 0, 22, A::LegA, 8},
    {{-0.5f, 9, -14}, {0.5f, 10, -6}, {0, 9.5f, -6}, 10, 22, A::Tail, 8},
    {{-0.5f, 4, -16}, {0.5f, 9, -15}, {0, 9.5f, -6}, 10, 22, A::Tail, 8},
    {{-2.7f, 7, 6.8f}, {2.7f, 11, 7.8f}, {0, 9, 7}, 36, 10, A::Head, 7, 0.1f},
}};
// Parrot (M26.1): a small upright body, head and beak, two wings that flap, a tail.
constexpr std::array<MobPart, 8> kParrot = {{
    {{-1.5f, 4, -1.5f}, {1.5f, 10, 1.5f}, {0, 4, 0}, 0, 0, A::None, 8},
    {{-1, 10, -1}, {1, 13, 1}, {0, 10, 0}, 12, 0, A::Head, 8},
    {{-0.5f, 10.5f, 1}, {0.5f, 12, 2.5f}, {0, 10, 0}, 20, 0, A::Head},
    {{-2.5f, 5, -1}, {-1.5f, 9, 1.5f}, {-1.5f, 9, 0}, 0, 10, A::WingL, 8},
    {{1.5f, 5, -1}, {2.5f, 9, 1.5f}, {1.5f, 9, 0}, 0, 10, A::WingR, 8},
    {{-1, 1, -2.5f}, {1, 4, -1.5f}, {0, 4, -1.5f}, 8, 10, A::None, 8},
    {{-1, 0, 0}, {-0.5f, 4, 0.5f}, {0, 4, 0}, 14, 10, A::LegA},
    {{0.5f, 0, 0}, {1, 4, 0.5f}, {0, 4, 0}, 14, 10, A::LegB},
}};

// Horses, donkeys and mules (M26.2): a long body on four tall legs, an upright neck with
// a mane, a long head, ears, a tail; gear from the mount-gear texture (layers 9-11:
// saddle, horse armor, chest packs). Leg 4x11x4 @ (0,0), head 5x5x10 @ (16,0), tail
// 3x10x4 @ (46,0), neck 4x10x6 @ (0,15), mane 2x10x3 @ (20,15), ear 2x3x1 @ (30,15),
// body 10x10x22 @ (0,32). Gear texture: saddle @ (0,0), chest @ (36,0), armor @ (0,16),
// carpet @ (0,48).
constexpr std::array<MobPart, 17> kHorse = {{
    {{-5, 11, -11}, {5, 21, 11}, {0, 11, 0}, 0, 32, A::None, 8},
    {{-5, 0, 7}, {-1, 11, 11}, {-3, 11, 9}, 0, 0, A::LegA, 8},
    {{1, 0, 7}, {5, 11, 11}, {3, 11, 9}, 0, 0, A::LegB, 8},
    {{-5, 0, -11}, {-1, 11, -7}, {-3, 11, -9}, 0, 0, A::LegB, 8},
    {{1, 0, -11}, {5, 11, -7}, {3, 11, -9}, 0, 0, A::LegA, 8},
    {{-2, 18, 6}, {2, 28, 12}, {0, 20, 9}, 0, 15, A::Head, 8},
    {{-2.5f, 24, 9}, {2.5f, 29, 19}, {0, 20, 9}, 16, 0, A::Head, 8},
    {{-1, 19, 5}, {1, 29, 8}, {0, 20, 9}, 20, 15, A::Head},
    {{-2.5f, 29, 10}, {-0.5f, 32, 11}, {0, 20, 9}, 30, 15, A::Head, 8},
    {{0.5f, 29, 10}, {2.5f, 32, 11}, {0, 20, 9}, 30, 15, A::Head, 8},
    {{-1.5f, 10, -13}, {1.5f, 20, -9}, {0, 20, -11}, 46, 0, A::Tail},
    {{-5, 21, -4}, {5, 23, 4}, {0, 0, 0}, 0, 0, A::None, 9, 0.3f},
    {{-5, 11, -11}, {5, 21, 11}, {0, 11, 0}, 0, 16, A::None, 10, 0.5f},
    {{-2, 18, 6}, {2, 28, 12}, {0, 20, 9}, 0, 16, A::Head, 10, 0.4f},
    {{-2.5f, 24, 9}, {2.5f, 29, 19}, {0, 20, 9}, 0, 16, A::Head, 10, 0.4f},
    {{-8, 13, -7}, {-5, 21, 1}, {0, 0, 0}, 36, 0, A::None, 11},
    {{5, 13, -7}, {8, 21, 1}, {0, 0, 0}, 36, 0, A::None, 11},
}};
// Llamas (M26.2): a woolly body, tall neck and head, upright ears, a stub of a tail; a
// carpet (layer 12, tinted) and chest packs. Leg 4x11x4 @ (0,0), neck 6x12x6 @ (16,0),
// head 6x5x6 @ (40,0), ear 2x3x2 @ (16,18), tail 2x4x2 @ (24,18), body 12x10x18 @ (0,36).
constexpr std::array<MobPart, 13> kLlama = {{
    {{-6, 11, -9}, {6, 21, 9}, {0, 11, 0}, 0, 36, A::None, 8},
    {{-5.5f, 0, 5}, {-1.5f, 11, 9}, {-3.5f, 11, 7}, 0, 0, A::LegA, 8},
    {{1.5f, 0, 5}, {5.5f, 11, 9}, {3.5f, 11, 7}, 0, 0, A::LegB, 8},
    {{-5.5f, 0, -9}, {-1.5f, 11, -5}, {-3.5f, 11, -7}, 0, 0, A::LegB, 8},
    {{1.5f, 0, -9}, {5.5f, 11, -5}, {3.5f, 11, -7}, 0, 0, A::LegA, 8},
    {{-3, 17, 5}, {3, 29, 11}, {0, 20, 8}, 16, 0, A::Head, 8},
    {{-3, 26, 9}, {3, 31, 15}, {0, 20, 8}, 40, 0, A::Head, 8},
    {{-3, 31, 10}, {-1, 34, 12}, {0, 20, 8}, 16, 18, A::Head, 8},
    {{1, 31, 10}, {3, 34, 12}, {0, 20, 8}, 16, 18, A::Head, 8},
    {{-1, 15, -11}, {1, 19, -9}, {0, 19, -9}, 24, 18, A::None, 8},
    {{-6, 21, -7}, {6, 22, 7}, {0, 0, 0}, 0, 48, A::None, 12, 0.3f},
    {{-9, 12, -6}, {-6, 20, 2}, {0, 0, 0}, 36, 0, A::None, 11},
    {{6, 12, -6}, {9, 20, 2}, {0, 0, 0}, 36, 0, A::None, 11},
}};
// Camel (M26.2): long legs, a body with a hump, a neck reaching forward, a long head,
// small ears and a tail; a saddle on the hump. Leg 4x18x4 @ (0,0), neck 5x12x5 @ (16,0),
// head 6x6x8 @ (36,0), hump 8x5x8 @ (16,17), ear 2x2x1 @ (48,14), tail 2x8x2 @ (0,22),
// body 12x10x20 @ (0,34).
constexpr std::array<MobPart, 12> kCamel = {{
    {{-6, 18, -10}, {6, 28, 10}, {0, 18, 0}, 0, 34, A::None},
    {{-4, 28, -4}, {4, 33, 4}, {0, 28, 0}, 16, 17, A::None},
    {{-6, 0, 6}, {-2, 18, 10}, {-4, 18, 8}, 0, 0, A::LegA},
    {{2, 0, 6}, {6, 18, 10}, {4, 18, 8}, 0, 0, A::LegB},
    {{-6, 0, -10}, {-2, 18, -6}, {-4, 18, -8}, 0, 0, A::LegB},
    {{2, 0, -10}, {6, 18, -6}, {4, 18, -8}, 0, 0, A::LegA},
    {{-2.5f, 22, 10}, {2.5f, 34, 15}, {0, 24, 10}, 16, 0, A::Head},
    {{-3, 30, 12}, {3, 36, 20}, {0, 24, 10}, 36, 0, A::Head},
    {{-4, 34, 13}, {-3, 36, 14}, {0, 24, 10}, 48, 14, A::Head},
    {{3, 34, 13}, {4, 36, 14}, {0, 24, 10}, 48, 14, A::Head},
    {{-1, 18, -12}, {1, 26, -10}, {0, 26, -10}, 0, 22, A::None},
    {{-5, 33, -4}, {5, 35, 4}, {0, 0, 0}, 0, 0, A::None, 9, 0.3f},
}};
// A chest boat (M26.2): the boat with a chest standing at its back (layer 11).
constexpr std::array<MobPart, 6> kChestBoat = {{
    {{-5, 0, -7}, {5, 1, 7}, {0, 0, 0}, 0, 0, A::None, 6},
    {{-5, 1, -7}, {-4, 4, 7}, {0, 0, 0}, 0, 16, A::None, 6},
    {{4, 1, -7}, {5, 4, 7}, {0, 0, 0}, 0, 16, A::None, 6},
    {{-4, 1, 6}, {4, 4, 7}, {0, 0, 0}, 0, 36, A::None, 6},
    {{-4, 1, -7}, {4, 4, -6}, {0, 0, 0}, 0, 36, A::None, 6},
    {{-3, 1, -6}, {3, 6, -1}, {0, 0, 0}, 36, 0, A::None, 11},
}};

// Wildlife (M26.3). Rabbit: body 5x5x7 @ (0,0), head 4x4x4 @ (24,0), ear 1x4x1 @
// (40,0), tail 2x2x1 @ (44,0), hind leg 2x2x4 @ (0,12), front leg 1x3x1 @ (12,12) - the
// legs move together: it hops.
constexpr std::array<MobPart, 9> kRabbit = {{
    {{-2.5f, 2, -3.5f}, {2.5f, 7, 3.5f}, {0, 2, 0}, 0, 0, A::None, 8},
    {{-2, 5, 3}, {2, 9, 7}, {0, 6, 3}, 24, 0, A::Head, 8},
    {{-1.5f, 9, 4}, {-0.5f, 13, 5}, {0, 6, 3}, 40, 0, A::Head, 8},
    {{0.5f, 9, 4}, {1.5f, 13, 5}, {0, 6, 3}, 40, 0, A::Head, 8},
    {{-1, 4, -4.5f}, {1, 6, -3.5f}, {0, 5, -3.5f}, 44, 0, A::None},
    {{-2.5f, 0, -3}, {-0.5f, 2, 1}, {-1.5f, 2, -1}, 0, 12, A::LegA, 8},
    {{0.5f, 0, -3}, {2.5f, 2, 1}, {1.5f, 2, -1}, 0, 12, A::LegA, 8},
    {{-2, 0, 2}, {-1, 3, 3}, {-1.5f, 3, 2.5f}, 12, 12, A::LegB, 8},
    {{1, 0, 2}, {2, 3, 3}, {1.5f, 3, 2.5f}, 12, 12, A::LegB, 8},
}};
// Fox: body 6x6x9 @ (0,0), head 8x6x6 @ (30,0), snout 3x3x3 @ (0,15), ear 2x2x1 @
// (12,15), leg 2x3x2 @ (18,15), tail 4x4x8 @ (26,15).
constexpr std::array<MobPart, 10> kFox = {{
    {{-3, 3, -5}, {3, 9, 4}, {0, 3, 0}, 0, 0, A::None, 8},
    {{-4, 5, 4}, {4, 11, 10}, {0, 8, 4}, 30, 0, A::Head, 8},
    {{-1.5f, 5, 10}, {1.5f, 8, 13}, {0, 8, 4}, 0, 15, A::Head},
    {{-4, 11, 7}, {-2, 13, 8}, {0, 8, 4}, 12, 15, A::Head, 8},
    {{2, 11, 7}, {4, 13, 8}, {0, 8, 4}, 12, 15, A::Head, 8},
    {{-3, 0, 1}, {-1, 3, 3}, {-2, 3, 2}, 18, 15, A::LegA},
    {{1, 0, 1}, {3, 3, 3}, {2, 3, 2}, 18, 15, A::LegB},
    {{-3, 0, -4}, {-1, 3, -2}, {-2, 3, -3}, 18, 15, A::LegB},
    {{1, 0, -4}, {3, 3, -2}, {2, 3, -3}, 18, 15, A::LegA},
    {{-2, 4, -13}, {2, 8, -5}, {0, 6, -5}, 26, 15, A::Tail, 8},
}};
// Polar bear: body 12x11x20 @ (0,33), leg 5x10x5 @ (0,0), head 7x7x7 @ (20,0), snout
// 4x3x3 @ (48,0), ear 2x2x1 @ (48,6).
constexpr std::array<MobPart, 9> kPolarBear = {{
    {{-6, 10, -10}, {6, 21, 10}, {0, 10, 0}, 0, 33, A::None},
    {{-6, 0, 5}, {-1, 10, 10}, {-3.5f, 10, 7.5f}, 0, 0, A::LegA},
    {{1, 0, 5}, {6, 10, 10}, {3.5f, 10, 7.5f}, 0, 0, A::LegB},
    {{-6, 0, -10}, {-1, 10, -5}, {-3.5f, 10, -7.5f}, 0, 0, A::LegB},
    {{1, 0, -10}, {6, 10, -5}, {3.5f, 10, -7.5f}, 0, 0, A::LegA},
    {{-3.5f, 13, 9}, {3.5f, 20, 16}, {0, 16, 9}, 20, 0, A::Head},
    {{-2, 13, 16}, {2, 16, 19}, {0, 16, 9}, 48, 0, A::Head},
    {{-3.5f, 20, 11}, {-1.5f, 22, 12}, {0, 16, 9}, 48, 6, A::Head},
    {{1.5f, 20, 11}, {3.5f, 22, 12}, {0, 16, 9}, 48, 6, A::Head},
}};
// Panda: body 13x11x18 @ (0,35), leg 5x9x5 @ (0,0), head 9x8x7 @ (20,0), ear 3x3x1 @
// (52,0), snout 4x3x2 @ (52,4); a brown panda is tinted (layer 8).
constexpr std::array<MobPart, 9> kPanda = {{
    {{-6.5f, 9, -9}, {6.5f, 20, 9}, {0, 9, 0}, 0, 35, A::None, 8},
    {{-6.5f, 0, 4}, {-1.5f, 9, 9}, {-4, 9, 6.5f}, 0, 0, A::LegA, 8},
    {{1.5f, 0, 4}, {6.5f, 9, 9}, {4, 9, 6.5f}, 0, 0, A::LegB, 8},
    {{-6.5f, 0, -9}, {-1.5f, 9, -4}, {-4, 9, -6.5f}, 0, 0, A::LegB, 8},
    {{1.5f, 0, -9}, {6.5f, 9, -4}, {4, 9, -6.5f}, 0, 0, A::LegA, 8},
    {{-4.5f, 12, 8}, {4.5f, 20, 15}, {0, 15, 8}, 20, 0, A::Head, 8},
    {{-5, 19, 10}, {-2, 22, 11}, {0, 15, 8}, 52, 0, A::Head, 8},
    {{2, 19, 10}, {5, 22, 11}, {0, 15, 8}, 52, 0, A::Head, 8},
    {{-2, 12, 15}, {2, 15, 17}, {0, 15, 8}, 52, 4, A::Head, 8},
}};
// Goat: body 9x8x16 @ (0,40), leg 3x9x3 @ (0,0), head 5x7x6 @ (12,0), horn 2x6x2 @
// (34,0) (layers 13 / 14: drawn while it has them), beard 1x4x2 @ (42,0).
constexpr std::array<MobPart, 9> kGoat = {{
    {{-4.5f, 9, -8}, {4.5f, 17, 8}, {0, 9, 0}, 0, 40, A::None},
    {{-4.5f, 0, 4.5f}, {-1.5f, 9, 7.5f}, {-3, 9, 6}, 0, 0, A::LegA},
    {{1.5f, 0, 4.5f}, {4.5f, 9, 7.5f}, {3, 9, 6}, 0, 0, A::LegB},
    {{-4.5f, 0, -7.5f}, {-1.5f, 9, -4.5f}, {-3, 9, -6}, 0, 0, A::LegB},
    {{1.5f, 0, -7.5f}, {4.5f, 9, -4.5f}, {3, 9, -6}, 0, 0, A::LegA},
    {{-2.5f, 13, 7}, {2.5f, 20, 13}, {0, 16, 8}, 12, 0, A::Head},
    {{-2.5f, 20, 9}, {-0.5f, 26, 11}, {0, 16, 8}, 34, 0, A::Head, 13},
    {{0.5f, 20, 9}, {2.5f, 26, 11}, {0, 16, 8}, 34, 0, A::Head, 14},
    {{-0.5f, 10, 11}, {0.5f, 14, 13}, {0, 16, 8}, 42, 0, A::Head},
}};
// Armadillo: shell 7x6x9 @ (0,0), head 3x3x4 @ (32,0), ear 1x2x1 @ (46,0), leg 2x3x2 @
// (0,15), tail 1x1x4 @ (8,15).
constexpr std::array<MobPart, 9> kArmadillo = {{
    {{-3.5f, 3, -4.5f}, {3.5f, 9, 4.5f}, {0, 3, 0}, 0, 0, A::None},
    {{-1.5f, 3, 4.5f}, {1.5f, 6, 8.5f}, {0, 4.5f, 4.5f}, 32, 0, A::Head},
    {{-1.5f, 6, 6}, {-0.5f, 8, 7}, {0, 4.5f, 4.5f}, 46, 0, A::Head},
    {{0.5f, 6, 6}, {1.5f, 8, 7}, {0, 4.5f, 4.5f}, 46, 0, A::Head},
    {{-3, 0, 2}, {-1, 3, 4}, {-2, 3, 3}, 0, 15, A::LegA},
    {{1, 0, 2}, {3, 3, 4}, {2, 3, 3}, 0, 15, A::LegB},
    {{-3, 0, -4}, {-1, 3, -2}, {-2, 3, -3}, 0, 15, A::LegB},
    {{1, 0, -4}, {3, 3, -2}, {2, 3, -3}, 0, 15, A::LegA},
    {{-0.5f, 3, -8.5f}, {0.5f, 4, -4.5f}, {0, 3.5f, -4.5f}, 8, 15, A::None},
}};

// Bee (M26.3b): a striped body, two beating wings, a stinger, antennae, little legs.
// Body 7x7x10 @ (0,0), wing 8x0.5x6 @ (0,18), stinger 1x1x2 @ (34,0), antenna 1x2x3 @
// (34,4), legs 7x2x1 @ (16,18).
constexpr std::array<MobPart, 8> kBee = {{
    {{-3.5f, 2, -5}, {3.5f, 9, 5}, {0, 5, 0}, 0, 0, A::None},
    {{-9, 9, -2}, {-1, 9.5f, 4}, {-1, 9, 0}, 0, 18, A::WingL},
    {{1, 9, -2}, {9, 9.5f, 4}, {1, 9, 0}, 0, 18, A::WingR},
    {{-0.5f, 4.5f, -7}, {0.5f, 5.5f, -5}, {0, 5, -5}, 34, 0, A::None},
    {{-2, 9, 5}, {-1, 11, 8}, {0, 7, 5}, 34, 4, A::Head},
    {{1, 9, 5}, {2, 11, 8}, {0, 7, 5}, 34, 4, A::Head},
    {{-3.5f, 0, -2}, {3.5f, 2, -1}, {0, 2, 0}, 16, 18, A::LegA},
    {{-3.5f, 0, 1}, {3.5f, 2, 2}, {0, 2, 0}, 16, 18, A::LegB},
}};

// Frog (M26.3c; tinted by its kind): a squat body, a wide flat head with eyes on top,
// back legs folded at the sides, small front legs. Body 7x3x9 @ (0,0), head 7x3x6 @
// (32,0), eye 3x2x2 @ (0,12), back leg 3x3x4 @ (10,12), front leg 2x3x2 @ (24,12).
constexpr std::array<MobPart, 8> kFrog = {{
    {{-3.5f, 2, -4.5f}, {3.5f, 5, 4.5f}, {0, 2, 0}, 0, 0, A::None, 8},
    {{-3.5f, 3, 3}, {3.5f, 6, 9}, {0, 4, 3}, 32, 0, A::Head, 8},
    {{-3.5f, 6, 5}, {-0.5f, 8, 7}, {0, 4, 3}, 0, 12, A::Head, 8},
    {{0.5f, 6, 5}, {3.5f, 8, 7}, {0, 4, 3}, 0, 12, A::Head, 8},
    {{-5.5f, 0, -4}, {-2.5f, 3, 0}, {-4, 3, -2}, 10, 12, A::LegA, 8},
    {{2.5f, 0, -4}, {5.5f, 3, 0}, {4, 3, -2}, 10, 12, A::LegB, 8},
    {{-3.5f, 0, 2}, {-1.5f, 3, 4}, {-2.5f, 3, 3}, 24, 12, A::LegB, 8},
    {{1.5f, 0, 2}, {3.5f, 3, 4}, {2.5f, 3, 3}, 24, 12, A::LegA, 8},
}};
// Tadpole: a round body and a wagging tail. Body 3x2x3 @ (0,0), tail 0x2x7 drawn 1x2x7
// @ (0,6).
constexpr std::array<MobPart, 2> kTadpole = {{
    {{-1.5f, 0, 0}, {1.5f, 2, 3}, {0, 1, 0}, 0, 0, A::None},
    {{-0.5f, 0, -7}, {0.5f, 2, 0}, {0, 1, 0}, 0, 6, A::Tail},
}};
// Axolotl (M26.3c; tinted by its colour): a long body, a broad head with gills, four
// legs, a finned tail. Body 8x4x10 @ (0,0), head 8x5x5 @ (36,0), gills 10x3x1 @ (0,14),
// leg 3x5x1 -> 3x1x5 @ (24,14), tail 1x5x12 @ (0,20).
constexpr std::array<MobPart, 8> kAxolotl = {{
    {{-4, 1, -5}, {4, 5, 5}, {0, 1, 0}, 0, 0, A::None, 8},
    {{-4, 1, 5}, {4, 6, 10}, {0, 3, 5}, 36, 0, A::Head, 8},
    {{-5, 4, 8}, {5, 7, 9}, {0, 3, 5}, 0, 14, A::Head, 8},
    {{-7, 0, 1}, {-4, 1, 5}, {-4, 1, 3}, 24, 14, A::LegA, 8},
    {{4, 0, 1}, {7, 1, 5}, {4, 1, 3}, 24, 14, A::LegB, 8},
    {{-7, 0, -4}, {-4, 1, 0}, {-4, 1, -2}, 24, 14, A::LegB, 8},
    {{4, 0, -4}, {7, 1, 0}, {4, 1, -2}, 24, 14, A::LegA, 8},
    {{-0.5f, 1, -17}, {0.5f, 6, -5}, {0, 3, -5}, 0, 20, A::Tail, 8},
}};

// Silverfish (M26.4a): a striped body that wriggles, a small head, a tail. Body 4x3x8 @
// (0,0), head 3x2x2 @ (24,0), tail 2x2x3 @ (24,4).
constexpr std::array<MobPart, 3> kSilverfish = {{
    {{-2, 0, -4}, {2, 3, 4}, {0, 1.5f, 0}, 0, 0, A::None},
    {{-1.5f, 0, 4}, {1.5f, 2, 6}, {0, 1, 4}, 24, 0, A::Head},
    {{-1, 0, -7}, {1, 2, -4}, {0, 1, -4}, 24, 4, A::Tail},
}};
// Phantom (M26.4a): a flat body, a broad head, long wings that beat, a tail. Body 5x3x9 @
// (0,0), head 7x3x5 @ (28,0), wing 10x1x9 @ (0,12), tail 3x2x6 @ (40,12).
constexpr std::array<MobPart, 5> kPhantom = {{
    {{-2.5f, 1, -4.5f}, {2.5f, 4, 4.5f}, {0, 2, 0}, 0, 0, A::None},
    {{-3.5f, 1, 4.5f}, {3.5f, 4, 9.5f}, {0, 2, 4.5f}, 28, 0, A::Head},
    {{-12.5f, 3, -4}, {-2.5f, 4, 5}, {-2.5f, 3.5f, 0}, 0, 12, A::WingL},
    {{2.5f, 3, -4}, {12.5f, 4, 5}, {2.5f, 3.5f, 0}, 0, 12, A::WingR},
    {{-1.5f, 1.5f, -10.5f}, {1.5f, 3.5f, -4.5f}, {0, 2.5f, -4.5f}, 40, 12, A::Tail},
}};

// The Wither (M26.4b): three skull heads (the middle one larger) on a bar of shoulders,
// a spine with ribs, a tail. Head 8x8x8 @ (0,0), side head 6x6x6 @ (32,0), shoulders
// 20x3x3 @ (0,16), spine 3x10x3 @ (0,22), rib 11x2x2 @ (24,22), tail 3x6x3 @ (12,22).
constexpr std::array<MobPart, 10> kWither = {{
    {{-4, 46, -4}, {4, 54, 4}, {0, 46, 0}, 0, 0, A::Head},
    {{-15, 42, -3}, {-9, 48, 3}, {-12, 42, 0}, 32, 0, A::Head},
    {{9, 42, -3}, {15, 48, 3}, {12, 42, 0}, 32, 0, A::Head},
    {{-10, 43, -1.5f}, {10, 46, 1.5f}, {0, 44, 0}, 0, 16, A::None},
    {{-1.5f, 33, -1.5f}, {1.5f, 43, 1.5f}, {0, 43, 0}, 0, 22, A::None},
    {{-5.5f, 40, -1}, {5.5f, 42, 1}, {0, 41, 0}, 24, 22, A::None},
    {{-5.5f, 37, -1}, {5.5f, 39, 1}, {0, 38, 0}, 24, 22, A::None},
    {{-5.5f, 34, -1}, {5.5f, 36, 1}, {0, 35, 0}, 24, 22, A::None},
    {{-1.5f, 27, -1.5f}, {1.5f, 33, 1.5f}, {0, 33, 0}, 12, 22, A::Tail},
    {{-1, 22, -1}, {1, 27, 1}, {0, 27, 0}, 12, 22, A::Tail},
}};

// Breeze (M26.4c): a head over a whirl of wind and three rods circling it. Head 8x8x8 @
// (0,0), whirl 6x10x6 @ (32,0), rod 2x8x2 @ (0,16).
constexpr std::array<MobPart, 5> kBreeze = {{
    {{-4, 20, -4}, {4, 28, 4}, {0, 20, 0}, 0, 0, A::Head},
    {{-3, 4, -3}, {3, 14, 3}, {0, 9, 0}, 32, 0, A::Tail},
    {{-1, 12, 4}, {1, 20, 6}, {0, 16, 0}, 0, 16, A::Tail},
    {{-6, 10, -3}, {-4, 18, -1}, {0, 16, 0}, 0, 16, A::Tail},
    {{4, 10, -3}, {6, 18, -1}, {0, 16, 0}, 0, 16, A::Tail},
}};

// Allay (M26.5a): a small blue spirit - head, body, arms, two wings. Head 5x5x5 @ (0,0),
// body 3x4x2 @ (20,0), arm 1x4x1 @ (30,0), wing 0.5x5x8 -> 1x5x8 @ (0,10).
constexpr std::array<MobPart, 6> kAllay = {{
    {{-2.5f, 5, -2.5f}, {2.5f, 10, 2.5f}, {0, 5, 0}, 0, 0, A::Head},
    {{-1.5f, 1, -1}, {1.5f, 5, 1}, {0, 3, 0}, 20, 0, A::None},
    {{-2.5f, 1, -0.5f}, {-1.5f, 5, 0.5f}, {-2, 5, 0}, 30, 0, A::LegA},
    {{1.5f, 1, -0.5f}, {2.5f, 5, 0.5f}, {2, 5, 0}, 30, 0, A::LegB},
    {{-0.5f, 2, -9}, {0.5f, 7, -1}, {0, 5, -1}, 0, 10, A::WingL},
    {{-0.5f, 2, -9}, {0.5f, 7, -1}, {0, 5, -1}, 0, 10, A::WingR},
}};
// Nautilus (M26.5a): a coiled shell, a body peeking out, tentacles. Shell 8x8x8 @ (0,0),
// body 6x5x4 @ (32,0), tentacle 1x1x5 @ (0,16).
constexpr std::array<MobPart, 6> kNautilus = {{
    {{-4, 4, -5}, {4, 12, 3}, {0, 8, 0}, 0, 0, A::None},
    {{-3, 3, 3}, {3, 8, 7}, {0, 5, 3}, 32, 0, A::Head},
    {{-2, 3, 7}, {-1, 4, 12}, {0, 4, 7}, 0, 16, A::Tail},
    {{1, 3, 7}, {2, 4, 12}, {0, 4, 7}, 0, 16, A::Tail},
    {{-0.5f, 5, 7}, {0.5f, 6, 13}, {0, 5, 7}, 0, 16, A::Tail},
    {{-3, 4, 7}, {-2, 5, 11}, {0, 4, 7}, 0, 16, A::Tail},
}};

// Happy ghast (M26.5b): the ghast's body and tentacles (its own gentle skin) and a
// harness over the top (layer 12, tinted by its dye) when it wears one.
constexpr std::array<MobPart, 11> kHappyGhast = {{
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
    {{-7, 15, -7}, {7, 16, 7}, {0, 8, 0}, 0, 48, A::None, 12, 0.6f},
}};
// Copper golem (M26.5b; tinted by its oxidation, layer 8): a big head with a nose and a
// lightning-rod antenna, a small body, arms and legs. Head 8x5x6 @ (0,0), nose 2x3x2 @
// (28,0), rod 1x4x1 @ (36,0), body 4x6x3 @ (0,11), arm 2x6x2 @ (14,11), leg 2x5x2 @
// (22,11).
constexpr std::array<MobPart, 8> kCopperGolem = {{
    {{-4, 11, -3}, {4, 16, 3}, {0, 11, 0}, 0, 0, A::Head, 8},
    {{-1, 10, 3}, {1, 13, 5}, {0, 11, 0}, 28, 0, A::Head, 8},
    {{-0.5f, 16, -0.5f}, {0.5f, 20, 0.5f}, {0, 11, 0}, 36, 0, A::Head, 8},
    {{-2, 5, -1.5f}, {2, 11, 1.5f}, {0, 8, 0}, 0, 11, A::None, 8},
    {{-4, 5, -1}, {-2, 11, 1}, {-3, 11, 0}, 14, 11, A::LegB, 8},
    {{2, 5, -1}, {4, 11, 1}, {3, 11, 0}, 14, 11, A::LegA, 8},
    {{-2, 0, -1}, {0, 5, 1}, {-1, 5, 0}, 22, 11, A::LegA, 8},
    {{0, 0, -1}, {2, 5, 1}, {1, 5, 0}, 22, 11, A::LegB, 8},
}};

// Creaking (M27.1c): a tall, thin figure of pale oak. Head 8x10x8 @ (0,0), body 8x16x6 @
// (0,20), arms 3x20x3 @ (32,20), legs 4x16x4 @ (48,20).
constexpr std::array<MobPart, 6> kCreaking = {{
    {{-4, 32, -4}, {4, 42, 4}, {0, 32, 0}, 0, 0, A::Head},
    {{-4, 16, -3}, {4, 32, 3}, {0, 24, 0}, 0, 20, A::None},
    {{-7, 13, -1.5f}, {-4, 33, 1.5f}, {-5.5f, 32, 0}, 32, 20, A::LegB},
    {{4, 13, -1.5f}, {7, 33, 1.5f}, {5.5f, 32, 0}, 32, 20, A::LegA},
    {{-4, 0, -2}, {0, 16, 2}, {-2, 16, 0}, 48, 20, A::LegA},
    {{0, 0, -2}, {4, 16, 2}, {2, 16, 0}, 48, 20, A::LegB},
}};

// Warden (M27.3c): a hulking figure, half size (drawn x2: MobInfo::modelScale). Head
// 8x8x5 @ (0,0), body 9x10x6 @ (0,16), arms 4x14x4 @ (32,16), legs 3x7x3 @ (0,36).
constexpr std::array<MobPart, 6> kWarden = {{
    {{-4, 17, -2.5f}, {4, 25, 2.5f}, {0, 17, 0}, 0, 0, A::Head},
    {{-4.5f, 7, -3}, {4.5f, 17, 3}, {0, 12, 0}, 0, 16, A::None},
    {{-8.5f, 3, -2}, {-4.5f, 17, 2}, {-6.5f, 16.5f, 0}, 32, 16, A::LegB},
    {{4.5f, 3, -2}, {8.5f, 17, 2}, {6.5f, 16.5f, 0}, 32, 16, A::LegA},
    {{-3.5f, 0, -1.5f}, {-0.5f, 7, 1.5f}, {-2, 7, 0}, 0, 36, A::LegA},
    {{0.5f, 0, -1.5f}, {3.5f, 7, 1.5f}, {2, 7, 0}, 0, 36, A::LegB},
}};

// Sniffer (M27.5c): a long shaggy body on six short legs, a big head with a snout; half
// size (drawn x2). Body 12x9x16 @ (0,0), head 7x6x8 @ (0,26), legs 3x5x3 @ (32,26).
constexpr std::array<MobPart, 8> kSniffer = {{
    {{-6, 5, -8}, {6, 14, 8}, {0, 9, 0}, 0, 0, A::None},
    {{-3.5f, 6, -14}, {3.5f, 12, -6}, {0, 9, -7}, 0, 26, A::Head},
    {{-5, 0, -6}, {-2, 5, -3}, {-3.5f, 5, -4.5f}, 32, 26, A::LegA},
    {{2, 0, -6}, {5, 5, -3}, {3.5f, 5, -4.5f}, 32, 26, A::LegB},
    {{-5, 0, -1.5f}, {-2, 5, 1.5f}, {-3.5f, 5, 0}, 32, 26, A::LegB},
    {{2, 0, -1.5f}, {5, 5, 1.5f}, {3.5f, 5, 0}, 32, 26, A::LegA},
    {{-5, 0, 3}, {-2, 5, 6}, {-3.5f, 5, 4.5f}, 32, 26, A::LegA},
    {{2, 0, 3}, {5, 5, 6}, {3.5f, 5, 4.5f}, 32, 26, A::LegB},
}};

} // namespace

std::span<const MobPart> chestBoatModel() { return kChestBoat; }

std::span<const MobPart> mobModel(world::MobType type) {
    switch (type) {
    case world::MobType::Zombie: return kZombie;
    case world::MobType::Sheep: return kSheep;
    case world::MobType::Pig: return kPig;
    case world::MobType::Chicken: return kChicken;
    case world::MobType::Skeleton: return kSkeleton;
    case world::MobType::Stray:
    case world::MobType::Bogged:
    case world::MobType::Parched: return kSkeleton; // (M29.1a: their own skins)
    case world::MobType::Husk: return kZombie;
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
    case world::MobType::Pillager:
    case world::MobType::Vindicator: return kIllager;
    case world::MobType::Evoker: return kEvoker;
    case world::MobType::Vex: return kVex;
    case world::MobType::Cod: return kCod;
    case world::MobType::Salmon: return kSalmon;
    case world::MobType::TropicalFish: return kTropicalFish;
    case world::MobType::Pufferfish: return kPufferfish;
    case world::MobType::Squid:
    case world::MobType::GlowSquid: return kSquid;
    case world::MobType::Boat: return kBoat;
    case world::MobType::Drowned: return kZombie; // (the zombie's shape, our own drowned skin)
    case world::MobType::Dolphin: return kDolphin;
    case world::MobType::Turtle: return kTurtle;
    case world::MobType::Guardian:
    case world::MobType::ElderGuardian: return kGuardian;
    case world::MobType::Wolf: return kWolf;
    case world::MobType::Cat:
    case world::MobType::Ocelot: return kCat;
    case world::MobType::Parrot: return kParrot;
    case world::MobType::Ravager: return kRavager;
    case world::MobType::Horse:
    case world::MobType::Donkey:
    case world::MobType::Mule:
    case world::MobType::SkeletonHorse:
    case world::MobType::ZombieHorse: return kHorse;
    case world::MobType::Llama:
    case world::MobType::TraderLlama: return kLlama;
    case world::MobType::Camel:
    case world::MobType::CamelHusk: return kCamel;
    case world::MobType::Rabbit: return kRabbit;
    case world::MobType::Fox: return kFox;
    case world::MobType::PolarBear: return kPolarBear;
    case world::MobType::Panda: return kPanda;
    case world::MobType::Goat: return kGoat;
    case world::MobType::Armadillo: return kArmadillo;
    case world::MobType::Bee: return kBee;
    case world::MobType::Frog: return kFrog;
    case world::MobType::Tadpole: return kTadpole;
    case world::MobType::Axolotl: return kAxolotl;
    case world::MobType::CaveSpider: return kSpider; // (drawn 0.7x, its own skin)
    case world::MobType::Silverfish: return kSilverfish;
    case world::MobType::WitherSkeleton: return kSkeleton; // (drawn 1.2x, its own skin)
    case world::MobType::Phantom: return kPhantom;
    case world::MobType::Wither: return kWither;
    case world::MobType::Breeze: return kBreeze;
    case world::MobType::Allay: return kAllay;
    case world::MobType::Nautilus:
    case world::MobType::ZombieNautilus: return kNautilus;
    case world::MobType::HappyGhast: return kHappyGhast;
    case world::MobType::CopperGolem: return kCopperGolem;
    case world::MobType::Creaking: return kCreaking;
    case world::MobType::Warden: return kWarden;
    case world::MobType::Sniffer: return kSniffer;
    case world::MobType::ArmorStand: return kArmorStand;
    default: return kCow;
    }
}

} // namespace mc::gfx
