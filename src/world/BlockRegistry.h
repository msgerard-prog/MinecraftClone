#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc::world {

using BlockId = uint16_t;      // index of a block type (registration order)
using BlockStateId = uint16_t; // dense global id of one block + property values

// A block-state property, e.g. axis = x|y|z. Values are kept in vanilla's order
// (booleans are [true, false], as vanilla's BooleanProperty lists them).
struct Property {
    std::string_view name;
    std::vector<std::string_view> values;
};

// Which render pass draws the block (vanilla "render types").
enum class RenderLayer : uint8_t { Invisible, Solid, Cutout, Translucent };

// Shaped block families (M23.1): blocks of a kind share placement, shape, model and
// connection rules, taking textures, tool and sound from their base block.
enum class BlockKind : uint8_t { Plain, Slab, Stairs, Wall, Pane, Carpet, Sign, WallSign, HangingSign, WallHangingSign,
                                 Banner, WallBanner };
// The tool that mines a block fastest and is needed for drops (wiki: each block's
// "Tool"); blocks registered before M23 keep theirs in gameplay/Mining.
enum class HarvestTool : uint8_t { None, Pickaxe, Axe, Shovel, Hoe };

struct BlockSettings {
    float hardness = 0.0f;   // wiki infobox "Hardness"
    float resistance = 0.0f; // wiki infobox "Blast resistance"
    uint8_t lightEmission = 0;
    // How much light this block absorbs (wiki: Light): -1 = automatic (opaque cube
    // 15, otherwise 0). Water and ice use 1: sky light fades 1 per block of depth.
    int8_t lightOpacity = -1;
    // A full opaque cube: hides the faces of neighbouring blocks and blocks light.
    bool opaqueCube = true;
    // Entities collide with it (a full cube for now; shaped boxes come with slabs etc.).
    bool collision = true;
    RenderLayer layer = RenderLayer::Solid;
    // Receives random ticks (wiki: Tick › Random tick): grass, leaves, saplings, ice...
    bool randomTicks = false;
    BlockKind kind = BlockKind::Plain;
    BlockId base = 0; // slabs, stairs, walls: the full block they are cut from
    HarvestTool tool = HarvestTool::None;
    uint8_t tier = 0; // 0 any, 1 stone, 2 iron, 3 diamond (needed for drops)
    // Behaves like this block (M23.3: a birch door like the oak door) in block
    // updates, shapes and mining; 0 = itself. Models and drops keep the real block.
    BlockId like = 0;
    // Always holds water (M25.1: kelp, seagrass - vanilla's inherently waterlogged
    // plants). Blocks with a `waterlogged` property hold water in its true states.
    bool water = false;
};

// A property plus the value this block uses in its default state.
struct PropertyDefault {
    const Property* property;
    std::string_view defaultValue;
};

struct BlockDef {
    std::string id; // "minecraft:stone"
    BlockSettings settings;
    std::vector<const Property*> properties; // sorted by name (vanilla's state string order)
    std::vector<uint32_t> strides;           // mixed-radix stride per property
    BlockStateId firstState = 0;
    uint32_t stateCount = 1;
    BlockStateId defaultState = 0;
};

// Every block and every combination of its property values ("block state").
// State ids are dense: each block owns the range [firstState, firstState + stateCount),
// with the last property (by name) varying fastest. Ids are runtime-only — saves store
// names + properties — so this order can change without breaking worlds.
class BlockRegistry {
public:
    // Registration (before freeze). Air must be registered first so air = state 0.
    BlockId add(std::string_view id, const BlockSettings& settings,
                std::initializer_list<PropertyDefault> properties = {});

    const BlockDef& block(BlockId id) const { return m_blocks[id]; }
    size_t blockCount() const { return m_blocks.size(); }
    size_t stateCount() const { return m_stateBlock.size(); }
    std::optional<BlockId> findBlock(std::string_view id) const;

    BlockId blockOf(BlockStateId state) const { return m_stateBlock[state]; }
    BlockStateId defaultState(BlockId id) const { return m_blocks[id].defaultState; }
    // Hot-path flags, one array lookup per state (used by meshing and lighting).
    bool isAir(BlockStateId state) const { return state == 0; }
    bool opaqueCube(BlockStateId state) const { return m_stateOpaque[state] != 0; }
    bool collides(BlockStateId state) const { return m_stateCollides[state] != 0; }
    uint8_t lightOpacity(BlockStateId state) const { return m_stateOpacity[state]; }
    uint8_t lightEmission(BlockStateId state) const { return m_stateEmission[state]; }
    bool randomTicks(BlockStateId state) const { return m_stateRandomTicks[state] != 0; }
    // Holds a water source as well as the block (M25.1, wiki: Waterlogging): fluids,
    // swimming, light and the mesher treat it as water too; breaking it leaves water.
    bool waterlogged(BlockStateId state) const { return m_stateWaterlogged[state] != 0; }
    // Registration time: only some states tick (persistent leaves, lit redstone ore).
    void setStateRandomTicks(BlockStateId state, bool ticks) { m_stateRandomTicks[state] = ticks ? 1 : 0; }
    // Registration time: light depending on state (furnace lit=true emits 13).
    void setStateEmission(BlockStateId state, uint8_t level) { m_stateEmission[state] = level; }
    // Registration time: per-state shape (an extended piston is not a full cube).
    void setStateOpaque(BlockStateId state, bool opaque) {
        m_stateOpaque[state] = opaque ? 1 : 0;
        m_stateOpacity[state] = opaque ? 15 : 0;
    }
    RenderLayer layer(BlockStateId state) const;
    BlockKind kind(BlockId id) const { return m_blocks[id].settings.kind; }
    BlockId likeOf(BlockId id) const {
        const BlockId l = m_blocks[id].settings.like;
        return l ? l : id;
    }

    // Fast property access by Property object (hot paths: redstone). `get` returns
    // the value's index in the property's list (-1 if the block lacks it); `set`
    // returns the state with that value index (unchanged if the block lacks it).
    int get(BlockStateId state, const Property& property) const;
    BlockStateId set(BlockStateId state, const Property& property, int valueIndex) const;

    // Property access by name. Return nullopt for unknown property/value.
    std::optional<std::string_view> value(BlockStateId state, std::string_view property) const;
    std::optional<BlockStateId> with(BlockStateId state, std::string_view property,
                                     std::string_view value) const;

    // "minecraft:oak_log[axis=y]" (no brackets when the block has no properties).
    std::string toString(BlockStateId state) const;
    // Parses the string form; missing properties take the default state's values.
    // A bare id without "minecraft:" is accepted too.
    std::optional<BlockStateId> parse(std::string_view text) const;

private:
    int propertyIndex(const BlockDef& def, std::string_view property) const;
    uint32_t valueIndex(const BlockDef& def, BlockStateId state, int prop) const;

    std::vector<BlockDef> m_blocks;
    std::vector<BlockId> m_stateBlock;    // state -> block
    std::vector<uint8_t> m_stateOpaque;   // state -> opaqueCube
    std::vector<uint8_t> m_stateCollides; // state -> collision
    std::vector<uint8_t> m_stateOpacity;  // state -> light opacity 0..15
    std::vector<uint8_t> m_stateEmission; // state -> light emission 0..15
    std::vector<uint8_t> m_stateRandomTicks; // state -> receives random ticks
    std::vector<uint8_t> m_stateWaterlogged; // state -> holds water
};

} // namespace mc::world
