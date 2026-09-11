#pragma once

// Routing diagnostics into a mod's own log.
//
// Every mod here already decides where its config messages go, and all of them
// gate the informational stream behind a debug flag while letting warnings and
// errors through unconditionally. CallbackSink is that policy, so a mod supplies
// one function and keeps its own logging.

#include <functional>
#include <string>
#include <utility>

#include <PalCfg/Diagnostics.hpp>

namespace PalCfg
{
    // "WARN  [arousalRate]          7 is outside 0 to 1; using 1"
    //
    // The key sits in a fixed column, so a run of messages reads as a table. A
    // key too long for the column is followed by a single space.
    std::string FormatDiagnostic(const Diagnostic& diagnostic);

    class CallbackSink final : public IDiagnosticSink
    {
      public:
        using Callback = std::function<void(Severity, const std::string&)>;

        explicit CallbackSink(Callback callback) : m_callback(std::move(callback)) {}

        // Anything below this is dropped. Note by default, so nothing is lost
        // until a mod asks for quiet.
        void SetMinimumSeverity(Severity severity) { m_minimum = severity; }

        void Report(const Diagnostic& diagnostic) override
        {
            if (diagnostic.severity < m_minimum) return;
            if (!m_callback) return;

            m_callback(diagnostic.severity, FormatDiagnostic(diagnostic));
        }

      private:
        Callback m_callback;
        Severity m_minimum = Severity::Note;
    };
} // namespace PalCfg
