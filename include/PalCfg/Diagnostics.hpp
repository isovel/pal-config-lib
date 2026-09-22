#pragma once

// What went wrong, and how badly.
//
// The severities carry a deliberate policy. A coercion that succeeded is a Note:
// reading "42" as a number is what a hand-edited file looks like, and warning
// about it trains users to ignore warnings. A value that had to be discarded or
// changed is a Warning. Only a document that cannot be read at all is an Error.
//
// A mod installs its own sink to route these. PerkyPals and dynamic-pals both
// send warnings and errors to an in-game toast as well as the log, and both gate
// the informational stream behind a debug flag. That decision stays with the mod:
// this library never reads a mod's log settings.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <PalCfg/Field.hpp>

namespace PalCfg
{
    enum class Severity
    {
        // Worth knowing, nothing lost. A coercion that worked, a stray comma
        // removed, a key nobody claims.
        Note,

        // Something was discarded or altered. A field kept its default, or a value
        // was clamped to its bound.
        Warning,

        // The document could not be read, so nothing at all was extracted.
        Error,
    };

    struct Diagnostic
    {
        Severity severity = Severity::Note;

        // Dotted key of the field concerned. Empty for a document-level report.
        std::string field;
        std::string message;
    };

    class IDiagnosticSink
    {
      public:
        virtual ~IDiagnosticSink() = default;
        virtual void Report(const Diagnostic& diagnostic) = 0;
    };

    // Discards everything. The default, so a caller that does not care about
    // diagnostics writes no extra code.
    class NullSink final : public IDiagnosticSink
    {
      public:
        void Report(const Diagnostic&) override {}
    };

    // Keeps everything in order. Useful in tests, and for a mod that wants to
    // decide what to show after seeing the whole picture.
    class CollectingSink final : public IDiagnosticSink
    {
      public:
        void Report(const Diagnostic& diagnostic) override { m_all.push_back(diagnostic); }

        const std::vector<Diagnostic>& All() const { return m_all; }

        std::size_t Count(Severity severity) const
        {
            std::size_t total = 0;
            for (const auto& diagnostic : m_all)
            {
                if (diagnostic.severity == severity) ++total;
            }
            return total;
        }

        bool Has(Severity severity) const { return Count(severity) > 0; }

        void Clear() { m_all.clear(); }

      private:
        std::vector<Diagnostic> m_all;
    };

    // "Note", "Warning" or "Error".
    const char* SeverityName(Severity severity);

    // A JSON array of {"severity","field","message"} objects; "[]" when empty.
    std::string DiagnosticsToJson(const std::vector<Diagnostic>& diagnostics);

    // Everything a field's traits need while reading. One struct, so adding to it
    // later leaves every ValueTraits specialisation compiling.
    struct ReadContext
    {
        const FieldMeta& meta;

        // The dotted key this value came from. Reported in diagnostics, so a
        // message names the setting a user recognises.
        std::string_view key;

        IDiagnosticSink& diag;
    };

    namespace Detail
    {
        // Reporting helpers, defined in src/core/Diagnostics.cpp so the message
        // formatting stays out of every consumer's translation unit.

        // Reports a Warning naming the bound that won, since a clamp changed the
        // number the user wrote.
        double Clamp(double value, const ReadContext& context);

        // A coercion that worked is a Note: reading "42" as a number is what a
        // hand-edited file looks like, and warning about it teaches users to
        // ignore warnings.
        void NoteCoercion(const ReadContext& context, const IValueSource& source, const char* target);

        // Reports that a field kept its default because nothing could be read.
        void WarnUnreadable(const ReadContext& context, const IValueSource& source);

        // "text", "a number", "a bool", "a list", "an object" or "null", for
        // messages.
        const char* TypeName(const IValueSource& source);
    } // namespace Detail

    struct LoadResult
    {
        // False when the document could not be parsed, in which case nothing was
        // extracted and a caller MUST keep whatever settings it already had.
        bool parsed = false;

        std::size_t fieldsLoaded = 0;

        // Keys the schema does not claim, as dotted paths, in whatever order the
        // document exposes them. nlohmann keeps object members sorted, so in
        // practice that is alphabetical. Kept
        // so the writer can carry them forward, letting a downgrade or a future
        // version still read its own settings.
        std::vector<std::string> unknownKeys;

        bool Ok() const { return parsed; }
    };
} // namespace PalCfg
