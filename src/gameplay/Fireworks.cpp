#include "gameplay/Fireworks.h"

#include "gameplay/Loom.h"
#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

std::optional<ItemStack> craftFirework(std::span<const ItemStack> grid) {
    static const ItemId gunpowder = *itemRegistry().find("gunpowder"), paper = *itemRegistry().find("paper"),
                        star = *itemRegistry().find("firework_star"), rocket = *itemRegistry().find("firework_rocket"),
                        fireCharge = *itemRegistry().find("fire_charge"), nugget = *itemRegistry().find("gold_nugget"),
                        feather = *itemRegistry().find("feather"), diamond = *itemRegistry().find("diamond"),
                        glowstone = *itemRegistry().find("glowstone_dust");
    int powder = 0, papers = 0, stars = 0, dyes = 0, shapes = 0, diamonds = 0, glows = 0, other = 0;
    uint16_t dyeMask = 0;
    uint8_t shape = 0;
    const ItemStack* theStar = nullptr;
    Fireworks rocketData;
    for (const ItemStack& s : grid) {
        if (s.empty()) continue;
        const int dye = dyeColour(s.item);
        if (s.item == gunpowder) ++powder;
        else if (s.item == paper) ++papers;
        else if (s.item == star) {
            ++stars;
            theStar = &s;
            if (const auto f = fireworks(s.extra); f && f->count > 0 && rocketData.count < Fireworks::kMax)
                rocketData.explosions[rocketData.count++] = f->explosions[0];
        } else if (dye >= 0) {
            ++dyes;
            dyeMask = uint16_t(dyeMask | (1u << dye));
        } else if (s.item == fireCharge) ++shapes, shape = 1;
        else if (s.item == nugget) ++shapes, shape = 2;
        else if (isMobHead(itemRegistry().item(s.item).block)) ++shapes, shape = 3;
        else if (s.item == feather) ++shapes, shape = 4;
        else if (s.item == diamond) ++diamonds;
        else if (s.item == glowstone) ++glows;
        else ++other;
    }
    if (other > 0) return std::nullopt;
    // A rocket: paper, 1-3 gunpowder, stars.
    if (papers == 1 && powder >= 1 && powder <= 3 && dyes == 0 && shapes == 0 && diamonds == 0 && glows == 0) {
        rocketData.flight = uint8_t(powder);
        ItemStack out{rocket, 3};
        out.extra = addFireworks(rocketData);
        return out;
    }
    // A star: gunpowder, dyes, extras.
    if (papers == 0 && powder == 1 && stars == 0 && dyes >= 1 && shapes <= 1 && diamonds <= 1 && glows <= 1) {
        Fireworks f;
        f.count = 1;
        f.explosions[0] = {shape, dyeMask, 0, diamonds == 1, glows == 1};
        ItemStack out{star, 1};
        out.extra = addFireworks(f);
        return out;
    }
    // Fading: a star and dyes.
    if (papers == 0 && powder == 0 && stars == 1 && dyes >= 1 && shapes == 0 && diamonds == 0 && glows == 0 && theStar) {
        auto f = fireworks(theStar->extra);
        if (!f || f->count == 0) return std::nullopt;
        f->explosions[0].fades = dyeMask;
        ItemStack out = *theStar;
        out.count = 1;
        out.extra = addFireworks(*f);
        return out;
    }
    return std::nullopt;
}

int rocketLifetime(int flight, uint32_t r1, uint32_t r2) { return 10 * (flight + 1) + int(r1 % 6) + int(r2 % 7); }

float fireworkDamage(int explosions, double distance) {
    if (explosions <= 0 || distance >= 5.0) return 0.0f;
    return float((5.0 + 2.0 * explosions) * std::sqrt((5.0 - distance) / 5.0));
}

glm::dvec3 boostedVelocity(const glm::dvec3& vel, const glm::dvec3& look) {
    return vel + look * 0.1 + (look * 1.5 - vel) * 0.5;
}

} // namespace mc
