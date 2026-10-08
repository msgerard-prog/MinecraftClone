#include "world/Blocks.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <tuple>
#include <vector>

namespace mc::world {

namespace properties {
const Property axis{"axis", {"x", "y", "z"}};
const Property snowy{"snowy", {"true", "false"}};
const Property level{
    "level",
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}};
const Property lit{"lit", {"true", "false"}};
const Property distance{"distance", {"1", "2", "3", "4", "5", "6", "7"}};
const Property persistent{"persistent", {"true", "false"}};
const Property layers{"layers", {"1", "2", "3", "4", "5", "6", "7", "8"}};
const Property facing{"facing", {"north", "south", "west", "east"}};
const Property power{
    "power",
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}};
const Property north{"north", {"up", "side", "none"}};
const Property east{"east", {"up", "side", "none"}};
const Property south{"south", {"up", "side", "none"}};
const Property west{"west", {"up", "side", "none"}};
const Property delay{"delay", {"1", "2", "3", "4"}};
const Property locked{"locked", {"true", "false"}};
const Property powered{"powered", {"true", "false"}};
const Property face{"face", {"floor", "wall", "ceiling"}};
const Property facing6{"facing", {"down", "up", "north", "south", "west", "east"}};
const Property extended{"extended", {"true", "false"}};
const Property shortArm{"short", {"true", "false"}};
const Property pistonType{"type", {"normal", "sticky"}};
const Property haxis{"axis", {"x", "z"}};
const Property eye{"eye", {"true", "false"}};
const Property stage{"stage", {"0", "1"}};
const Property age{"age", {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}};
const Property fireUp{"up", {"true", "false"}};
const Property fireNorth{"north", {"true", "false"}};
const Property fireEast{"east", {"true", "false"}};
const Property fireSouth{"south", {"true", "false"}};
const Property fireWest{"west", {"true", "false"}};
const Property faceDown{"down", {"true", "false"}};
const Property moisture{"moisture", {"0", "1", "2", "3", "4", "5", "6", "7"}};
const Property age7{"age", {"0", "1", "2", "3", "4", "5", "6", "7"}};
const Property age3{"age", {"0", "1", "2", "3"}};
const Property open{"open", {"true", "false"}};
const Property doorHalf{"half", {"upper", "lower"}};
const Property hinge{"hinge", {"left", "right"}};
const Property slabHalf{"half", {"top", "bottom"}};
const Property slabType{"type", {"top", "bottom", "double"}};
const Property stairShape{"shape", {"straight", "inner_left", "inner_right", "outer_left", "outer_right"}};
const Property wallNorth{"north", {"none", "low", "tall"}};
const Property wallEast{"east", {"none", "low", "tall"}};
const Property wallSouth{"south", {"none", "low", "tall"}};
const Property wallWest{"west", {"none", "low", "tall"}};
const Property hanging{"hanging", {"true", "false"}};
const Property bambooLeaves{"leaves", {"none", "small", "large"}};
const Property rotation16{"rotation", {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}};
const Property attached{"attached", {"true", "false"}};
const Property signalFire{"signal_fire", {"true", "false"}};
const Property inWall{"in_wall", {"true", "false"}};
const Property comparatorMode{"mode", {"compare", "subtract"}};
const Property hopperFacing{"facing", {"down", "north", "south", "west", "east"}};
const Property enabled{"enabled", {"true", "false"}};
const Property triggered{"triggered", {"true", "false"}};
const Property railShape{"shape",
                         {"north_south", "east_west", "ascending_east", "ascending_west", "ascending_north",
                          "ascending_south", "south_east", "south_west", "north_west", "north_east"}};
const Property straightRailShape{"shape",
                                 {"north_south", "east_west", "ascending_east", "ascending_west", "ascending_north",
                                  "ascending_south"}};
const Property age5{"age", {"0", "1", "2", "3", "4", "5"}};
const Property age25{"age", {"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "11", "12",
                             "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25"}};
const Property chestType{"type", {"single", "left", "right"}};
const Property bedPart{"part", {"head", "foot"}};
const Property occupied{"occupied", {"true", "false"}};
const Property composterLevel{"level", {"0", "1", "2", "3", "4", "5", "6", "7", "8"}};
const Property cauldronLevel{"level", {"1", "2", "3"}};
const Property noteInstrument{"instrument",
                              {"harp", "basedrum", "snare", "hat", "bass", "flute", "bell", "guitar", "chime",
                               "xylophone", "iron_xylophone", "cow_bell", "didgeridoo", "bit", "banjo", "pling"}};
const Property note{"note", {"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "11", "12",
                             "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24"}};
const Property hasRecord{"has_record", {"true", "false"}};
const Property hasBook{"has_book", {"true", "false"}};
const Property waterlogged{"waterlogged", {"true", "false"}};
const Property pickles{"pickles", {"1", "2", "3", "4"}};
const Property honeyLevel{"honey_level", {"0", "1", "2", "3", "4", "5"}};
const Property mossTip{"tip", {"true", "false"}};
const Property creakingState{"creaking_heart_state", {"uprooted", "dormant", "awake"}};
const Property natural{"natural", {"true", "false"}};
const Property berries{"berries", {"true", "false"}};
const Property tilt{"tilt", {"none", "unstable", "partial", "full"}};
const Property thickness{"thickness", {"tip_merge", "tip", "frustum", "middle", "base"}};
const Property verticalDirection{"vertical_direction", {"up", "down"}};
const Property sculkPhase{"sculk_sensor_phase", {"inactive", "active", "cooldown"}};
const Property bloom{"bloom", {"true", "false"}};
const Property shrieking{"shrieking", {"true", "false"}};
const Property canSummon{"can_summon", {"true", "false"}};
const Property trialState{"trial_spawner_state",
                          {"inactive", "waiting_for_players", "active", "waiting_for_reward_ejection", "ejecting_reward",
                           "cooldown"}};
const Property vaultState{"vault_state", {"inactive", "active", "unlocking", "ejecting"}};
const Property ominous{"ominous", {"true", "false"}};
const Property dusted{"dusted", {"0", "1", "2", "3"}};
const Property age1{"age", {"0", "1"}};
const Property age4{"age", {"0", "1", "2", "3", "4"}};
const Property hydration{"hydration", {"0", "1", "2", "3"}};
const Property eggs{"eggs", {"1", "2", "3", "4"}};
const Property hatch{"hatch", {"0", "1", "2"}};
} // namespace properties

namespace {

// Wood (bark on every side) and stripped logs and wood for every wood (M23.3b; wiki:
// Wood, Stripped Log): the Nether's stems and hyphae too.
void addWoodBlocks(BlockRegistry& r) {
    using namespace properties;
    const BlockSettings st{.hardness = 2.0f, .resistance = 2.0f, .tool = HarvestTool::Axe};
    for (const char* w : {"oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "cherry", "mangrove", "pale_oak"}) {
        const std::string wood(w);
        r.add(wood + "_wood", st, {{&axis, "y"}});
        r.add("stripped_" + wood + "_log", st, {{&axis, "y"}});
        r.add("stripped_" + wood + "_wood", st, {{&axis, "y"}});
    }
    for (const char* w : {"crimson", "warped"}) {
        const std::string wood(w);
        r.add(wood + "_hyphae", st, {{&axis, "y"}});
        r.add("stripped_" + wood + "_stem", st, {{&axis, "y"}});
        r.add("stripped_" + wood + "_hyphae", st, {{&axis, "y"}});
    }
}

// Stained glass and panes, carpets (M23.2; wiki: Stained Glass, Stained Glass Pane,
// Carpet): 16 colours each.
void addColouredBlocks(BlockRegistry& r) {
    using namespace properties;
    for (int c = 0; c < 16; ++c) {
        const std::string colour(kDyeColours[c]);
        r.add(colour + "_stained_glass",
              {.hardness = 0.3f, .resistance = 0.3f, .opaqueCube = false, .layer = RenderLayer::Translucent});
    }
    for (int c = 0; c < 16; ++c) {
        const std::string colour(kDyeColours[c]);
        r.add(colour + "_stained_glass_pane",
              {.hardness = 0.3f, .resistance = 0.3f, .opaqueCube = false, .layer = RenderLayer::Translucent,
               .kind = BlockKind::Pane, .base = *r.findBlock(colour + "_stained_glass")},
              {{&fireEast, "false"}, {&fireNorth, "false"}, {&fireSouth, "false"}, {&fireWest, "false"}});
    }
    for (int c = 0; c < 16; ++c) {
        const std::string colour(kDyeColours[c]);
        r.add(colour + "_carpet", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false,
                                   .kind = BlockKind::Carpet, .base = *r.findBlock(colour + "_wool")});
    }
    // M23.4a (wiki: Terracotta, Glazed Terracotta, Concrete, Concrete Powder): the dyed
    // terracottas we lacked, glazed terracotta (facing turns its pattern), concrete and
    // concrete powder (falls like sand, hardens into concrete in water).
    for (int c = 0; c < 16; ++c) {
        const std::string colour(kDyeColours[c]);
        if (!r.findBlock(colour + "_terracotta"))
            r.add(colour + "_terracotta", {.hardness = 1.25f, .resistance = 4.2f, .tool = HarvestTool::Pickaxe});
        r.add(colour + "_glazed_terracotta", {.hardness = 1.4f, .resistance = 1.4f, .tool = HarvestTool::Pickaxe},
              {{&facing, "north"}});
        r.add(colour + "_concrete", {.hardness = 1.8f, .resistance = 1.8f, .tool = HarvestTool::Pickaxe});
        r.add(colour + "_concrete_powder",
              {.hardness = 0.5f, .resistance = 0.5f, .tool = HarvestTool::Shovel, .like = blocks::Sand});
    }
}

// The copper family (M23.4b; wiki: Block of Copper, Cut Copper, Chiseled Copper, Copper
// Grate, Copper Door, Copper Trapdoor, Copper Bulb): four oxidation stages, each also
// waxed. Unwaxed ones oxidize on random ticks (BlockUpdates::tickCopper); doors and
// trapdoors open by hand like wooden ones; bulbs toggle on redstone pulses.
void addCopperBlocks(BlockRegistry& r) {
    using namespace properties;
    static constexpr const char* kStages[4] = {"", "exposed_", "weathered_", "oxidized_"};
    const BlockSettings metal{.hardness = 3.0f, .resistance = 6.0f, .tool = HarvestTool::Pickaxe, .tier = 1};
    for (const bool waxed : {false, true}) {
        const std::string w = waxed ? "waxed_" : "";
        for (int i = 0; i < 4; ++i) {
            const std::string st(kStages[i]);
            BlockSettings s = metal;
            s.randomTicks = !waxed && i < 3; // (oxidized copper ages no further)
            r.add(w + (i == 0 ? "copper_block" : st + "copper"), s);
            r.add(w + st + "cut_copper", s);
            r.add(w + st + "chiseled_copper", s);
            BlockSettings grate = s;
            grate.opaqueCube = false; // (light and sight pass the holes)
            grate.layer = RenderLayer::Cutout;
            r.add(w + st + "copper_grate", grate);
            BlockSettings door = s;
            door.opaqueCube = false;
            door.layer = RenderLayer::Cutout;
            door.like = blocks::OakDoor;
            r.add(w + st + "copper_door", door,
                  {{&facing, "north"}, {&doorHalf, "lower"}, {&hinge, "left"}, {&open, "false"}, {&powered, "false"}});
            door.like = blocks::OakTrapdoor;
            r.add(w + st + "copper_trapdoor", door, {{&facing, "north"}, {&slabHalf, "bottom"}, {&open, "false"}, {&powered, "false"}});
            const BlockId bulb = r.add(w + st + "copper_bulb", s, {{&lit, "false"}, {&powered, "false"}});
            // Lit bulbs glow 15, 12, 8, 4 as they oxidize (wiki: Copper Bulb).
            static constexpr uint8_t kLight[4] = {15, 12, 8, 4};
            for (uint32_t k = 0; k < r.block(bulb).stateCount; ++k) {
                const BlockStateId bs = static_cast<BlockStateId>(r.block(bulb).firstState + k);
                if (r.get(bs, lit) == 0) r.setStateEmission(bs, kLight[i]);
            }
        }
    }
}

// Building blocks (M23.1; wiki: each block's page): the full blocks the families need
// that weren't registered yet, then the slabs, stairs and walls of vanilla's stone,
// brick, sandstone, deepslate, Nether, End and wood families.
void addBuildingFamilies(BlockRegistry& r) {
    using namespace properties;
    using HT = HarvestTool;
    struct NewBase {
        const char* id;
        float hardness, resistance;
        bool pillar; // axis property (quartz pillar)
    };
    static constexpr NewBase kBases[] = {
        {"bricks", 2.0f, 6.0f, false},
        {"polished_granite", 1.5f, 6.0f, false},
        {"polished_diorite", 1.5f, 6.0f, false},
        {"polished_andesite", 1.5f, 6.0f, false},
        {"smooth_stone", 2.0f, 6.0f, false},
        {"cobbled_deepslate", 3.5f, 6.0f, false},
        {"polished_deepslate", 3.5f, 6.0f, false},
        {"deepslate_bricks", 3.5f, 6.0f, false},
        {"cracked_deepslate_bricks", 3.5f, 6.0f, false},
        {"deepslate_tiles", 3.5f, 6.0f, false},
        {"cracked_deepslate_tiles", 3.5f, 6.0f, false},
        {"chiseled_deepslate", 3.5f, 6.0f, false},
        {"red_nether_bricks", 2.0f, 6.0f, false},
        {"polished_blackstone", 2.0f, 6.0f, false},
        {"quartz_block", 0.8f, 0.8f, false},
        {"smooth_quartz", 2.0f, 6.0f, false},
        {"quartz_bricks", 0.8f, 0.8f, false},
        {"quartz_pillar", 0.8f, 0.8f, true},
        {"chiseled_quartz_block", 0.8f, 0.8f, false},
        {"prismarine", 1.5f, 6.0f, false},
        {"prismarine_bricks", 1.5f, 6.0f, false},
        {"dark_prismarine", 1.5f, 6.0f, false},
        {"mud_bricks", 1.5f, 3.0f, false},
        {"polished_tuff", 1.5f, 6.0f, false},
        {"tuff_bricks", 1.5f, 6.0f, false},
        {"chiseled_tuff", 1.5f, 6.0f, false},
        {"chiseled_tuff_bricks", 1.5f, 6.0f, false},
        {"smooth_red_sandstone", 2.0f, 6.0f, false},
        {"cut_red_sandstone", 0.8f, 0.8f, false},
        {"chiseled_red_sandstone", 0.8f, 0.8f, false},
        {"resin_bricks", 1.5f, 6.0f, false}, // (M27.1c; wiki: Resin Bricks)
        {"chiseled_resin_bricks", 1.5f, 6.0f, false},
    };
    for (const NewBase& b : kBases) {
        const BlockSettings st{.hardness = b.hardness, .resistance = b.resistance, .tool = HT::Pickaxe};
        if (b.pillar) r.add(b.id, st, {{&axis, "y"}});
        else r.add(b.id, st);
    }
    // prefix -> "<prefix>_stairs", "_slab", "_wall"; base: the full block.
    struct Family {
        const char* prefix;
        const char* base;
        bool stairs, slab, wall;
    };
    static constexpr Family kFamilies[] = {
        {"stone", "stone", true, true, false},
        {"cobblestone", "cobblestone", true, true, true},
        {"mossy_cobblestone", "mossy_cobblestone", true, true, true},
        {"stone_brick", "stone_bricks", true, true, true},
        {"mossy_stone_brick", "mossy_stone_bricks", true, true, true},
        {"smooth_stone", "smooth_stone", false, true, false},
        {"granite", "granite", true, true, true},
        {"polished_granite", "polished_granite", true, true, false},
        {"diorite", "diorite", true, true, true},
        {"polished_diorite", "polished_diorite", true, true, false},
        {"andesite", "andesite", true, true, true},
        {"polished_andesite", "polished_andesite", true, true, false},
        {"brick", "bricks", true, true, true},
        {"sandstone", "sandstone", true, true, true},
        {"smooth_sandstone", "smooth_sandstone", true, true, false},
        {"cut_sandstone", "cut_sandstone", false, true, false},
        {"red_sandstone", "red_sandstone", true, true, true},
        {"smooth_red_sandstone", "smooth_red_sandstone", true, true, false},
        {"cut_red_sandstone", "cut_red_sandstone", false, true, false},
        {"nether_brick", "nether_bricks", true, true, true},
        {"red_nether_brick", "red_nether_bricks", true, true, true},
        {"blackstone", "blackstone", true, true, true},
        {"polished_blackstone", "polished_blackstone", true, true, true},
        {"polished_blackstone_brick", "polished_blackstone_bricks", true, true, true},
        {"cobbled_deepslate", "cobbled_deepslate", true, true, true},
        {"polished_deepslate", "polished_deepslate", true, true, true},
        {"deepslate_brick", "deepslate_bricks", true, true, true},
        {"deepslate_tile", "deepslate_tiles", true, true, true},
        {"quartz", "quartz_block", true, true, false},
        {"smooth_quartz", "smooth_quartz", true, true, false},
        {"purpur", "purpur_block", true, true, false},
        {"end_stone_brick", "end_stone_bricks", true, true, true},
        {"prismarine", "prismarine", true, true, true},
        {"prismarine_brick", "prismarine_bricks", true, true, false},
        {"dark_prismarine", "dark_prismarine", true, true, false},
        {"mud_brick", "mud_bricks", true, true, true},
        {"tuff", "tuff", true, true, true},
        {"polished_tuff", "polished_tuff", true, true, true},
        {"tuff_brick", "tuff_bricks", true, true, true},
        {"resin_brick", "resin_bricks", true, true, true},
        {"oak", "oak_planks", true, true, false},
        {"spruce", "spruce_planks", true, true, false},
        {"birch", "birch_planks", true, true, false},
        {"jungle", "jungle_planks", true, true, false},
        {"acacia", "acacia_planks", true, true, false},
        {"dark_oak", "dark_oak_planks", true, true, false},
        {"cherry", "cherry_planks", true, true, false},
        {"crimson", "crimson_planks", true, true, false},
        {"warped", "warped_planks", true, true, false},
        {"mangrove", "mangrove_planks", true, true, false},
        {"pale_oak", "pale_oak_planks", true, true, false},
        {"bamboo", "bamboo_planks", true, true, false},
        {"bamboo_mosaic", "bamboo_mosaic", true, true, false},
        {"cut_copper", "cut_copper", true, true, false},
        {"exposed_cut_copper", "exposed_cut_copper", true, true, false},
        {"weathered_cut_copper", "weathered_cut_copper", true, true, false},
        {"oxidized_cut_copper", "oxidized_cut_copper", true, true, false},
        {"waxed_cut_copper", "waxed_cut_copper", true, true, false},
        {"waxed_exposed_cut_copper", "waxed_exposed_cut_copper", true, true, false},
        {"waxed_weathered_cut_copper", "waxed_weathered_cut_copper", true, true, false},
        {"waxed_oxidized_cut_copper", "waxed_oxidized_cut_copper", true, true, false},
    };
    for (const Family& f : kFamilies) {
        const auto base = r.findBlock(f.base);
        assert(base && "family base block must be registered");
        const BlockSettings& bs = r.block(*base).settings;
        const bool wood = std::string_view(f.base).ends_with("_planks") || bs.tool == HT::Axe; // (bamboo mosaic)
        BlockSettings st{.hardness = bs.hardness, .resistance = bs.resistance, .opaqueCube = false,
                         .randomTicks = bs.randomTicks, // (cut copper stairs oxidize too)
                         .base = *base, .tool = wood ? HT::Axe : HT::Pickaxe, .tier = bs.tier};
        const std::string prefix(f.prefix);
        if (f.stairs) {
            st.kind = BlockKind::Stairs;
            r.add(prefix + "_stairs", st, {{&facing, "north"}, {&slabHalf, "bottom"}, {&stairShape, "straight"}});
        }
        if (f.slab) {
            // Slabs: 2 / 6 for stone kinds (deepslate keeps its 3.5), wood 2 / 3 (wiki: Slab).
            BlockSettings ss = st;
            ss.kind = BlockKind::Slab;
            ss.hardness = wood ? 2.0f : std::max(2.0f, bs.hardness);
            ss.resistance = wood ? 3.0f : 6.0f;
            const BlockId slab = r.add(prefix + "_slab", ss, {{&slabType, "bottom"}});
            // A double slab is a full block: it hides neighbours' faces and blocks light.
            const BlockStateId first = r.block(slab).firstState;
            for (uint32_t i = 0; i < r.block(slab).stateCount; ++i)
                if (r.get(BlockStateId(first + i), slabType) == 2) r.setStateOpaque(BlockStateId(first + i), true);
        }
        if (f.wall) {
            st.kind = BlockKind::Wall;
            r.add(prefix + "_wall", st,
                  {{&fireUp, "true"}, {&wallNorth, "none"}, {&wallEast, "none"}, {&wallSouth, "none"}, {&wallWest, "none"}});
        }
    }
}

// Every wood's doors, trapdoors, fences, fence gates, buttons and pressure plates (M23.3;
// wiki: Door, Trapdoor, Fence, Fence Gate, Button, Pressure Plate): copies of the oak
// ones that behave like them (`like`); polished blackstone's button and plate behave
// like stone's.
// Corals (M25.1; wiki: Coral, Coral Block, Coral Fan): 5 kinds, each a block, a plant
// and a fan, alive and dead. Living ones die without water next to them (blocks) or
// when not waterlogged (plants, fans). Fans stand on the floor (no wall fans yet).
void addCorals(BlockRegistry& r) {
    using namespace properties;
    for (const char* kind : kCoralKinds)
        for (const char* dead : {"", "dead_"}) {
            const std::string k = std::string(dead) + kind;
            r.add(k + "_coral_block", {.hardness = 1.5f, .resistance = 6.0f, .tool = HarvestTool::Pickaxe, .tier = 1});
            r.add(k + "_coral", {.opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout},
                  {{&waterlogged, "true"}});
            r.add(k + "_coral_fan", {.opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout},
                  {{&waterlogged, "true"}});
        }
}

void addWoodSets(BlockRegistry& r) {
    using namespace properties;
    static constexpr const char* kWoods[] = {"spruce", "birch",   "jungle",   "acacia", "dark_oak", "cherry",
                                             "crimson", "warped", "mangrove", "bamboo", "pale_oak"};
    auto copy = [&](BlockId oak, const std::string& id, HarvestTool tool = HarvestTool::Axe) {
        BlockSettings st = r.block(oak).settings;
        st.like = oak;
        st.tool = tool;
        std::vector<PropertyDefault> props;
        const BlockDef& def = r.block(oak);
        const BlockStateId d = def.defaultState;
        for (const Property* p : def.properties)
            props.push_back({p, p->values[size_t(r.get(d, *p))]});
        // (the registry takes an initializer list: rebuild one per property count)
        switch (props.size()) {
        case 1: r.add(id, st, {props[0]}); break;
        case 3: r.add(id, st, {props[0], props[1], props[2]}); break;
        case 4: r.add(id, st, {props[0], props[1], props[2], props[3]}); break;
        case 5: r.add(id, st, {props[0], props[1], props[2], props[3], props[4]}); break;
        default: assert(false && "unexpected property count");
        }
    };
    for (const char* w : kWoods) {
        const std::string wood(w);
        copy(blocks::OakDoor, wood + "_door");
        copy(blocks::OakTrapdoor, wood + "_trapdoor");
        copy(blocks::OakFence, wood + "_fence");
        copy(blocks::OakFenceGate, wood + "_fence_gate");
        copy(blocks::OakButton, wood + "_button");
        copy(blocks::OakPressurePlate, wood + "_pressure_plate");
    }
    // Signs (M23.3c; wiki: Sign, Hanging Sign - 1.0 hardness, no collision): standing
    // (16 rotations) and wall signs, hanging and wall hanging signs, for every wood.
    for (const char* w : {"oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "cherry", "crimson", "warped",
                          "mangrove", "bamboo", "pale_oak"}) {
        const std::string wood(w);
        const BlockId planks = *r.findBlock(wood + "_planks");
        BlockSettings st{.hardness = 1.0f, .resistance = 1.0f, .opaqueCube = false, .collision = false,
                         .layer = RenderLayer::Cutout, .base = planks, .tool = HarvestTool::Axe};
        st.kind = BlockKind::Sign;
        r.add(wood + "_sign", st, {{&rotation16, "0"}});
        st.kind = BlockKind::WallSign;
        r.add(wood + "_wall_sign", st, {{&facing, "north"}});
        st.kind = BlockKind::HangingSign;
        r.add(wood + "_hanging_sign", st, {{&attached, "false"}, {&rotation16, "0"}});
        st.kind = BlockKind::WallHangingSign;
        r.add(wood + "_wall_hanging_sign", st, {{&facing, "north"}});
    }
    copy(blocks::StoneButton, "polished_blackstone_button", HarvestTool::Pickaxe);
    copy(blocks::StonePressurePlate, "polished_blackstone_pressure_plate", HarvestTool::Pickaxe);
}

// Values from each block's minecraft.wiki infobox (Java Edition).
BlockRegistry buildVanillaBlocks() {
    using namespace properties;
    BlockRegistry r;
    auto check = [](BlockId got, BlockId expected) {
        assert(got == expected && "registration order must match the blocks:: enum");
        (void)got;
        (void)expected;
    };
    check(r.add("air", {.opaqueCube = false, .collision = false, .layer = RenderLayer::Invisible}),
          blocks::Air);
    check(r.add("stone", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Stone);
    check(r.add("grass_block", {.hardness = 0.6f, .resistance = 0.6f}, {{&snowy, "false"}}),
          blocks::GrassBlock);
    check(r.add("dirt", {.hardness = 0.5f, .resistance = 0.5f}), blocks::Dirt);
    check(r.add("cobblestone", {.hardness = 2.0f, .resistance = 6.0f}), blocks::Cobblestone);
    check(r.add("oak_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::OakPlanks);
    check(r.add("bedrock", {.hardness = -1.0f, .resistance = 3600000.0f}), blocks::Bedrock);
    check(r.add("sand", {.hardness = 0.5f, .resistance = 0.5f}), blocks::Sand);
    check(r.add("oak_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), blocks::OakLog);
    // Fluids: not opaque, drawn in the translucent pass; level 0 is a source block.
    check(r.add("water",
                {.hardness = 100.0f,
                 .resistance = 100.0f,
                 .lightOpacity = 1,
                 .opaqueCube = false,
                 .collision = false,
                 .layer = RenderLayer::Translucent},
                {{&level, "0"}}),
          blocks::Water);
    check(r.add("deepslate", {.hardness = 3.0f, .resistance = 6.0f}, {{&axis, "y"}}),
          blocks::Deepslate);
    check(r.add("gravel", {.hardness = 0.6f, .resistance = 0.6f}), blocks::Gravel);
    // Light sources (wiki: Light - glowstone 15, torch 14) and glass.
    check(r.add("glowstone", {.hardness = 0.3f, .resistance = 0.3f, .lightEmission = 15}),
          blocks::Glowstone);
    check(r.add("torch", {.lightEmission = 14,
                          .opaqueCube = false,
                          .collision = false,
                          .layer = RenderLayer::Cutout}),
          blocks::Torch);
    check(r.add("glass", {.hardness = 0.3f,
                          .resistance = 0.3f,
                          .opaqueCube = false,
                          .layer = RenderLayer::Cutout}),
          blocks::Glass);

    // --- World generation blocks (M8) ---
    // Leaves: cutout, not full-opaque for culling, filter light by 1 (wiki: Leaves).
    constexpr BlockSettings kLeaves{.hardness = 0.2f,
                                    .resistance = 0.2f,
                                    .lightOpacity = 1,
                                    .opaqueCube = false,
                                    .layer = RenderLayer::Cutout};
    // Plants: no collision, broken instantly, cross models.
    constexpr BlockSettings kPlant{
        .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout};
    // Ores (wiki: each ore's infobox): stone ores 3.0, deepslate ores 4.5.
    check(r.add("coal_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::CoalOre);
    check(r.add("deepslate_coal_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateCoalOre);
    check(r.add("iron_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::IronOre);
    check(r.add("deepslate_iron_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateIronOre);
    check(r.add("copper_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::CopperOre);
    check(r.add("deepslate_copper_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateCopperOre);
    check(r.add("gold_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::GoldOre);
    check(r.add("deepslate_gold_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateGoldOre);
    check(r.add("redstone_ore", {.hardness = 3.0f, .resistance = 3.0f}, {{&lit, "false"}}), blocks::RedstoneOre);
    check(r.add("deepslate_redstone_ore", {.hardness = 4.5f, .resistance = 3.0f}, {{&lit, "false"}}),
          blocks::DeepslateRedstoneOre);
    check(r.add("lapis_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::LapisOre);
    check(r.add("deepslate_lapis_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateLapisOre);
    check(r.add("diamond_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::DiamondOre);
    check(r.add("deepslate_diamond_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateDiamondOre);
    check(r.add("emerald_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::EmeraldOre);
    check(r.add("deepslate_emerald_ore", {.hardness = 4.5f, .resistance = 3.0f}),
          blocks::DeepslateEmeraldOre);
    check(r.add("granite", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Granite);
    check(r.add("diorite", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Diorite);
    check(r.add("andesite", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Andesite);
    check(r.add("tuff", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Tuff);
    check(r.add("calcite", {.hardness = 0.75f, .resistance = 0.75f}), blocks::Calcite);
    check(r.add("sandstone", {.hardness = 0.8f, .resistance = 0.8f}), blocks::Sandstone);
    check(r.add("red_sand", {.hardness = 0.5f, .resistance = 0.5f}), blocks::RedSand);
    check(r.add("red_sandstone", {.hardness = 0.8f, .resistance = 0.8f}), blocks::RedSandstone);
    check(r.add("terracotta", {.hardness = 1.25f, .resistance = 4.2f}), blocks::Terracotta);
    check(r.add("snow_block", {.hardness = 0.2f, .resistance = 0.2f}), blocks::SnowBlock);
    // Ice filters light like water; packed ice is opaque.
    check(r.add("ice", {.hardness = 0.5f,
                        .resistance = 0.5f,
                        .lightOpacity = 1,
                        .opaqueCube = false,
                        .layer = RenderLayer::Translucent}),
          blocks::Ice);
    check(r.add("packed_ice", {.hardness = 0.5f, .resistance = 0.5f}), blocks::PackedIce);
    check(r.add("clay", {.hardness = 0.6f, .resistance = 0.6f}), blocks::Clay);
    check(r.add("coarse_dirt", {.hardness = 0.5f, .resistance = 0.5f}), blocks::CoarseDirt);
    check(r.add("mossy_cobblestone", {.hardness = 2.0f, .resistance = 6.0f}),
          blocks::MossyCobblestone);
    // Lava: a fluid that emits 15 (wiki: Lava); no flow yet.
    check(r.add("lava",
                {.hardness = 100.0f,
                 .resistance = 100.0f,
                 .lightEmission = 15,
                 .lightOpacity = 1,
                 .opaqueCube = false,
                 .collision = false,
                 .layer = RenderLayer::Translucent},
                {{&level, "0"}}),
          blocks::Lava);
    check(r.add("oak_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}),
          blocks::OakLeaves);
    check(r.add("birch_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}),
          blocks::BirchLog);
    check(r.add("birch_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::BirchPlanks);
    check(r.add("birch_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}),
          blocks::BirchLeaves);
    check(r.add("spruce_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}),
          blocks::SpruceLog);
    check(r.add("spruce_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::SprucePlanks);
    check(r.add("spruce_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}),
          blocks::SpruceLeaves);
    check(r.add("acacia_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}),
          blocks::AcaciaLog);
    check(r.add("acacia_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::AcaciaPlanks);
    check(r.add("acacia_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}),
          blocks::AcaciaLeaves);
    check(r.add("short_grass", kPlant), blocks::ShortGrass);
    check(r.add("fern", kPlant), blocks::Fern);
    check(r.add("dandelion", kPlant), blocks::Dandelion);
    check(r.add("poppy", kPlant), blocks::Poppy);
    check(r.add("cornflower", kPlant), blocks::Cornflower);
    check(r.add("azure_bluet", kPlant), blocks::AzureBluet);
    check(r.add("oxeye_daisy", kPlant), blocks::OxeyeDaisy);
    check(r.add("dead_bush", kPlant), blocks::DeadBush);
    // Snow layers (wiki: Snow): 1/8 block per layer, lets light through; collision
    // simplified to none (vanilla: layers-1 eighths).
    check(r.add("snow", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false, .collision = false,
                         .layer = RenderLayer::Cutout},
                {{&layers, "1"}}),
          blocks::Snow);
    // Workstations (wiki: Crafting Table 2.5; Furnace 3.5, emits 13 when lit).
    check(r.add("crafting_table", {.hardness = 2.5f, .resistance = 2.5f}), blocks::CraftingTable);
    check(r.add("furnace", {.hardness = 3.5f, .resistance = 3.5f}, {{&facing, "north"}, {&lit, "false"}}),
          blocks::Furnace);
    for (uint32_t i = 0; i < r.block(blocks::Furnace).stateCount; ++i) {
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::Furnace).firstState + i);
        if (r.value(s, "lit") == "true") r.setStateEmission(s, 13);
    }

    // --- Redstone (M11; wiki: Redstone Dust, Redstone Torch, Redstone Repeater, Lever,
    // Button, Block of Redstone, Redstone Lamp, Piston). Components without full
    // collision are walked through (vanilla: thin boxes; known deviation).
    constexpr BlockSettings kComponent{
        .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout};
    check(r.add("redstone_wire", kComponent,
                {{&east, "none"}, {&north, "none"}, {&power, "0"}, {&south, "none"}, {&west, "none"}}),
          blocks::RedstoneWire);
    check(r.add("redstone_torch", kComponent, {{&lit, "true"}}), blocks::RedstoneTorch);
    check(r.add("redstone_wall_torch", kComponent, {{&facing, "north"}, {&lit, "true"}}),
          blocks::RedstoneWallTorch);
    check(r.add("repeater", kComponent,
                {{&delay, "1"}, {&facing, "north"}, {&locked, "false"}, {&powered, "false"}}),
          blocks::Repeater);
    BlockSettings lever = kComponent;
    lever.hardness = lever.resistance = 0.5f;
    check(r.add("lever", lever, {{&face, "wall"}, {&facing, "north"}, {&powered, "false"}}), blocks::Lever);
    check(r.add("stone_button", lever, {{&face, "wall"}, {&facing, "north"}, {&powered, "false"}}),
          blocks::StoneButton);
    check(r.add("oak_button", lever, {{&face, "wall"}, {&facing, "north"}, {&powered, "false"}}),
          blocks::OakButton);
    check(r.add("redstone_block", {.hardness = 5.0f, .resistance = 6.0f}), blocks::RedstoneBlock);
    check(r.add("redstone_lamp", {.hardness = 0.3f, .resistance = 0.3f}, {{&lit, "false"}}),
          blocks::RedstoneLamp);
    check(r.add("piston", {.hardness = 1.5f, .resistance = 1.5f}, {{&extended, "false"}, {&facing6, "north"}}),
          blocks::Piston);
    check(r.add("sticky_piston", {.hardness = 1.5f, .resistance = 1.5f},
                {{&extended, "false"}, {&facing6, "north"}}),
          blocks::StickyPiston);
    check(r.add("piston_head", {.hardness = 1.5f, .resistance = 1.5f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&facing6, "north"}, {&shortArm, "false"}, {&pistonType, "normal"}}),
          blocks::PistonHead);
    // Light: lit torches 7, lit lamps 15 (wiki: Light).
    for (BlockId b : {blocks::RedstoneTorch, blocks::RedstoneWallTorch, blocks::RedstoneLamp})
        for (uint32_t i = 0; i < r.block(b).stateCount; ++i) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(b).firstState + i);
            if (r.get(s, lit) == 0) r.setStateEmission(s, b == blocks::RedstoneLamp ? 15 : 7);
        }
    // --- The Nether and the End (M12; wiki: each block's infobox).
    check(r.add("netherrack", {.hardness = 0.4f, .resistance = 0.4f}), blocks::Netherrack);
    check(r.add("soul_sand", {.hardness = 0.5f, .resistance = 0.5f}), blocks::SoulSand);
    check(r.add("nether_quartz_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::NetherQuartzOre);
    check(r.add("nether_gold_ore", {.hardness = 3.0f, .resistance = 3.0f}), blocks::NetherGoldOre);
    check(r.add("magma_block", {.hardness = 0.5f, .resistance = 0.5f, .lightEmission = 3}), blocks::MagmaBlock);
    check(r.add("obsidian", {.hardness = 50.0f, .resistance = 1200.0f}), blocks::Obsidian);
    // Portal blocks: unbreakable, no collision; nether portals glow 11, end portals 15.
    check(r.add("nether_portal",
                {.hardness = -1.0f, .lightEmission = 11, .opaqueCube = false, .collision = false,
                 .layer = RenderLayer::Translucent},
                {{&haxis, "x"}}),
          blocks::NetherPortal);
    check(r.add("end_stone", {.hardness = 3.0f, .resistance = 9.0f}), blocks::EndStone);
    check(r.add("end_portal_frame", {.hardness = -1.0f, .resistance = 3600000.0f, .lightEmission = 1,
                                     .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&eye, "false"}, {&facing, "north"}}),
          blocks::EndPortalFrame);
    check(r.add("end_portal", {.hardness = -1.0f, .resistance = 3600000.0f, .lightEmission = 15,
                               .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout}),
          blocks::EndPortal);

    // --- Random ticks and fire (M15; wiki: Sapling, Fire).
    BlockSettings sapling = kPlant;
    sapling.randomTicks = true;
    check(r.add("oak_sapling", sapling, {{&stage, "0"}}), blocks::OakSapling);
    check(r.add("birch_sapling", sapling, {{&stage, "0"}}), blocks::BirchSapling);
    check(r.add("spruce_sapling", sapling, {{&stage, "0"}}), blocks::SpruceSapling);
    check(r.add("acacia_sapling", sapling, {{&stage, "0"}}), blocks::AcaciaSapling);
    // Fire: light 15, no collision, broken by hand instantly; ticks are scheduled (not
    // random). Its side properties say which neighbours it burns on (model only).
    check(r.add("fire", {.lightEmission = 15, .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout},
                {{&age, "0"}, {&fireEast, "false"}, {&fireNorth, "false"}, {&fireSouth, "false"},
                 {&fireUp, "false"}, {&fireWest, "false"}}),
          blocks::Fire);
    // Wool (wiki: Wool - hardness 0.8), 16 dye colours in vanilla's order.
    for (int c = 0; c < 16; ++c)
        check(r.add(std::string(kDyeColours[c]) + "_wool", {.hardness = 0.8f, .resistance = 0.8f}),
              static_cast<BlockId>(blocks::WhiteWool + c));
    // Farming (M17.1; wiki: Farmland - hardness 0.6, 15/16 tall: a full cube here;
    // crops break instantly, no collision).
    check(r.add("farmland", {.hardness = 0.6f, .resistance = 0.6f, .randomTicks = true}, {{&moisture, "0"}}),
          blocks::Farmland);
    BlockSettings crop = kPlant;
    crop.randomTicks = true;
    check(r.add("wheat", crop, {{&age7, "0"}}), blocks::Wheat);
    check(r.add("carrots", crop, {{&age7, "0"}}), blocks::Carrots);
    check(r.add("potatoes", crop, {{&age7, "0"}}), blocks::Potatoes);
    check(r.add("beetroots", crop, {{&age3, "0"}}), blocks::Beetroots);
    // Chest (M17.2; wiki: Chest - hardness 2.5; a 14/16 box: not a full cube).
    check(r.add("chest", {.hardness = 2.5f, .resistance = 2.5f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&facing, "north"}, {&chestType, "single"}}),
          blocks::Chest);
    // Bed (M17.4; wiki: Bed - hardness 0.2, 9/16 tall: not a full cube).
    check(r.add("red_bed", {.hardness = 0.2f, .resistance = 0.2f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&facing, "north"}, {&occupied, "false"}, {&bedPart, "foot"}}),
          blocks::RedBed);
    // Enchanting and anvils (M17.5; wiki: Bookshelf 1.5, Enchanting Table 5 / 1200 and
    // light 7, Anvil 5 / 1200, Block of Iron 5 / 6, Sugar Cane 0).
    check(r.add("bookshelf", {.hardness = 1.5f, .resistance = 1.5f}), blocks::Bookshelf);
    check(r.add("enchanting_table", {.hardness = 5.0f, .resistance = 1200.0f, .lightEmission = 7, .opaqueCube = false,
                                     .layer = RenderLayer::Cutout}),
          blocks::EnchantingTable);
    for (const auto& [name, id] : {std::pair{"anvil", blocks::Anvil}, std::pair{"chipped_anvil", blocks::ChippedAnvil},
                                   std::pair{"damaged_anvil", blocks::DamagedAnvil}})
        check(r.add(name, {.hardness = 5.0f, .resistance = 1200.0f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                    {{&facing, "north"}}),
              id);
    check(r.add("iron_block", {.hardness = 5.0f, .resistance = 6.0f}), blocks::IronBlock);
    BlockSettings cane = kPlant;
    cane.randomTicks = true;
    check(r.add("sugar_cane", cane, {{&age, "0"}}), blocks::SugarCane);
    // M18.1 vegetation (wiki: Cactus 0.4, Pumpkin 1.0, Mushroom 0 - the brown one gives
    // light 1). The cactus collides as a full cube here (vanilla: 14/16 wide, 15/16 tall).
    check(r.add("cactus",
                {.hardness = 0.4f, .resistance = 0.4f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                 .randomTicks = true},
                {{&age, "0"}}),
          blocks::Cactus);
    check(r.add("pumpkin", {.hardness = 1.0f, .resistance = 1.0f}), blocks::Pumpkin);
    BlockSettings mushroom = kPlant;
    mushroom.randomTicks = true;
    BlockSettings brownMushroom = mushroom;
    brownMushroom.lightEmission = 1;
    check(r.add("brown_mushroom", brownMushroom), blocks::BrownMushroom);
    check(r.add("red_mushroom", mushroom), blocks::RedMushroom);
    // M18.2 woods (wiki: Log 2.0, Planks 2.0 / 3.0, Leaves 0.2) and biome blocks
    // (Podzol 0.5, Mycelium 0.6 - spreads like grass, Mushroom Block 0.2).
    for (const auto& [wood, log, planks, leaves, saplingId] :
         {std::tuple{"jungle", blocks::JungleLog, blocks::JunglePlanks, blocks::JungleLeaves, blocks::JungleSapling},
          std::tuple{"dark_oak", blocks::DarkOakLog, blocks::DarkOakPlanks, blocks::DarkOakLeaves, blocks::DarkOakSapling},
          std::tuple{"cherry", blocks::CherryLog, blocks::CherryPlanks, blocks::CherryLeaves, blocks::CherrySapling}}) {
        const std::string w(wood);
        check(r.add(w + "_log", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), log);
        check(r.add(w + "_planks", {.hardness = 2.0f, .resistance = 3.0f}), planks);
        check(r.add(w + "_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}), leaves);
        check(r.add(w + "_sapling", sapling, {{&stage, "0"}}), saplingId);
    }
    check(r.add("podzol", {.hardness = 0.5f, .resistance = 0.5f}, {{&snowy, "false"}}), blocks::Podzol);
    check(r.add("mycelium", {.hardness = 0.6f, .resistance = 0.6f, .randomTicks = true}, {{&snowy, "false"}}),
          blocks::Mycelium);
    const std::initializer_list<PropertyDefault> capFaces = {{&faceDown, "true"},  {&fireEast, "true"},
                                                             {&fireNorth, "true"}, {&fireSouth, "true"},
                                                             {&fireUp, "true"},    {&fireWest, "true"}};
    check(r.add("brown_mushroom_block", {.hardness = 0.2f, .resistance = 0.2f}, capFaces), blocks::BrownMushroomBlock);
    check(r.add("red_mushroom_block", {.hardness = 0.2f, .resistance = 0.2f}, capFaces), blocks::RedMushroomBlock);
    check(r.add("mushroom_stem", {.hardness = 0.2f, .resistance = 0.2f}, capFaces), blocks::MushroomStem);
    // Monster spawner (wiki: hardness 5, blast resistance 5; a see-through cage).
    check(r.add("spawner", {.hardness = 5.0f, .resistance = 5.0f, .opaqueCube = false, .layer = RenderLayer::Cutout}),
          blocks::Spawner);
    // Structure blocks (M18.4; wiki: Sandstone 0.8, Smooth Sandstone 2.0 / 6.0,
    // Terracotta 1.25 / 4.2, Stone Bricks 1.5 / 6.0, TNT 0).
    check(r.add("chiseled_sandstone", {.hardness = 0.8f, .resistance = 0.8f}), blocks::ChiseledSandstone);
    check(r.add("cut_sandstone", {.hardness = 0.8f, .resistance = 0.8f}), blocks::CutSandstone);
    check(r.add("smooth_sandstone", {.hardness = 2.0f, .resistance = 6.0f}), blocks::SmoothSandstone);
    check(r.add("orange_terracotta", {.hardness = 1.25f, .resistance = 4.2f}), blocks::OrangeTerracotta);
    check(r.add("blue_terracotta", {.hardness = 1.25f, .resistance = 4.2f}), blocks::BlueTerracotta);
    check(r.add("stone_bricks", {.hardness = 1.5f, .resistance = 6.0f}), blocks::StoneBricks);
    check(r.add("mossy_stone_bricks", {.hardness = 1.5f, .resistance = 6.0f}), blocks::MossyStoneBricks);
    check(r.add("cracked_stone_bricks", {.hardness = 1.5f, .resistance = 6.0f}), blocks::CrackedStoneBricks);
    check(r.add("chiseled_stone_bricks", {.hardness = 1.5f, .resistance = 6.0f}), blocks::ChiseledStoneBricks);
    check(r.add("tnt", {}), blocks::Tnt);
    // Dirt path (wiki: hardness 0.65; 15/16 tall in vanilla, a full cube here).
    check(r.add("dirt_path", {.hardness = 0.65f, .resistance = 0.65f}), blocks::DirtPath);
    // Nether 2 (M19.1; wiki: Nylium 0.4, Stems 2.0, Planks 2.0/3.0 (not flammable),
    // Wart Block 1.0, Shroomlight 1.0 and light 15, fungi/roots/sprouts/vines 0, Soul
    // Soil 0.5, Basalt 1.25/4.2, Blackstone 1.5/6.0, Bone Block 2.0).
    check(r.add("crimson_nylium", {.hardness = 0.4f, .resistance = 0.4f}), blocks::CrimsonNylium);
    check(r.add("warped_nylium", {.hardness = 0.4f, .resistance = 0.4f}), blocks::WarpedNylium);
    check(r.add("crimson_stem", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), blocks::CrimsonStem);
    check(r.add("warped_stem", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), blocks::WarpedStem);
    check(r.add("crimson_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::CrimsonPlanks);
    check(r.add("warped_planks", {.hardness = 2.0f, .resistance = 3.0f}), blocks::WarpedPlanks);
    check(r.add("nether_wart_block", {.hardness = 1.0f, .resistance = 1.0f}), blocks::NetherWartBlock);
    check(r.add("warped_wart_block", {.hardness = 1.0f, .resistance = 1.0f}), blocks::WarpedWartBlock);
    check(r.add("shroomlight", {.hardness = 1.0f, .resistance = 1.0f, .lightEmission = 15}), blocks::Shroomlight);
    for (const auto& [name, id] : {std::pair{"crimson_fungus", blocks::CrimsonFungus},
                                   std::pair{"warped_fungus", blocks::WarpedFungus},
                                   std::pair{"crimson_roots", blocks::CrimsonRoots},
                                   std::pair{"warped_roots", blocks::WarpedRoots},
                                   std::pair{"nether_sprouts", blocks::NetherSprouts}})
        check(r.add(name, kPlant), id);
    check(r.add("weeping_vines", kPlant, {{&age25, "0"}}), blocks::WeepingVines);
    check(r.add("weeping_vines_plant", kPlant), blocks::WeepingVinesPlant);
    check(r.add("twisting_vines", kPlant, {{&age25, "0"}}), blocks::TwistingVines);
    check(r.add("twisting_vines_plant", kPlant), blocks::TwistingVinesPlant);
    check(r.add("soul_soil", {.hardness = 0.5f, .resistance = 0.5f}), blocks::SoulSoil);
    check(r.add("basalt", {.hardness = 1.25f, .resistance = 4.2f}, {{&axis, "y"}}), blocks::Basalt);
    check(r.add("blackstone", {.hardness = 1.5f, .resistance = 6.0f}), blocks::Blackstone);
    check(r.add("bone_block", {.hardness = 2.0f, .resistance = 2.0f}, {{&axis, "y"}}), blocks::BoneBlock);
    // M19.3 (wiki: Nether Bricks 2.0/6.0, fence the same - a post here, no connections;
    // Nether Wart 0 with ages 0-3, random ticks; Polished Blackstone Bricks and Gilded
    // Blackstone 1.5/6.0; Block of Gold 3.0/6.0; Polished Basalt 1.25/4.2).
    check(r.add("nether_bricks", {.hardness = 2.0f, .resistance = 6.0f}), blocks::NetherBricks);
    check(r.add("nether_brick_fence",
                {.hardness = 2.0f, .resistance = 6.0f, .opaqueCube = false, .layer = RenderLayer::Cutout}),
          blocks::NetherBrickFence);
    BlockSettings wart = kPlant;
    wart.randomTicks = true;
    check(r.add("nether_wart", wart, {{&age3, "0"}}), blocks::NetherWart);
    check(r.add("polished_blackstone_bricks", {.hardness = 1.5f, .resistance = 6.0f}), blocks::PolishedBlackstoneBricks);
    check(r.add("cracked_polished_blackstone_bricks", {.hardness = 1.5f, .resistance = 6.0f}),
          blocks::CrackedPolishedBlackstoneBricks);
    check(r.add("chiseled_polished_blackstone", {.hardness = 1.5f, .resistance = 6.0f}),
          blocks::ChiseledPolishedBlackstone);
    check(r.add("gilded_blackstone", {.hardness = 1.5f, .resistance = 6.0f}), blocks::GildedBlackstone);
    check(r.add("gold_block", {.hardness = 3.0f, .resistance = 6.0f}), blocks::GoldBlock);
    check(r.add("polished_basalt", {.hardness = 1.25f, .resistance = 4.2f}, {{&axis, "y"}}), blocks::PolishedBasalt);
    // Brewing stand (wiki: hardness 0.5, light 1; a rod on a base - not a full cube).
    check(r.add("brewing_stand", {.hardness = 0.5f, .resistance = 0.5f, .lightEmission = 1, .opaqueCube = false,
                                  .collision = false, .layer = RenderLayer::Cutout}),
          blocks::BrewingStand);
    // The End 2 (M20.1; wiki: End Stone Bricks 3.0/9.0, Purpur Block and Pillar
    // 1.5/6.0, Chorus Plant and Flower 0.4 (they collide as full cubes here; vanilla:
    // the plant's arms only), Iron Bars 5.0/6.0 (full cube collision here too)).
    check(r.add("end_stone_bricks", {.hardness = 3.0f, .resistance = 9.0f}), blocks::EndStoneBricks);
    check(r.add("purpur_block", {.hardness = 1.5f, .resistance = 6.0f}), blocks::PurpurBlock);
    check(r.add("purpur_pillar", {.hardness = 1.5f, .resistance = 6.0f}, {{&axis, "y"}}), blocks::PurpurPillar);
    const std::initializer_list<PropertyDefault> sixWays = {{&faceDown, "false"},  {&fireEast, "false"},
                                                            {&fireNorth, "false"}, {&fireSouth, "false"},
                                                            {&fireUp, "false"},    {&fireWest, "false"}};
    check(r.add("chorus_plant",
                {.hardness = 0.4f, .resistance = 0.4f, .opaqueCube = false, .layer = RenderLayer::Cutout}, sixWays),
          blocks::ChorusPlant);
    check(r.add("chorus_flower",
                {.hardness = 0.4f, .resistance = 0.4f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&age5, "0"}}),
          blocks::ChorusFlower);
    check(r.add("iron_bars", {.hardness = 5.0f, .resistance = 6.0f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&fireEast, "false"}, {&fireNorth, "false"}, {&fireSouth, "false"}, {&fireWest, "false"}}),
          blocks::IronBars);
    // wiki: Dragon Egg - hardness 3, resistance 9, light 1; falls like sand.
    check(r.add("dragon_egg", {.hardness = 3.0f, .resistance = 9.0f, .lightEmission = 1, .opaqueCube = false,
                               .layer = RenderLayer::Cutout}),
          blocks::DragonEgg);
    // wiki: End Gateway - unbreakable, light 15, no collision (entities pass into it).
    check(r.add("end_gateway", {.hardness = -1.0f, .resistance = 3600000.0f, .lightEmission = 15, .opaqueCube = false,
                                .collision = false, .layer = RenderLayer::Cutout}),
          blocks::EndGateway);
    // wiki: End Rod - breaks at once, light 14; points away from what it's put on.
    check(r.add("end_rod", {.hardness = 0.0f, .resistance = 0.0f, .lightEmission = 14, .opaqueCube = false,
                            .collision = false, .layer = RenderLayer::Cutout},
                {{&facing6, "up"}}),
          blocks::EndRod);
    // Redstone 2 (M21.1; wiki: Door, Trapdoor, Fence, Fence Gate, Pressure Plate -
    // wood 3.0/3.0 (fences and gates 2.0/3.0), iron door 5.0/5.0, iron trapdoor 5.0/5.0,
    // plates 0.5/0.5). Their collision shapes are in world/BlockShapes.
    constexpr BlockSettings kThin{.hardness = 3.0f, .resistance = 3.0f, .opaqueCube = false, .layer = RenderLayer::Cutout};
    BlockSettings ironThin = kThin;
    ironThin.hardness = ironThin.resistance = 5.0f;
    const std::initializer_list<PropertyDefault> doorProps = {
        {&facing, "north"}, {&doorHalf, "lower"}, {&hinge, "left"}, {&open, "false"}, {&powered, "false"}};
    const std::initializer_list<PropertyDefault> trapProps = {
        {&facing, "north"}, {&slabHalf, "bottom"}, {&open, "false"}, {&powered, "false"}};
    check(r.add("oak_door", kThin, doorProps), blocks::OakDoor);
    check(r.add("iron_door", ironThin, doorProps), blocks::IronDoor);
    check(r.add("oak_trapdoor", kThin, trapProps), blocks::OakTrapdoor);
    check(r.add("iron_trapdoor", ironThin, trapProps), blocks::IronTrapdoor);
    BlockSettings fence = kThin;
    fence.hardness = 2.0f;
    check(r.add("oak_fence", fence,
                {{&fireEast, "false"}, {&fireNorth, "false"}, {&fireSouth, "false"}, {&fireWest, "false"}}),
          blocks::OakFence);
    check(r.add("oak_fence_gate", fence,
                {{&facing, "north"}, {&inWall, "false"}, {&open, "false"}, {&powered, "false"}}),
          blocks::OakFenceGate);
    constexpr BlockSettings kPlate{
        .hardness = 0.5f, .resistance = 0.5f, .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout};
    check(r.add("oak_pressure_plate", kPlate, {{&powered, "false"}}), blocks::OakPressurePlate);
    check(r.add("stone_pressure_plate", kPlate, {{&powered, "false"}}), blocks::StonePressurePlate);
    check(r.add("light_weighted_pressure_plate", kPlate, {{&power, "0"}}), blocks::LightWeightedPressurePlate);
    check(r.add("heavy_weighted_pressure_plate", kPlate, {{&power, "0"}}), blocks::HeavyWeightedPressurePlate);
    // wiki: Redstone Comparator (breaks at once, a 2/16 slab like the repeater),
    // Observer (hardness 3.0, a pickaxe to drop).
    check(r.add("comparator",
                {.hardness = 0.0f, .resistance = 0.0f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&facing, "north"}, {&comparatorMode, "compare"}, {&powered, "false"}}),
          blocks::Comparator);
    check(r.add("observer", {.hardness = 3.0f, .resistance = 3.0f}, {{&facing6, "south"}, {&powered, "false"}}),
          blocks::Observer);
    // wiki: Hopper (3.0/4.8, pickaxe), Dispenser and Dropper (3.5/3.5, pickaxe).
    check(r.add("hopper", {.hardness = 3.0f, .resistance = 4.8f, .opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&enabled, "true"}, {&hopperFacing, "down"}}),
          blocks::Hopper);
    check(r.add("dispenser", {.hardness = 3.5f, .resistance = 3.5f}, {{&facing6, "north"}, {&triggered, "false"}}),
          blocks::Dispenser);
    check(r.add("dropper", {.hardness = 3.5f, .resistance = 3.5f}, {{&facing6, "north"}, {&triggered, "false"}}),
          blocks::Dropper);
    // wiki: Rail (0.7, no collision: carts ride on them), Powered/Detector/Activator Rail.
    constexpr BlockSettings kRail{
        .hardness = 0.7f, .resistance = 0.7f, .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout};
    check(r.add("rail", kRail, {{&railShape, "north_south"}}), blocks::Rail);
    check(r.add("powered_rail", kRail, {{&powered, "false"}, {&straightRailShape, "north_south"}}), blocks::PoweredRail);
    check(r.add("detector_rail", kRail, {{&powered, "false"}, {&straightRailShape, "north_south"}}), blocks::DetectorRail);
    check(r.add("activator_rail", kRail, {{&powered, "false"}, {&straightRailShape, "north_south"}}),
          blocks::ActivatorRail);
    // wiki: Slime Block (breaks at once, bouncy, sticky to pistons; light passes);
    // Moving Piston (the technical block where a pushed block is in flight).
    check(r.add("slime_block", {.hardness = 0.0f, .resistance = 0.0f, .opaqueCube = false, .layer = RenderLayer::Translucent}),
          blocks::SlimeBlock);
    check(r.add("moving_piston", {.hardness = -1.0f, .resistance = 0.0f, .opaqueCube = false, .collision = false},
                {{&facing6, "north"}, {&pistonType, "normal"}}),
          blocks::MovingPiston);
    // Thin and small blocks (M23.2; wiki: Torch, Soul Torch, Lantern, Chain, Ladder,
    // Glass Pane): no collision for torches, small boxes for the rest.
    const BlockSettings torchLike{.lightEmission = 14, .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout};
    check(r.add("wall_torch", torchLike, {{&facing, "north"}}), blocks::WallTorch);
    BlockSettings soul = torchLike;
    soul.lightEmission = 10; // (wiki: Soul Torch)
    check(r.add("soul_torch", soul), blocks::SoulTorch);
    check(r.add("soul_wall_torch", soul, {{&facing, "north"}}), blocks::SoulWallTorch);
    check(r.add("lantern", {.hardness = 3.5f, .resistance = 3.5f, .lightEmission = 15, .opaqueCube = false,
                            .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                {{&hanging, "false"}}),
          blocks::Lantern);
    check(r.add("soul_lantern", {.hardness = 3.5f, .resistance = 3.5f, .lightEmission = 10, .opaqueCube = false,
                                 .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                {{&hanging, "false"}}),
          blocks::SoulLantern);
    check(r.add("chain", {.hardness = 5.0f, .resistance = 6.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                          .tool = HarvestTool::Pickaxe},
                {{&axis, "y"}}),
          blocks::Chain);
    check(r.add("ladder", {.hardness = 0.4f, .resistance = 0.4f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                           .tool = HarvestTool::Axe},
                {{&facing, "north"}}),
          blocks::Ladder);
    check(r.add("glass_pane", {.hardness = 0.3f, .resistance = 0.3f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                               .kind = BlockKind::Pane, .base = blocks::Glass},
                {{&fireEast, "false"}, {&fireNorth, "false"}, {&fireSouth, "false"}, {&fireWest, "false"}}),
          blocks::GlassPane);
    // New woods (M23.3b; wiki: Mangrove Log/Leaves/Propagule/Roots, Pale Oak, Bamboo -
    // block 2.0, planks 2.0/3.0, mosaic 2.0/3.0, the plant 1.0 and breaks at once by
    // sword; mangrove roots 0.7).
    const BlockSettings logS{.hardness = 2.0f, .resistance = 2.0f, .tool = HarvestTool::Axe};
    const BlockSettings planksS{.hardness = 2.0f, .resistance = 3.0f, .tool = HarvestTool::Axe};
    check(r.add("mangrove_log", logS, {{&axis, "y"}}), blocks::MangroveLog);
    check(r.add("mangrove_planks", planksS), blocks::MangrovePlanks);
    check(r.add("mangrove_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}), blocks::MangroveLeaves);
    check(r.add("mangrove_propagule", sapling, {{&stage, "0"}}), blocks::MangrovePropagule);
    check(r.add("mangrove_roots", {.hardness = 0.7f, .resistance = 0.7f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                                   .tool = HarvestTool::Axe}),
          blocks::MangroveRoots);
    check(r.add("pale_oak_log", logS, {{&axis, "y"}}), blocks::PaleOakLog);
    check(r.add("pale_oak_planks", planksS), blocks::PaleOakPlanks);
    check(r.add("pale_oak_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}), blocks::PaleOakLeaves);
    check(r.add("pale_oak_sapling", sapling, {{&stage, "0"}}), blocks::PaleOakSapling);
    check(r.add("bamboo_block", logS, {{&axis, "y"}}), blocks::BambooBlock);
    check(r.add("stripped_bamboo_block", logS, {{&axis, "y"}}), blocks::StrippedBambooBlock);
    check(r.add("bamboo_planks", planksS), blocks::BambooPlanks);
    check(r.add("bamboo_mosaic", planksS), blocks::BambooMosaic);
    check(r.add("bamboo", {.hardness = 1.0f, .resistance = 1.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                           .randomTicks = true, .tool = HarvestTool::Axe},
                {{&bambooLeaves, "none"}, {&stage, "0"}}),
          blocks::Bamboo);
    // Campfires (M23.4c; wiki: Campfire - 2.0 hardness, light 15 / soul 10 when lit).
    for (const auto& [id, block, light] : {std::tuple{"campfire", blocks::Campfire, uint8_t(15)},
                                           std::tuple{"soul_campfire", blocks::SoulCampfire, uint8_t(10)}}) {
        check(r.add(id, {.hardness = 2.0f, .resistance = 2.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                         .tool = HarvestTool::Axe},
                    {{&facing, "north"}, {&lit, "true"}, {&signalFire, "false"}}),
              block);
        for (uint32_t k = 0; k < r.block(block).stateCount; ++k) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(block).firstState + k);
            r.setStateEmission(s, r.get(s, lit) == 0 ? light : 0);
        }
    }
    // Hay bale (wiki: Hay Bale - 0.5, hoe; landing on it takes 80% of the fall damage).
    check(r.add("hay_block", {.hardness = 0.5f, .resistance = 0.5f, .tool = HarvestTool::Hoe}, {{&axis, "y"}}),
          blocks::HayBlock);
    // Workstations 1 (M23.5; wiki: Smoker 3.5, Blast Furnace 3.5 - both glow 13 when lit
    // and behave like furnaces; Barrel 2.5, axe).
    for (const auto& [id, block] : {std::pair{"smoker", blocks::Smoker}, std::pair{"blast_furnace", blocks::BlastFurnace}}) {
        check(r.add(id, {.hardness = 3.5f, .resistance = 3.5f, .tool = HarvestTool::Pickaxe, .like = blocks::Furnace},
                    {{&facing, "north"}, {&lit, "false"}}),
              block);
        for (uint32_t k = 0; k < r.block(block).stateCount; ++k) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(block).firstState + k);
            if (r.get(s, lit) == 0) r.setStateEmission(s, 13);
        }
    }
    check(r.add("barrel", {.hardness = 2.5f, .resistance = 2.5f, .tool = HarvestTool::Axe},
                {{&facing6, "north"}, {&open, "false"}}),
          blocks::Barrel);
    // Composter (wiki: Composter - 0.6, axe) and cauldrons (wiki: Cauldron - 2.0, needs
    // a pickaxe; the filled ones are separate blocks, as in vanilla since 1.17).
    check(r.add("composter", {.hardness = 0.6f, .resistance = 0.6f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                              .tool = HarvestTool::Axe},
                {{&composterLevel, "0"}}),
          blocks::Composter);
    const BlockSettings cauldron{.hardness = 2.0f, .resistance = 2.0f, .opaqueCube = false,
                                 .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe, .tier = 0};
    check(r.add("cauldron", cauldron), blocks::Cauldron);
    check(r.add("water_cauldron", cauldron, {{&cauldronLevel, "1"}}), blocks::WaterCauldron);
    BlockSettings lavaCauldron = cauldron;
    lavaCauldron.lightEmission = 15; // (wiki: a lava cauldron gives light 15)
    check(r.add("lava_cauldron", lavaCauldron), blocks::LavaCauldron);
    check(r.add("powder_snow_cauldron", cauldron, {{&cauldronLevel, "1"}}), blocks::PowderSnowCauldron);
    // wiki: Stonecutter (3.5, pickaxe), Grindstone (2.0 / 6.0, pickaxe).
    check(r.add("stonecutter", {.hardness = 3.5f, .resistance = 3.5f, .opaqueCube = false,
                                .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                {{&facing, "north"}}),
          blocks::Stonecutter);
    check(r.add("grindstone", {.hardness = 2.0f, .resistance = 6.0f, .opaqueCube = false,
                               .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                {{&face, "wall"}, {&facing, "north"}}),
          blocks::Grindstone);
    // wiki: Ender Chest (22.5 / 600, pickaxe, light 7), Shulker Box (2.0, any tool).
    check(r.add("ender_chest", {.hardness = 22.5f, .resistance = 600.0f, .lightEmission = 7, .opaqueCube = false,
                                .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                {{&facing, "north"}}),
          blocks::EnderChest);
    check(r.add("shulker_box", {.hardness = 2.0f, .resistance = 2.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                                .tool = HarvestTool::Pickaxe},
                {{&facing6, "up"}}),
          blocks::ShulkerBox);
    // wiki: Ancient Debris (30 / 1200), Block of Netherite (50 / 1200): diamond pickaxe.
    check(r.add("ancient_debris", {.hardness = 30.0f, .resistance = 1200.0f, .tool = HarvestTool::Pickaxe, .tier = 3}),
          blocks::AncientDebris);
    check(r.add("netherite_block", {.hardness = 50.0f, .resistance = 1200.0f, .tool = HarvestTool::Pickaxe, .tier = 3}),
          blocks::NetheriteBlock);
    // wiki: Smithing Table, Loom, Cartography Table (2.5, axe).
    check(r.add("smithing_table", {.hardness = 2.5f, .resistance = 2.5f, .tool = HarvestTool::Axe}),
          blocks::SmithingTable);
    check(r.add("loom", {.hardness = 2.5f, .resistance = 2.5f, .tool = HarvestTool::Axe}, {{&facing, "north"}}),
          blocks::Loom);
    check(r.add("cartography_table", {.hardness = 2.5f, .resistance = 2.5f, .tool = HarvestTool::Axe}),
          blocks::CartographyTable);
    // wiki: Beacon (3.0, any tool, light 15), Conduit (3.0, pickaxe, light 15).
    check(r.add("beacon", {.hardness = 3.0f, .resistance = 3.0f, .lightEmission = 15, .opaqueCube = false,
                           .layer = RenderLayer::Cutout}),
          blocks::Beacon);
    check(r.add("conduit", {.hardness = 3.0f, .resistance = 3.0f, .lightEmission = 15, .opaqueCube = false,
                            .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe}),
          blocks::Conduit);
    // Storage blocks (wiki: Block of Diamond / Emerald 5.0 / 6.0, iron pickaxe; Lapis 3.0,
    // stone pickaxe; Coal 5.0 / 6.0, any pickaxe) and the sea lantern (0.3, light 15).
    check(r.add("diamond_block", {.hardness = 5.0f, .resistance = 6.0f, .tool = HarvestTool::Pickaxe, .tier = 2}),
          blocks::DiamondBlock);
    check(r.add("emerald_block", {.hardness = 5.0f, .resistance = 6.0f, .tool = HarvestTool::Pickaxe, .tier = 2}),
          blocks::EmeraldBlock);
    check(r.add("lapis_block", {.hardness = 3.0f, .resistance = 3.0f, .tool = HarvestTool::Pickaxe, .tier = 1}),
          blocks::LapisBlock);
    check(r.add("coal_block", {.hardness = 5.0f, .resistance = 6.0f, .tool = HarvestTool::Pickaxe}), blocks::CoalBlock);
    check(r.add("sea_lantern", {.hardness = 0.3f, .resistance = 0.3f, .lightEmission = 15}), blocks::SeaLantern);
    // wiki: Note Block (0.8, axe), Jukebox (2.0 / 6.0, axe).
    check(r.add("note_block", {.hardness = 0.8f, .resistance = 0.8f, .tool = HarvestTool::Axe},
                {{&noteInstrument, "harp"}, {&note, "0"}, {&powered, "false"}}),
          blocks::NoteBlock);
    check(r.add("jukebox", {.hardness = 2.0f, .resistance = 6.0f, .tool = HarvestTool::Axe}, {{&hasRecord, "false"}}),
          blocks::Jukebox);
    // Job sites and the bell (M24.1; wiki: Lectern 2.5, Fletching Table 2.5 - axe; Bell
    // 5.0, pickaxe, hung from below here: vanilla's floor attachment only).
    check(r.add("lectern", {.hardness = 2.5f, .resistance = 2.5f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                            .tool = HarvestTool::Axe},
                {{&facing, "north"}, {&hasBook, "false"}}),
          blocks::Lectern);
    check(r.add("fletching_table", {.hardness = 2.5f, .resistance = 2.5f, .tool = HarvestTool::Axe}),
          blocks::FletchingTable);
    check(r.add("bell", {.hardness = 5.0f, .resistance = 5.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                         .tool = HarvestTool::Pickaxe},
                {{&facing, "north"}}),
          blocks::Bell);
    check(r.add("carved_pumpkin", {.hardness = 1.0f, .resistance = 1.0f, .tool = HarvestTool::Axe}, {{&facing, "north"}}),
          blocks::CarvedPumpkin);
    // Ocean plants (M25.1; wiki: Kelp, Seagrass, Sea Pickle, Dried Kelp Block, Blue Ice):
    // kelp and seagrass always stand in water; sea pickles glow 6/9/12/15 (1-4 pickles)
    // only when waterlogged.
    constexpr BlockSettings kWaterPlant{
        .opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout, .water = true};
    BlockSettings kelpTip = kWaterPlant;
    kelpTip.randomTicks = true; // (grows a block on 14% of its random ticks)
    check(r.add("kelp", kelpTip, {{&age25, "0"}}), blocks::Kelp);
    check(r.add("kelp_plant", kWaterPlant), blocks::KelpPlant);
    check(r.add("seagrass", kWaterPlant), blocks::Seagrass);
    check(r.add("tall_seagrass", kWaterPlant, {{&doorHalf, "lower"}}), blocks::TallSeagrass);
    check(r.add("sea_pickle", {.opaqueCube = false, .layer = RenderLayer::Cutout},
                {{&pickles, "1"}, {&waterlogged, "true"}}),
          blocks::SeaPickle);
    check(r.add("dried_kelp_block", {.hardness = 0.5f, .resistance = 2.5f, .tool = HarvestTool::Hoe}),
          blocks::DriedKelpBlock);
    check(r.add("blue_ice", {.hardness = 2.8f, .resistance = 2.8f, .tool = HarvestTool::Pickaxe}), blocks::BlueIce);
    // (M25.3b; wiki: Turtle Egg - 0.5, cracks toward hatching on random ticks)
    check(r.add("turtle_egg", {.hardness = 0.5f, .resistance = 0.5f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                               .randomTicks = true},
                {{&eggs, "1"}, {&hatch, "0"}}),
          blocks::TurtleEgg);
    // (M25.5; wiki: Sponge - 0.6, hoe)
    check(r.add("sponge", {.hardness = 0.6f, .resistance = 0.6f, .tool = HarvestTool::Hoe}), blocks::Sponge);
    check(r.add("wet_sponge", {.hardness = 0.6f, .resistance = 0.6f, .tool = HarvestTool::Hoe}), blocks::WetSponge);
    // (M26.3; wiki: Sweet Berry Bush - breaks instantly, no collision (it slows and pricks
    // what walks through), grows on random ticks)
    check(r.add("sweet_berry_bush", {.opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout,
                                     .randomTicks = true},
                {{&age3, "0"}}),
          blocks::SweetBerryBush);
    // (M26.3b; wiki: Bee Nest 0.3, Beehive 0.6 - axe; Honey Block 0, translucent; Honeycomb
    // Block 0.6)
    check(r.add("bee_nest", {.hardness = 0.3f, .resistance = 0.3f, .tool = HarvestTool::Axe},
                {{&facing, "north"}, {&honeyLevel, "0"}}),
          blocks::BeeNest);
    check(r.add("beehive", {.hardness = 0.6f, .resistance = 0.6f, .tool = HarvestTool::Axe},
                {{&facing, "north"}, {&honeyLevel, "0"}}),
          blocks::Beehive);
    check(r.add("honey_block", {.opaqueCube = false, .layer = RenderLayer::Translucent}), blocks::HoneyBlock);
    check(r.add("honeycomb_block", {.hardness = 0.6f, .resistance = 0.6f}), blocks::HoneycombBlock);
    // (M26.3c; wiki: Froglight - 0.3, light 15; Frogspawn - breaks at once, no collision,
    // sits on water and hatches on its own)
    for (const auto& [id, b] : {std::pair{"ochre_froglight", blocks::OchreFroglight},
                                std::pair{"verdant_froglight", blocks::VerdantFroglight},
                                std::pair{"pearlescent_froglight", blocks::PearlescentFroglight}})
        check(r.add(id, {.hardness = 0.3f, .resistance = 0.3f, .lightEmission = 15}, {{&axis, "y"}}), b);
    check(r.add("frogspawn", {.opaqueCube = false, .collision = false, .layer = RenderLayer::Cutout, .randomTicks = true}),
          blocks::Frogspawn);
    // (M26.4a; wiki: Cobweb - 4, no collision; Infested Block - half its stone's hardness:
    // 0.75, cobblestone 1, deepslate 1.5 - any tool, no drop)
    check(r.add("cobweb", {.hardness = 4.0f, .resistance = 4.0f, .opaqueCube = false, .collision = false,
                           .layer = RenderLayer::Cutout}),
          blocks::Cobweb);
    for (const auto& [id, b, hard] :
         {std::tuple{"infested_stone", blocks::InfestedStone, 0.75f},
          std::tuple{"infested_cobblestone", blocks::InfestedCobblestone, 1.0f},
          std::tuple{"infested_stone_bricks", blocks::InfestedStoneBricks, 0.75f},
          std::tuple{"infested_mossy_stone_bricks", blocks::InfestedMossyStoneBricks, 0.75f},
          std::tuple{"infested_cracked_stone_bricks", blocks::InfestedCrackedStoneBricks, 0.75f},
          std::tuple{"infested_chiseled_stone_bricks", blocks::InfestedChiseledStoneBricks, 0.75f}})
        check(r.add(id, {.hardness = hard, .resistance = 0.75f}), b);
    check(r.add("infested_deepslate", {.hardness = 1.5f, .resistance = 0.75f}, {{&axis, "y"}}), blocks::InfestedDeepslate);
    // (M26.4b; wiki: Head - 1, no tool; standing heads turn in 16 directions)
    for (const auto& [floor, wall, b] : {std::tuple{"skeleton_skull", "skeleton_wall_skull", blocks::SkeletonSkull},
                                         std::tuple{"wither_skeleton_skull", "wither_skeleton_wall_skull", blocks::WitherSkeletonSkull},
                                         std::tuple{"zombie_head", "zombie_wall_head", blocks::ZombieHead},
                                         std::tuple{"creeper_head", "creeper_wall_head", blocks::CreeperHead},
                                         std::tuple{"piglin_head", "piglin_wall_head", blocks::PiglinHead},
                                         std::tuple{"dragon_head", "dragon_wall_head", blocks::DragonHead}}) {
        check(r.add(floor, {.hardness = 1.0f, .resistance = 1.0f, .opaqueCube = false}, {{&rotation16, "0"}}), b);
        check(r.add(wall, {.hardness = 1.0f, .resistance = 1.0f, .opaqueCube = false}, {{&facing, "north"}}),
              static_cast<BlockId>(b + 1));
    }
    // (M26.5b; wiki: Dried Ghast - breaks at once; waterlogged, it soaks up water in
    // three stages on random ticks and becomes a ghastling)
    check(r.add("dried_ghast", {.opaqueCube = false, .layer = RenderLayer::Cutout, .randomTicks = true},
                {{&facing, "north"}, {&hydration, "0"}, {&waterlogged, "false"}}),
          blocks::DriedGhast);
    // Two-block plants (M27.1; wiki: each plant - broken at once, the halves together).
    for (const auto& [id, b] : {std::pair{"sunflower", blocks::Sunflower}, std::pair{"lilac", blocks::Lilac},
                                std::pair{"rose_bush", blocks::RoseBush}, std::pair{"peony", blocks::Peony},
                                std::pair{"tall_grass", blocks::TallGrass}, std::pair{"large_fern", blocks::LargeFern}})
        check(r.add(id, kPlant, {{&doorHalf, "lower"}}), b);
    // Mud (wiki: Mud - 0.5, shovel; its top sits 2 pixels low, see BlockShapes), packed
    // mud (1.0 / 3.0), muddy mangrove roots (0.7, shovel), moss and pale moss (0.1, hoe)
    // with their carpets.
    check(r.add("mud", {.hardness = 0.5f, .resistance = 0.5f, .tool = HarvestTool::Shovel}), blocks::Mud);
    check(r.add("packed_mud", {.hardness = 1.0f, .resistance = 3.0f}), blocks::PackedMud);
    check(r.add("muddy_mangrove_roots", {.hardness = 0.7f, .resistance = 0.7f, .tool = HarvestTool::Shovel},
                {{&axis, "y"}}),
          blocks::MuddyMangroveRoots);
    check(r.add("moss_block", {.hardness = 0.1f, .resistance = 0.1f, .tool = HarvestTool::Hoe}), blocks::MossBlock);
    check(r.add("moss_carpet", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false, .kind = BlockKind::Carpet,
                                .base = blocks::MossBlock}),
          blocks::MossCarpet);
    check(r.add("pale_moss_block", {.hardness = 0.1f, .resistance = 0.1f, .tool = HarvestTool::Hoe}),
          blocks::PaleMossBlock);
    check(r.add("pale_moss_carpet", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false,
                                     .kind = BlockKind::Carpet, .base = blocks::PaleMossBlock}),
          blocks::PaleMossCarpet);
    // (wiki: Pale Hanging Moss - hangs under a block or more moss; the lowest is the tip)
    check(r.add("pale_hanging_moss", kPlant, {{&mossTip, "true"}}), blocks::PaleHangingMoss);
    // (M27.1c; wiki: Creaking Heart - 10 / 10, axe; it wakes at night between pale oak
    // logs and calls a creaking. Eyeblossoms open at night. Resin clumps sit on a face.)
    BlockSettings heart{.hardness = 10.0f, .resistance = 10.0f, .randomTicks = true, .tool = HarvestTool::Axe};
    check(r.add("creaking_heart", heart, {{&axis, "y"}, {&creakingState, "uprooted"}, {&natural, "false"}}),
          blocks::CreakingHeart);
    BlockSettings blossom = kPlant;
    blossom.randomTicks = true;
    check(r.add("open_eyeblossom", blossom), blocks::OpenEyeblossom);
    check(r.add("closed_eyeblossom", blossom), blocks::ClosedEyeblossom);
    check(r.add("resin_clump", kPlant, {{&facing6, "down"}}), blocks::ResinClump);
    check(r.add("resin_block", {}), blocks::ResinBlock);
    // Lush caves (M27.2; wiki: each block - all break at once but rooted dirt 0.5 and big
    // dripleaves 0.1; cave vines with berries glow 14).
    BlockSettings vines = kPlant;
    vines.randomTicks = true; // (the tip grows down)
    check(r.add("cave_vines", vines, {{&age25, "0"}, {&berries, "false"}}), blocks::CaveVines);
    check(r.add("cave_vines_plant", kPlant, {{&berries, "false"}}), blocks::CaveVinesPlant);
    check(r.add("spore_blossom", kPlant), blocks::SporeBlossom);
    constexpr BlockSettings kBush{.opaqueCube = false, .layer = RenderLayer::Cutout};
    check(r.add("azalea", kBush), blocks::Azalea);
    check(r.add("flowering_azalea", kBush), blocks::FloweringAzalea);
    check(r.add("azalea_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}), blocks::AzaleaLeaves);
    check(r.add("flowering_azalea_leaves", kLeaves, {{&distance, "7"}, {&persistent, "false"}}),
          blocks::FloweringAzaleaLeaves);
    check(r.add("rooted_dirt", {.hardness = 0.5f, .resistance = 0.5f, .tool = HarvestTool::Shovel}), blocks::RootedDirt);
    check(r.add("hanging_roots", kPlant), blocks::HangingRoots);
    check(r.add("small_dripleaf", kPlant, {{&doorHalf, "lower"}, {&facing, "north"}}), blocks::SmallDripleaf);
    check(r.add("big_dripleaf", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                                 .tool = HarvestTool::Axe},
                {{&facing, "north"}, {&tilt, "none"}}),
          blocks::BigDripleaf);
    check(r.add("big_dripleaf_stem", {.hardness = 0.1f, .resistance = 0.1f, .opaqueCube = false, .collision = false,
                                      .layer = RenderLayer::Cutout, .tool = HarvestTool::Axe},
                {{&facing, "north"}}),
          blocks::BigDripleafStem);
    // Dripstone (M27.2b; wiki: Pointed Dripstone - 1.5 / 3, pickaxe; Dripstone Block 1.5 / 1).
    check(r.add("pointed_dripstone", {.hardness = 1.5f, .resistance = 3.0f, .opaqueCube = false,
                                      .layer = RenderLayer::Cutout, .randomTicks = true, .tool = HarvestTool::Pickaxe},
                {{&thickness, "tip"}, {&verticalDirection, "up"}, {&waterlogged, "false"}}),
          blocks::PointedDripstone);
    check(r.add("dripstone_block", {.hardness = 1.5f, .resistance = 1.0f, .tool = HarvestTool::Pickaxe}),
          blocks::DripstoneBlock);
    // The deep dark (M27.3; wiki: sculk 0.2, vein 0.2, catalyst 3, sensor 1.5, shrieker 3, all
    // hoes; reinforced deepslate 55 / 1200, unbreakable by hand in survival practice).
    check(r.add("sculk", {.hardness = 0.2f, .resistance = 0.2f, .tool = HarvestTool::Hoe}), blocks::Sculk);
    check(r.add("sculk_vein", {.hardness = 0.2f, .resistance = 0.2f, .opaqueCube = false, .collision = false,
                               .layer = RenderLayer::Cutout, .tool = HarvestTool::Hoe},
                {{&facing6, "down"}}),
          blocks::SculkVein);
    check(r.add("sculk_catalyst", {.hardness = 3.0f, .resistance = 3.0f, .lightEmission = 6, .tool = HarvestTool::Hoe},
                {{&bloom, "false"}}),
          blocks::SculkCatalyst);
    check(r.add("sculk_sensor", {.hardness = 1.5f, .resistance = 1.5f, .lightEmission = 1, .opaqueCube = false,
                                 .layer = RenderLayer::Cutout, .tool = HarvestTool::Hoe},
                {{&sculkPhase, "inactive"}, {&power, "0"}, {&waterlogged, "false"}}),
          blocks::SculkSensor);
    check(r.add("sculk_shrieker", {.hardness = 3.0f, .resistance = 3.0f, .opaqueCube = false,
                                   .layer = RenderLayer::Cutout, .tool = HarvestTool::Hoe},
                {{&shrieking, "false"}, {&canSummon, "false"}, {&waterlogged, "false"}}),
          blocks::SculkShrieker);
    check(r.add("reinforced_deepslate", {.hardness = 55.0f, .resistance = 1200.0f}), blocks::ReinforcedDeepslate);
    // Geodes (M27.4a; wiki: amethyst 1.5, buds and clusters 1.5, smooth basalt 1.25 / 4.2,
    // all pickaxe; buds glow 1, 2, 4 and clusters 5; tinted glass 0.3 and blocks light).
    check(r.add("amethyst_block", {.hardness = 1.5f, .resistance = 1.5f, .tool = HarvestTool::Pickaxe}),
          blocks::AmethystBlock);
    check(r.add("budding_amethyst", {.hardness = 1.5f, .resistance = 1.5f, .randomTicks = true,
                                     .tool = HarvestTool::Pickaxe}),
          blocks::BuddingAmethyst);
    for (const auto& [id, b, glow] : {std::tuple{"small_amethyst_bud", blocks::SmallAmethystBud, 1},
                                       std::tuple{"medium_amethyst_bud", blocks::MediumAmethystBud, 2},
                                       std::tuple{"large_amethyst_bud", blocks::LargeAmethystBud, 4},
                                       std::tuple{"amethyst_cluster", blocks::AmethystCluster, 5}})
        check(r.add(id, {.hardness = 1.5f, .resistance = 1.5f, .lightEmission = uint8_t(glow), .opaqueCube = false,
                         .collision = false, .layer = RenderLayer::Cutout, .tool = HarvestTool::Pickaxe},
                    {{&facing6, "up"}, {&waterlogged, "false"}}),
              b);
    check(r.add("smooth_basalt", {.hardness = 1.25f, .resistance = 4.2f, .tool = HarvestTool::Pickaxe, .tier = 0}),
          blocks::SmoothBasalt);
    check(r.add("tinted_glass", {.hardness = 0.3f, .resistance = 0.3f, .lightOpacity = 15, .opaqueCube = false,
                                 .layer = RenderLayer::Cutout}),
          blocks::TintedGlass);
    check(r.add("crying_obsidian", {.hardness = 50.0f, .resistance = 1200.0f, .lightEmission = 10,
                                    .tool = HarvestTool::Pickaxe, .tier = 3}),
          blocks::CryingObsidian);
    // (M27.4d; wiki: Trial Spawner - 50 / 50, glows 4; Vault - 50 / 50, glows 6 when active)
    check(r.add("trial_spawner", {.hardness = 50.0f, .resistance = 50.0f, .lightEmission = 4, .opaqueCube = false,
                                  .layer = RenderLayer::Cutout},
                {{&trialState, "inactive"}, {&ominous, "false"}}),
          blocks::TrialSpawner);
    check(r.add("vault", {.hardness = 50.0f, .resistance = 50.0f, .lightEmission = 6, .opaqueCube = false,
                          .layer = RenderLayer::Cutout},
                {{&facing, "north"}, {&vaultState, "inactive"}, {&ominous, "false"}}),
          blocks::Vault);
    // (M27.5; wiki: Suspicious Sand 0.25, Suspicious Gravel 0.25, shovel; Decorated Pot 0)
    check(r.add("suspicious_sand", {.hardness = 0.25f, .resistance = 0.25f, .tool = HarvestTool::Shovel}, {{&dusted, "0"}}),
          blocks::SuspiciousSand);
    check(r.add("suspicious_gravel", {.hardness = 0.25f, .resistance = 0.25f, .tool = HarvestTool::Shovel},
                {{&dusted, "0"}}),
          blocks::SuspiciousGravel);
    check(r.add("decorated_pot", {.opaqueCube = false, .layer = RenderLayer::Cutout}, {{&facing, "north"}}),
          blocks::DecoratedPot);
    // (M27.5c; wiki: Sniffer Egg 0.5; the crops and flowers break at once)
    check(r.add("sniffer_egg", {.hardness = 0.5f, .resistance = 0.5f, .opaqueCube = false}, {{&hatch, "0"}}),
          blocks::SnifferEgg);
    BlockSettings sniffCrop = kPlant;
    sniffCrop.randomTicks = true;
    check(r.add("torchflower_crop", sniffCrop, {{&age1, "0"}}), blocks::TorchflowerCrop);
    check(r.add("torchflower", kPlant), blocks::Torchflower);
    check(r.add("pitcher_crop", sniffCrop, {{&age4, "0"}}), blocks::PitcherCrop);
    check(r.add("pitcher_plant", kPlant, {{&doorHalf, "lower"}}), blocks::PitcherPlant);
    // (M28.2a; wiki: Lodestone - 3.5 / 3.5, any pickaxe)
    check(r.add("lodestone", {.hardness = 3.5f, .resistance = 3.5f, .tool = HarvestTool::Pickaxe, .tier = 0}),
          blocks::Lodestone);
    // A kelp tip at age 25 never grows again: it doesn't random-tick (M25 review: whole
    // ocean-floor sections dropped out of the random tick pass).
    r.setStateRandomTicks(r.set(r.defaultState(blocks::Kelp), age25, 25), false);
    for (uint32_t i = 0; i < r.block(blocks::SeaPickle).stateCount; ++i) {
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::SeaPickle).firstState + i);
        r.setStateEmission(s, r.get(s, waterlogged) == 0 ? uint8_t(6 + 3 * r.get(s, pickles)) : 0);
    }
    for (uint32_t i = 0; i < r.block(blocks::CaveVines).stateCount; ++i) { // (glow berries light 14)
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::CaveVines).firstState + i);
        r.setStateEmission(s, r.get(s, berries) == 0 ? 14 : 0);
    }
    // (M27 review; wiki) trial spawners glow 0 asleep, 4 waiting, 8 active; vaults 6, 12 open.
    for (uint32_t i = 0; i < r.block(blocks::TrialSpawner).stateCount; ++i) {
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::TrialSpawner).firstState + i);
        const int st = r.get(s, trialState);
        r.setStateEmission(s, st == 0 ? 0 : st == 1 || st == 5 ? 4 : 8);
    }
    for (uint32_t i = 0; i < r.block(blocks::Vault).stateCount; ++i) {
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::Vault).firstState + i);
        r.setStateEmission(s, r.get(s, vaultState) == 0 ? 6 : 12);
    }
    for (uint32_t i = 0; i < r.block(blocks::CaveVinesPlant).stateCount; ++i) {
        const BlockStateId s = static_cast<BlockStateId>(r.block(blocks::CaveVinesPlant).firstState + i);
        r.setStateEmission(s, r.get(s, berries) == 0 ? 14 : 0);
    }
    for (const BlockId leaves : {BlockId(blocks::MangroveLeaves), BlockId(blocks::PaleOakLeaves),
                                 BlockId(blocks::AzaleaLeaves), BlockId(blocks::FloweringAzaleaLeaves)})
        for (uint32_t i = 0; i < r.block(leaves).stateCount; ++i) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(leaves).firstState + i);
            r.setStateRandomTicks(s, r.get(s, distance) == 6 && r.get(s, persistent) == 1);
        }
    addWoodBlocks(r);
    addColouredBlocks(r);
    addCopperBlocks(r);
    addBuildingFamilies(r); // (M23.1: after every enum block, so earlier state ids stay put)
    // Copper chests (M26.5b; wiki: Copper Chest): chests (like) of every oxidation stage,
    // each waxed too; they age as copper does (Copper.cpp finds them by name).
    for (const bool waxed : {false, true})
        for (const char* rust : {"", "exposed_", "weathered_", "oxidized_"}) {
            BlockSettings s{.hardness = 3.0f, .resistance = 6.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
                            .tool = HarvestTool::Axe, .like = blocks::Chest};
            s.randomTicks = !waxed && std::string(rust) != "oxidized_";
            r.add(std::string(waxed ? "waxed_" : "") + rust + "copper_chest", s, {{&facing, "north"}, {&chestType, "single"}});
        }
    addWoodSets(r);
    for (const char* colour : kDyeColours) // (M23.6: dyed shulker boxes behave like the plain one)
        r.add(std::string(colour) + "_shulker_box",
              {.hardness = 2.0f, .resistance = 2.0f, .opaqueCube = false, .layer = RenderLayer::Cutout,
               .tool = HarvestTool::Pickaxe, .like = blocks::ShulkerBox},
              {{&facing6, "up"}});
    addCorals(r);
    // Random ticks (wiki: Tick › Random tick): grass spreads/dies, snow layers and ice
    // melt, lava sets fires; leaves only while they can decay (distance 7, not
    // persistent: vanilla's isRandomlyTicking).
    for (BlockId b : {blocks::GrassBlock, blocks::Snow, blocks::Ice, blocks::Lava})
        for (uint32_t i = 0; i < r.block(b).stateCount; ++i)
            r.setStateRandomTicks(static_cast<BlockStateId>(r.block(b).firstState + i), true);
    for (BlockId b : {blocks::OakLeaves, blocks::BirchLeaves, blocks::SpruceLeaves, blocks::AcaciaLeaves,
                      blocks::JungleLeaves, blocks::DarkOakLeaves, blocks::CherryLeaves})
        for (uint32_t i = 0; i < r.block(b).stateCount; ++i) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(b).firstState + i);
            r.setStateRandomTicks(s, r.get(s, distance) == 6 && r.get(s, persistent) == 1);
        }

    // An extended piston's base is not a full cube (light and faces pass its front).
    for (BlockId b : {blocks::Piston, blocks::StickyPiston})
        for (uint32_t i = 0; i < r.block(b).stateCount; ++i) {
            const BlockStateId s = static_cast<BlockStateId>(r.block(b).firstState + i);
            if (r.get(s, extended) == 0) r.setStateOpaque(s, false);
        }
    return r;
}

} // namespace

const BlockRegistry& blockRegistry() {
    static const BlockRegistry registry = buildVanillaBlocks();
    return registry;
}

} // namespace mc::world
