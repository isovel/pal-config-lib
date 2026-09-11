#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <PalCfg/LogSink.hpp>

TEST_CASE("a diagnostic formats as one aligned line", "[log]")
{
    CHECK(PalCfg::FormatDiagnostic({PalCfg::Severity::Warning, "arousalRate", "7 is outside 0 to 1; using 1"}) ==
          "WARN  [arousalRate]          7 is outside 0 to 1; using 1");
    CHECK(PalCfg::FormatDiagnostic({PalCfg::Severity::Note, "count", "read text as a whole number"}) ==
          "NOTE  [count]                read text as a whole number");
    CHECK(PalCfg::FormatDiagnostic({PalCfg::Severity::Error, "", "the document's root is not an object"}) ==
          "ERROR []                     the document's root is not an object");
}

TEST_CASE("a field too long for its column keeps one space after it", "[log]")
{
    CHECK(PalCfg::FormatDiagnostic({PalCfg::Severity::Note,
                                    "activity.aVeryLongSettingNameIndeed",
                                    "kept as it is"}) ==
          "NOTE  [activity.aVeryLongSettingNameIndeed] kept as it is");
}

TEST_CASE("a sink hands each formatted line to its callback", "[log]")
{
    std::vector<std::string> lines;
    PalCfg::CallbackSink sink{[&](PalCfg::Severity, const std::string& line) { lines.push_back(line); }};

    sink.Report({PalCfg::Severity::Note, "count", "read text as a whole number"});

    REQUIRE(lines.size() == 1);
    CHECK(lines.front() == "NOTE  [count]                read text as a whole number");
}

TEST_CASE("a sink can be told to pass on notes", "[log]")
{
    std::vector<PalCfg::Severity> seen;
    PalCfg::CallbackSink sink{[&](PalCfg::Severity severity, const std::string&) { seen.push_back(severity); }};

    // What every mod here does already: warnings and errors always show, and the
    // informational stream waits for a debug flag.
    sink.SetMinimumSeverity(PalCfg::Severity::Warning);

    sink.Report({PalCfg::Severity::Note, "count", "read text as a whole number"});
    sink.Report({PalCfg::Severity::Warning, "count", "keeping the default"});
    sink.Report({PalCfg::Severity::Error, "", "the document does not parse"});

    CHECK(seen == std::vector{PalCfg::Severity::Warning, PalCfg::Severity::Error});
}
