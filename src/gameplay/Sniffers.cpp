// Sniffers (M27.5c; wiki: Sniffer). Part of Mobs.
//
// An adult sniffer wanders and, every few minutes, stops on diggable ground (grass, dirt,
// coarse dirt, podzol, rooted dirt, moss, pale moss, mud or muddy roots) to dig for 4 s, turning up torchflower
// seeds or a pitcher pod. Fed torchflower seeds, two of them lay a sniffer egg (Animals.cpp);
// eggs hatch into snifflets (BlockUpdates).
#include "gameplay/Mobs.h"

#include "gameplay/ItemEntities.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <cmath>

namespace mc {

using namespace world;

bool Mobs::snifferTick(Context& ctx, MobData& m) {
    if (m.type != MobType::Sniffer) return false;
    if (m.phase == 1) { // digging: still, then the find
        m.vel.x = m.vel.z = 0.0;
        m.goal = m.pos;
        animalUpkeep(ctx, m);
        if (--m.phaseTicks <= 0) {
            m.phase = 0;
            static const ItemId seeds = *itemRegistry().find("torchflower_seeds"), pod = *itemRegistry().find("pitcher_pod");
            ctx.items.spawn(m.pos + glm::dvec3(0.0, 0.5, 0.0), {ctx.rng.nextInt(2) ? seeds : pod, 1}, ctx.rng);
        }
        return true;
    }
    if (m.isBaby() || !m.onGround) return false;
    if (m.eggTicks <= 0) m.eggTicks = 4800 + int(ctx.rng.nextInt(4800)); // (ours: every 4-8 minutes)
    if (--m.eggTicks > 0) return false;
    const BlockId under = blockRegistry().blockOf(
        ctx.world.getBlock({int(std::floor(m.pos.x)), int(std::floor(m.pos.y - 0.2)), int(std::floor(m.pos.z))}));
    if (under != blocks::GrassBlock && under != blocks::Dirt && under != blocks::CoarseDirt && under != blocks::Podzol &&
        under != blocks::RootedDirt && under != blocks::MossBlock && under != blocks::Mud &&
        under != blocks::PaleMossBlock && under != blocks::MuddyMangroveRoots) {
        m.eggTicks = 200; // (try again soon somewhere else)
        return false;
    }
    m.phase = 1;
    m.phaseTicks = 80;
    return true;
}

} // namespace mc
