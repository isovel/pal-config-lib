# Registry design

Status: approved 2026-09-21. Next stage after the PerkyPals migration.

## Purpose

An in-game settings menu enumerates every mod built on PalCfg, shows each mod's
fields with labels, help and ranges, edits values, and has the owning mod write
its own `config.json`. The menu and the mods do not share a compiler, so the
only thing crossing a DLL edge is a C function signature carrying UTF-8 JSON
text.

## Decisions

| Decision | Choice |
| --- | --- |
| Granularity | Whole document. The menu edits a JSON document and commits it once. Per-field set can be layered later inside the mod with no ABI change. |
| Discovery | The menu mod ships `PalCfgRegistry.dll`. A mod probes `GetModuleHandleW`, then `LoadLibraryW` on `<own module>/../../PalConfigMenu/dlls/PalCfgRegistry.dll`, and no-ops when absent. |
| Threading | Menu and mods run on the game thread. Callbacks run synchronously on the caller's thread. The registry table is guarded by one mutex. |
| Allocation | No allocator crosses the edge. The registry copies mod strings into its own heap; `PalCfgRegistry_Free` frees registry memory, `freeText` frees mod memory. |
| Dependencies | The registry DLL depends on nothing: no PalCfg core, no nlohmann, no UE4SS. |

## The C ABI

`registry/PalCfgRegistry.h` is the only shared header. Plain C.

```c
#define PALCFG_REGISTRY_ABI 1

typedef struct PalCfgMod PalCfgMod;                 // opaque handle
typedef void (*PalCfgFree)(char* text);             // frees a string the mod allocated

typedef struct PalCfgModDesc {
    uint32_t    abi;            // PALCFG_REGISTRY_ABI
    const char* name;           // "PerkyPals"; matches Schema(...)'s name
    const char* schemaJson;     // static, lives as long as the registration
    void*       user;
    char* (*getDocument)(void* user);                    // current settings as JSON
    char* (*setDocument)(void* user, const char* json);  // returns diagnostics JSON
    PalCfgFree  freeText;
} PalCfgModDesc;

uint32_t     PalCfgRegistry_Abi(void);
PalCfgMod*   PalCfgRegistry_Register(const PalCfgModDesc*);
void         PalCfgRegistry_Unregister(PalCfgMod*);
uint32_t     PalCfgRegistry_Count(void);
const char*  PalCfgRegistry_Name(uint32_t index);
const char*  PalCfgRegistry_Schema(uint32_t index);
char*        PalCfgRegistry_GetDocument(uint32_t index);
char*        PalCfgRegistry_SetDocument(uint32_t index, const char* json);
void         PalCfgRegistry_Free(char*);
```

Rules:

- Every string is UTF-8 and NUL-terminated.
- `Register` returns null on ABI mismatch, null `name` or `schemaJson`,
  duplicate name, or a null `getDocument`, `setDocument` or `freeText`.
- `Name` and `Schema` return pointers valid until the entry is unregistered.
  `GetDocument` and `SetDocument` return registry-owned copies the caller frees
  with `PalCfgRegistry_Free`.
- `SetDocument` always returns a diagnostics array, `[]` on clean success. The
  mod applies the document only when the array holds no `Error`.
- Indices are unstable across `Register` and `Unregister`. The menu MUST
  re-enumerate each time it opens. An out-of-range index returns null from
  every accessor.
- A mod callback that returns null surfaces as
  `[{"severity":"Error","field":"","message":"mod returned nothing"}]`.

## Schema JSON

Generated at compile time from `FieldRuntime[]` by `BuildSchemaJson(fields)` in
`core/`, held as a static string in the mod.

```jsonc
{
  "name": "PerkyPals",
  "fields": [
    {
      "key": "arousalRate",
      "label": "Arousal rate",
      "help": "Weight per second while rising",
      "kind": "float",            // bool | int | float | string | list<int> | list<string>
      "default": 0.08,            // emitted from T{} by the field's Write thunk
      "min": 0.0, "max": 1.0,     // present only when hasRange
      "hidden": false,
      "advanced": false,
      "pair": "lower"             // "lower" | "upper"; present only for a FieldPair
    }
  ]
}
```

- `kind` derives from `FieldOps::kind`. `Float` and `Double` both emit `float`.
  `List` and `Set` emit `list<element>`, which needs an `elementKind` on
  `FieldOps`; this stage adds it.
- `pair` on the lower field comes from `pairPartner`; the partner is marked
  `upper`. `SwapIfInverted` still runs inside the mod on commit.
- Aliases and `keepDefaultIfEmpty` stay out; they are read-side concerns.

Document JSON is the config file content without comments: the writer emits it
with `WriteOptions{.includeHelp = false}`, and `SanitiseJsonc` reads it.

## Mod side

`include/PalCfg/Registry.hpp`, header-only, Win32 only.

```cpp
PalCfg::RegistryLink Link;                  // next to the ConfigFile
Link.Attach(*ConfigFile, kSchemaJson);      // after LoadOrCreate()
```

`RegistryLink`:

- The constructor probes `GetModuleHandleW(L"PalCfgRegistry.dll")`, then
  `LoadLibraryW` on the path resolved by `ModuleRelativePath`. Absent or ABI
  mismatch leaves it detached, reports a `Note` through the mod's sink, and
  every later call is a no-op.
- `Attach<T, N>` fills `PalCfgModDesc` with two static thunks over
  `ConfigFile<T, N>`, `user` = the `ConfigFile*`, `freeText` = a `delete[]`
  thunk, and calls `Register`.
- The destructor calls `Unregister`, then `FreeLibrary` when the link loaded
  the DLL. A UE4SS mod restart therefore leaves no dangling entry.
- The mod destroys `Link` from its own unload path (a `CppUserModBase`
  destructor or `on_unload`), before it destroys the `ConfigFile`, and never
  from a static destructor: that runs at `DLL_PROCESS_DETACH`, where
  `FreeLibrary` is unsafe and the registry DLL may already be gone.

`ConfigFile` gains one method:

```cpp
// Parses `json` as a full document, saves it, puts it live and fires the
// reload callback. Returns the diagnostics; nothing is written on an Error.
std::vector<Diagnostic> ApplyDocument(std::string json);
```

PerkyPals MUST route `Publish()` through `ConfigFile->SetOnReload` so a menu
commit republishes; that edit lands in the mod alongside this stage.

## Registry DLL

`registry/Registry.cpp` plus the header. Pure C++23: a `std::mutex` and a
`std::vector<Entry>` where `Entry` copies `name` and `schemaJson` into
`std::string` at register time and keeps the callbacks and `user`. Exported
functions copy mod strings into registry-owned buffers, call `freeText` on the
mod's buffer, and return the copy.

Built as `SHARED` under `PALCFG_BUILD_REGISTRY`, off by default. The host-native
Linux build produces a `.so` for tests.

## Errors

- Registration failures return null and leave the table unchanged.
- Out-of-range indices return null from every accessor.
- Every exported call re-checks its index under the mutex, so a mod that
  unregistered while the menu held an index yields null.
- `ApplyDocument` on a document with an `Error` writes nothing and leaves the
  live snapshot unchanged.

## Tests

All host-native on Linux.

- `core`: `BuildSchemaJson` on the PerkyPals mirror schema matches a checked-in
  fixture. `ApplyDocument` round-trips a document, refuses one with an `Error`,
  fires the reload callback, and preserves unclaimed keys.
- `registry`: register, unregister, duplicate name, ABI mismatch, out-of-range
  index, and two fake mods with distinct allocators proving copies isolate them.
- `RegistryLink` on Win32 joins the open-verification bucket with
  `src/win32/Module.cpp`.

## Out of scope

The menu itself, per-field set, label i18n, live apply while dragging.
