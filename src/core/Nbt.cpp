#include "core/Nbt.h"

#include <cstring>

namespace mc::nbt {

const Tag* Compound::find(std::string_view name) const {
    for (const auto& e : entries)
        if (e.name == name) return &e.value;
    return nullptr;
}

Tag* Compound::find(std::string_view name) {
    for (auto& e : entries)
        if (e.name == name) return &e.value;
    return nullptr;
}

Tag& Compound::put(std::string name, Tag value) {
    if (Tag* t = find(name)) {
        *t = std::move(value);
        return *t;
    }
    entries.push_back({std::move(name), std::move(value)});
    return entries.back().value;
}

std::optional<int64_t> Compound::integer(std::string_view name) const {
    const Tag* t = find(name);
    if (!t) return std::nullopt;
    if (auto v = t->get<int8_t>()) return *v;
    if (auto v = t->get<int16_t>()) return *v;
    if (auto v = t->get<int32_t>()) return *v;
    if (auto v = t->get<int64_t>()) return *v;
    return std::nullopt;
}

std::optional<double> Compound::real(std::string_view name) const {
    const Tag* t = find(name);
    if (!t) return std::nullopt;
    if (auto v = t->get<float>()) return *v;
    if (auto v = t->get<double>()) return *v;
    return std::nullopt;
}

const std::string* Compound::string(std::string_view name) const {
    const Tag* t = find(name);
    return t ? t->get<std::string>() : nullptr;
}
const Compound* Compound::compound(std::string_view name) const {
    const Tag* t = find(name);
    return t ? t->get<Compound>() : nullptr;
}
const List* Compound::list(std::string_view name) const {
    const Tag* t = find(name);
    return t ? t->get<List>() : nullptr;
}
const std::vector<int64_t>* Compound::longArray(std::string_view name) const {
    const Tag* t = find(name);
    return t ? t->get<std::vector<int64_t>>() : nullptr;
}
const std::vector<int8_t>* Compound::byteArray(std::string_view name) const {
    const Tag* t = find(name);
    return t ? t->get<std::vector<int8_t>>() : nullptr;
}

List listOf(TagType type, std::vector<Tag> items) { return List{type, std::move(items)}; }

namespace {

// --- Writing ---------------------------------------------------------------------

struct Writer {
    std::vector<uint8_t> out;
    template <typename T> void be(T v) {
        using U = std::make_unsigned_t<std::conditional_t<sizeof(T) == 8, int64_t,
                                                         std::conditional_t<sizeof(T) == 4, int32_t,
                                                                            std::conditional_t<sizeof(T) == 2, int16_t, int8_t>>>>;
        U u;
        std::memcpy(&u, &v, sizeof(T));
        for (int i = sizeof(T) - 1; i >= 0; --i)
            out.push_back(static_cast<uint8_t>(u >> (8 * i)));
    }
    void str(std::string_view s) {
        be(static_cast<uint16_t>(s.size()));
        out.insert(out.end(), s.begin(), s.end());
    }
    void payload(const Tag& t);
    void compound(const Compound& c) {
        for (const auto& e : c.entries) {
            out.push_back(static_cast<uint8_t>(e.value.type()));
            str(e.name);
            payload(e.value);
        }
        out.push_back(0); // TAG_End
    }
};

void Writer::payload(const Tag& t) {
    std::visit(
        [&](const auto& v) {
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<V>) {
                be(v);
            } else if constexpr (std::is_same_v<V, std::string>) {
                str(v);
            } else if constexpr (std::is_same_v<V, List>) {
                const TagType et = v.items.empty() ? v.elementType : v.items.front().type();
                out.push_back(static_cast<uint8_t>(v.items.empty() && et == TagType::End ? 0 : int(et)));
                be(static_cast<int32_t>(v.items.size()));
                for (const Tag& item : v.items)
                    payload(item);
            } else if constexpr (std::is_same_v<V, Compound>) {
                compound(v);
            } else { // arrays
                be(static_cast<int32_t>(v.size()));
                for (auto x : v)
                    be(x);
            }
        },
        t.value);
}

// --- Reading ---------------------------------------------------------------------

struct Reader {
    std::span<const uint8_t> in;
    size_t pos = 0;
    bool ok = true;
    int depth = 0;

    bool need(size_t n) {
        if (pos + n > in.size()) ok = false;
        return ok;
    }
    template <typename T> T be() {
        if (!need(sizeof(T))) return T{};
        std::conditional_t<sizeof(T) == 8, uint64_t,
                           std::conditional_t<sizeof(T) == 4, uint32_t,
                                              std::conditional_t<sizeof(T) == 2, uint16_t, uint8_t>>>
            u = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            u = static_cast<decltype(u)>((u << 8) | in[pos++]);
        T v;
        std::memcpy(&v, &u, sizeof(T));
        return v;
    }
    std::string str() {
        const uint16_t n = be<uint16_t>();
        if (!need(n)) return {};
        std::string s(reinterpret_cast<const char*>(in.data() + pos), n);
        pos += n;
        return s;
    }
    template <typename T> std::vector<T> array() {
        const int32_t n = be<int32_t>();
        if (n < 0 || !need(size_t(n) * sizeof(T))) {
            ok = false;
            return {};
        }
        std::vector<T> v(static_cast<size_t>(n));
        for (auto& x : v)
            x = be<T>();
        return v;
    }
    Tag payload(TagType type);
    Compound compound() {
        Compound c;
        while (ok) {
            const auto type = static_cast<TagType>(be<uint8_t>());
            if (!ok || type == TagType::End) break;
            std::string name = str();
            Tag value = payload(type);
            c.entries.push_back({std::move(name), std::move(value)});
        }
        return c;
    }
};

Tag Reader::payload(TagType type) {
    if (++depth > 512) { // vanilla's nesting limit
        ok = false;
        return {};
    }
    Tag t;
    switch (type) {
    case TagType::Byte: t = be<int8_t>(); break;
    case TagType::Short: t = be<int16_t>(); break;
    case TagType::Int: t = be<int32_t>(); break;
    case TagType::Long: t = be<int64_t>(); break;
    case TagType::Float: t = be<float>(); break;
    case TagType::Double: t = be<double>(); break;
    case TagType::ByteArray: t = array<int8_t>(); break;
    case TagType::String: t = str(); break;
    case TagType::List: {
        List l;
        l.elementType = static_cast<TagType>(be<uint8_t>());
        const int32_t n = be<int32_t>();
        if (n < 0 || static_cast<uint8_t>(l.elementType) > 12 ||
            (n > 0 && l.elementType == TagType::End)) {
            ok = false;
            break;
        }
        l.items.reserve(size_t(std::min(n, 65536)));
        for (int32_t i = 0; i < n && ok; ++i)
            l.items.push_back(payload(l.elementType));
        t = std::move(l);
        break;
    }
    case TagType::Compound: t = compound(); break;
    case TagType::IntArray: t = array<int32_t>(); break;
    case TagType::LongArray: t = array<int64_t>(); break;
    default: ok = false; break;
    }
    --depth;
    return t;
}

} // namespace

std::vector<uint8_t> write(const Compound& root, std::string_view rootName) {
    Writer w;
    w.out.push_back(static_cast<uint8_t>(TagType::Compound));
    w.str(rootName);
    w.compound(root);
    return std::move(w.out);
}

std::optional<Compound> read(std::span<const uint8_t> bytes, std::string* rootName) {
    Reader r{bytes};
    if (r.be<uint8_t>() != static_cast<uint8_t>(TagType::Compound)) return std::nullopt;
    std::string name = r.str();
    Compound c = r.compound();
    if (!r.ok) return std::nullopt;
    if (rootName) *rootName = std::move(name);
    return c;
}

} // namespace mc::nbt
