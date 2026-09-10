#pragma once

// UTF-8 to and from the platform's wchar_t.
//
// Config files are UTF-8; UE4SS and the Unreal API speak wide strings. The
// conversion between them has to be real. PerkyPals' Widen did
// std::wstring(s.begin(), s.end()), which copies each BYTE into its own wchar_t,
// so any non-ASCII morph target or action name silently became mojibake.
//
// wchar_t is 16 bits on Windows and 32 on Linux, so these produce UTF-16 or
// UTF-32 to match the platform. Code points outside the BMP therefore yield a
// surrogate pair on Windows and a single unit elsewhere.

#include <string>
#include <string_view>

namespace PalCfg
{
    // Malformed input yields U+FFFD REPLACEMENT CHARACTER for each bad sequence,
    // so a corrupt file still loads with a visible marker.
    std::wstring Utf8ToWide(std::string_view utf8);
    std::string WideToUtf8(std::wstring_view wide);
} // namespace PalCfg
