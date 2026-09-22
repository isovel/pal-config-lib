#pragma once

// The POD description of one field: what it is called, how it presents, and the
// type-erased hook that fills it.
//
// Separate from Schema.hpp so ValueTraits can take a FieldMeta without the schema
// builder having to know about traits first.

#include <cstddef>
#include <cstdint>
#include <string>

namespace PalCfg
{
    class IValueSource;
    struct ReadContext;

    // What a field holds, for a settings menu deciding how to present it and for
    // the wire format's "kind" tag.
    enum class Kind : std::uint8_t
    {
        Unknown,
        Bool,
        Int,
        Float,
        Double,
        String,
        Enum,
        List,
        Set,
        Optional,
    };

    // No paired field.
    inline constexpr std::size_t kNoPair = static_cast<std::size_t>(-1);

    // How many alternative spellings one key may answer to. Four covers the
    // renames in the mods being migrated; a fixed array keeps FieldMeta a
    // literal type with no allocation.
    inline constexpr std::size_t kMaxAliases = 4;

    struct FieldMeta
    {
        const char* key = nullptr;
        const char* label = nullptr;

        // Shown in the generated config file as // comments and as help text in
        // a settings menu. Embedded newlines become separate comment lines.
        const char* help = nullptr;

        // Older spellings of `key`, accepted on read so a rename preserves a
        // user's existing setting.
        const char* aliases[kMaxAliases]{};
        std::size_t aliasCount = 0;

        double min = 0.0;
        double max = 0.0;
        bool hasRange = false;

        // An empty list in the file keeps the default. Per-field, because for
        // some lists empty is a meaningful answer.
        bool keepDefaultIfEmpty = false;

        // Presentation only: absent from a settings menu, and shown behind an
        // "advanced" disclosure respectively.
        bool hidden = false;
        bool advanced = false;

        // Set for every field of a schema that asked for it. Carried per-field so
        // one flat descriptor array stays the whole contract for loading.
        bool caseInsensitiveKeys = false;

        // Set on the lower field of a .FieldPair, naming the upper one. A settings
        // menu reads this to draw one two-handled slider for the pair.
        std::size_t pairPartner = kNoPair;

        // Whether that pair restores order when the file has the two the wrong way
        // round. Held on the lower field, so the rule is applied once.
        bool swapIfInverted = false;
    };

    // The type-erased hook for one field type. One instance per (struct, member
    // type) pair, so a schema of N fields over K distinct types yields K of these.
    struct FieldOps
    {
        Kind kind = Kind::Unknown;

        // Reads `source` into the member of `base` named by `boundMemberPtr`.
        // Returns false to leave whatever value `base` already held, which is how
        // an unreadable value keeps its default.
        bool (*parse)(void* base,
                      const void* boundMemberPtr,
                      const IValueSource& source,
                      const ReadContext& context) = nullptr;

        // Exchanges two members of `base` when the lower holds more than the
        // upper. Null for types with no ordering, which is what makes
        // .SwapIfInverted meaningless on them.
        void (*swapIfInverted)(void* base,
                               const void* lowerMemberPtr,
                               const void* upperMemberPtr) = nullptr;

        // Appends the JSON literal for the member of `base` named by
        // `boundMemberPtr`. Null for a type whose ValueTraits declares no Write,
        // which keeps a read-only trait out of a generated file.
        void (*format)(const void* base, const void* boundMemberPtr, std::string& out) = nullptr;

        // For a List or Set, the kind of one element; Unknown otherwise. A
        // settings menu reads this to pick a widget for the list.
        Kind elementKind = Kind::Unknown;
    };

    // Flat, homogeneous descriptor. One per field, consumed by the loader, the
    // writer and the menu serialiser alike.
    struct FieldRuntime
    {
        FieldMeta meta{};
        const FieldOps* ops = nullptr;
        const void* boundMemberPtr = nullptr;
    };
} // namespace PalCfg
