# pal-config-lib

A shared config manager for Palworld UE4SS C++ mods. One field declaration yields
the JSON parser, the defaults, a documented-file writer and a settings-menu schema.

Extracted from three mods that each hand-rolled their own: PerkyPals, iaho
(I Already Have One) and DynamicPals.

## Status: stage 2 of v1

`PalCfg::Schema` describes a settings struct at compile time, and
`PalCfg::Document` parses JSONC behind an abstract seam. Binding the two
together, plus file writing, hot reload and the menu registry, arrives in later
stages; see the roadmap.

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

## Reading a document

```cpp
#include <PalCfg/Document.hpp>

PalCfg::Document doc;
if (!doc.Parse(std::move(text)))
{
    // Document-level failure. A caller MUST keep the settings it already has.
    Log(doc.Error().message);
    return;
}

doc.Root().Child("arousalRate", [&](const PalCfg::IValueSource& value) {
    double rate = 0.0;
    if (value.AsDouble(rate)) settings.ArousalRate = static_cast<float>(rate);
});
```

`Child()` and `Element()` take a visitor, so no node handle escapes the seam and
a borrowed node cannot outlive its visit. Each `AsX` reports only what the node
naturally holds and leaves its output alone otherwise; cross-type coercion
arrives with `ValueTraits` in stage 3.

### Tolerating hand-edited files

`Parse` runs `SanitiseJsonc` first, which removes a UTF-8 BOM and any trailing
commas, and reports both so a caller can raise a note. nlohmann rejects a
trailing comma outright, which would cost a user every setting in the file over
one character that most config formats accept.

The scan tracks string and comment state, so a comma inside `"Idle,Wait"`, inside
`// rate, hold, decay`, or behind an escaped quote survives untouched. Comments
between a comma and its closing brace are skipped, so `1,  // last one` followed
by `}` still counts as trailing.

`SanitiseJsonc` is public, and usable on its own.

## Design notes

**No public header includes `json.hpp` or `Windows.h`.** PerkyPals kept nlohmann
(~956 KB) out of its own config header; here the build enforces it. `tests/trap/`
puts an `#error`-ing stub of each ahead of the real include path, so either
include fails the build. nlohmann 3.12.0 is vendored under `third_party/`, the
same file both JSON-using mods already carry, and it is a PRIVATE include of the
static library.

Field parsing will be templated on the member type, so it compiles in the
consumer's own translation unit. `IValueSource` is what keeps those templates
away from `nlohmann::json`, at the cost of one virtual call per scalar read.

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
| 2 ✅ | `IValueSource`, the nlohmann backend behind that seam, `Document`, and `SanitiseJsonc` |
| 3 | `ValueTraits` for scalars, `string`/`wstring`, `vector`, `optional`, `unordered_set` and enums; a real UTF-8 ↔ UTF-16 converter, and the `FieldOps` thunks that bind a schema to a document |
| 4 | Coercion, dotted-key nesting, clamping, aliasing, case-insensitive keys, `FieldPair().SwapIfInverted()` |
| 5 | Diagnostics and the two-tier error model: a bad field keeps its default, a bad document keeps the live settings |
| later | The documented-file writer, hot reload, the Win32 platform layer, and the optional C-ABI registry that lets a settings menu enumerate every mod |

On-disk format is JSONC. Comments are load-bearing: the schema's `.Help()` text
regenerates them, so a menu writing settings back preserves a config file's
documentation.
