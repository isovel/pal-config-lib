#include <PalCfg/Paths.hpp>

#include <cctype>
#include <filesystem>

namespace PalCfg
{
    namespace
    {
        // std::filesystem answers this for the host it is compiled on, so a Linux
        // build of a generator tool would treat "D:/mods" as a relative name. A
        // config path names a Windows file whichever machine is reading it.
        bool IsAbsolute(std::string_view path)
        {
            if (path.empty()) return false;
            if (path.front() == '/' || path.front() == '\\') return true;

            return path.size() >= 2 && path[1] == ':' &&
                   std::isalpha(static_cast<unsigned char>(path.front())) != 0;
        }
    } // namespace

    std::string ResolveAgainst(std::string_view directory, std::string_view relative)
    {
        namespace fs = std::filesystem;

        const fs::path target{relative};
        const fs::path resolved =
            IsAbsolute(relative) || directory.empty() ? target : fs::path{directory} / target;

        return resolved.lexically_normal().generic_string();
    }
} // namespace PalCfg
