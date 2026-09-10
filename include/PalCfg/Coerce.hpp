#pragma once

// Reading a value as a type the document did not quite use.
//
// Config files are hand-edited, so `"enabled": "true"` and `"count": "42"` turn
// up constantly. dynamic-pals' SafeGetInt/SafeGetDouble/SafeGetOptionalBool and
// iaho's IsTruthy each solved this separately; these are the union of both.
//
// Declared here and defined in src/core/Coerce.cpp, so the parsing lives in the
// static library and every consumer's translation unit stays free of it.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include <PalCfg/Value.hpp>

namespace PalCfg::Detail
{
    // Each returns false when nothing sensible can be read, leaving `out` alone.
    bool CoerceBool(const IValueSource& source, bool& out);
    bool CoerceInt64(const IValueSource& source, std::int64_t& out);
    bool CoerceDouble(const IValueSource& source, double& out);
    bool CoerceUtf8(const IValueSource& source, std::string& out);

    bool EqualsIgnoringCase(std::string_view a, std::string_view b);

    // Calls `visit` with a source holding `text` as a string, so a token split out
    // of a separated list can be read by the element's own traits.
    void WithStringSource(std::string_view text, const IValueSource::Visitor& visit);

    // Splits on ',' and ';', trimming each token and skipping empty ones. iaho
    // spelled its id lists this way, so its config.ini values load verbatim.
    void ForEachSeparatedToken(std::string_view text,
                               const std::function<void(std::string_view)>& visit);
} // namespace PalCfg::Detail
