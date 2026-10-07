#include "world/Blocks.h"

#include <cassert>

namespace mc::world {

namespace properties {
const Property axis{"axis", {"x", "y", "z"}};
const Property snowy{"snowy", {"true", "false"}};
} // namespace properties

namespace {

// Values from each block's minecraft.wiki infobox (Java Edition).
BlockRegistry buildVanillaBlocks() {
    using namespace properties;
    BlockRegistry r;
    auto check = [](BlockId got, BlockId expected) {
        assert(got == expected && "registration order must match the blocks:: enum");
        (void)got;
        (void)expected;
    };
    check(r.add("air", {.opaqueCube = false, .layer = RenderLayer::Invisible}), blocks::Air);
    check(r.add("stone", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Stone);
    check(r.add("grass_block", {.hardness = 0.6f, .resistance = 0.6f}, {{&snowy, "false"}}),
          blocks::GrassBlock);
    check(r.add("dirt", {.hardness = 0.5f, .resistance = 0.5f}), blocks::Dirt);
    check(r.add("cobblestone", {.hardness = 2.0f, .resistance = 6.0f}), blocks::Cobblestone);
    check(r.add("oak_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::OakPlanks);
    check(r.add("bedrock", {.hardness = -1.0f, .resistance = 3600000.0f}), blocks::Bedrock);
    check(r.add("sand", {.hardness = 0.5f, .resistance = 0.5f}), blocks::Sand);
    check(r.add("oak_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), blocks::OakLog);
    return r;
}

} // namespace

const BlockRegistry& blockRegistry() {
    static const BlockRegistry registry = buildVanillaBlocks();
    return registry;
}

} // namespace mc::world
