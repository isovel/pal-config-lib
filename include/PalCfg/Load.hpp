#pragma once

// Fills a settings struct from a parsed document, one field at a time.
//
// A field whose key is absent, or whose value the field's traits decline, keeps
// whatever the struct already held. Since a load starts from a freshly
// default-constructed struct, that means deleting a key from the file restores
// its default. One bad field costs that field alone.

#include <array>
#include <cstddef>
#include <string_view>

#include <PalCfg/Coerce.hpp>
#include <PalCfg/Field.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg
{
    namespace Detail
    {
        inline bool FindChild(const IValueSource& node,
                             std::string_view name,
                             bool caseInsensitive,
                             const IValueSource::Visitor& visit)
        {
            if (!caseInsensitive) return node.Child(name, visit);

            // The document's own spelling has to be recovered before Child can be
            // asked for it, so this walks the keys once. dynamic-pals built a
            // lowercased index per object for the same reason.
            std::string actual;
            bool found = false;
            node.ForEachKey([&](std::string_view key) {
                if (found || !EqualsIgnoringCase(key, name)) return;
                actual.assign(key);
                found = true;
            });

            return found && node.Child(actual, visit);
        }

        // Walks a dotted key such as "activity.idleTasks" through nested objects.
        // A missing or non-object link resolves to nothing, so the default stands.
        inline bool ResolvePath(const IValueSource& node,
                               std::string_view path,
                               bool caseInsensitive,
                               const IValueSource::Visitor& visit)
        {
            const auto dot = path.find('.');
            if (dot == std::string_view::npos) return FindChild(node, path, caseInsensitive, visit);

            bool resolved = false;
            FindChild(node, path.substr(0, dot), caseInsensitive, [&](const IValueSource& child) {
                resolved = ResolvePath(child, path.substr(dot + 1), caseInsensitive, visit);
            });
            return resolved;
        }
    } // namespace Detail

    template <class T, std::size_t N>
    void LoadFields(const std::array<FieldRuntime, N>& fields, const IValueSource& root, T& out)
    {
        for (const auto& field : fields)
        {
            if (field.ops == nullptr || field.ops->parse == nullptr) continue;
            if (field.meta.key == nullptr) continue;

            const auto Parse = [&](const IValueSource& value) {
                field.ops->parse(&out, field.boundMemberPtr, value, field.meta);
            };

            const bool insensitive = field.meta.caseInsensitiveKeys;
            if (Detail::ResolvePath(root, field.meta.key, insensitive, Parse)) continue;

            // Aliases answer only for a key that is absent, so a present primary
            // key stays authoritative even when its value turns out unreadable.
            for (std::size_t i = 0; i < field.meta.aliasCount; ++i)
            {
                if (Detail::ResolvePath(root, field.meta.aliases[i], insensitive, Parse)) break;
            }
        }

        // Pair invariants run last, so they see final values - clamped, coerced,
        // and defaulted alike.
        for (const auto& field : fields)
        {
            if (!field.meta.swapIfInverted) continue;
            if (field.meta.pairPartner >= N) continue;
            if (field.ops == nullptr || field.ops->swapIfInverted == nullptr) continue;

            field.ops->swapIfInverted(&out,
                                      field.boundMemberPtr,
                                      fields[field.meta.pairPartner].boundMemberPtr);
        }
    }
} // namespace PalCfg
