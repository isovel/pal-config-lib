#pragma once

// Describes a schema as JSON for a settings menu: keys, labels, help, kinds,
// defaults, ranges and pair membership. Aliases stay out; they are a
// read-side concern.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <PalCfg/Field.hpp>
#include <PalCfg/Write.hpp>

namespace PalCfg
{
    // "bool", "int", "float", "string" or "unknown". Double is a float to a
    // menu, and an enum is edited as its string spelling.
    const char* KindName(Kind kind);

    namespace Detail
    {
        // Everything except "default", which needs the field's Format thunk
        // and the value it formats.
        void AppendFieldHead(std::string& out, const FieldRuntime& field);
        void AppendFieldTail(std::string& out,
                             const FieldRuntime& field,
                             std::string_view pairRole);
    } // namespace Detail

    template <class T, std::size_t N>
    std::string BuildSchemaJson(const char* name, const std::array<FieldRuntime, N>& fields)
    {
        const T defaults{};

        std::string out = R"({"name":)";
        AppendJsonString(out, name != nullptr ? name : "");
        out += R"(,"fields":[)";

        // The upper of a pair is found from its lower, so roles are resolved
        // up front rather than as each field is met.
        std::array<std::string_view, N> roles{};
        for (std::size_t i = 0; i < N; ++i)
        {
            const std::size_t partner = fields[i].meta.pairPartner;
            if (partner == kNoPair) continue;
            roles[i] = "lower";
            roles[partner] = "upper";
        }

        for (std::size_t i = 0; i < N; ++i)
        {
            if (i > 0) out += ',';
            Detail::AppendFieldHead(out, fields[i]);
            out += R"(,"default":)";
            if (fields[i].ops->format != nullptr)
            {
                fields[i].ops->format(&defaults, fields[i].boundMemberPtr, out);
            }
            else
            {
                out += "null";
            }
            Detail::AppendFieldTail(out, fields[i], roles[i]);
        }

        out += "]}";
        return out;
    }
} // namespace PalCfg
