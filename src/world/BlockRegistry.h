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

struct BlockSettings {
    float hardness = 0.0f;   // wiki infobox "Hardness"
    float resistance = 0.0f; // wiki infobox "Blast resistance"
    uint8_t lightEmission = 0;
    // A full opaque cube: hides the faces of neighbouring blocks and blocks light.
    bool opaqueCube = true;
    // Entities collide with it (a full cube for now; shaped boxes come with slabs etc.).
    bool collision = true;
    RenderLayer layer = RenderLayer::Solid;
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
    RenderLayer layer(BlockStateId state) const;

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
};

} // namespace mc::world
