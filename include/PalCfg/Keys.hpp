#pragma once

// Matching a schema's dotted keys against a document's objects.
//
// Shared by the loader and the writer: both have to decide which node a key such
// as "activity.idleTasks" names, and which keys at a level belong to no field.

#include <string>
#include <string_view>

#include <PalCfg/Coerce.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg::Detail
{
        inline bool KeysEqual(std::string_view a, std::string_view b, bool caseInsensitive)
        {
            return caseInsensitive ? EqualsIgnoringCase(a, b) : a == b;
        }

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

        // Whether `candidate` is a dotted key living underneath `path`.
        inline bool IsUnderPath(std::string_view candidate, std::string_view path, bool caseInsensitive)
        {
            if (candidate.size() <= path.size() + 1) return false;
            if (candidate[path.size()] != '.') return false;

            return KeysEqual(candidate.substr(0, path.size()), path, caseInsensitive);
        }

} // namespace PalCfg::Detail
