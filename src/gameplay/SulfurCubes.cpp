// Sulfur cubes (M33.2c; wiki: Sulfur Cube, 26.2): part of Mobs. Passive jelly cubes of the
// sulfur caves that hop about aimlessly, follow a player holding a block they can take in
// and take it in - fed by hand or picking it up from the ground. The block gives the cube
// its "archetype": how it moves, bounces, slides and floats; TNT makes it explosive, a magma
// block hot. With a block inside it shrugs off hits, arrows, explosions and falls - a hit
// launches it instead. Shears take the block back out. Small cubes are its young (they grow
// in 20 minutes, sooner on slime balls); a large one dies into two small ones.
#include "gameplay/Mobs.h"

#include "gameplay/ItemEntities.h"
#include "world/Blocks.h"
#include "world/World.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace mc {

using namespace world;

namespace {

// The archetypes (wiki: Sulfur Cube › Archetypes): speed (x the cube's), bounce (part of a
// landing kept), ground friction and air drag. Our numbers turn the wiki's table into
// per-tick factors (see `sulfurCubeAi`).
constexpr Mobs::SulfurArchetype kArchetypes[] = {
    {Mobs::SulfurKind::Bouncy, 1.4, 0.9, 0.3, 0.01},       {Mobs::SulfurKind::Regular, 1.0, 0.5, 0.3, 0.1},
    {Mobs::SulfurKind::SlowBouncy, 0.6, 0.6, 0.3, 0.05},   {Mobs::SulfurKind::FastFlat, 1.4, 0.5, 0.2, 0.01},
    {Mobs::SulfurKind::Light, 0.6, 1.0, 0.3, 1.8},         {Mobs::SulfurKind::FastSliding, 1.4, 0.1, 0.05, 0.01},
    {Mobs::SulfurKind::SlowSliding, 0.6, 0.1, 0.05, 0.01}, {Mobs::SulfurKind::HighResistance, 0.3, 0.2, 1.0, 0.01},
    {Mobs::SulfurKind::Sticky, 1.4, 0.0, 2.0, 0.01},       {Mobs::SulfurKind::SlowFlat, 0.6, 0.4, 0.4, 0.1},
    {Mobs::SulfurKind::Explosive, 1.0, 0.5, 0.3, 0.3},     {Mobs::SulfurKind::Hot, 1.0, 0.5, 0.3, 0.1},
};

bool any(std::string_view id, std::initializer_list<std::string_view> names) {
    for (std::string_view n : names)
        if (id == n) return true;
    return false;
}

float yawTo(const glm::dvec3& from, const glm::dvec3& to) { // (vanilla degrees: 0 = south)
    return float(std::atan2(-(to.x - from.x), to.z - from.z) * 180.0 / std::numbers::pi);
}

Chunk* chunkOf(World& world, const MobData& m) {
    return world.chunk({blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))});
}

} // namespace

namespace {
const Mobs::SulfurArchetype* archetypeByName(ItemId item);
}

// (M33 perf review) looked up per item scanned each tick: one byte per item, built once.
const Mobs::SulfurArchetype* Mobs::sulfurArchetype(ItemId item) {
    static const std::vector<int8_t> table = [] {
        std::vector<int8_t> t(itemRegistry().count(), -1);
        for (size_t i = 1; i < t.size(); ++i)
            if (const SulfurArchetype* a = archetypeByName(ItemId(i))) t[i] = int8_t(a - kArchetypes);
        return t;
    }();
    return item < table.size() && table[item] >= 0 ? &kArchetypes[table[item]] : nullptr;
}

namespace {
const Mobs::SulfurArchetype* archetypeByName(ItemId item) {
    // Which blocks it takes and how they act (the wiki's examples, extended to their kin; no
    // slabs, stairs, sand, gravel or redstone blocks - and nothing outside these groups).
    if (item == kNoItem) return nullptr;
    const BlockId b = itemRegistry().item(item).block;
    if (b == 0) return nullptr;
    std::string_view id = blockRegistry().block(b).id;
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    if (id.ends_with("_slab") || id.ends_with("_stairs") || id.ends_with("_wall")) return nullptr;
    using K = Mobs::SulfurKind;
    K k;
    if (id.ends_with("_planks") || id.ends_with("_log") || id.ends_with("_wood") || id.ends_with("_stem") ||
        id.ends_with("_hyphae") || id == "bamboo_block" || id == "bamboo_mosaic")
        k = K::Bouncy;
    else if (any(id, {"dirt", "coarse_dirt", "rooted_dirt", "podzol", "clay", "coal_block", "mud", "packed_mud",
                      "moss_block"}))
        k = K::Regular;
    else if (id.ends_with("_ore") ||
             any(id, {"stone", "cobblestone", "deepslate", "cobbled_deepslate", "granite", "diorite", "andesite",
                      "tuff", "calcite", "blackstone", "basalt", "netherrack", "end_stone", "sandstone",
                      "red_sandstone", "sulfur", "cinnabar", "dripstone_block", "smooth_stone", "stone_bricks"}))
        k = K::SlowBouncy;
    else if (id.ends_with("_coral_block") ||
             any(id, {"sponge", "wet_sponge", "pumpkin", "carved_pumpkin", "jack_o_lantern", "melon"}))
        k = K::FastFlat;
    else if (id.ends_with("_wool"))
        k = K::Light;
    else if (any(id, {"ice", "packed_ice", "blue_ice"}))
        k = K::FastSliding;
    else if (any(id, {"mushroom_stem", "red_mushroom_block", "brown_mushroom_block", "mycelium"}))
        k = K::SlowSliding;
    else if (any(id, {"soul_sand", "soul_soil"}))
        k = K::HighResistance;
    else if (id == "honeycomb_block")
        k = K::Sticky;
    else if (any(id, {"iron_block", "gold_block"}) || (id.ends_with("copper") && !id.starts_with("cut_")) ||
             id.ends_with("copper_block"))
        k = K::SlowFlat;
    else if (id == "tnt")
        k = K::Explosive;
    else if (id == "magma_block")
        k = K::Hot;
    else
        return nullptr;
    return &kArchetypes[static_cast<int>(k)];
}
} // namespace

Mobs::Use Mobs::sulfurCubeInteract(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    static const ItemId shears = *itemRegistry().find("shears"), slime = *itemRegistry().find("slime_ball"),
                        bucket = *itemRegistry().find("bucket"), flint = *itemRegistry().find("flint_and_steel");
    if (held == kNoItem) return Use::None;
    if (m.isBaby()) { // slime balls hurry a small one along (a tenth of the time left)
        if (held != slime) return Use::None;
        if (!m.ageLocked) m.age += -m.age / 10;
        return Use::Fed;
    }
    if (held == bucket) return m.fuse > 0 ? Use::None : Use::Bucket; // (M33 review: not while primed)
    if (held == shears) { // the block comes back out
        if (m.absorbed == 0) return Use::None;
        items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {m.absorbed, 1}, rng);
        m.absorbed = 0;
        m.fuse = 0;
        return Use::Sheared;
    }
    if (held == flint && m.absorbed != 0 && sulfurArchetype(m.absorbed)->kind == SulfurKind::Explosive &&
        m.fuse == 0) {
        m.fuse = 120; // (lit by hand: 6 s)
        return Use::Ignited;
    }
    if (!sulfurArchetype(held) || held == m.absorbed) return Use::None;
    if (m.absorbed != 0) items.spawn(m.pos + glm::dvec3(0, 0.5, 0), {m.absorbed, 1}, rng); // (the old one drops)
    m.absorbed = uint16_t(held);
    m.fuse = 0;
    return Use::Fed;
}

bool Mobs::sulfurCubeAi(Context& ctx, MobData& m) {
    const SulfurArchetype* arch = m.absorbed ? sulfurArchetype(m.absorbed) : nullptr;
    // Growing up: 20 minutes for a small one, held back by a golden dandelion.
    if (m.isBaby() && !m.ageLocked && ++m.age == 0) m.health = std::min(m.health * 2.0f, maxHealthOf(m));
    // A lit explosive cube: power 4 at the end of its fuse, gone without young or loot.
    if (m.fuse > 0 && --m.fuse == 0 && arch && arch->kind == SulfurKind::Explosive) {
        m_scratchEdits.clear();
        std::vector<BlockPos>& changed = ctx.edits ? *ctx.edits : m_scratchEdits;
        ExplosionTargets t;
        t.tnt = ctx.tnt;
        if (ctx.survival && !ctx.playerDead) {
            t.player = &ctx.player;
            t.vitals = &ctx.vitals;
        }
        t.breakBlocks = ctx.mobGriefing;
        m.health = 0.0f;
        m.deathTime = 19;
        m.ownBlast = true;
        m.absorbed = 0;
        m_explosion.explode(ctx.world, m.pos + glm::dvec3(0, 0.5, 0), 4.0f, ctx.rng, ctx.items, changed, t);
        return true;
    }
    // Wanting a block: a player within 8 holding one it takes, or a dropped one within 8.
    const glm::dvec3 p = ctx.player.position();
    glm::dvec3 want(0.0);
    bool wants = false;
    if (!m.isBaby() && !ctx.playerDead && sulfurArchetype(ctx.heldItem) && ctx.heldItem != m.absorbed &&
        glm::dot(p - m.pos, p - m.pos) < 8.0 * 8.0) {
        want = p;
        wants = true;
    } else if (!m.isBaby() && m.absorbed == 0) {
        double best = 8.0 * 8.0;
        for (const ItemEntity& e : ctx.items.items()) {
            if (e.stack.empty() || e.pickupDelay > 0) continue;
            const double d2 = glm::dot(e.pos - m.pos, e.pos - m.pos);
            if (d2 >= best || !sulfurArchetype(e.stack.item)) continue;
            best = d2;
            want = e.pos;
            wants = true;
            if (d2 < 1.0) { // touching it: in it goes
                m.absorbed = uint16_t(e.stack.item);
                ctx.items.takeOne(&e);
                wants = false;
                break;
            }
        }
    }
    // Hops (like a slime's, without a target): every 40-120 ticks, toward what it wants.
    const double speedOf = (m.isBaby() ? 0.3 : 0.4) * (arch ? arch->speed : 1.0);
    if (m.onGround) {
        if (--m.jumpTicks <= 0) {
            const float yaw = wants ? yawTo(m.pos, want) : ctx.rng.nextFloat() * 360.0f - 180.0f;
            m.yaw = m.headYaw = yaw;
            const double r = yaw * std::numbers::pi / 180.0;
            m.vel.y = m.isBaby() ? 0.42 : 0.52;
            m.goal = glm::dvec3(-std::sin(r), 0.0, std::cos(r));
            m.jumpTicks = int16_t(wants ? 10 + ctx.rng.nextInt(20) : 40 + ctx.rng.nextInt(81));
        } else {
            m.goal = glm::dvec3(0.0);
        }
    }
    const double fallingBefore = m.vel.y;
    physics(ctx.world, m, glm::dvec3(m.goal.x, 0.0, m.goal.z) * (0.2 * speedOf), false);
    if (arch) {
        // A bouncy one keeps part of its fall as a bounce; on the ground the friction
        // decides how far it slides (ours: the normal ground slowdown scaled by it); in the
        // air, drag (wool's is high: it floats down).
        if (m.onGround && fallingBefore < -0.15 && arch->bounce > 0.05) m.vel.y = -fallingBefore * arch->bounce;
        if (m.onGround) {
            const double keep = std::clamp((1.0 - 0.6 * arch->friction) / 0.546, 0.0, 1.7);
            m.vel.x *= keep;
            m.vel.z *= keep;
        } else {
            m.vel *= std::max(0.0, 1.0 - 0.02 * arch->drag);
        }
        // A hot cube burns what it touches, 1 a second (the player: as a magma block would).
        if (arch->kind == SulfurKind::Hot && m.attackCooldown-- <= 0) {
            m.attackCooldown = 20;
            const Aabb b = box(m);
            if (ctx.survival && !ctx.playerDead && b.intersects(ctx.player.box()))
                ctx.vitals.attacked(1.0f, nullptr, Vitals::Hit::Fire);
            if (Chunk* c = chunkOf(ctx.world, m))
                for (MobData& o : c->mobs())
                    if (&o != &m && o.health > 0.0f && o.hurtTime == 0 && !mobInfo(o.type).fireImmune &&
                        !(o.type == MobType::SulfurCube && o.absorbed != 0) && box(o).intersects(b)) {
                        o.health -= 1.0f;
                        o.hurtTime = 10;
                    }
        }
    }
    return true;
}

} // namespace mc
