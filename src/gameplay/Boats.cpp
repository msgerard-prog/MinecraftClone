// Boats (M25.2b; wiki: Boat). Part of Mobs. A boat floats on water; its rider paddles:
// forward adds 0.04 a tick to its speed (0.005 backward, or while only turning), left
// and right turn it 1 degree a tick more; speed and turning keep 90% a tick in water
// (top speed 0.4 blocks a tick), 98% on ice, 98.9% on blue ice, and much less on land.
#include "gameplay/Mobs.h"

#include "gameplay/FluidContact.h"
#include "world/Blocks.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

bool Mobs::placeBoat(World& world, const glm::dvec3& at, float yaw, int wood, Xoroshiro& rng) {
    MobData m = make(MobType::Boat, at, rng);
    m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = yaw;
    m.woolColour = uint8_t(wood);
    m.persistent = true;
    return add(world, m);
}

void Mobs::boatTick(Context& ctx, MobData& m) {
    const World& world = ctx.world;
    const FluidContact fluid = fluidContact(world, box(m));
    // What it rests on: the water (the surface around its middle), or the block below.
    const auto& r = blockRegistry();
    const BlockId under = r.blockOf(world.getBlock({int(std::floor(m.pos.x)), int(std::floor(m.pos.y - 0.05)),
                                                    int(std::floor(m.pos.z))}));
    const double friction = fluid.water        ? 0.9
                            : !m.onGround      ? 0.9
                            : under == blocks::BlueIce ? 0.989
                            : under == blocks::Ice || under == blocks::PackedIce ? 0.98
                                                                                : 0.45; // (dragging on land)
    // The rider's paddling (main sets paddleForward/paddleTurn each tick).
    m.yawVel = float(m.yawVel * friction) + float(m.paddleTurn);
    double accel = 0.0;
    if (m.paddleTurn != 0 && m.paddleForward == 0) accel = 0.005;
    if (m.paddleForward > 0) accel = 0.04;
    if (m.paddleForward < 0) accel = -0.005;
    m.yaw += m.yawVel;
    const glm::dvec3 f(forwardFlat(m.yaw));
    m.vel.x = m.vel.x * friction + f.x * accel;
    m.vel.z = m.vel.z * friction + f.z * accel;
    if (fluid.water) {
        // Floats with its bottom a little under the surface (our buoyancy model).
        const double surface = m.pos.y + fluid.height;
        m.vel.y = (m.vel.y + (surface - 0.15 - m.pos.y) * 0.08) * 0.75;
    } else {
        m.vel.y = (m.vel.y - 0.04) * 0.98;
    }
    m.headYaw = m.yaw;
    m.paddleForward = m.paddleTurn = 0;
    physics(world, m, glm::dvec3(0.0), false); // (collision only: a boat's velocity is set above)
}

} // namespace mc
