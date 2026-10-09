#include "gameplay/Dispensers.h"

#include "gameplay/Hoppers.h"
#include "gameplay/Mining.h"
#include "world/Blocks.h"

namespace mc {

using namespace world;

namespace {

Direction oppositeOf(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }

// Drops one item out of the front like a thrown item (wiki: 0.2 out, a little spread).
void spit(DispenseContext& ctx, const glm::dvec3& mouth, const glm::dvec3& dir,
          const ItemStack& one) {
    if (ItemEntity* e = ctx.items.spawn(mouth - glm::dvec3(0.0, 0.125, 0.0), one, ctx.rng)) {
        const double speed = 0.1 + ctx.rng.nextDouble() * 0.2;
        auto spread = [&] {
            return (ctx.rng.nextDouble() - 0.5) * 0.09;
        }; // (vanilla: gaussian x 0.0075 x 6)
        e->vel = dir * speed + glm::dvec3(spread(), spread(), spread());
        if (dir.y == 0.0) e->vel.y += 0.2;
    }
}

} // namespace

void dispense(DispenseContext& ctx, const BlockPos& p) {
    const auto& r = blockRegistry();
    const BlockStateId s = ctx.world.getBlock(p);
    const BlockId block = r.blockOf(s);
    if (block != blocks::Dispenser && block != blocks::Dropper) return;
    Chunk* c = ctx.world.chunk(p.chunk());
    DispenserData* d = c ? c->dispenser(blockToLocal(p.x), p.y, blockToLocal(p.z)) : nullptr;
    if (!d) return;
    // A random non-empty slot (wiki).
    int filled = 0;
    for (const ItemStack& st : d->items)
        filled += !st.empty();
    if (filled == 0) return; // (vanilla clicks; no sound here)
    int pick = static_cast<int>(ctx.rng.nextInt(uint32_t(filled)));
    ItemStack* slot = nullptr;
    for (ItemStack& st : d->items)
        if (!st.empty() && pick-- == 0) {
            slot = &st;
            break;
        }
    const Direction facing = static_cast<Direction>(r.get(s, properties::facing6));
    const glm::ivec3 n = normal(facing);
    const BlockPos front{p.x + n.x, p.y + n.y, p.z + n.z};
    const glm::dvec3 dir(n);
    const glm::dvec3 mouth = glm::dvec3(p.x + 0.5, p.y + 0.5, p.z + 0.5) + dir * 0.7;
    ItemStack one = *slot;
    one.count = 1;
    auto use = [&] { // one used up
        if (--slot->count == 0) *slot = {};
        c->markDirty();
    };
    if (block == blocks::Dropper) {
        if (isContainer(ctx.world, front)) {
            if (insertOne(ctx.world, front, oppositeOf(facing), one)) use();
            return;
        }
        spit(ctx, mouth, dir, one);
        use();
        return;
    }
    const std::string_view id = itemRegistry().item(one.item).id;
    auto shootDir = [&] {
        return dir.y == 0.0 ? glm::normalize(dir + glm::dvec3(0.0, 0.1, 0.0)) : dir;
    };
    if (id == "minecraft:arrow") { // (wiki: speed 1.1, spread 6)
        if (ctx.projectiles.shoot(ProjectileKind::Arrow, mouth, shootDir(), 1.1, 6.0, false, false,
                                  ctx.rng))
            use();
    } else if (id == "minecraft:egg" || id == "minecraft:blue_egg" || id == "minecraft:brown_egg" ||
               id == "minecraft:splash_potion") {
        const bool egg = id != "minecraft:splash_potion";
        const ProjectileKind k = egg ? ProjectileKind::Egg : ProjectileKind::SplashPotion;
        if (ctx.projectiles.shoot(k, mouth, shootDir(), egg ? 1.1 : 0.5, 6.0, false, false, ctx.rng)) {
            ctx.projectiles.last().potion = one.potion;
            ctx.projectiles.last().eggVariant = id == "minecraft:brown_egg" ? 1 : id == "minecraft:blue_egg" ? 2 : 0;
            ctx.projectiles.last().pickup = false;
            use();
        }
    } else if (id == "minecraft:fire_charge") { // a small fireball (wiki)
        if (ctx.projectiles.shoot(ProjectileKind::BlazeFireball, mouth, dir, 0.9, 1.0, false, false,
                                  ctx.rng))
            use();
    } else if (id == "minecraft:water_bucket" || id == "minecraft:lava_bucket") {
        const BlockStateId here = ctx.world.getBlock(front);
        if (!BlockUpdates::replaceable(here)) { // (wiki: dropped when the front is solid)
            spit(ctx, mouth, dir, one);
            use();
            return;
        }
        const bool water = id == "minecraft:water_bucket";
        if (!(water && ctx.world.isUltrawarm())) // (water boils away in the Nether)
            ctx.world.updateBlock(
                front, BlockUpdates::fluidState(water ? blocks::Water : blocks::Lava, 8, false));
        ctx.edits.push_back(front);
        static const ItemId bucket = *itemRegistry().find("bucket");
        *slot = ItemStack{bucket, 1};
        c->markDirty();
    } else if (id == "minecraft:bucket") {
        const BlockStateId here = ctx.world.getBlock(front);
        const BlockId hb = r.blockOf(here);
        if ((hb != blocks::Water && hb != blocks::Lava) || r.get(here, properties::level) != 0) {
            spit(ctx, mouth, dir, one);
            use();
            return;
        }
        ctx.world.updateBlock(front, 0);
        ctx.edits.push_back(front);
        static const ItemId waterBucket = *itemRegistry().find("water_bucket"),
                            lavaBucket = *itemRegistry().find("lava_bucket");
        const ItemStack full{hb == blocks::Water ? waterBucket : lavaBucket, 1};
        if (slot->count == 1) {
            *slot = full;
        } else {
            use();
            bool placed = false;
            for (ItemStack& st : d->items)
                if (st.empty()) {
                    st = full;
                    placed = true;
                    break;
                }
            if (!placed) spit(ctx, mouth, dir, full);
        }
        c->markDirty();
    } else if (id == "minecraft:flint_and_steel") {
        if (r.blockOf(ctx.world.getBlock(front)) == blocks::Tnt) {
            ctx.updates.primeTnt(front);
        } else if (ctx.world.getBlock(front) == 0 && BlockUpdates::fireCanStay(ctx.world, front)) {
            ctx.world.updateBlock(front, BlockUpdates::fireState(0));
            ctx.edits.push_back(front);
        } else {
            return;
        }
        *slot = wearItem(*slot, 1, ctx.rng);
        c->markDirty();
    } else if (id == "minecraft:tnt") { // primed in front, whatever is there (wiki)
        ctx.tnt.prime(front, 80, ctx.rng);
        use();
    } else if (id == "minecraft:bone_meal") {
        if (ctx.updates.boneMeal(front)) use();
    } else {
        spit(ctx, mouth, dir, one);
        use();
    }
}

} // namespace mc
