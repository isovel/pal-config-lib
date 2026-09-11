# pal-config-lib

A shared config manager for Palworld UE4SS C++ mods. One field declaration yields
the JSON parser, the defaults, a documented-file writer and a settings-menu schema.

Extracted from three mods that each hand-rolled their own: PerkyPals, iaho
(I Already Have One) and DynamicPals.

## Status: v1 core complete

A schema loads a settings struct from a JSONC document, with dotted keys,
aliases, coercion, clamping, pair invariants and diagnostics. File writing, hot
reload, the Win32 platform layer and the menu registry come next. See the
roadmap.

PerkyPals' real shipped `config.default.json` is checked in as a fixture and
loads through a schema mirroring its `src/Config.cpp` field for field, reporting
nothing and leaving no key unclaimed.

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
| `.CaseInsensitiveKeys()` | Schema-level: match every key without regard to case. May appear anywhere in the chain |
| `.Alias(key)` | An older spelling of the key, still accepted on read, so a rename preserves a user's setting. Up to four |
| `.KeepDefaultIfEmpty()` | An empty list in the file keeps the default. Per-field, because for some lists empty is a meaningful answer |
| `.Hidden()` | Kept out of a settings menu. For port-time values such as reflection name lists |
| `.Advanced()` | Shown behind an "advanced" disclosure |

### Declare schemas at namespace scope

`Flatten()` borrows addresses into the schema object, so a schema MUST have static
storage duration for `Flatten()` to be usable in a constant expression. Use
`inline constexpr` so every translation unit shares one object.

Exceeding four aliases is a compile error.

## Loading a struct

```cpp
#include <PalCfg/Load.hpp>

inline constexpr auto kFields = kSchema.Flatten();

PalCfg::CollectingSink sink;
const PalCfg::LoadResult result = PalCfg::LoadFromText(kFields, text, settings, sink);
```

`settings` is written only when the document parses, so it may arrive holding
values that are already live. Loading fills a fresh struct, so deleting a key
from the file restores that field's default.

Drop the sink argument to ignore diagnostics, or call `LoadFields` with an
already-parsed `Document` for finer control.

### Two tiers of failure

A document that cannot be parsed yields nothing, so the caller's struct is left
untouched and one `Error` is reported. PerkyPals got this right by parsing into a
local and assigning only on success; a reload that fails MUST keep the settings
already running.

A single field that cannot be read costs that field alone: it keeps the value
already there, a `Warning` names it, and every other field loads. A typo in
`arousalRate` must not cost a user their `idleActions` list.

### Severities

| Severity | Meaning | Examples |
| --- | --- | --- |
| `Note` | Worth knowing, nothing lost | a coercion that worked, a stray comma removed, a key nobody claims |
| `Warning` | Something was discarded or altered | a field kept its default, a value was clamped |
| `Error` | Nothing could be extracted | the document does not parse, or its root is not an object |

A coercion that succeeded is deliberately a `Note`. Reading `"42"` as a number is
what a hand-edited file looks like, and warning about it teaches users to ignore
warnings.

```
WARN  [arousalRate]      7 is outside 0 to 1; using 1
WARN  [arousalRate]      cannot read an object as this setting; keeping the default
NOTE  [count]            read text as a whole number
NOTE  [sanity.threshld]  not a setting this version knows; kept as it is
ERROR []                 the document's root is not an object
```

A mod installs its own sink to route these. PerkyPals and dynamic-pals both send
warnings and errors to an in-game toast alongside the log, and gate the
informational stream behind a debug flag. That decision stays with the mod: this
library never reads a mod's log settings.

`LoadResult::unknownKeys` holds the dotted paths nobody claimed, so the writer
can carry them forward and a downgrade still reads its own settings.

### Keys

A dotted key reads from nested objects to any depth, so `"activity.idleTasks"`
finds `{"activity": {"idleTasks": [...]}}`. PerkyPals' three nested objects need
no nested structs.

An `.Alias()` answers only for a key that is absent, so a present primary key
stays authoritative even when its value turns out unreadable.

A missing or non-object link along a dotted path resolves to nothing, leaving the
default standing.

### Coercion

Hand-edited files spell values loosely, so each type accepts what it reasonably
can. dynamic-pals' `SafeGetInt`/`SafeGetDouble`/`SafeGetOptionalBool` and iaho's
`IsTruthy` each solved this separately; this is the union of both.

| Target | Also accepts |
| --- | --- |
| `bool` | `true`/`yes`/`on`/`1` and their negatives, case-insensitively, and any number as non-zero |
| integrals | a numeric string, and a fractional number truncated toward zero |
| `float`, `double` | a numeric string, including exponent form |
| `std::string` | a number or a bool |
| lists and sets | one comma- or semicolon-separated string, which is how iaho spelled its id lists |

A partly-numeric string such as `"0.5x"` keeps the default, so a typo stays
visible.

### Pair invariants

Two fields that bound one quantity share their modifiers, and can restore their
own order:

```cpp
.FieldPair("onsetDelayMin", &Settings::OnsetDelayMin,
           "onsetDelayMax", &Settings::OnsetDelayMax)
    .Label("Onset delay")
    .Range(0.0, 300.0)
    .SwapIfInverted()
```

PerkyPals swapped these by hand after parsing. Declaring the invariant also tells
a settings menu to draw one two-handled slider in place of two disconnected spin
boxes.

`.SwapIfInverted()` MUST follow a `.FieldPair()`, which the compiler enforces:
calling it anywhere else fails the build. It runs after every field is loaded, so
it sees coerced and clamped values. Inference from a `Min`/`Max` name
suffix is deliberately absent: silently reordering a user's numbers on implicit
magic is the wrong kind of clever.

### Supported types

`bool`, every integral, `float`, `double`, `std::string`, `std::wstring`,
`std::vector<T>`, `std::unordered_set<T>`, `std::optional<T>`, and enums. Element
types recurse, so `std::vector<std::wstring>` works.

An integer too large for its target keeps the default, so one digit too many
avoids silently wrapping.

Enums opt in by naming their values, and accept a name case-insensitively or the
underlying number:

```cpp
template <>
struct PalCfg::EnumNames<LogVerbosity>
{
    static constexpr std::array<std::pair<std::string_view, LogVerbosity>, 3> kValues{{
        {"quiet", LogVerbosity::Quiet},
        {"normal", LogVerbosity::Normal},
        {"discovery", LogVerbosity::Discovery},
    }};
};
```

A mod supports a type of its own by specialising `PalCfg::ValueTraits<MyType>`
with `kKind` and a
`Read(MyType&, const IValueSource&, const ReadContext&)`. No registration, no
inheritance. `ReadContext` carries the field's metadata, its dotted key and the
diagnostic sink, so later additions leave existing specialisations compiling.

The specialisation MUST appear before the schema that uses the type.

### Wide strings

`Utf8ToWide` and `WideToUtf8` do real conversion. PerkyPals' `Widen` did
`std::wstring(s.begin(), s.end())`, which copies each byte into its own
`wchar_t`, so a non-ASCII morph target or action name silently became mojibake.
Malformed input yields U+FFFD per maximal subpart, following Unicode's
recommended practice, and overlong encodings, surrogates and out-of-range code
points are all rejected.

`wchar_t` is 16 bits on Windows and 32 on Linux, so these produce UTF-16 or
UTF-32 to match.

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

Field parsing is templated on the member type, so it compiles in the consumer's
own translation unit. `IValueSource` is what keeps those templates away from
`nlohmann::json`, at the cost of one virtual call per scalar read.

**`FieldOps` are keyed on types alone.** `OpsFor<T, M>` never takes the
member-pointer *value* as a template argument, so a schema of N fields over K
distinct member types yields K thunks. The member pointer travels as data in
`FieldRuntime::boundMemberPtr`, which is what lets
`.Field("key", &Settings::Member)` keep its syntax while the descriptor array
stays homogeneous.

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
| 3 ✅ | `ValueTraits`, the `FieldOps` thunks, `LoadFields`, and a real UTF-8 ↔ UTF-16 converter |
| 4 ✅ | Coercion across types, dotted-key nesting, clamping, aliases, case-insensitive keys, `FieldPair().SwapIfInverted()` |
| 5 ✅ | Diagnostics and the two-tier error model, plus unknown-key capture |
| gate ✅ | PerkyPals' shipped config loads to its documented values, with every field proven to read from the document |
| next | The documented-file writer and `palcfg-gen`, so `config.default.json` becomes a build artifact |
| later | Hot reload, the Win32 platform layer, and the optional C-ABI registry that lets a settings menu enumerate every mod |

On-disk format is JSONC. Comments are load-bearing: the schema's `.Help()` text
regenerates them, so a menu writing settings back preserves a config file's
documentation.
