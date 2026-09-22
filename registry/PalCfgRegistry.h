#pragma once

/* The one header shared across the DLL edge. Plain C: no C++ type crosses.
 * Every string is UTF-8 and NUL-terminated. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PALCFG_REGISTRY_ABI 1u

#if defined(_WIN32)
#if defined(PALCFG_REGISTRY_EXPORTS)
#define PALCFG_REGISTRY_API __declspec(dllexport)
#else
#define PALCFG_REGISTRY_API __declspec(dllimport)
#endif
#else
#define PALCFG_REGISTRY_API __attribute__((visibility("default")))
#endif

typedef struct PalCfgMod PalCfgMod;
typedef void (*PalCfgFree)(char* text);

typedef struct PalCfgModDesc {
    uint32_t abi;
    const char* name;
    const char* schemaJson;
    void* user;
    char* (*getDocument)(void* user);
    char* (*setDocument)(void* user, const char* json);
    PalCfgFree freeText;
} PalCfgModDesc;

PALCFG_REGISTRY_API uint32_t PalCfgRegistry_Abi(void);
PALCFG_REGISTRY_API PalCfgMod* PalCfgRegistry_Register(const PalCfgModDesc* desc);
PALCFG_REGISTRY_API void PalCfgRegistry_Unregister(PalCfgMod* mod);
PALCFG_REGISTRY_API uint32_t PalCfgRegistry_Count(void);
PALCFG_REGISTRY_API const char* PalCfgRegistry_Name(uint32_t index);
PALCFG_REGISTRY_API const char* PalCfgRegistry_Schema(uint32_t index);
PALCFG_REGISTRY_API char* PalCfgRegistry_GetDocument(uint32_t index);
PALCFG_REGISTRY_API char* PalCfgRegistry_SetDocument(uint32_t index, const char* json);
PALCFG_REGISTRY_API void PalCfgRegistry_Free(char* text);

#ifdef __cplusplus
}
#endif
