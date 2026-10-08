// The copper family's ageing (M23.4b; wiki: Block of Copper › Oxidation, Waxing,
// Scraping; Copper Bulb). Part of BlockUpdates.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <cmath>
#include <string>
#include <vector>

namespace mc::world {

namespace {

const BlockRegistry& R() { return blockRegistry(); }

// Each copper block's oxidation stage (0 unaffected .. 3 oxidized), whether it is waxed
// and its neighbours in the family (the next/previous stage, the waxed/unwaxed copy),
// worked out once from the names.
struct CopperInfo {
    int stage = -1; // -1: not copper
    bool waxed = false;
    BlockId next = 0, previous = 0, waxedCopy = 0, unwaxedCopy = 0;
};
const std::vector<CopperInfo>& copperTable() {
    static const std::vector<CopperInfo> table = [] {
        static constexpr const char* kStages[4] = {"", "exposed_", "weathered_", "oxidized_"};
        const auto& r = R();
        std::vector<CopperInfo> t(r.blockCount());
        auto nameFor = [&](bool waxed, int stage, const std::string& core) {
            std::string n = std::string("minecraft:") + (waxed ? "waxed_" : "");
            if (core == "copper") n += stage == 0 ? "copper_block" : std::string(kStages[stage]) + "copper";
            else n += std::string(kStages[stage]) + core;
            return n;
        };
        for (size_t b = 0; b < t.size(); ++b) {
            std::string id = r.block(BlockId(b)).id.substr(10);
            if (id.find("copper") == std::string::npos || id.find("ore") != std::string::npos ||
                id.find("raw_") != std::string::npos)
                continue;
            CopperInfo& c = t[b];
            c.waxed = id.starts_with("waxed_");
            if (c.waxed) id.erase(0, 6);
            c.stage = 0;
            for (int s = 1; s < 4; ++s)
                if (id.starts_with(kStages[s])) {
                    c.stage = s;
                    id.erase(0, std::string(kStages[s]).size());
                }
            const std::string core = id == "copper_block" ? "copper" : id;
            auto find = [&](bool waxed, int stage) -> BlockId {
                const auto f = r.findBlock(nameFor(waxed, stage, core));
                return f ? *f : BlockId(0);
            };
            if (!find(c.waxed, c.stage)) { // (not a staged copper block)
                c.stage = -1;
                continue;
            }
            if (c.stage < 3) c.next = find(c.waxed, c.stage + 1);
            if (c.stage > 0) c.previous = find(c.waxed, c.stage - 1);
            c.waxedCopy = find(true, c.stage);
            c.unwaxedCopy = find(false, c.stage);
        }
        return t;
    }();
    return table;
}
const CopperInfo& copperInfo(BlockId b) {
    static const CopperInfo none;
    return b < copperTable().size() ? copperTable()[b] : none;
}

// The same state on another block of the family (property values copied by name).
BlockStateId transfer(BlockStateId from, BlockId to) {
    const auto& r = R();
    BlockStateId s = r.defaultState(to);
    for (const Property* p : r.block(to).properties)
        if (const auto v = r.value(from, p->name)) s = r.with(s, p->name, *v).value_or(s);
    return s;
}

} // namespace

bool BlockUpdates::isCopper(BlockId b) { return copperInfo(b).stage >= 0; }

namespace {
// Copper doors age, wax and scrape as one: the other half follows (M23 review).
void changeCopper(World& world, const BlockPos& p, BlockStateId s, BlockId to) {
    world.updateBlock(p, transfer(s, to));
    if (R().likeOf(R().blockOf(s)) != blocks::OakDoor) return;
    const bool upper = R().get(s, properties::doorHalf) == 0;
    const BlockPos other{p.x, p.y + (upper ? -1 : 1), p.z};
    const BlockStateId os = world.getBlock(other);
    if (R().blockOf(os) == R().blockOf(s)) world.updateBlock(other, transfer(os, to));
}
} // namespace

void BlockUpdates::tickCopper(const BlockPos& p, BlockStateId s) {
    // Oxidation (wiki: Block of Copper › Oxidation): a 64/1125 chance each random tick
    // to try; no unwaxed copper within 4 blocks (taxicab) may be less oxidized; then a
    // chance ((b + 1) / (a + b + 1))^2 - a: as oxidized, b: more oxidized - times 0.75
    // for unaffected copper.
    const CopperInfo& self = copperInfo(R().blockOf(s));
    if (self.stage < 0 || self.waxed || !self.next) return;
    if (m_random.nextFloat() >= 64.0f / 1125.0f) return;
    int same = 0, more = 0;
    for (int dx = -4; dx <= 4; ++dx)
        for (int dy = -4; dy <= 4; ++dy)
            for (int dz = -4; dz <= 4; ++dz) {
                const int d = std::abs(dx) + std::abs(dy) + std::abs(dz);
                if (d == 0 || d > 4) continue;
                const CopperInfo& o = copperInfo(R().blockOf(at({p.x + dx, p.y + dy, p.z + dz})));
                if (o.stage < 0 || o.waxed) continue;
                if (o.stage < self.stage) return;
                if (o.stage > self.stage) ++more;
                else ++same;
            }
    float chance = float(more + 1) / float(same + more + 1);
    chance *= chance;
    if (self.stage == 0) chance *= 0.75f;
    if (m_random.nextFloat() < chance) {
        if (R().likeOf(R().blockOf(s)) == blocks::OakDoor && R().get(s, properties::doorHalf) == 0)
            return; // (a door ages from its lower half, which brings the upper along)
        changeCopper(m_world, p, s, self.next);
    }
}

bool BlockUpdates::waxCopper(World& world, const BlockPos& p) {
    const BlockStateId s = world.getBlock(p);
    const CopperInfo& c = copperInfo(R().blockOf(s));
    if (c.stage < 0 || c.waxed || !c.waxedCopy) return false;
    changeCopper(world, p, s, c.waxedCopy);
    world.playSound(Sound::WoodClick, p.x + 0.5, p.y + 0.5, p.z + 0.5, 1.0f, 1.2f); // (vanilla: item.honeycomb.wax_on)
    return true;
}

bool BlockUpdates::scrapeCopper(World& world, const BlockPos& p) {
    // An axe takes the wax off, else scrapes a stage of oxidation (wiki: Axe).
    const BlockStateId s = world.getBlock(p);
    const CopperInfo& c = copperInfo(R().blockOf(s));
    if (c.stage < 0) return false;
    const BlockId to = c.waxed ? c.unwaxedCopy : c.previous;
    if (!to) return false;
    changeCopper(world, p, s, to);
    world.playSound(Sound::WoodClick, p.x + 0.5, p.y + 0.5, p.z + 0.5, 1.0f, 0.9f);
    return true;
}

void BlockUpdates::updateBulb(const BlockPos& p, BlockStateId s) {
    // A copper bulb toggles its light on each rising edge of power (wiki: Copper Bulb).
    const bool power = bestNeighbourSignal(p) > 0;
    const bool was = R().get(s, properties::powered) == 0;
    if (power == was) return;
    BlockStateId now = R().set(s, properties::powered, power ? 0 : 1);
    if (power) now = R().set(now, properties::lit, R().get(s, properties::lit) == 0 ? 1 : 0);
    set(p, now);
}

} // namespace mc::world
