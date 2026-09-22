#include <PalCfg/Registry.hpp>

// There is no registry DLL to find on a host build; a mod compiled here is
// always detached.
namespace PalCfg
{
    bool ProbeRegistry(const void*, std::string_view, RegistryExports& out, void*& libraryHandle)
    {
        out = {};
        libraryHandle = nullptr;
        return false;
    }

    void ReleaseRegistry(void*) {}
} // namespace PalCfg
