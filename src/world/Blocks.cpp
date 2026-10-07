#include "world/Blocks.h"

#include <cassert>
#include <string>
#include <tuple>

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
    static constexpr const char* kColours[16] = {"white", "orange", "magenta", "light_blue", "yellow", "lime",
                                                 "pink", "gray", "light_gray", "cyan", "purple", "blue",
                                                 "brown", "green", "red", "black"};
    for (int c = 0; c < 16; ++c)
        check(r.add(std::string(kColours[c]) + "_wool", {.hardness = 0.8f, .resistance = 0.8f}),
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
