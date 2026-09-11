#include <PalCfg/Win32.hpp>

#include <vector>

#include <PalCfg/Paths.hpp>
#include <PalCfg/Utf.hpp>

#include <Windows.h>

namespace PalCfg
{
    namespace
    {
        // GetModuleFileNameW truncates rather than failing, so the buffer grows
        // until the name fits. A mod installed under a deep path is ordinary.
        bool ModuleFileName(HMODULE module, std::wstring& out)
        {
            std::vector<wchar_t> buffer(MAX_PATH);
            for (;;)
            {
                const DWORD written =
                    GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (written == 0) return false;

                if (written < buffer.size())
                {
                    out.assign(buffer.data(), written);
                    return true;
                }

                if (buffer.size() >= 32768) return false;
                buffer.resize(buffer.size() * 2);
            }
        }
    } // namespace

    bool ModuleDirectory(const void* addressInModule, std::string& out)
    {
        out.clear();

        HMODULE module = nullptr;
        const DWORD flags =
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
        if (GetModuleHandleExW(flags, static_cast<LPCWSTR>(addressInModule), &module) == 0) return false;
        if (module == nullptr) return false;

        std::wstring wide;
        if (!ModuleFileName(module, wide)) return false;

        const std::string path = WideToUtf8(wide);
        const auto slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return false;

        out = path.substr(0, slash);
        for (char& c : out)
        {
            if (c == '\\') c = '/';
        }

        return true;
    }

    bool ModuleRelativePath(const void* addressInModule, std::string_view relative, std::string& out)
    {
        std::string directory;
        if (!ModuleDirectory(addressInModule, directory)) return false;

        out = ResolveAgainst(directory, relative);
        return true;
    }
} // namespace PalCfg
