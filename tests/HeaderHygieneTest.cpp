// Compiled with tests/trap/ ahead of every real include path, so a public header
// reaching for nlohmann/json.hpp or Windows.h fails the build here. Left
// unchecked, either include costs every consumer compile time and portability.
//
// This target only has to COMPILE, so it carries no test cases.

#include <PalCfg/Schema.hpp>

namespace
{
    struct Probe
    {
        float Value = 1.0f;
    };

    inline constexpr auto kProbe =
        PalCfg::Schema<Probe>("HeaderHygiene").Field("value", &Probe::Value).Label("Value");
    inline constexpr auto kProbeFields = kProbe.Flatten();

    static_assert(kProbeFields.size() == 1);
} // namespace
