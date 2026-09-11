#pragma once

// Finding a mod's own files on Windows.
//
// A UE4SS mod's DLL is loaded at a path the mod itself does not choose, so the
// config file is located from the DLL rather than from a working directory or
// the engine. That works before on_unreal_init and needs no reflection, which is
// how PerkyPals resolves its own config today.
//
// Compiled only on Windows. Windows.h stays inside src/win32/Module.cpp.

#include <string>
#include <string_view>

namespace PalCfg
{
    // The folder holding the DLL that `addressInModule` belongs to, as UTF-8 with
    // forward slashes. Pass the address of one of your own functions. Returns
    // false when the module cannot be identified, leaving `out` empty.
    bool ModuleDirectory(const void* addressInModule, std::string& out);

    // `relative` resolved against that folder. A UE4SS mod whose main.dll sits in
    // <mod>/dlls/ asks for "../config.json".
    bool ModuleRelativePath(const void* addressInModule, std::string_view relative, std::string& out);
} // namespace PalCfg
