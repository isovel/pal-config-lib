#pragma once

// Working out where a config file lives, without std::filesystem in a header.

#include <string>
#include <string_view>

namespace PalCfg
{
    // `relative` against `directory`, normalised. An absolute `relative` wins.
    // Separators come back as forward slashes, which every Windows API accepts.
    std::string ResolveAgainst(std::string_view directory, std::string_view relative);
} // namespace PalCfg
