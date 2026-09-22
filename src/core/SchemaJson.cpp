#include <PalCfg/SchemaJson.hpp>

namespace PalCfg
{
    const char* KindName(Kind kind)
    {
        switch (kind)
        {
            case Kind::Bool: return "bool";
            case Kind::Int: return "int";
            case Kind::Float:
            case Kind::Double: return "float";
            case Kind::String:
            case Kind::Enum: return "string";
            default: return "unknown";
        }
    }

    namespace Detail
    {
        namespace
        {
            void AppendKind(std::string& out, const FieldOps& ops)
            {
                if (ops.kind == Kind::List || ops.kind == Kind::Set)
                {
                    out += "list<";
                    out += KindName(ops.elementKind);
                    out += '>';
                    return;
                }
                out += KindName(ops.kind);
            }
        } // namespace

        void AppendFieldHead(std::string& out, const FieldRuntime& field)
        {
            const FieldMeta& meta = field.meta;

            out += R"({"key":)";
            AppendJsonString(out, meta.key != nullptr ? meta.key : "");
            out += R"(,"label":)";
            AppendJsonString(out, meta.label != nullptr ? meta.label : "");
            out += R"(,"help":)";
            AppendJsonString(out, meta.help != nullptr ? meta.help : "");
            out += R"(,"kind":")";
            AppendKind(out, *field.ops);
            out += '"';
        }

        void AppendFieldTail(std::string& out, const FieldRuntime& field, std::string_view pairRole)
        {
            const FieldMeta& meta = field.meta;

            if (meta.hasRange)
            {
                out += R"(,"min":)";
                AppendJsonNumber(out, meta.min, true);
                out += R"(,"max":)";
                AppendJsonNumber(out, meta.max, true);
            }

            out += R"(,"hidden":)";
            out += meta.hidden ? "true" : "false";
            out += R"(,"advanced":)";
            out += meta.advanced ? "true" : "false";

            if (!pairRole.empty())
            {
                out += R"(,"pair":")";
                out += pairRole;
                out += '"';
            }

            out += '}';
        }
    } // namespace Detail
} // namespace PalCfg
