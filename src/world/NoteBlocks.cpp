// Note blocks (M23.6; wiki: Note Block). Part of BlockUpdates.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <cmath>
#include <string_view>

namespace mc::world {

namespace {
namespace B = blocks;
const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 n = kDirectionNormals[int(d)];
    return {p.x + n.x, p.y + n.y, p.z + n.z};
}
bool flag(BlockStateId s, const Property& p) { return R().get(s, p) == 0; } // [true, false]
BlockStateId withFlag(BlockStateId s, const Property& p, bool v) { return R().set(s, p, v ? 0 : 1); }
} // namespace

// The instrument a block below gives (wiki: Note Block › Instruments), in the
// instrument property's order: harp 0, basedrum 1, snare 2, hat 3, bass 4, flute 5,
// bell 6, guitar 7, chime 8, xylophone 9, iron_xylophone 10, cow_bell 11,
// didgeridoo 12, bit 13, banjo 14, pling 15.
int BlockUpdates::noteInstrument(BlockStateId below) {
    const BlockId b = R().blockOf(below);
    const std::string_view id = R().block(b).id;
    switch (b) {
    case blocks::GoldBlock: return 6;
    case blocks::Clay: return 5;
    case blocks::PackedIce: return 8;
    case blocks::BoneBlock: return 9;
    case blocks::IronBlock: return 10;
    case blocks::SoulSand: return 11;
    case blocks::Pumpkin: return 12;
    case blocks::EmeraldBlock: return 13;
    case blocks::HayBlock: return 14;
    case blocks::Glowstone: return 15;
    case blocks::Sand:
    case blocks::RedSand:
    case blocks::Gravel: return 2;
    case blocks::Glass:
    case blocks::SeaLantern:
    case blocks::Beacon: return 3;
    default: break;
    }
    if (id.ends_with("_wool")) return 7;
    if (id.ends_with("_concrete_powder")) return 2;
    if (id.ends_with("stained_glass") || id.ends_with("glass_pane")) return 3;
    const BlockSettings& st = R().block(b).settings;
    // Wood (planks, logs, wooden workstations...) gives bass; stone-like blocks the bass drum.
    if (st.tool == HarvestTool::Axe || id.ends_with("_planks") || id.ends_with("_log") || id.ends_with("_wood") ||
        id.ends_with("_stem") || id.ends_with("_hyphae"))
        return 4;
    if (R().opaqueCube(below) && (st.tool == HarvestTool::Pickaxe || b == blocks::Stone || b == blocks::Cobblestone ||
                                  b == blocks::Netherrack || b == blocks::Bedrock || b == blocks::Obsidian ||
                                  id.ends_with("_ore") || id.ends_with("stone") || id.ends_with("bricks")))
        return 1;
    return 0;
}

// Vanilla: a note sounds only with air above it; pitch 2^((note - 12) / 12).
void BlockUpdates::playNote(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (blockOf(s) != B::NoteBlock || at(rel(p, Direction::Up)) != 0) return;
    const int instrument = R().get(s, properties::noteInstrument);
    const int n = R().get(s, properties::note);
    m_world.playSound(static_cast<Sound>(int(Sound::NoteHarp) + instrument), p.x + 0.5, p.y + 0.5, p.z + 0.5, 1.0f,
                      std::pow(2.0f, float(n - 12) / 12.0f));
    m_world.levelEvent(LevelEvent::Type::Note, p.x + 0.5, p.y + 1.2, p.z + 0.5, uint32_t(n));
}

void BlockUpdates::noteBlockChanged(const BlockPos& p, BlockStateId s) {
    BlockStateId want = R().set(s, properties::noteInstrument, noteInstrument(at(rel(p, Direction::Down))));
    const bool on = bestNeighbourSignal(p) > 0;
    const bool rising = on && !flag(s, properties::powered);
    want = withFlag(want, properties::powered, on);
    if (want != s) setRaw(p, want);
    if (rising) playNote(p); // a rising edge plays it (wiki)
}

} // namespace mc::world
