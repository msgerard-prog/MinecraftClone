#pragma once

#include "world/Chunk.h"
#include "world/World.h"

#include <array>
#include <memory>
#include <vector>

namespace mc::world {

// A chunk and its 8 neighbours' sections, shared read-only: safe to read on a
// worker while the main thread edits (copy-on-write). Light within 15 blocks of a
// chunk is all that can reach it, so this is everything its light depends on.
struct ChunkNeighbourhood {
    ChunkPos center;
    // [(dz + 1) * 3 + (dx + 1)][section]
    std::array<std::array<std::shared_ptr<const Section>, kMaxSections>, 9> sections;
    bool hasSkyLight = true; // false in the Nether and the End (wiki: Light › Sky light)
    HeightRange height;      // the dimension's (sections [0, height.sections()) are set)

    // Main thread. Returns false if any of the 9 chunks isn't loaded.
    static bool capture(const World& world, ChunkPos center, ChunkNeighbourhood& out);
};

using ChunkLight = std::array<std::shared_ptr<const SectionLight>, kMaxSections>;

// Sky and block light of the centre chunk (vanilla rules, wiki: Light):
// - sky light is 15 under open sky and travels straight down without loss through
//   transparent blocks; through water (opacity 1) it drops 1 per block;
// - light spreads to the 6 neighbours losing max(1, opacity) per step; opaque blocks
//   (opacity 15) stop it;
// - block light starts at each emitter's level (glowstone 15, torch 14).
// GL-free and thread-safe (reads only the shared sections and the block registry).
ChunkLight computeChunkLight(const ChunkNeighbourhood& n);

// Incremental light (M31.1; vanilla LightEngine.checkBlock): after blocks of the centre
// chunk changed, the light of the 3x3 chunks around it is corrected from where it was,
// instead of computed again from scratch. At each edited block the light is taken away
// and spread back from its new emission and its neighbours; taking light away follows it
// out (a "decrease" queue: neighbours dimmer than it lose theirs, brighter ones become
// sources) and the "increase" queue lights everything up again (sky light keeps going
// straight down at 15 through clear blocks, as in computeChunkLight). Light never
// reaches more than 15 blocks, so the 3x3 chunks hold every change. Worker thread.
struct IncrementalLightInput {
    ChunkNeighbourhood blocks;      // the 9 chunks' sections, after the edits
    std::array<ChunkLight, 9> light; // their light before (consistent with the old blocks)
    std::vector<BlockPos> edits;    // the edited blocks (in the centre chunk)
};
struct IncrementalLightOutput {
    std::array<ChunkLight, 9> light;       // the new light (unchanged sections shared)
    std::array<uint32_t, 9> changed{};     // bit per section whose light changed
};
void updateLightIncremental(const IncrementalLightInput& in, IncrementalLightOutput& out);

// Shared immutable section light with no block light and uniform sky light 15 (open
// sky) or 0 (dark); computeChunkLight returns these for such sections.
const std::shared_ptr<const SectionLight>& sharedUniformLight(uint8_t sky);

} // namespace mc::world
