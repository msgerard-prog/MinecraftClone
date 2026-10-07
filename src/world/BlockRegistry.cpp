#include "world/BlockRegistry.h"

#include <algorithm>
#include <cassert>

namespace mc::world {

namespace {

constexpr std::string_view kNamespace = "minecraft:";

std::string_view trim(std::string_view s) {
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ')
        s.remove_suffix(1);
    return s;
}

} // namespace

BlockId BlockRegistry::add(std::string_view id, const BlockSettings& settings,
                           std::initializer_list<PropertyDefault> properties) {
    BlockDef def;
    def.id = id.find(':') == std::string_view::npos ? std::string(kNamespace) + std::string(id)
                                                    : std::string(id);
    def.settings = settings;

    std::vector<PropertyDefault> sorted(properties);
    std::sort(sorted.begin(), sorted.end(), [](const PropertyDefault& a, const PropertyDefault& b) {
        return a.property->name < b.property->name;
    });
    for (const auto& p : sorted)
        def.properties.push_back(p.property);

    // Mixed radix: last property varies fastest.
    def.strides.assign(def.properties.size(), 1);
    def.stateCount = 1;
    for (size_t i = def.properties.size(); i-- > 0;) {
        def.strides[i] = def.stateCount;
        def.stateCount *= static_cast<uint32_t>(def.properties[i]->values.size());
    }
    assert(m_stateBlock.size() + def.stateCount <= 0x10000 && "BlockStateId is 16 bits");
    def.firstState = static_cast<BlockStateId>(m_stateBlock.size());

    uint32_t defaultOffset = 0;
    for (size_t i = 0; i < sorted.size(); ++i) {
        const auto& values = sorted[i].property->values;
        const auto it = std::find(values.begin(), values.end(), sorted[i].defaultValue);
        assert(it != values.end() && "default value must be one of the property's values");
        defaultOffset += static_cast<uint32_t>(it - values.begin()) * def.strides[i];
    }
    def.defaultState = static_cast<BlockStateId>(def.firstState + defaultOffset);

    const auto blockId = static_cast<BlockId>(m_blocks.size());
    assert((blockId != 0 || def.id == "minecraft:air") && "air must be block 0 / state 0");
    m_stateBlock.insert(m_stateBlock.end(), def.stateCount, blockId);
    m_stateOpaque.insert(m_stateOpaque.end(), def.stateCount, settings.opaqueCube ? 1 : 0);
    m_stateCollides.insert(m_stateCollides.end(), def.stateCount, settings.collision ? 1 : 0);
    const uint8_t opacity = settings.lightOpacity >= 0 ? static_cast<uint8_t>(settings.lightOpacity)
                            : settings.opaqueCube      ? 15
                                                       : 0;
    m_stateOpacity.insert(m_stateOpacity.end(), def.stateCount, opacity);
    m_stateEmission.insert(m_stateEmission.end(), def.stateCount, settings.lightEmission);
    m_blocks.push_back(std::move(def));
    return blockId;
}

std::optional<BlockId> BlockRegistry::findBlock(std::string_view id) const {
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        const std::string_view name = m_blocks[i].id;
        // A query without a namespace means "minecraft:<id>".
        const bool bare = id.find(':') == std::string_view::npos;
        if (name == id ||
            (bare && name.starts_with(kNamespace) && name.substr(kNamespace.size()) == id)) {
            return static_cast<BlockId>(i);
        }
    }
    return std::nullopt;
}

RenderLayer BlockRegistry::layer(BlockStateId state) const {
    return m_blocks[m_stateBlock[state]].settings.layer;
}

int BlockRegistry::propertyIndex(const BlockDef& def, std::string_view property) const {
    for (size_t i = 0; i < def.properties.size(); ++i) {
        if (def.properties[i]->name == property) return static_cast<int>(i);
    }
    return -1;
}

uint32_t BlockRegistry::valueIndex(const BlockDef& def, BlockStateId state, int prop) const {
    const uint32_t offset = state - def.firstState;
    return offset / def.strides[prop] % static_cast<uint32_t>(def.properties[prop]->values.size());
}

std::optional<std::string_view> BlockRegistry::value(BlockStateId state,
                                                     std::string_view property) const {
    const BlockDef& def = m_blocks[m_stateBlock[state]];
    const int p = propertyIndex(def, property);
    if (p < 0) return std::nullopt;
    return def.properties[p]->values[valueIndex(def, state, p)];
}

std::optional<BlockStateId> BlockRegistry::with(BlockStateId state, std::string_view property,
                                                std::string_view value) const {
    const BlockDef& def = m_blocks[m_stateBlock[state]];
    const int p = propertyIndex(def, property);
    if (p < 0) return std::nullopt;
    const auto& values = def.properties[p]->values;
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) return std::nullopt;
    const uint32_t oldIndex = valueIndex(def, state, p);
    const uint32_t newIndex = static_cast<uint32_t>(it - values.begin());
    return static_cast<BlockStateId>(state + (newIndex - oldIndex) * def.strides[p]);
}

std::string BlockRegistry::toString(BlockStateId state) const {
    const BlockDef& def = m_blocks[m_stateBlock[state]];
    std::string out = def.id;
    if (def.properties.empty()) return out;
    out += '[';
    for (size_t i = 0; i < def.properties.size(); ++i) {
        if (i) out += ',';
        out += def.properties[i]->name;
        out += '=';
        out += def.properties[i]->values[valueIndex(def, state, static_cast<int>(i))];
    }
    out += ']';
    return out;
}

std::optional<BlockStateId> BlockRegistry::parse(std::string_view text) const {
    text = trim(text);
    const size_t bracket = text.find('[');
    const auto block = findBlock(trim(text.substr(0, bracket)));
    if (!block) return std::nullopt;
    BlockStateId state = m_blocks[*block].defaultState;
    if (bracket == std::string_view::npos) return state;
    if (text.back() != ']') return std::nullopt;

    std::string_view props = text.substr(bracket + 1, text.size() - bracket - 2);
    const BlockDef& def = m_blocks[*block];
    uint32_t seen = 0; // bit per property: vanilla rejects a property given twice
    while (!props.empty()) {
        const size_t comma = props.find(',');
        const std::string_view pair = props.substr(0, comma);
        const size_t eq = pair.find('=');
        if (eq == std::string_view::npos) return std::nullopt;
        const std::string_view name = trim(pair.substr(0, eq));
        const int p = propertyIndex(def, name);
        if (p < 0 || (seen & (1u << p))) return std::nullopt;
        seen |= 1u << p;
        const auto next = with(state, name, trim(pair.substr(eq + 1)));
        if (!next) return std::nullopt;
        state = *next;
        if (comma == std::string_view::npos) break;
        props.remove_prefix(comma + 1);
    }
    return state;
}

} // namespace mc::world
