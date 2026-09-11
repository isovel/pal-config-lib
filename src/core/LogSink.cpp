#include <PalCfg/LogSink.hpp>

namespace PalCfg
{
    namespace
    {
        // Wide enough for the keys these mods use, and never zero, so a longer key
        // still reads as a key followed by a message.
        constexpr std::size_t kKeyColumn = 23;

        const char* Label(Severity severity)
        {
            switch (severity)
            {
                case Severity::Warning: return "WARN ";
                case Severity::Error: return "ERROR";
                case Severity::Note: break;
            }
            return "NOTE ";
        }
    } // namespace

    std::string FormatDiagnostic(const Diagnostic& diagnostic)
    {
        std::string line = Label(diagnostic.severity);
        line += ' ';

        const std::size_t start = line.size();
        line += '[';
        line += diagnostic.field;
        line += ']';

        const std::size_t width = line.size() - start;
        line.append(width < kKeyColumn ? kKeyColumn - width : 1, ' ');

        line += diagnostic.message;
        return line;
    }
} // namespace PalCfg
