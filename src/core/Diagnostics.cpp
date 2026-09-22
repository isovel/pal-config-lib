// Message formatting for the reporting helpers the traits call, kept out of
// consumer translation units.

#include <PalCfg/Diagnostics.hpp>

#include <array>
#include <charconv>
#include <string>

#include <PalCfg/Value.hpp>
#include <PalCfg/Write.hpp>

namespace PalCfg
{
    const char* SeverityName(Severity severity)
    {
        switch (severity)
        {
            case Severity::Note: return "Note";
            case Severity::Warning: return "Warning";
            case Severity::Error: return "Error";
        }
        return "Note";
    }

    std::string DiagnosticsToJson(const std::vector<Diagnostic>& diagnostics)
    {
        std::string out = "[";
        for (std::size_t i = 0; i < diagnostics.size(); ++i)
        {
            if (i > 0) out += ',';
            out += R"({"severity":")";
            out += SeverityName(diagnostics[i].severity);
            out += R"(","field":)";
            AppendJsonString(out, diagnostics[i].field);
            out += R"(,"message":)";
            AppendJsonString(out, diagnostics[i].message);
            out += '}';
        }
        out += ']';
        return out;
    }
} // namespace PalCfg

namespace PalCfg::Detail
{
    namespace
    {
        // Shortest round-trip form, so a bound of 1.0 reads as "1" in a message.
        std::string Format(double value)
        {
            std::array<char, 32> buffer{};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (result.ec != std::errc{}) return "?";

            return std::string(buffer.data(), result.ptr);
        }
    } // namespace

    const char* TypeName(const IValueSource& source)
    {
        if (source.IsNull()) return "null";
        if (source.IsBool()) return "a bool";
        if (source.IsNumber()) return "a number";
        if (source.IsString()) return "text";
        if (source.IsArray()) return "a list";
        if (source.IsObject()) return "an object";
        return "an unknown value";
    }

    double Clamp(double value, const ReadContext& context)
    {
        if (!context.meta.hasRange) return value;
        if (value >= context.meta.min && value <= context.meta.max) return value;

        const double bound = value < context.meta.min ? context.meta.min : context.meta.max;

        context.diag.Report({Severity::Warning,
                             std::string{context.key},
                             Format(value) + " is outside " + Format(context.meta.min) + " to " +
                                 Format(context.meta.max) + "; using " + Format(bound)});

        return bound;
    }

    void NoteCoercion(const ReadContext& context, const IValueSource& source, const char* target)
    {
        context.diag.Report({Severity::Note,
                             std::string{context.key},
                             std::string{"read "} + TypeName(source) + " as " + target});
    }

    void WarnUnreadable(const ReadContext& context, const IValueSource& source)
    {
        context.diag.Report({Severity::Warning,
                             std::string{context.key},
                             std::string{"cannot read "} + TypeName(source) +
                                 " as this setting; keeping the default"});
    }
} // namespace PalCfg::Detail
