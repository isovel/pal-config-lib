#pragma once

// Makes a hand-edited config file parseable.
//
// nlohmann accepts // and /* */ comments with ignore_comments, and it rejects a
// trailing comma outright. A user who adds one gets a document-level parse
// failure and silently falls back to every default - a harsh outcome for a
// character that most other config formats permit. SanitiseJsonc removes them,
// along with the UTF-8 BOM that Windows editors prepend.
//
// Pure string work, so this header stays free of json.hpp.

#include <cstddef>
#include <string>

namespace PalCfg
{
    struct SanitiseResult
    {
        std::size_t trailingCommasRemoved = 0;
        bool bomRemoved = false;
    };

    // Rewrites `text` in place.
    SanitiseResult SanitiseJsonc(std::string& text);
} // namespace PalCfg
