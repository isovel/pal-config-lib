#pragma once

// Registers a mod's ConfigFile with PalCfgRegistry.dll when the menu mod that
// ships it is installed, and does nothing at all when it is absent.
//
// No C++ type crosses the DLL edge: the registry sees three function pointers
// and JSON text. Windows.h stays inside src/win32/Registry.cpp.
//
// SchemaJson.hpp is included so a mod needs one header for schema JSON plus
// the link.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include <PalCfg/ConfigFile.hpp>
#include <PalCfg/Diagnostics.hpp>
#include <PalCfg/SchemaJson.hpp>

namespace PalCfg
{
    inline constexpr std::uint32_t kRegistryAbi = 1;

    // Mirrors PalCfgModDesc in registry/PalCfgRegistry.h field for field. Kept
    // here so a mod needs no header from the registry tree.
    struct RegistryModDesc
    {
        std::uint32_t abi = kRegistryAbi;
        const char* name = nullptr;
        const char* schemaJson = nullptr;
        void* user = nullptr;
        char* (*getDocument)(void* user) = nullptr;
        char* (*setDocument)(void* user, const char* json) = nullptr;
        void (*freeText)(char* text) = nullptr;
    };

    static_assert(std::is_standard_layout_v<RegistryModDesc> && sizeof(RegistryModDesc) == 7 * sizeof(void*),
                  "RegistryModDesc must mirror PalCfgModDesc");

    struct RegistryExports
    {
        std::uint32_t (*abi)() = nullptr;
        void* (*registerMod)(const void* desc) = nullptr;
        void (*unregisterMod)(void* mod) = nullptr;
    };

    // Takes a reference on the already-loaded library, else loads it from
    // `relativeToModule` against the module holding `addressInModule`. False
    // when absent or on ABI mismatch, leaving `out` empty and no reference
    // held.
    bool ProbeRegistry(const void* addressInModule,
                       std::string_view relativeToModule,
                       RegistryExports& out,
                       void*& libraryHandle);

    // Releases the reference ProbeRegistry took; null is a no-op.
    void ReleaseRegistry(void* libraryHandle);

    // Destroy this from the mod's unload path (a UE4SS CppUserModBase
    // destructor or on_unload), before the ConfigFile it points at, and never
    // from a static destructor: a namespace-scope static runs at
    // DLL_PROCESS_DETACH, where FreeLibrary is unsafe and the registry DLL may
    // already be gone. Declare it after the ConfigFile.
    template <class T, std::size_t N>
    class RegistryLink
    {
      public:
        using File = ConfigFile<T, N>;

        RegistryLink(const void* addressInModule, std::string_view relativeToModule)
        {
            m_attached = ProbeRegistry(addressInModule, relativeToModule, m_exports, m_library);
        }

        // Unregisters, then releases its reference on the registry library.
        ~RegistryLink()
        {
            if (m_registration != nullptr) m_exports.unregisterMod(m_registration);
            ReleaseRegistry(m_library);
        }

        RegistryLink(const RegistryLink&) = delete;
        RegistryLink& operator=(const RegistryLink&) = delete;

        bool Attached() const { return m_attached && m_registration != nullptr; }

        // `schemaJson` MUST outlive this link; a static built from
        // BuildSchemaJson at startup is the expected source.
        void Attach(File& file, const char* schemaJson)
        {
            if (m_registration != nullptr) return;

            if (!m_attached)
            {
                file.ReportNote("no settings registry; menu integration is off");
                return;
            }

            const char* name = ModName(file);
            if (name == nullptr || *name == '\0')
            {
                file.ReportNote("no mod id on the ConfigFile; menu integration is off");
                return;
            }

            m_desc.name = name;
            m_desc.schemaJson = schemaJson;
            m_desc.user = &file;
            m_desc.getDocument = &GetDocumentThunk;
            m_desc.setDocument = &SetDocumentThunk;
            m_desc.freeText = &FreeThunk;

            m_registration = m_exports.registerMod(&m_desc);
            if (m_registration == nullptr)
            {
                file.ReportNote("the settings registry refused this mod; menu integration is off");
            }
        }

        // The registry calls these from extern "C" under its mutex, so no
        // exception may cross.
        static char* GetDocumentThunk(void* user)
        {
            try
            {
                return Copy(static_cast<File*>(user)->Document());
            }
            catch (...)
            {
                return nullptr;
            }
        }

        static char* SetDocumentThunk(void* user, const char* json)
        {
            try
            {
                auto& file = *static_cast<File*>(user);
                return Copy(DiagnosticsToJson(file.ApplyDocument(json != nullptr ? json : "")));
            }
            catch (...)
            {
                try
                {
                    return Copy(DiagnosticsToJson(
                        {{Severity::Error, {}, "the mod threw while applying the document"}}));
                }
                catch (...)
                {
                    return nullptr;
                }
            }
        }

        static void FreeThunk(char* text) { delete[] text; }

      private:
        static char* Copy(const std::string& text)
        {
            char* out = new char[text.size() + 1];
            std::memcpy(out, text.c_str(), text.size() + 1);
            return out;
        }

        static const char* ModName(const File& file) { return file.ModId(); }

        bool m_attached = false;
        RegistryExports m_exports;
        void* m_library = nullptr;
        void* m_registration = nullptr;
        RegistryModDesc m_desc;
    };
} // namespace PalCfg
