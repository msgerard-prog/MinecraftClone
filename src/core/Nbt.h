#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mc::nbt {

// Named Binary Tag, the format of vanilla saves (wiki: NBT format). Big-endian;
// strings are (modified) UTF-8 with a 2-byte length. Not for hot paths: save/load
// runs on the IO thread.
enum class TagType : uint8_t {
    End = 0,
    Byte = 1,
    Short = 2,
    Int = 3,
    Long = 4,
    Float = 5,
    Double = 6,
    ByteArray = 7,
    String = 8,
    List = 9,
    Compound = 10,
    IntArray = 11,
    LongArray = 12,
};

struct Tag;
struct CompoundEntry;

struct List {
    TagType elementType = TagType::End;
    std::vector<Tag> items;
};

struct Compound {
    std::vector<CompoundEntry> entries; // insertion order (vanilla's order is irrelevant)

    const Tag* find(std::string_view name) const;
    Tag* find(std::string_view name);
    // Replaces an existing entry of that name.
    Tag& put(std::string name, Tag value);

    // Typed lookups: nullopt / nullptr if missing or of another type.
    std::optional<int64_t> integer(std::string_view name) const; // Byte..Long
    std::optional<double> real(std::string_view name) const;     // Float/Double
    const std::string* string(std::string_view name) const;
    const Compound* compound(std::string_view name) const;
    const List* list(std::string_view name) const;
    const std::vector<int64_t>* longArray(std::string_view name) const;
    const std::vector<int8_t>* byteArray(std::string_view name) const;
};

struct Tag {
    std::variant<int8_t, int16_t, int32_t, int64_t, float, double, std::vector<int8_t>,
                 std::string, List, Compound, std::vector<int32_t>, std::vector<int64_t>>
        value;

    Tag() : value(int8_t{0}) {}
    template <typename T> Tag(T v) : value(std::move(v)) {}
    TagType type() const { return static_cast<TagType>(value.index() + 1); }
    template <typename T> const T* get() const { return std::get_if<T>(&value); }
    template <typename T> T* get() { return std::get_if<T>(&value); }
};

struct CompoundEntry {
    std::string name;
    Tag value;
};

// Uncompressed NBT bytes <-> root compound (the root is a named compound; its name
// is usually empty).
std::vector<uint8_t> write(const Compound& root, std::string_view rootName = {});
std::optional<Compound> read(std::span<const uint8_t> bytes, std::string* rootName = nullptr);

// Helpers for building lists.
List listOf(TagType type, std::vector<Tag> items);

} // namespace mc::nbt
