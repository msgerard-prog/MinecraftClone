// Monster equipment (M32.2c; wiki: Zombie, Skeleton › Spawning, Regional difficulty, Armor,
// Mob › Item pickup): part of Mobs. Zombies and skeletons may spawn wearing armor - more
// often and better where the clamped regional difficulty is high - and holding enchanted
// gear; some pick up what lies around. What they wear protects them (vanilla's armor
// formula) and what they hold hits harder; on death it drops now and then.
#include "gameplay/Mobs.h"

#include "gameplay/Enchanting.h"
#include "gameplay/ItemEntities.h"
#include "world/Enchantments.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

Chunk* chunkOf(World& world, const MobData& m) {
    return world.chunk({blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))});
}
const Chunk* chunkOf(const World& world, const MobData& m) {
    return world.chunk({blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))});
}

// Armor points by `armorMaterial` (1 leather, 2 chainmail, 3 iron, 4 gold, 5 diamond,
// 6 netherite, 7 turtle, 8 copper) and slot head..feet (wiki: Armor › Defense points).
constexpr int kPoints[10][4] = {{0, 0, 0, 0}, {1, 3, 2, 1}, {2, 5, 4, 1}, {2, 6, 5, 2}, {2, 5, 3, 1},
                                {3, 8, 6, 3}, {3, 8, 6, 3}, {2, 0, 0, 0}, {2, 4, 3, 1}, {0, 0, 0, 0}};

// The pieces vanilla's spawn rolls choose from: tier 0 leather, 1 gold, 2 chainmail,
// 3 iron, 4 diamond (Mob.getEquipmentForSlot); slot head..feet.
ItemId spawnArmor(int tier, int slot) {
    static const auto ids = [] {
        static constexpr const char* kTiers[5] = {"leather", "golden", "chainmail", "iron", "diamond"};
        static constexpr const char* kPieces[4] = {"helmet", "chestplate", "leggings", "boots"};
        std::array<std::array<ItemId, 4>, 5> out{};
        for (int t = 0; t < 5; ++t)
            for (int s = 0; s < 4; ++s)
                out[size_t(t)][size_t(s)] =
                    itemRegistry().find(std::string(kTiers[t]) + "_" + kPieces[s]).value_or(kNoItem);
        return out;
    }();
    return ids[size_t(std::clamp(tier, 0, 4))][size_t(slot)];
}

// Vanilla EnchantmentHelper.enchantItem at level 5 + crd x random(18), no treasure (the
// enchanting table's picking; a seed from the mob's own roll).
void enchantSpawned(ItemStack& s, double crd, Xoroshiro& rng) {
    const int level = 5 + int(crd * double(rng.nextInt(18)));
    const EnchantPick pick = pickEnchantments(s, level, rng.nextLong(), 0);
    for (int i = 0; i < pick.count; ++i) setEnchantment(s, pick.list[size_t(i)].first, pick.list[size_t(i)].second);
}

// Vanilla Mob.canReplaceCurrentItem, simplified: an empty slot takes anything; armor with
// more points (then toughness) wins; a sword beats any non-sword, a stronger sword a sword;
// a tool beats what isn't a sword or a weaker tool.
bool better(const ItemStack& now, const ItemStack& old) {
    if (old.empty()) return true;
    const ItemDef& n = itemRegistry().item(now.item);
    const ItemDef& o = itemRegistry().item(old.item);
    if (n.armorSlot != 0) {
        if (n.armor != o.armor) return n.armor > o.armor;
        return n.toughness > o.toughness;
    }
    const bool nSword = n.tool == ToolType::Sword, oSword = o.tool == ToolType::Sword;
    if (nSword) return !oSword || n.attackDamage > o.attackDamage;
    if (oSword) return false;
    if (n.tool != ToolType::None && n.durability > 0)
        return o.tool == ToolType::None || n.attackDamage > o.attackDamage;
    return false;
}

} // namespace

void Mobs::setGear(World& world, MobData& m, int slot, const ItemStack& stack) {
    Chunk* c = chunkOf(world, m);
    if (!c || slot < 0 || slot > 4) return;
    c->addMobStore(m.uuidHi)[size_t(slot)] = stack;
    m.hasGear = true;
    refreshGear(world, m);
}

void Mobs::refreshGear(World& world, MobData& m) {
    const Chunk* c = chunkOf(world, m);
    const ItemContents* s = c && m.hasGear ? c->mobStore(m.uuidHi) : nullptr;
    int epf = 0;
    for (int i = 0; i < 4; ++i) {
        const ItemStack& st = s ? (*s)[size_t(i)] : ItemStack{};
        m.worn[size_t(i)] = st.empty() ? 0 : armorMaterial(itemRegistry().item(st.item).id);
        if (!st.empty()) epf += enchantLevel(st, Enchantment::Protection);
    }
    m.gearEpf = uint8_t(std::min(epf, 20));
    if (s && !(*s)[4].empty()) m.heldItem = (*s)[4].item;
}

int Mobs::armorPoints(const MobData& m) {
    // (wiki: Zombie - 2 points of natural armor; husks, drowned, zombie villagers alike)
    int points = isZombie(m.type) ? 2 : 0;
    if (m.type != MobType::ArmorStand)
        for (int i = 0; i < 4; ++i) points += kPoints[m.worn[size_t(i)] % 10][i];
    return std::min(points, 30);
}

float Mobs::armorToughness(const MobData& m) {
    float t = 0.0f;
    if (m.type != MobType::ArmorStand)
        for (int i = 0; i < 4; ++i) t += m.worn[size_t(i)] == 5 ? 2.0f : m.worn[size_t(i)] == 6 ? 3.0f : 0.0f;
    return t;
}

float Mobs::weaponBonus(const World& world, const MobData& m) {
    // A mob's attack damage attribute plus the held item's modifier (its damage over a
    // fist) and Sharpness (+0.5 a level + 0.5).
    const uint16_t held = heldItemOf(m);
    if (held == 0) return 0.0f;
    const ItemDef& d = itemRegistry().item(held);
    float bonus = d.tool != ToolType::None ? std::max(0.0f, d.attackDamage - 1.0f) : 0.0f;
    if (m.hasGear)
        if (const Chunk* c = chunkOf(world, m))
            if (const ItemContents* s = c->mobStore(m.uuidHi)) {
                const int sharp = enchantLevel((*s)[4], Enchantment::Sharpness);
                if (sharp > 0) bonus += 0.5f * float(sharp) + 0.5f;
            }
    return bonus;
}

void Mobs::rollSpawnGear(Context& ctx, MobData& mob, double crd) {
    const bool zombie = isZombie(mob.type) && mob.type != MobType::Drowned;
    if (!zombie && !isSkeleton(mob.type)) return;
    // Vanilla finalizeSpawn: picking up loot 55% at the highest clamped difficulty.
    mob.canPickUpLoot = ctx.rng.nextFloat() < float(0.55 * crd);
    std::array<ItemStack, 5> gear{};
    // Armor (Mob.populateDefaultEquipmentSlots): 15% x crd; tier random(2) + 3 tries of
    // 9.5% for one better; feet first, each further piece stops with 10% (Hard) / 25%.
    if (ctx.rng.nextFloat() < float(0.15 * crd)) {
        int tier = int(ctx.rng.nextInt(2));
        for (int k = 0; k < 3; ++k)
            if (ctx.rng.nextFloat() < 0.095f) ++tier;
        const float stop = ctx.difficulty >= 3 ? 0.1f : 0.25f;
        for (int slot = 3; slot >= 0; --slot) { // (feet, legs, chest, head)
            if (slot != 3 && ctx.rng.nextFloat() < stop) break;
            if (const ItemId id = spawnArmor(tier, slot); id != kNoItem) gear[size_t(slot)] = {id, 1};
        }
    }
    // Zombies: an iron sword (1 in 3) or shovel, 1% (5% on Hard). Skeletons hold their bow.
    if (zombie && ctx.rng.nextFloat() < (ctx.difficulty >= 3 ? 0.05f : 0.01f)) {
        static const ItemId sword = *itemRegistry().find("iron_sword"), shovel = *itemRegistry().find("iron_shovel");
        gear[4] = {ctx.rng.nextInt(3) == 0 ? sword : shovel, 1};
    }
    // Enchantments (populateDefaultEquipmentEnchantments): the weapon 25% x crd (a skeleton's
    // bow too), each armor piece 50% x crd.
    static const ItemId bow = *itemRegistry().find("bow");
    if (ctx.rng.nextFloat() < float(0.25 * crd)) {
        if (gear[4].empty() && isSkeleton(mob.type) && mob.heldItem == 0) gear[4] = {bow, 1};
        if (!gear[4].empty()) enchantSpawned(gear[4], crd, ctx.rng);
    }
    for (int slot = 0; slot < 4; ++slot)
        if (!gear[size_t(slot)].empty() && ctx.rng.nextFloat() < float(0.5 * crd))
            enchantSpawned(gear[size_t(slot)], crd, ctx.rng);
    bool any = false;
    for (const ItemStack& g : gear) any = any || !g.empty();
    if (!any) return;
    Chunk* c = chunkOf(ctx.world, mob);
    if (!c) return;
    ItemContents& store = c->addMobStore(mob.uuidHi);
    for (int i = 0; i < 5; ++i) store[size_t(i)] = gear[size_t(i)];
    mob.hasGear = true;
    refreshGear(ctx.world, mob);
}

void Mobs::gearPickup(Context& ctx, MobData& m) {
    // Vanilla Mob.aiStep: with CanPickUpLoot (and mob griefing on), stacks it touches (its
    // box grown 1 block sideways) that it wants: armor into its slot, anything else in hand.
    // A picked-up stack always drops on death and keeps the mob from despawning; what it
    // replaces falls (picked-up ones always, spawn gear 18.5% - chance + 0.1).
    if (!m.canPickUpLoot || !ctx.mobGriefing || m.health <= 0.0f || ctx.items.items().empty()) return;
    const Aabb b = box(m);
    const Aabb reach{b.min - glm::dvec3(1.0, 0.0, 1.0), b.max + glm::dvec3(1.0, 0.0, 1.0)};
    for (const ItemEntity& e : ctx.items.items()) {
        if (e.pickupDelay > 0 || e.stack.empty() ||
            !reach.intersects(Aabb::fromFeet(e.pos, ItemEntities::kSize, ItemEntities::kSize)))
            continue;
        const ItemDef& d = itemRegistry().item(e.stack.item);
        const int slot = d.armorSlot >= 1 && d.armorSlot <= 4 ? d.armorSlot - 1 : 4;
        if (slot == 4 && !isZombie(m.type)) continue; // (skeletons keep their bows: ours shoot only)
        Chunk* c = chunkOf(ctx.world, m);
        if (!c) return;
        ItemContents* store = m.hasGear ? c->mobStore(m.uuidHi) : nullptr;
        ItemStack old = store ? (*store)[size_t(slot)] : ItemStack{};
        if (slot == 4 && old.empty() && m.heldItem != 0) old = {m.heldItem, 1}; // (a spear, say)
        ItemStack one = e.stack;
        one.count = 1;
        if (!better(one, old)) continue;
        const bool oldKept = (m.gearKept >> slot & 1) != 0;
        if (!old.empty() && (oldKept || ctx.rng.nextFloat() - 0.1f < 0.085f))
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), old, ctx.rng);
        ctx.items.takeOne(&e);
        setGear(ctx.world, m, slot, one);
        m.gearKept = uint8_t(m.gearKept | 1 << slot);
        m.persistent = true;
        return; // (one a tick; the item list just changed)
    }
}

void Mobs::dropGear(Context& ctx, MobData& m) {
    // Vanilla dropCustomDeathLoot: each piece 8.5% (+1% a Looting level) when the player
    // killed it, worn down to a random durability; picked-up stacks always, as they were.
    Chunk* c = chunkOf(ctx.world, m);
    ItemContents* store = m.hasGear && c ? c->mobStore(m.uuidHi) : nullptr;
    const float chance = 0.085f + 0.01f * float(m.looting);
    for (int slot = 0; slot < 5; ++slot) {
        ItemStack st = store ? (*store)[size_t(slot)] : ItemStack{};
        if (slot == 4 && st.empty() && isSkeleton(m.type)) st = {heldItemOf(m), 1}; // (its plain bow)
        if (st.empty() || st.item == kNoItem) continue;
        if (enchantLevel(st, Enchantment::VanishingCurse) > 0) continue;
        if ((m.gearKept >> slot & 1) != 0) {
            ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), st, ctx.rng);
            continue;
        }
        if (!m.lastHurtByPlayer || !ctx.mobDrops || ctx.rng.nextFloat() >= chance) continue;
        const int max = itemRegistry().item(st.item).durability;
        if (max > 0) { // (vanilla: max - random(1 + random(max(max - 3, 1))) uses left)
            const int left = max - int(ctx.rng.nextInt(1u + ctx.rng.nextInt(uint32_t(std::max(max - 3, 1)))));
            st.damage = uint16_t(std::clamp(max - left, 0, max - 1));
        }
        ctx.items.spawn(m.pos + glm::dvec3(0, 0.5, 0), st, ctx.rng);
    }
    if (store) c->removeMobStore(m.uuidHi);
    m.hasGear = false;
}

} // namespace mc
