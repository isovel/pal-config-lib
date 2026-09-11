#pragma once

// Fills a settings struct from a parsed document, one field at a time.
//
// Two tiers of failure, deliberately.
//
// A document that cannot be parsed yields nothing, so LoadFromText leaves the
// caller's struct untouched and reports one Error. PerkyPals got this right by
// parsing into a local and assigning only on success; a reload that fails MUST
// keep the settings already running.
//
// A single field that cannot be read costs that field alone: it keeps the value
// already there, a Warning names it, and every other field loads. A typo in
// arousalRate must not cost a user their idleActions list.
//
// Since a load starts from a freshly default-constructed struct, deleting a key
// from the file restores that field's default.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <PalCfg/Coerce.hpp>
#include <PalCfg/Diagnostics.hpp>
#include <PalCfg/Document.hpp>
#include <PalCfg/Field.hpp>
#include <PalCfg/Keys.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg
{
    namespace Detail
    {
        template <std::size_t N>
        void CollectUnknownKeys(const IValueSource& node,
                                const std::array<FieldRuntime, N>& fields,
                                const std::string& prefix,
                                LoadResult& result,
                                IDiagnosticSink& diag)
        {
            node.ForEachKey([&](std::string_view key) {
                const std::string path = prefix.empty() ? std::string{key} : prefix + "." + std::string{key};

                bool claimed = false;
                bool hasDescendant = false;

                for (const auto& field : fields)
                {
                    if (field.meta.key == nullptr) continue;
                    const bool insensitive = field.meta.caseInsensitiveKeys;

                    if (KeysEqual(field.meta.key, path, insensitive))
                    {
                        claimed = true;
                        break;
                    }
                    for (std::size_t i = 0; i < field.meta.aliasCount; ++i)
                    {
                        if (!KeysEqual(field.meta.aliases[i], path, insensitive)) continue;
                        claimed = true;
                        break;
                    }
                    if (claimed) break;

                    if (IsUnderPath(field.meta.key, path, insensitive)) hasDescendant = true;
                }

                if (claimed) return;

                // A path some field lives beneath is a container, so the search
                // continues inside it. Its own name belongs to nobody, and saying so
                // would report every parent of every nested setting.
                if (hasDescendant)
                {
                    node.Child(key, [&](const IValueSource& child) {
                        if (child.IsObject()) CollectUnknownKeys(child, fields, path, result, diag);
                    });
                    return;
                }

                result.unknownKeys.push_back(path);
                diag.Report({Severity::Note, path, "not a setting this version knows; kept as it is"});
            });
        }
    } // namespace Detail

    template <class T, std::size_t N>
    LoadResult LoadFields(const std::array<FieldRuntime, N>& fields,
                          const IValueSource& root,
                          T& out,
                          IDiagnosticSink& diag)
    {
        LoadResult result{};
        result.parsed = true;

        for (const auto& field : fields)
        {
            if (field.ops == nullptr || field.ops->parse == nullptr) continue;
            if (field.meta.key == nullptr) continue;

            const ReadContext context{field.meta, field.meta.key, diag};

            bool read = false;
            const auto Parse = [&](const IValueSource& value) {
                read = field.ops->parse(&out, field.boundMemberPtr, value, context);
                if (!read) Detail::WarnUnreadable(context, value);
            };

            const bool insensitive = field.meta.caseInsensitiveKeys;
            bool present = Detail::ResolvePath(root, field.meta.key, insensitive, Parse);

            // Aliases answer only for a key that is absent, so a present primary
            // key stays authoritative even when its value turns out unreadable.
            for (std::size_t i = 0; !present && i < field.meta.aliasCount; ++i)
            {
                present = Detail::ResolvePath(root, field.meta.aliases[i], insensitive, Parse);
            }

            if (read) ++result.fieldsLoaded;
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

        Detail::CollectUnknownKeys(root, fields, std::string{}, result, diag);

        return result;
    }

    template <class T, std::size_t N>
    LoadResult LoadFields(const std::array<FieldRuntime, N>& fields, const IValueSource& root, T& out)
    {
        NullSink sink;
        return LoadFields(fields, root, out, sink);
    }

    // Parses and loads in one step. `out` is written only when the document parses,
    // so a caller may hand it settings that are already live.
    template <class T, std::size_t N>
    LoadResult LoadFromText(const std::array<FieldRuntime, N>& fields,
                            std::string text,
                            T& out,
                            IDiagnosticSink& diag)
    {
        Document doc;
        if (!doc.Parse(std::move(text)))
        {
            diag.Report({Severity::Error, {}, doc.Error().message});
            return LoadResult{};
        }

        if (!doc.Root().IsObject())
        {
            diag.Report({Severity::Error, {}, "the document's root is not an object"});
            return LoadResult{};
        }

        if (doc.Sanitised().bomRemoved)
        {
            diag.Report({Severity::Note, {}, "removed a UTF-8 byte order mark"});
        }
        if (doc.Sanitised().trailingCommasRemoved > 0)
        {
            diag.Report({Severity::Note,
                         {},
                         "removed " + std::to_string(doc.Sanitised().trailingCommasRemoved) +
                             " trailing comma(s)"});
        }

        // Into a fresh struct, so every absent key resolves to its declared default
        // even when `out` arrives holding older values.
        T loaded{};
        LoadResult result = LoadFields(fields, doc.Root(), loaded, diag);

        out = std::move(loaded);
        return result;
    }

    template <class T, std::size_t N>
    LoadResult LoadFromText(const std::array<FieldRuntime, N>& fields, std::string text, T& out)
    {
        NullSink sink;
        return LoadFromText(fields, std::move(text), out, sink);
    }
} // namespace PalCfg
