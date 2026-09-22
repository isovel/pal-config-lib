#include <PalCfg/Registry.hpp>

#include <string>

#include <PalCfg/Utf.hpp>
#include <PalCfg/Win32.hpp>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace PalCfg
{
    namespace
    {
        constexpr const wchar_t* kLibraryName = L"PalCfgRegistry.dll";

        bool Resolve(HMODULE module, RegistryExports& out)
        {
            out.abi = reinterpret_cast<std::uint32_t (*)()>(GetProcAddress(module, "PalCfgRegistry_Abi"));
            out.registerMod =
                reinterpret_cast<void* (*)(const void*)>(GetProcAddress(module, "PalCfgRegistry_Register"));
            out.unregisterMod =
                reinterpret_cast<void (*)(void*)>(GetProcAddress(module, "PalCfgRegistry_Unregister"));

            if (out.abi == nullptr || out.registerMod == nullptr || out.unregisterMod == nullptr) return false;
            return out.abi() == kRegistryAbi;
        }
    } // namespace

    bool ProbeRegistry(const void* addressInModule,
                       std::string_view relativeToModule,
                       RegistryExports& out,
                       void*& libraryHandle)
    {
        out = {};
        libraryHandle = nullptr;

        // Already in the process: another mod or the menu loaded it first.
        HMODULE module = GetModuleHandleW(kLibraryName);
        if (module != nullptr)
        {
            if (Resolve(module, out)) return true;
            out = {};
            return false;
        }

        std::string path;
        if (!ModuleRelativePath(addressInModule, relativeToModule, path)) return false;

        module = LoadLibraryW(Utf8ToWide(path).c_str());
        if (module == nullptr) return false;

        if (!Resolve(module, out))
        {
            FreeLibrary(module);
            out = {};
            return false;
        }

        libraryHandle = module;
        return true;
    }

    void ReleaseRegistry(void* libraryHandle)
    {
        if (libraryHandle != nullptr) FreeLibrary(static_cast<HMODULE>(libraryHandle));
    }
} // namespace PalCfg
