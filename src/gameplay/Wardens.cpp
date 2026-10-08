// The warden (M27.3c; wiki: Warden). Part of Mobs.
#include "gameplay/Mobs.h"

namespace mc {

using namespace world;

bool Mobs::summonWarden(World& world, const BlockPos& shrieker, Xoroshiro& rng) {
    (void)world;
    (void)shrieker;
    (void)rng;
    return false; // (the warden itself comes in M27.3c)
}

} // namespace mc
