// Compiled with tests/trap/ ahead of every real include path, so a public header
// reaching for nlohmann/json.hpp or Windows.h fails the build here. Left
// unchecked, either include costs every consumer compile time and portability.
//
// This target only has to COMPILE, so it carries no test cases.

#include <PalCfg/ConfigFile.hpp>
#include <PalCfg/Document.hpp>
#include <PalCfg/Generate.hpp>
#include <PalCfg/Jsonc.hpp>
#include <PalCfg/Load.hpp>
#include <PalCfg/LogSink.hpp>
#include <PalCfg/Paths.hpp>
#include <PalCfg/Schema.hpp>
#include <PalCfg/Win32.hpp>
#include <PalCfg/Value.hpp>

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

    // Instantiated so the seam's types are exercised, going beyond a parse.
    void TouchSeam(const PalCfg::IValueSource& source, PalCfg::Document& doc)
    {
        double value = 0.0;
        source.Child("value", [&](const PalCfg::IValueSource& child) { child.AsDouble(value); });
        (void)doc.Ok();

        std::string text = "{}";
        (void)PalCfg::SanitiseJsonc(text);

        Probe probe;
        (void)PalCfg::LoadFields(kProbeFields, source, probe);
        (void)PalCfg::RenderDocument(kProbeFields, probe, source);

        PalCfg::CallbackSink log{[](PalCfg::Severity, const std::string&) {}};
        log.Report({PalCfg::Severity::Note, "value", "read"});
        (void)PalCfg::ResolveAgainst("dlls", "../config.json");

        PalCfg::ConfigFile<Probe, kProbeFields.size()> file{kProbeFields, "probe.json"};
        (void)file.Tick(PalCfg::ConfigFile<Probe, kProbeFields.size()>::Clock::now());
    }
} // namespace
