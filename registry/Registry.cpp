#include "PalCfgRegistry.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Registered mods, in registration order. One mutex: everything here is a
// short table operation on the game thread.

struct PalCfgMod
{
    std::string name;
    std::string schema;
    void* user = nullptr;
    char* (*getDocument)(void*) = nullptr;
    char* (*setDocument)(void*, const char*) = nullptr;
    PalCfgFree freeText = nullptr;
};

namespace
{
    struct State
    {
        std::mutex mutex;
        std::vector<std::unique_ptr<PalCfgMod>> mods;
    };

    // Never destroyed, so a mod unregistering during process teardown finds a
    // live table.
    State& Table()
    {
        static State& state = *new State{};
        return state;
    }

    constexpr const char* kNothing =
        R"([{"severity":"Error","field":"","message":"mod returned nothing"}])";

    char* Copy(const char* text)
    {
        const std::size_t size = std::strlen(text) + 1;
        char* out = new char[size];
        std::memcpy(out, text, size);
        return out;
    }

    // Copies a mod's answer into registry memory, then hands the mod's buffer
    // back to the mod's own allocator. The mutex stays held across this
    // callback deliberately: a mod unregistering from inside its own callback
    // would deadlock, and none has a reason to.
    char* Take(const PalCfgMod& mod, char* text)
    {
        if (text == nullptr) return Copy(kNothing);
        char* out = Copy(text);
        mod.freeText(text);
        return out;
    }

    PalCfgMod* At(uint32_t index)
    {
        auto& mods = Table().mods;
        return index < mods.size() ? mods[index].get() : nullptr;
    }
}

extern "C"
{
    uint32_t PalCfgRegistry_Abi(void)
    {
        return PALCFG_REGISTRY_ABI;
    }

    PalCfgMod* PalCfgRegistry_Register(const PalCfgModDesc* desc)
    {
        if (desc == nullptr || desc->abi != PALCFG_REGISTRY_ABI) return nullptr;
        if (desc->name == nullptr || desc->schemaJson == nullptr) return nullptr;
        if (desc->getDocument == nullptr || desc->setDocument == nullptr || desc->freeText == nullptr)
        {
            return nullptr;
        }

        State& state = Table();
        std::lock_guard lock(state.mutex);
        for (const auto& mod : state.mods)
        {
            if (mod->name == desc->name) return nullptr;
        }

        auto mod = std::make_unique<PalCfgMod>();
        mod->name = desc->name;
        mod->schema = desc->schemaJson;
        mod->user = desc->user;
        mod->getDocument = desc->getDocument;
        mod->setDocument = desc->setDocument;
        mod->freeText = desc->freeText;

        PalCfgMod* handle = mod.get();
        state.mods.push_back(std::move(mod));
        return handle;
    }

    void PalCfgRegistry_Unregister(PalCfgMod* mod)
    {
        if (mod == nullptr) return;

        State& state = Table();
        std::lock_guard lock(state.mutex);
        for (auto it = state.mods.begin(); it != state.mods.end(); ++it)
        {
            if (it->get() == mod)
            {
                state.mods.erase(it);
                return;
            }
        }
    }

    uint32_t PalCfgRegistry_Count(void)
    {
        State& state = Table();
        std::lock_guard lock(state.mutex);
        return static_cast<uint32_t>(state.mods.size());
    }

    const char* PalCfgRegistry_Name(uint32_t index)
    {
        State& state = Table();
        std::lock_guard lock(state.mutex);
        const PalCfgMod* mod = At(index);
        return mod != nullptr ? mod->name.c_str() : nullptr;
    }

    const char* PalCfgRegistry_Schema(uint32_t index)
    {
        State& state = Table();
        std::lock_guard lock(state.mutex);
        const PalCfgMod* mod = At(index);
        return mod != nullptr ? mod->schema.c_str() : nullptr;
    }

    char* PalCfgRegistry_GetDocument(uint32_t index)
    {
        State& state = Table();
        std::lock_guard lock(state.mutex);
        const PalCfgMod* mod = At(index);
        if (mod == nullptr) return nullptr;
        return Take(*mod, mod->getDocument(mod->user));
    }

    char* PalCfgRegistry_SetDocument(uint32_t index, const char* json)
    {
        if (json == nullptr) return nullptr;

        State& state = Table();
        std::lock_guard lock(state.mutex);
        const PalCfgMod* mod = At(index);
        if (mod == nullptr) return nullptr;
        return Take(*mod, mod->setDocument(mod->user, json));
    }

    void PalCfgRegistry_Free(char* text)
    {
        delete[] text;
    }
}
