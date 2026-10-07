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
