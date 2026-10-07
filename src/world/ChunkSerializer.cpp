#include "world/ChunkSerializer.h"

#include "world/Blocks.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_map>

namespace mc::world {

ChunkSnapshot ChunkSnapshot::of(const Chunk& chunk, int64_t gameTime) {
    ChunkSnapshot s;
    s.pos = chunk.pos();
    s.gameTime = gameTime;
    s.biomes = chunk.biomes();
    s.furnaces = chunk.furnaces();
    s.mobs = chunk.mobs();
    for (int i = 0; i < kSectionsPerChunk; ++i) {
        s.sections[size_t(i)] = chunk.shareSection(i);
        s.light[size_t(i)] = chunk.light(i);
    }
    return s;
}

namespace {

// "minecraft:oak_log[axis=x]" -> {Name: "minecraft:oak_log", Properties: {axis: "x"}}
nbt::Compound paletteEntry(const std::string& text) {
    nbt::Compound e;
    const size_t open = text.find('[');
    e.put("Name", text.substr(0, open));
    if (open != std::string::npos) {
        nbt::Compound props;
        const std::string_view list(text.data() + open + 1, text.size() - open - 2);
        size_t start = 0;
        while (start < list.size()) {
            size_t end = list.find(',', start);
            if (end == std::string_view::npos) end = list.size();
            const std::string_view kv = list.substr(start, end - start);
            const size_t eq = kv.find('=');
            if (eq != std::string_view::npos)
                props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
            start = end + 1;
        }
        e.put("Properties", std::move(props));
    }
    return e;
}

std::string paletteText(const nbt::Compound& e) {
    const std::string* name = e.string("Name");
    if (!name) return {};
    std::string text = *name;
    if (const nbt::Compound* props = e.compound("Properties"); props && !props->entries.empty()) {
        text += '[';
        bool first = true;
        for (const auto& p : props->entries) {
            const std::string* v = p.value.get<std::string>();
            if (!v) continue;
            if (!first) text += ',';
            first = false;
            text += p.name + '=' + *v;
        }
        text += ']';
    }
    return text;
}

std::vector<int8_t> nibbles(const LightLayer& layer) {
    std::vector<int8_t> out(2048);
    for (int i = 0; i < 2048; ++i) {
        const int lo = layer.get(2 * i), hi = layer.get(2 * i + 1);
        out[size_t(i)] = static_cast<int8_t>(lo | (hi << 4));
    }
    return out;
}

} // namespace

namespace {

// An item stack as vanilla 1.20.5+ saves it: id, count, components.
nbt::Compound itemNbt(const ItemStack& s, int slot) {
    nbt::Compound c;
    c.put("Slot", static_cast<int8_t>(slot));
    c.put("id", itemRegistry().item(s.item).id);
    c.put("count", int32_t{s.count});
    nbt::Compound components;
    if (s.damage) components.put("minecraft:damage", int32_t{s.damage});
    if (s.state) { // exact block state (vanilla's minecraft:block_state component)
        nbt::Compound props;
        const std::string text = blockRegistry().toString(s.state);
        if (const size_t open = text.find('['); open != std::string::npos) {
            std::string_view list(text.data() + open + 1, text.size() - open - 2);
            while (!list.empty()) {
                const size_t comma = std::min(list.find(','), list.size());
                const std::string_view kv = list.substr(0, comma);
                if (const size_t eq = kv.find('='); eq != std::string_view::npos)
                    props.put(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
                list.remove_prefix(std::min(comma + 1, list.size()));
            }
        }
        components.put("minecraft:block_state", std::move(props));
    }
    if (!components.entries.empty()) c.put("components", std::move(components));
    return c;
}

ItemStack itemFromNbt(const nbt::Compound& c) {
    const std::string* id = c.string("id");
    const auto item = id ? itemRegistry().find(*id) : std::nullopt;
    if (!item) return {};
    const int maxStack = std::max<int>(1, itemRegistry().item(*item).maxStack);
    ItemStack s{*item, static_cast<uint8_t>(std::clamp<int64_t>(c.integer("count").value_or(1), 1, maxStack))};
    if (const nbt::Compound* comps = c.compound("components")) {
        s.damage = static_cast<uint16_t>(comps->integer("minecraft:damage").value_or(0));
        const ItemDef& def = itemRegistry().item(*item);
        if (const nbt::Compound* props = comps->compound("minecraft:block_state"); props && def.block) {
            BlockStateId st = blockRegistry().defaultState(def.block);
            for (const auto& p : props->entries)
                if (const std::string* v = p.value.get<std::string>())
                    st = blockRegistry().with(st, p.name, *v).value_or(st);
            if (st != blockRegistry().defaultState(def.block)) s.state = st;
        }
    }
    return s;
}

} // namespace

nbt::Compound chunkToNbt(const ChunkSnapshot& chunk) {
    const auto& reg = blockRegistry();
    nbt::Compound root;
    root.put("DataVersion", kDataVersion);
    root.put("xPos", chunk.pos.x);
    root.put("zPos", chunk.pos.z);
    root.put("yPos", int32_t{kMinY >> 4});
    root.put("Status", std::string("minecraft:full"));
    root.put("LastUpdate", chunk.gameTime); // game tick of this save
    root.put("InhabitedTime", int64_t{0});
    bool lit = true;
    for (const auto& l : chunk.light)
        lit = lit && l != nullptr;
    root.put("isLightOn", static_cast<int8_t>(lit ? 1 : 0));

    std::vector<nbt::Tag> sections;
    std::vector<BlockStateId> states(Section::kVolume);
    std::vector<BlockStateId> palette;
    std::unordered_map<BlockStateId, uint32_t> paletteIndex;
    for (int s = 0; s < kSectionsPerChunk; ++s) {
        nbt::Compound sec;
        sec.put("Y", static_cast<int8_t>((kMinY >> 4) + s));
        // Block states: local palette in order of first appearance.
        chunk.sections[size_t(s)]->copyTo(states.data());
        palette.clear();
        paletteIndex.clear();
        for (BlockStateId st : states)
            if (paletteIndex.try_emplace(st, uint32_t(palette.size())).second) palette.push_back(st);
        nbt::Compound blockStates;
        std::vector<nbt::Tag> entries;
        for (BlockStateId st : palette)
            entries.emplace_back(paletteEntry(reg.toString(st)));
        blockStates.put("palette", nbt::listOf(nbt::TagType::Compound, std::move(entries)));
        if (palette.size() > 1) {
            const int bits = std::max(4, int(std::bit_width(palette.size() - 1)));
            const int perLong = 64 / bits;
            std::vector<int64_t> data((Section::kVolume + perLong - 1) / perLong, 0);
            for (int i = 0; i < Section::kVolume; ++i) {
                const uint64_t v = paletteIndex[states[size_t(i)]];
                data[size_t(i / perLong)] |= static_cast<int64_t>(v << ((i % perLong) * bits));
            }
            blockStates.put("data", std::move(data));
        }
        sec.put("block_states", std::move(blockStates));
        {
            const ChunkBiomes& cb = chunk.biomes ? *chunk.biomes : *Chunk::defaultBiomes();
            std::vector<Biome> bpal;
            uint8_t indexOf[256] = {};
            for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                const Biome b = cb.cells[size_t(s * ChunkBiomes::kPerSection + i)];
                if (std::find(bpal.begin(), bpal.end(), b) == bpal.end()) {
                    indexOf[static_cast<int>(b)] = static_cast<uint8_t>(bpal.size());
                    bpal.push_back(b);
                }
            }
            std::vector<nbt::Tag> names;
            for (Biome b : bpal)
                names.emplace_back(std::string(biomeInfo(b).id));
            nbt::Compound biomes;
            biomes.put("palette", nbt::listOf(nbt::TagType::String, std::move(names)));
            if (bpal.size() > 1) {
                const int bits = int(std::bit_width(bpal.size() - 1));
                const int perLong = 64 / bits;
                std::vector<int64_t> data((ChunkBiomes::kPerSection + perLong - 1) / perLong, 0);
                for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                    const uint64_t v =
                        indexOf[static_cast<int>(cb.cells[size_t(s * ChunkBiomes::kPerSection + i)])];
                    data[size_t(i / perLong)] |= static_cast<int64_t>(v << ((i % perLong) * bits));
                }
                biomes.put("data", std::move(data));
            }
            sec.put("biomes", std::move(biomes));
        }
        if (const auto& l = chunk.light[size_t(s)]) {
            // SkyLight is always written (an omitted one means "same as the section
            // above"); BlockLight is omitted when no light reaches the section.
            sec.put("SkyLight", nibbles(l->sky));
            if (!(l->block.isUniform() && l->block.uniformValue() == 0))
                sec.put("BlockLight", nibbles(l->block));
        }
        sections.emplace_back(std::move(sec));
    }
    root.put("sections", nbt::listOf(nbt::TagType::Compound, std::move(sections)));
    // Block entities (wiki: Chunk format › block_entities; Furnace › Block data, 1.21.1).
    std::vector<nbt::Tag> entities;
    for (const auto& f : chunk.furnaces) {
        nbt::Compound e;
        e.put("id", std::string("minecraft:furnace"));
        e.put("x", int32_t{chunk.pos.x * 16 + f.x});
        e.put("y", int32_t{f.y});
        e.put("z", int32_t{chunk.pos.z * 16 + f.z});
        e.put("keepPacked", int8_t{0});
        std::vector<nbt::Tag> items;
        const ItemStack* slots[3] = {&f.data.input, &f.data.fuel, &f.data.output};
        for (int i = 0; i < 3; ++i)
            if (!slots[i]->empty()) items.emplace_back(itemNbt(*slots[i], i));
        e.put("Items", nbt::listOf(nbt::TagType::Compound, std::move(items)));
        e.put("BurnTime", static_cast<int16_t>(f.data.burnLeft));
        e.put("CookTime", static_cast<int16_t>(f.data.cookTime));
        e.put("CookTimeTotal", int16_t{200});
        entities.emplace_back(std::move(e));
    }
    root.put("block_entities", nbt::listOf(nbt::TagType::Compound, std::move(entities)));
    return root;
}

bool chunkFromNbt(const nbt::Compound& root, Chunk& chunk, int* unknownBlocks) {
    const auto& reg = blockRegistry();
    if (root.integer("xPos") != chunk.pos().x || root.integer("zPos") != chunk.pos().z)
        return false;
    const nbt::List* sections = root.list("sections");
    if (!sections) return false;
    std::vector<BlockStateId> states(Section::kVolume);
    std::vector<BlockStateId> palette;
    for (int s = 0; s < kSectionsPerChunk; ++s)
        chunk.mutableSection(s).fill(0);
    auto biomes = std::make_shared<ChunkBiomes>(*Chunk::defaultBiomes());
    for (const nbt::Tag& t : sections->items) {
        const nbt::Compound* sec = t.get<nbt::Compound>();
        if (!sec) continue;
        const auto y = sec->integer("Y");
        if (!y) continue;
        const int index = static_cast<int>(*y) - (kMinY >> 4);
        if (index < 0 || index >= kSectionsPerChunk) continue; // outside our height
        // Biomes (unknown names become plains).
        if (const nbt::Compound* bio = sec->compound("biomes")) {
            const nbt::List* bpal = bio->list("palette");
            std::vector<Biome> ids;
            if (bpal)
                for (const nbt::Tag& name : bpal->items) {
                    const std::string* n = name.get<std::string>();
                    ids.push_back(n ? findBiome(*n).value_or(Biome::Plains) : Biome::Plains);
                }
            const std::vector<int64_t>* bdata = bio->longArray("data");
            if (ids.size() == 1) {
                for (int i = 0; i < ChunkBiomes::kPerSection; ++i)
                    biomes->cells[size_t(index * ChunkBiomes::kPerSection + i)] = ids[0];
            } else if (ids.size() > 1 && bdata) {
                const int bits = int(std::bit_width(ids.size() - 1));
                const int perLong = 64 / bits;
                const uint64_t mask = (uint64_t{1} << bits) - 1;
                if (bdata->size() >= size_t((ChunkBiomes::kPerSection + perLong - 1) / perLong))
                    for (int i = 0; i < ChunkBiomes::kPerSection; ++i) {
                        const uint64_t v =
                            (static_cast<uint64_t>((*bdata)[size_t(i / perLong)]) >> ((i % perLong) * bits)) & mask;
                        biomes->cells[size_t(index * ChunkBiomes::kPerSection + i)] =
                            v < ids.size() ? ids[v] : Biome::Plains;
                    }
            }
        }
        const nbt::Compound* bs = sec->compound("block_states");
        const nbt::List* pal = bs ? bs->list("palette") : nullptr;
        if (!pal || pal->items.empty()) continue;
        palette.clear();
        for (const nbt::Tag& e : pal->items) {
            const nbt::Compound* entry = e.get<nbt::Compound>();
            std::optional<BlockStateId> st;
            if (entry) {
                std::string text = paletteText(*entry);
                if (text.starts_with("minecraft:")) text.erase(0, 10);
                st = reg.parse(text);
                if (!st) { // lenient: known block, unknown property or value
                    const std::string* name = entry->string("Name");
                    std::string_view id = name ? std::string_view(*name) : std::string_view();
                    if (id.starts_with("minecraft:")) id.remove_prefix(10);
                    if (const auto block = reg.findBlock(id)) {
                        st = reg.defaultState(*block);
                        if (const nbt::Compound* props = entry->compound("Properties"))
                            for (const auto& p : props->entries)
                                if (const std::string* v = p.value.get<std::string>())
                                    st = reg.with(*st, p.name, *v).value_or(*st);
                    }
                }
            }
            if (!st && unknownBlocks) ++*unknownBlocks;
            palette.push_back(st.value_or(0));
        }
        const std::vector<int64_t>* data = bs->longArray("data");
        if (palette.size() == 1 || !data) {
            chunk.mutableSection(index).fill(palette[0]);
            continue;
        }
        const int bits = std::max(4, int(std::bit_width(palette.size() - 1)));
        const int perLong = 64 / bits;
        if (data->size() < size_t((Section::kVolume + perLong - 1) / perLong)) continue;
        const uint64_t mask = (uint64_t{1} << bits) - 1;
        for (int i = 0; i < Section::kVolume; ++i) {
            const uint64_t word = static_cast<uint64_t>((*data)[size_t(i / perLong)]);
            const uint64_t v = (word >> ((i % perLong) * bits)) & mask;
            states[size_t(i)] = v < palette.size() ? palette[v] : BlockStateId{0};
        }
        chunk.mutableSection(index).assign(states.data());
    }
    // Block entities (furnaces) inside this chunk.
    if (const nbt::List* entities = root.list("block_entities"))
        for (const nbt::Tag& t : entities->items) {
            const nbt::Compound* e = t.get<nbt::Compound>();
            const std::string* id = e ? e->string("id") : nullptr;
            if (!id || *id != "minecraft:furnace") continue;
            const int x = static_cast<int>(e->integer("x").value_or(0)) - chunk.pos().x * 16;
            const int y = static_cast<int>(e->integer("y").value_or(kMinY - 1));
            const int z = static_cast<int>(e->integer("z").value_or(0)) - chunk.pos().z * 16;
            if (x < 0 || x > 15 || z < 0 || z > 15 || !isInBuildHeight(y)) continue;
            // An entry whose block isn't a furnace (foreign or edited saves) is dropped.
            if (blockRegistry().blockOf(chunk.get(x, y, z)) != blocks::Furnace) continue;
            FurnaceData& f = chunk.addFurnace(x, y, z);
            if (const nbt::List* items = e->list("Items"))
                for (const nbt::Tag& it : items->items)
                    if (const nbt::Compound* c = it.get<nbt::Compound>()) {
                        const auto slot = c->integer("Slot").value_or(-1);
                        ItemStack* dst = slot == 0 ? &f.input : slot == 1 ? &f.fuel : slot == 2 ? &f.output : nullptr;
                        if (dst) *dst = itemFromNbt(*c);
                    }
            f.burnLeft = static_cast<int>(e->integer("BurnTime").value_or(0));
            f.burnDuration = f.burnLeft; // not saved by vanilla: the gauge restarts full
            f.cookTime = static_cast<int>(e->integer("CookTime").value_or(0));
            f.cooking = f.input.item; // not saved (vanilla neither): progress belongs to the input
        }
    chunk.setBiomes(std::move(biomes));
    return true;
}

nbt::Compound entitiesToNbt(const ChunkSnapshot& chunk) {
    nbt::Compound root;
    root.put("DataVersion", kDataVersion);
    root.put("Position", std::vector<int32_t>{chunk.pos.x, chunk.pos.z});
    std::vector<nbt::Tag> list;
    for (const MobData& m : chunk.mobs) {
        if (m.health <= 0.0f) continue; // dying mobs are not saved
        nbt::Compound e;
        e.put("id", std::string(mobInfo(m.type).id));
        e.put("Pos", nbt::listOf(nbt::TagType::Double, {m.pos.x, m.pos.y, m.pos.z}));
        e.put("Motion", nbt::listOf(nbt::TagType::Double, {m.vel.x, m.vel.y, m.vel.z}));
        e.put("Rotation", nbt::listOf(nbt::TagType::Float, {m.yaw, m.pitch}));
        e.put("Health", m.health);
        e.put("OnGround", static_cast<int8_t>(m.onGround ? 1 : 0));
        e.put("FallDistance", m.fallDistance);
        e.put("Fire", static_cast<int16_t>(m.fireTicks));
        e.put("HurtTime", static_cast<int16_t>(m.hurtTime));
        e.put("DeathTime", int16_t{0});
        e.put("PersistenceRequired", static_cast<int8_t>(m.persistent ? 1 : 0));
        e.put("UUID", std::vector<int32_t>{int32_t(m.uuidHi >> 32), int32_t(m.uuidHi), int32_t(m.uuidLo >> 32),
                                           int32_t(m.uuidLo)});
        list.emplace_back(std::move(e));
    }
    root.put("Entities", nbt::listOf(nbt::TagType::Compound, std::move(list)));
    return root;
}

void entitiesFromNbt(const nbt::Compound& root, Chunk& chunk) {
    chunk.mobs().clear();
    const nbt::List* list = root.list("Entities");
    if (!list) return;
    for (const nbt::Tag& t : list->items) {
        const nbt::Compound* e = t.get<nbt::Compound>();
        const std::string* id = e ? e->string("id") : nullptr;
        if (!id) continue;
        MobData m;
        bool known = false;
        for (int k = 0; k < static_cast<int>(MobType::Count); ++k)
            if (mobInfo(static_cast<MobType>(k)).id == *id) {
                m.type = static_cast<MobType>(k);
                known = true;
            }
        if (!known) continue; // entity types we don't have yet are skipped
        auto vec3 = [&](const char* key, glm::dvec3& out) {
            if (const nbt::List* l = e->list(key); l && l->items.size() == 3)
                for (int i = 0; i < 3; ++i)
                    if (auto d = l->items[size_t(i)].get<double>()) out[i] = *d;
        };
        vec3("Pos", m.pos);
        vec3("Motion", m.vel);
        // Corrupt or hand-edited files: skip mobs outside the world, clamp motion
        // (vanilla clamps each component to +-10) and health.
        if (!isValidMobPosition(m.pos)) continue;
        for (int i = 0; i < 3; ++i)
            m.vel[i] = std::isfinite(m.vel[i]) ? std::clamp(m.vel[i], -10.0, 10.0) : 0.0;
        m.prevPos = m.goal = m.pos;
        if (const nbt::List* r = e->list("Rotation"); r && r->items.size() == 2) {
            if (auto v = r->items[0].get<float>()) m.yaw = m.prevYaw = m.headYaw = m.prevHeadYaw = *v;
            if (auto v = r->items[1].get<float>()) m.pitch = m.prevPitch = *v;
        }
        m.health = static_cast<float>(e->real("Health").value_or(mobInfo(m.type).maxHealth));
        m.health = std::isfinite(m.health) ? std::min(m.health, mobInfo(m.type).maxHealth) : 0.0f;
        m.onGround = e->integer("OnGround").value_or(0) != 0;
        m.fireTicks = static_cast<int16_t>(e->integer("Fire").value_or(0));
        m.persistent = e->integer("PersistenceRequired").value_or(0) != 0;
        if (const nbt::Tag* u = e->find("UUID"))
            if (const auto* a = u->get<std::vector<int32_t>>(); a && a->size() == 4) {
                m.uuidHi = (uint64_t(uint32_t((*a)[0])) << 32) | uint32_t((*a)[1]);
                m.uuidLo = (uint64_t(uint32_t((*a)[2])) << 32) | uint32_t((*a)[3]);
            }
        if (m.health > 0.0f) chunk.mobs().push_back(m);
    }
}

} // namespace mc::world
