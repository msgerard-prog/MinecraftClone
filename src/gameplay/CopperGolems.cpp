// Copper golems (M26.5b; wiki: Copper Golem, Copper Chest). Part of Mobs.
//
// A carved pumpkin set on a block of copper (any stage, waxed or not) makes a copper
// golem: the copper block becomes a copper chest of its stage and the golem stands where
// the pumpkin was. The golem sorts: it takes up to 16 items of one kind out of a copper
// chest, looks over the chests around for one that already holds that item (or an
// empty one), and puts them in - 3 s at each chest. It oxidizes like copper, a stage
// every 7 hours or so unless waxed with honeycomb; an axe scrapes a stage (or the wax)
// off. Fully oxidized it freezes (vanilla: it becomes a statue block).
#include "gameplay/Mobs.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace mc {

using namespace world;

namespace {

constexpr int kOxidizeTicks = 504000; // (420-460 minutes a stage - wiki)
constexpr const char* kStages[4] = {"", "exposed_", "weathered_", "oxidized_"};

// The oxidation stage and waxing of a copper block id (-1: not a copper block).
int copperStage(BlockId b, bool& waxed) {
    std::string id = blockRegistry().block(b).id.substr(10);
    waxed = id.starts_with("waxed_");
    if (waxed) id.erase(0, 6);
    for (int s = 0; s < 4; ++s)
        if (id == (s == 0 ? std::string("copper_block") : std::string(kStages[s]) + "copper"))
            return s;
    return -1;
}
bool isCopperChest(BlockId b) {
    return blockRegistry().likeOf(b) == blocks::Chest && b != blocks::Chest;
}

} // namespace

bool Mobs::buildCopperGolem(World& world, const BlockPos& pumpkin, Xoroshiro& rng) {
    const auto& r = blockRegistry();
    if (r.likeOf(r.blockOf(world.getBlock(pumpkin))) != blocks::CarvedPumpkin) return false; // (or a jack o'lantern)
    const BlockPos below{pumpkin.x, pumpkin.y - 1, pumpkin.z};
    bool waxed = false;
    const int stage = copperStage(r.blockOf(world.getBlock(below)), waxed);
    if (stage < 0) return false;
    const auto chest =
        r.findBlock(std::string(waxed ? "waxed_" : "") + kStages[stage] + "copper_chest");
    if (!chest) return false;
    world.updateBlock(pumpkin, 0);
    world.updateBlock(below, r.defaultState(*chest));
    MobData g =
        make(MobType::CopperGolem, {pumpkin.x + 0.5, double(pumpkin.y), pumpkin.z + 0.5}, rng);
    g.woolColour = uint8_t(stage);
    g.sheared = waxed;
    g.persistent = true;
    return add(world, g);
}

Mobs::Use Mobs::copperGolemInteract(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    (void)rng;
    (void)items;
    if (held == kNoItem) return Use::None;
    const ItemDef& def = itemRegistry().item(held);
    if (def.id == "minecraft:honeycomb" && !m.sheared) { // waxed: it ages no further
        m.sheared = true;
        return Use::Fed;
    }
    if (def.tool == ToolType::Axe) { // scraped: the wax, else a stage of oxidation
        if (m.sheared)
            m.sheared = false;
        else if (m.woolColour > 0)
            --m.woolColour;
        else
            return Use::None;
        m.goal = m.pos;
        return Use::Sheared; // (the axe wears)
    }
    return Use::None;
}

bool Mobs::copperGolemGoal(Context& ctx, MobData& m, double& speed) {
    if (m.type != MobType::CopperGolem) return false;
    // Oxidation.
    if (!m.sheared && m.woolColour < 3) {
        if (m.eggTicks <= 0 || m.eggTicks > kOxidizeTicks + 48000)
            m.eggTicks = kOxidizeTicks + int(ctx.rng.nextInt(48000));
        if (--m.eggTicks <= 0) ++m.woolColour;
    }
    if (m.woolColour >= 3 && !m.sheared) { // fully oxidized: frozen
        m.goal = m.pos;
        m.vel.x = m.vel.z = 0.0;
        // (M29.6; wiki: Copper Golem Statue) after 5-10 minutes frozen it is a statue block
        if (m.eggTicks <= 0) m.eggTicks = 6000 + int(ctx.rng.nextInt(6000));
        if (--m.eggTicks == 0) {
            const BlockPos at{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)), int(std::floor(m.pos.z))};
            if (BlockUpdates::replaceable(ctx.world.getBlock(at))) {
                static constexpr const char* kFacing[4] = {"south", "west", "north", "east"}; // (vanilla yaw quarters)
                const int q = int(std::floor(std::fmod(std::fmod(m.yaw, 360.0f) + 360.0f, 360.0f) / 90.0f + 0.5f)) & 3;
                const BlockStateId st = blockRegistry().defaultState(*blockRegistry().findBlock("minecraft:oxidized_copper_golem_statue"));
                ctx.world.updateBlock(at, blockRegistry().with(st, "facing", kFacing[q]).value_or(st));
                m.vanish = true;
            }
        }
        return true;
    }
    speed *= 1.2;
    // Waiting at a chest (3 s), then taking or putting.
    const ChunkPos c0{blockToChunk(int(std::floor(m.pos.x))),
                      blockToChunk(int(std::floor(m.pos.z)))};
    auto chestAt = [&](const glm::ivec3& p) -> ChestData* {
        Chunk* ch = ctx.world.chunk({blockToChunk(p.x), blockToChunk(p.z)});
        return ch ? ch->chest(blockToLocal(p.x), p.y, blockToLocal(p.z)) : nullptr;
    };
    if (m.workTarget.y != kNoPoint) {
        const glm::dvec3 t(m.workTarget.x + 0.5, double(m.workTarget.y), m.workTarget.z + 0.5);
        if (glm::length(glm::dvec2(t.x - m.pos.x, t.z - m.pos.z)) > 1.8) {
            m.goal = t;
            m.spellTicks = 60;
            return true;
        }
        m.goal = m.pos;
        if (--m.spellTicks > 0) return true;
        ChestData* chest = chestAt(m.workTarget);
        if (chest && m.allayCount == 0) { // taking: up to 16 of the first kind it finds
            for (ItemStack& s : chest->items)
                if (!s.empty()) {
                    const uint8_t n = uint8_t(std::min<int>(16, s.count));
                    m.mouthItem = s.item;
                    m.allayCount = n;
                    s.count = uint8_t(s.count - n);
                    if (s.count == 0) s = {};
                    break;
                }
        } else if (chest) { // putting: onto its stacks, then into empty slots
            const int max = std::max<int>(1, itemRegistry().item(m.mouthItem).maxStack);
            for (int pass = 0; pass < 2 && m.allayCount > 0; ++pass)
                for (ItemStack& s : chest->items) {
                    if (m.allayCount == 0) break;
                    if (pass == 0 && s.item == m.mouthItem && s.count < max) {
                        const int n = std::min<int>(m.allayCount, max - s.count);
                        s.count = uint8_t(s.count + n);
                        m.allayCount = uint8_t(m.allayCount - n);
                    } else if (pass == 1 && s.empty()) {
                        s = {m.mouthItem, m.allayCount};
                        m.allayCount = 0;
                    }
                }
            if (m.allayCount == 0) m.mouthItem = kNoItem;
        }
        if (Chunk* ch =
                ctx.world.chunk({blockToChunk(m.workTarget.x), blockToChunk(m.workTarget.z)}))
            ch->markDirty();
        m.workTarget.y = kNoPoint;
        return true;
    }
    // Looking for the next chest now and then: carrying nothing, a copper chest with
    // items; carrying, a wooden chest holding that item, else one with an empty slot.
    if (ctx.rng.nextInt(20) != 0) return false;
    double best = 16.0 * 16.0, bestEmpty = 16.0 * 16.0;
    glm::ivec3 found{0, kNoPoint, 0}, empty{0, kNoPoint, 0};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (Chunk* ch = ctx.world.chunk({c0.x + dx, c0.z + dz}))
                for (const auto& e : ch->chests()) {
                    const glm::ivec3 p{(c0.x + dx) * 16 + e.x, e.y, (c0.z + dz) * 16 + e.z};
                    if (std::abs(p.y - int(std::floor(m.pos.y))) > 8) continue;
                    const BlockId b = blockRegistry().blockOf(ch->get(e.x, e.y, e.z));
                    const double d = glm::dot(glm::dvec3(p) - m.pos, glm::dvec3(p) - m.pos);
                    bool any = false, same = false, room = false;
                    for (const ItemStack& s : e.data.items) {
                        any = any || !s.empty();
                        same = same || (m.allayCount > 0 && s.item == m.mouthItem && !s.empty());
                        room = room || s.empty();
                    }
                    if (m.allayCount == 0) {
                        if (isCopperChest(b) && any && d < best) best = d, found = p;
                    } else if (b == blocks::Chest) {
                        if (same && room && d < best) best = d, found = p;
                        if (!any && d < bestEmpty) bestEmpty = d, empty = p;
                    }
                }
    if (found.y == kNoPoint) found = empty;
    if (found.y == kNoPoint) return false;
    m.workTarget = found;
    m.spellTicks = 60;
    m.goal = glm::dvec3(found) + glm::dvec3(0.5, 0.0, 0.5);
    return true;
}

} // namespace mc
