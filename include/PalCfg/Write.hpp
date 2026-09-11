#pragma once

// Renders a settings struct back out as the JSONC file a user edits.
//
// The generated file is the documentation: every field carries its .Help() text
// as comments directly above it, so the schema stays the single place a setting
// is described. All three mods being migrated keep a hand-written
// config.default.json in sync with their loader by hand; this makes it a build
// artifact instead.

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <PalCfg/Field.hpp>
#include <PalCfg/Keys.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg
{
    struct WriteOptions
    {
        // Emit .Help() text as // comments above each field.
        bool includeHelp = true;

        std::size_t indentWidth = 4;
    };

    namespace Detail
    {
        // Emit helpers, defined in src/core/Write.cpp so escaping and number
        // formatting stay out of every consumer's translation unit.

        // Appends `text` as a quoted JSON string, escaping what JSON requires.
        void AppendJsonString(std::string& out, std::string_view text);

        // Appends the shortest form that reads back as the same value. A whole
        // number gains a trailing ".0" when `fractional`, so a float field stays
        // recognisably a float in the file.
        void AppendJsonNumber(std::string& out, double value, bool fractional);

        void AppendJsonNumber(std::string& out, float value);

        // Appends `text` as // comment lines, one per embedded newline, each
        // prefixed with `indent`. An empty line stays empty, so help text can
        // separate paragraphs.
        void AppendComment(std::string& out, std::string_view text, std::string_view indent);

        // Marks a key carried forward from the file being replaced.
        inline constexpr const char* kCarriedNote =
            "Kept from the previous file; not a setting this version knows.";
    } // namespace Detail

    namespace Detail
    {
        // One pass per object level. A dotted key such as "activity.idleTasks"
        // opens its object the first time that prefix is seen, and declaration
        // order decides where each key and each object lands.
        template <std::size_t N>
        struct Renderer
        {
            const std::array<FieldRuntime, N>& fields;
            const void* base;
            const WriteOptions& options;

            // The file being replaced, whose unclaimed keys are carried forward so
            // a downgrade or a future version still finds its own settings.
            const IValueSource* preserved;

            std::string out;

            // Whether any field lives at `prefix` + `segment`.
            bool Claimed(const std::string& prefix, std::string_view segment) const
            {
                for (const auto& field : fields)
                {
                    if (field.meta.key == nullptr) continue;

                    const std::string_view key{field.meta.key};
                    if (!key.starts_with(prefix)) continue;

                    const std::string_view rest = key.substr(prefix.size());
                    const auto dot = rest.find('.');
                    const std::string_view own = rest.substr(0, dot);

                    if (KeysEqual(own, segment, field.meta.caseInsensitiveKeys)) return true;
                }
                return false;
            }

            void Level(const std::string& prefix, std::size_t depth)
            {
                const std::string indent(depth * options.indentWidth, ' ');
                std::vector<std::string_view> opened;
                bool first = true;

                // Returns whether this was the level's first entry, which decides
                // whether a commented field gets a blank line above it.
                const auto Separate = [&] {
                    const bool wasFirst = first;
                    if (!first) out += ",\n";
                    first = false;
                    return wasFirst;
                };

                const auto Document = [&](const char* text, bool wasFirst) {
                    if (!options.includeHelp || text == nullptr) return;
                    if (!wasFirst) out += "\n";
                    AppendComment(out, text, indent);
                };

                for (const auto& field : fields)
                {
                    if (field.meta.key == nullptr) continue;
                    if (field.ops == nullptr || field.ops->format == nullptr) continue;

                    const std::string_view key{field.meta.key};
                    if (!key.starts_with(prefix)) continue;

                    const std::string_view rest = key.substr(prefix.size());
                    const auto dot = rest.find('.');

                    if (dot == std::string_view::npos)
                    {
                        Document(field.meta.help, Separate());
                        out += indent;
                        AppendJsonString(out, rest);
                        out += ": ";
                        field.ops->format(base, field.boundMemberPtr, out);
                        continue;
                    }

                    const std::string_view segment = rest.substr(0, dot);
                    if (std::find(opened.begin(), opened.end(), segment) != opened.end()) continue;
                    opened.push_back(segment);

                    Separate();
                    out += indent;
                    AppendJsonString(out, segment);
                    out += ": {\n";
                    Level(prefix + std::string{segment} + ".", depth + 1);
                    out += "\n";
                    out += indent;
                    out += "}";
                }

                Carry(prefix, indent, Separate, Document);
            }

            // Emits this level's unclaimed keys, verbatim, after everything the
            // schema owns.
            template <class Separator, class Documenter>
            void Carry(const std::string& prefix,
                       const std::string& indent,
                       const Separator& Separate,
                       const Documenter& Document)
            {
                if (preserved == nullptr) return;

                const auto Emit = [&](const IValueSource& node) {
                    if (!node.IsObject()) return;

                    node.ForEachKey([&](std::string_view key) {
                        if (Claimed(prefix, key)) return;

                        Document(kCarriedNote, Separate());
                        out += indent;
                        AppendJsonString(out, key);
                        out += ": ";
                        // Dump replaces its argument, so the subtree is taken
                        // aside before it joins the document being built.
                        node.Child(key, [&](const IValueSource& child) {
                            std::string verbatim;
                            child.Dump(verbatim);
                            out += verbatim;
                        });
                    });
                };

                if (prefix.empty()) Emit(*preserved);
                else ResolvePath(*preserved, std::string_view{prefix}.substr(0, prefix.size() - 1), false, Emit);
            }
        };
    } // namespace Detail

    // `preserved` is the file being replaced, whose unclaimed keys are carried
    // forward. Pass nothing to render the schema alone.
    template <class T, std::size_t N>
    std::string RenderDocument(const std::array<FieldRuntime, N>& fields,
                               const T& value,
                               const IValueSource* preserved,
                               const WriteOptions& options)
    {
        Detail::Renderer<N> renderer{fields, &value, options, preserved, "{\n"};
        renderer.Level(std::string{}, 1);
        renderer.out += "\n}\n";

        return std::move(renderer.out);
    }

    template <class T, std::size_t N>
    std::string RenderDocument(const std::array<FieldRuntime, N>& fields,
                               const T& value,
                               const WriteOptions& options = {})
    {
        return RenderDocument(fields, value, nullptr, options);
    }

    template <class T, std::size_t N>
    std::string RenderDocument(const std::array<FieldRuntime, N>& fields,
                               const T& value,
                               const IValueSource& preserved,
                               const WriteOptions& options = {})
    {
        return RenderDocument(fields, value, &preserved, options);
    }

} // namespace PalCfg
