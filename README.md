# pal-config-lib

A shared config manager for Palworld UE4SS C++ mods. One field declaration yields
the JSON parser, the defaults, a documented-file writer and a settings-menu schema.

Extracted from three mods that each hand-rolled their own: PerkyPals, iaho
(I Already Have One) and DynamicPals.

## Status: stage 1 of v1 — the schema only

`PalCfg::Schema` describes a settings struct at compile time. Parsing, file
writing, hot reload and the menu registry arrive in later stages; see the
roadmap.

```cpp
#include <PalCfg/Schema.hpp>

struct Settings
{
    float ArousalRate = 0.08f;
    bool  PlayerEnabled = true;
};

inline constexpr auto kSchema = PalCfg::Schema<Settings>("PerkyPals")
    .Field("arousalRate", &Settings::ArousalRate)
        .Label("Arousal rate")
        .Help("Weight per second while rising")
        .Range(0.0, 1.0)
    .Field("player.enabled", &Settings::PlayerEnabled)
        .Label("Drive the player");

inline constexpr auto kFields = kSchema.Flatten();
```

The struct's own member initialisers are the single statement of the defaults,
so they stay in step by construction.

### Modifiers

Each applies to the preceding `.Field()`.

| Modifier | Effect |
| --- | --- |
| `.Label(text)` | Display name in a settings menu |
| `.Help(text)` | Documentation. Emitted as `//` comments in the generated file; embedded newlines become separate comment lines |
| `.Range(lo, hi)` | Numeric bounds. Clamped on read, and rendered as a slider |
| `.Alias(key)` | An older spelling of the key, still accepted on read, so a rename preserves a user's setting. Up to four |
| `.KeepDefaultIfEmpty()` | An empty list in the file keeps the default. Per-field, because for some lists empty is a meaningful answer |
| `.Hidden()` | Kept out of a settings menu. For port-time values such as reflection name lists |
| `.Advanced()` | Shown behind an "advanced" disclosure |

### Declare schemas at namespace scope

`Flatten()` borrows addresses into the schema object, so a schema MUST have static
storage duration for `Flatten()` to be usable in a constant expression. Use
`inline constexpr` so every translation unit shares one object.

Exceeding four aliases is a compile error.

## Design notes

**No public header includes `json.hpp` or `Windows.h`.** PerkyPals kept nlohmann
(~956 KB) out of its own config header; here the build enforces it. `tests/trap/`
puts an `#error`-ing stub of each ahead of the real include path, so either
include fails the build.

**Schemas cost nothing at runtime.** Verified with `llvm-nm`: `kSchema` and
`kFields` land in read-only data with zero dynamic initialisers, so they are fully
formed before any code runs inside a DLL that UE4SS loads at an unpredictable
point.

**Only `.Field()` grows the schema's type.** Modifiers return the same type, so
template instantiation stays linear in the field count. A 30-field schema with 60
modifier calls costs 30 instantiations.

## Build and test

Host-native, no UE4SS submodule, no Windows, no cross-compile:

```
cmake -B build-host -G Ninja -DPALCFG_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Catch2 v3 is fetched at configure time for tests alone, keeping the network
dependency inside this repo's own build. Tests default off when the project is
consumed via `add_subdirectory`.

Verified on clang 19 (Linux), g++ 12 (Linux), and clang-cl 19 cross-compiling to
`x86_64-pc-windows-msvc` against the Microsoft STL via
[xwin](https://github.com/Jake-Shadle/xwin). MSVC's own front-end remains
untested, as this environment has no Windows host. You MUST run the suite under
MSVC before tagging a release. Every compile-time claim is a `static_assert`, so
a divergence fails the build loudly.

## Consuming it

Submodule plus `add_subdirectory`, matching how the mods already consume
RE-UE4SS. Each mod pins its own commit, so an update is explicit.

```cmake
add_subdirectory(deps/pal-config-lib)
target_link_libraries(MyMod PRIVATE PalCfg::Core)
```

## Roadmap

| Stage | Contents |
| --- | --- |
| 1 ✅ | `Schema`, `FieldMeta`, `Flatten()` |
| 2 | `IValueSource`/`IValueSink`, the nlohmann backend behind that seam, and a tolerant pre-pass for trailing commas |
| 3 | `ValueTraits` for scalars, `string`/`wstring`, `vector`, `optional`, `unordered_set` and enums; a real UTF-8 ↔ UTF-16 converter |
| 4 | Coercion, dotted-key nesting, clamping, aliasing, case-insensitive keys, `FieldPair().SwapIfInverted()` |
| 5 | Diagnostics and the two-tier error model: a bad field keeps its default, a bad document keeps the live settings |
| later | The documented-file writer, hot reload, the Win32 platform layer, and the optional C-ABI registry that lets a settings menu enumerate every mod |

On-disk format is JSONC. Comments are load-bearing: the schema's `.Help()` text
regenerates them, so a menu writing settings back preserves a config file's
documentation.
