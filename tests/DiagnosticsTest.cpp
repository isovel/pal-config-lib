#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <PalCfg/Diagnostics.hpp>
#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        float Rate = 0.08f;
        bool Enabled = true;
        int Count = 5;
        std::vector<std::wstring> Targets{L"Aroused"};
        float Threshold = 0.8f;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("PerkyPals")
                                       .Field("arousalRate", &Settings::Rate).Range(0.0, 1.0)
                                       .Field("player.enabled", &Settings::Enabled)
                                       .Field("count", &Settings::Count)
                                       .Field("morphTargets", &Settings::Targets)
                                       .Field("sanity.threshold", &Settings::Threshold);

    inline constexpr auto kFields = kSchema.Flatten();

    struct Outcome
    {
        Settings settings{};
        PalCfg::CollectingSink sink{};
        PalCfg::LoadResult result{};
    };

    Outcome Load(const char* json)
    {
        Outcome outcome;
        outcome.result = PalCfg::LoadFromText(kFields, json, outcome.settings, outcome.sink);
        return outcome;
    }
} // namespace

TEST_CASE("a collecting sink keeps what it is given, in order")
{
    PalCfg::CollectingSink sink;

    sink.Report({PalCfg::Severity::Note, "a", "first"});
    sink.Report({PalCfg::Severity::Error, "b", "second"});

    REQUIRE(sink.All().size() == 2);
    CHECK(sink.All()[0].message == "first");
    CHECK(sink.All()[1].severity == PalCfg::Severity::Error);
    CHECK(sink.Count(PalCfg::Severity::Error) == 1);
    CHECK(sink.Has(PalCfg::Severity::Error));
    CHECK_FALSE(sink.Has(PalCfg::Severity::Warning));
}

TEST_CASE("a clean load reports nothing")
{
    const Outcome o = Load(R"({"arousalRate": 0.5})");

    CHECK(o.result.Ok());
    CHECK(o.sink.All().empty());
    CHECK(o.settings.Rate == 0.5f);
}

TEST_CASE("a malformed document is one error, and the struct is left alone")
{
    Settings settings{};
    settings.Rate = 0.42f; // stands in for settings already live

    PalCfg::CollectingSink sink;
    const PalCfg::LoadResult result = PalCfg::LoadFromText(kFields, "{not json", settings, sink);

    CHECK_FALSE(result.parsed);
    CHECK_FALSE(result.Ok());
    REQUIRE(sink.Count(PalCfg::Severity::Error) == 1);
    CHECK(sink.All()[0].field.empty());
    CHECK(settings.Rate == 0.42f);
}

TEST_CASE("a root that is not an object is a document-level error")
{
    const Outcome o = Load("[1, 2]");

    CHECK_FALSE(o.result.parsed);
    CHECK(o.sink.Count(PalCfg::Severity::Error) == 1);
}

TEST_CASE("one unreadable field is a warning naming it, and costs only itself")
{
    const Outcome o = Load(R"({"arousalRate": {}, "count": 9})");

    CHECK(o.result.parsed);
    REQUIRE(o.sink.Count(PalCfg::Severity::Warning) == 1);
    CHECK(o.sink.All()[0].field == "arousalRate");
    CHECK(o.settings.Rate == 0.08f); // default stands
    CHECK(o.settings.Count == 9);    // the other field loaded
}

TEST_CASE("a clamped value is a warning that names the bound")
{
    const Outcome o = Load(R"({"arousalRate": 7.0})");

    REQUIRE(o.sink.Count(PalCfg::Severity::Warning) == 1);
    CHECK(o.sink.All()[0].field == "arousalRate");
    CHECK(o.sink.All()[0].message.find("1") != std::string::npos);
    CHECK(o.settings.Rate == 1.0f);
}

TEST_CASE("a coercion that worked is a note, never a warning")
{
    const Outcome o = Load(R"({"count": "9"})");

    CHECK(o.settings.Count == 9);
    CHECK(o.sink.Count(PalCfg::Severity::Warning) == 0);
    CHECK(o.sink.Count(PalCfg::Severity::Note) == 1);
    CHECK(o.sink.All()[0].field == "count");
}

TEST_CASE("an unknown key is a note and is kept for the writer")
{
    const Outcome o = Load(R"({"arousalRte": 0.5, "experimentalThing": 42})");

    CHECK(o.result.parsed);
    CHECK(o.sink.Count(PalCfg::Severity::Note) == 2);
    REQUIRE(o.result.unknownKeys.size() == 2);
    CHECK(o.result.unknownKeys[0] == "arousalRte");
}

TEST_CASE("an unknown key inside a known object is reported by its full path")
{
    const Outcome o = Load(R"({"sanity": {"threshld": 0.5}})");

    REQUIRE(o.result.unknownKeys.size() == 1);
    CHECK(o.result.unknownKeys[0] == "sanity.threshld");
}

TEST_CASE("a key a field owns is never called unknown")
{
    const Outcome o = Load(R"({"player": {"enabled": false}, "morphTargets": ["A"]})");

    CHECK(o.result.unknownKeys.empty());
    CHECK(o.sink.All().empty());
}

TEST_CASE("what sanitising had to fix is reported as notes")
{
    const Outcome o = Load("\xEF\xBB\xBF{\"count\": [1,],}");

    CHECK(o.result.parsed);
    CHECK(o.sink.Count(PalCfg::Severity::Note) >= 2);
}

TEST_CASE("the count of fields actually loaded is reported")
{
    const Outcome o = Load(R"({"arousalRate": 0.5, "count": 9})");

    CHECK(o.result.fieldsLoaded == 2);
}

TEST_CASE("a list read from a separated string is a note")
{
    const Outcome o = Load(R"({"morphTargets": "Aroused, Blush"})");

    REQUIRE(o.settings.Targets.size() == 2);
    CHECK(o.sink.Count(PalCfg::Severity::Warning) == 0);
    REQUIRE(o.sink.Count(PalCfg::Severity::Note) == 1);
    CHECK(o.sink.All()[0].field == "morphTargets");
}
