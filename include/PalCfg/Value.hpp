#pragma once

// The seam that keeps json.hpp out of consumer translation units.
//
// Field parsing is templated on the member type, so it is instantiated in the
// consumer's own TU. Were those templates to touch nlohmann::json directly, every
// consumer would pay ~1MB of parser - the cost PerkyPals set out to avoid by
// keeping json.hpp inside src/Config.cpp. Reading through this abstract interface
// confines the parser to src/core/JsonBackend.cpp.
//
// The price is one virtual call per scalar read. For a config of a few dozen
// fields loaded at 1 Hz, that is beneath measurement.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace PalCfg
{
    // A node in a parsed document. Borrowed, never owned: a source is valid only
    // while the Document that produced it lives.
    class IValueSource
    {
      public:
        virtual ~IValueSource() = default;

        virtual bool IsNull() const = 0;
        virtual bool IsBool() const = 0;
        virtual bool IsNumber() const = 0;
        virtual bool IsString() const = 0;
        virtual bool IsArray() const = 0;
        virtual bool IsObject() const = 0;

        // Each returns false when the node holds nothing readable as that type,
        // leaving the output untouched. Cross-type coercion arrives with
        // ValueTraits; these report only what the node naturally holds.
        virtual bool AsBool(bool& out) const = 0;
        virtual bool AsInt64(std::int64_t& out) const = 0;
        virtual bool AsDouble(double& out) const = 0;
        virtual bool AsUtf8(std::string& out) const = 0;

        // Element count for an array or object, otherwise zero.
        virtual std::size_t Size() const = 0;

        using Visitor = std::function<void(const IValueSource&)>;

        // Callbacks, so no node type appears in this interface and a borrowed
        // node stays confined to its visit.
        virtual bool Element(std::size_t index, const Visitor& visit) const = 0;
        virtual bool Child(std::string_view key, const Visitor& visit) const = 0;

        // Member keys of an object, in document order. Empty for other types.
        virtual void ForEachKey(const std::function<void(std::string_view)>& visit) const = 0;

        // Serialised form of this subtree, for the menu's JSON-string wire
        // format. Replaces `out`.
        virtual void Dump(std::string& out) const = 0;
    };
} // namespace PalCfg
