#include <PalCfg/File.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace PalCfg
{
    namespace
    {
        namespace fs = std::filesystem;
    }

    bool ReadFileText(const std::string& path, std::string& out)
    {
        out.clear();

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        out.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        return !file.bad();
    }

    WriteFileResult WriteFileIfChanged(const std::string& path, std::string_view content)
    {
        WriteFileResult result;

        std::string existing;
        const bool exists = ReadFileText(path, existing);
        if (exists && existing == content) return result;

        const fs::path target{path};
        std::error_code code;
        fs::create_directories(target.parent_path(), code);

        const fs::path temporary = fs::path{path + ".tmp"};
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file.is_open())
            {
                result.error = "cannot write " + temporary.string();
                return result;
            }
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!file)
            {
                result.error = "cannot write " + temporary.string();
                return result;
            }
        }

        if (exists)
        {
            fs::rename(target, fs::path{path + ".bak"}, code);
            if (code)
            {
                result.error = "cannot keep a backup of " + path + ": " + code.message();
                fs::remove(temporary, code);
                return result;
            }
            result.backedUp = true;
        }

        fs::rename(temporary, target, code);
        if (code)
        {
            result.error = "cannot replace " + path + ": " + code.message();
            fs::remove(temporary, code);
            return result;
        }

        result.written = true;
        return result;
    }
} // namespace PalCfg
