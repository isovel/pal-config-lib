// The v1 acceptance gate: PerkyPals' real shipped config, loaded through a schema
// that mirrors src/Config.cpp field for field.
//
// config.default.json holds only default values, so on its own it cannot tell a
// value that was read from a value that was defaulted. Three tests together close
// that gap: the shipped file loads to its documented values with nothing
// reported, a fully non-default document proves every field is actually read into
// the right member, and the empty-list cases pin the one field where empty means
// something.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    // Mirrors perky-pals/include/Config.hpp, defaults included.
    struct Settings
    {
        std::vector<std::wstring> MorphTargets{L"Aroused"};

        float ArousalRate = 0.08f;
        float HoldDelay = 60.0f;
        float DecayRate = 0.10f;

        float OnsetDelayMin = 0.0f;
        float OnsetDelayMax = 30.0f;
        float RateMultiplierMin = 0.5f;
        float RateMultiplierMax = 1.5f;

        bool PlayerEnabled = true;

        bool DebugEnabled = false;
        bool Trace = false;
        bool EnableDriver = true;
        bool EnableSanity = true;
        bool EnableRegistry = true;

        bool SanityEnabled = true;
        float SanityThreshold = 0.8f;
        std::vector<std::wstring> SanityGetters{L"GetSanityValue"};
        std::vector<std::wstring> SanityMaxGetters{L"GetMaxSanityValue"};
        std::vector<std::wstring> SanityProperties{};
        std::vector<std::wstring> SanityMaxProperties{};

        bool ActivityEnabled = true;
        std::vector<std::wstring> ActivityFighting{L"GetBattleMode"};
        std::vector<std::wstring> ActivityWorking{};
        std::vector<std::wstring> ActivityRiding{};
        std::vector<std::wstring> ActivityRiderGetters{L"FindRiderByRidingActor"};
        std::vector<std::wstring> ActivityComponents{L"ActionComponent"};
        std::vector<std::wstring> ActivityTaskObjects{L"CurrentAction"};
        std::vector<std::wstring> ActivityIdleActions{L"Idle"};
        std::vector<std::wstring> ActivityTaskGetters{L"GetCurrentActionType"};
        std::vector<std::wstring> ActivityTaskProperties{};
        std::vector<float> ActivityIdleTasks{0.0f,  1.0f,  2.0f,  6.0f,  38.0f, 39.0f, 54.0f,
                                             55.0f, 56.0f, 57.0f, 58.0f, 59.0f, 60.0f, 61.0f,
                                             62.0f, 63.0f, 64.0f, 77.0f, 78.0f};
        std::vector<std::wstring> ActivityBattleGetters{};
        std::vector<std::wstring> ActivityBattleProperties{};
        std::vector<float> ActivityPeacefulBattleModes{0.0f};
    };

    // Config.cpp's ReadStringList and ReadFloatList keep the default when the
    // parsed list comes out empty. idleActions is the one exception: it assigns
    // unconditionally, so an empty array there clears the list.
    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("PerkyPals")
            .Field("morphTargets", &Settings::MorphTargets).KeepDefaultIfEmpty()
            .Field("arousalRate", &Settings::ArousalRate)
            .Field("holdDelay", &Settings::HoldDelay)
            .Field("decayRate", &Settings::DecayRate)
            .FieldPair("onsetDelayMin", &Settings::OnsetDelayMin,
                       "onsetDelayMax", &Settings::OnsetDelayMax).SwapIfInverted()
            .FieldPair("rateMultiplierMin", &Settings::RateMultiplierMin,
                       "rateMultiplierMax", &Settings::RateMultiplierMax).SwapIfInverted()
            .Field("player.enabled", &Settings::PlayerEnabled)
            .Field("debug.enabled", &Settings::DebugEnabled)
            .Field("debug.trace", &Settings::Trace)
            .Field("debug.enableDriver", &Settings::EnableDriver)
            .Field("debug.enableSanity", &Settings::EnableSanity)
            .Field("debug.enableRegistry", &Settings::EnableRegistry)
            .Field("sanity.enabled", &Settings::SanityEnabled)
            .Field("sanity.threshold", &Settings::SanityThreshold)
            .Field("sanity.getters", &Settings::SanityGetters).KeepDefaultIfEmpty()
            .Field("sanity.maxGetters", &Settings::SanityMaxGetters).KeepDefaultIfEmpty()
            .Field("sanity.properties", &Settings::SanityProperties).KeepDefaultIfEmpty()
            .Field("sanity.maxProperties", &Settings::SanityMaxProperties).KeepDefaultIfEmpty()
            .Field("activity.enabled", &Settings::ActivityEnabled)
            .Field("activity.fighting", &Settings::ActivityFighting).KeepDefaultIfEmpty()
            .Field("activity.working", &Settings::ActivityWorking).KeepDefaultIfEmpty()
            .Field("activity.riding", &Settings::ActivityRiding).KeepDefaultIfEmpty()
            .Field("activity.riderGetters", &Settings::ActivityRiderGetters).KeepDefaultIfEmpty()
            .Field("activity.components", &Settings::ActivityComponents).KeepDefaultIfEmpty()
            .Field("activity.taskObjects", &Settings::ActivityTaskObjects).KeepDefaultIfEmpty()
            .Field("activity.idleActions", &Settings::ActivityIdleActions)
            .Field("activity.taskGetters", &Settings::ActivityTaskGetters).KeepDefaultIfEmpty()
            .Field("activity.taskProperties", &Settings::ActivityTaskProperties).KeepDefaultIfEmpty()
            .Field("activity.idleTasks", &Settings::ActivityIdleTasks).KeepDefaultIfEmpty()
            .Field("activity.battleGetters", &Settings::ActivityBattleGetters).KeepDefaultIfEmpty()
            .Field("activity.battleProperties", &Settings::ActivityBattleProperties).KeepDefaultIfEmpty()
            .Field("activity.peacefulBattleModes", &Settings::ActivityPeacefulBattleModes)
                .KeepDefaultIfEmpty();

    inline constexpr auto kFields = kSchema.Flatten();

    std::string ReadFixture(const char* name)
    {
        std::ifstream file(std::string{PALCFG_FIXTURE_DIR} + "/" + name, std::ios::binary);
        REQUIRE(file.is_open());

        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
} // namespace

TEST_CASE("the schema covers every field src/Config.cpp parses")
{
    STATIC_REQUIRE(kFields.size() == 34);
}

TEST_CASE("the shipped config.default.json loads to its documented values")
{
    Settings s{};
    PalCfg::CollectingSink sink;

    const PalCfg::LoadResult result =
        PalCfg::LoadFromText(kFields, ReadFixture("perkypals.config.default.json"), s, sink);

    REQUIRE(result.Ok());

    // Nothing in the shipped file should need fixing, coercing or clamping.
    for (const auto& diagnostic : sink.All())
    {
        INFO("unexpected: [" << diagnostic.field << "] " << diagnostic.message);
        CHECK(false);
    }

    // Every key in the file is claimed by a field, and the file's 34 keys match
    // the schema's 34, so neither side has drifted from the other.
    CHECK(result.unknownKeys.empty());
    CHECK(result.fieldsLoaded == 34);

    CHECK(s.MorphTargets == std::vector<std::wstring>{L"Aroused"});
    CHECK(s.ArousalRate == 0.08f);
    CHECK(s.HoldDelay == 60.0f);
    CHECK(s.DecayRate == 0.10f);
    CHECK(s.OnsetDelayMin == 0.0f);
    CHECK(s.OnsetDelayMax == 30.0f);
    CHECK(s.RateMultiplierMin == 0.5f);
    CHECK(s.RateMultiplierMax == 1.5f);
    CHECK(s.PlayerEnabled);
    CHECK_FALSE(s.DebugEnabled);
    CHECK_FALSE(s.Trace);
    CHECK(s.EnableDriver);
    CHECK(s.EnableSanity);
    CHECK(s.EnableRegistry);
    CHECK(s.SanityEnabled);
    CHECK(s.SanityThreshold == 0.8f);
    CHECK(s.SanityGetters == std::vector<std::wstring>{L"GetSanityValue"});
    CHECK(s.SanityMaxGetters == std::vector<std::wstring>{L"GetMaxSanityValue"});
    CHECK(s.SanityProperties.empty());
    CHECK(s.SanityMaxProperties.empty());
    CHECK(s.ActivityEnabled);
    CHECK(s.ActivityFighting == std::vector<std::wstring>{L"GetBattleMode"});
    CHECK(s.ActivityWorking.empty());
    CHECK(s.ActivityRiding.empty());
    CHECK(s.ActivityRiderGetters == std::vector<std::wstring>{L"FindRiderByRidingActor"});
    CHECK(s.ActivityComponents == std::vector<std::wstring>{L"ActionComponent"});
    CHECK(s.ActivityTaskObjects == std::vector<std::wstring>{L"CurrentAction"});
    CHECK(s.ActivityIdleActions == std::vector<std::wstring>{L"Idle"});
    CHECK(s.ActivityTaskGetters == std::vector<std::wstring>{L"GetCurrentActionType"});
    CHECK(s.ActivityTaskProperties.empty());
    REQUIRE(s.ActivityIdleTasks.size() == 19);
    CHECK(s.ActivityIdleTasks.front() == 0.0f);
    CHECK(s.ActivityIdleTasks.back() == 78.0f);
    CHECK(s.ActivityBattleGetters.empty());
    CHECK(s.ActivityBattleProperties.empty());
    CHECK(s.ActivityPeacefulBattleModes == std::vector<float>{0.0f});
}

// The shipped file holds defaults throughout, so this is what proves a value
// actually travels from the document into the member it names.
TEST_CASE("every field reads from the document rather than defaulting")
{
    static constexpr const char* kAllChanged = R"({
        "morphTargets": ["Blush", "Sweat"],
        "arousalRate": 0.5,
        "holdDelay": 12.5,
        "decayRate": 0.75,
        "onsetDelayMin": 3.0,
        "onsetDelayMax": 9.0,
        "rateMultiplierMin": 0.25,
        "rateMultiplierMax": 4.0,
        "player":   { "enabled": false },
        "debug":    { "enabled": true, "trace": true, "enableDriver": false,
                      "enableSanity": false, "enableRegistry": false },
        "sanity":   { "enabled": false, "threshold": 0.25,
                      "getters": ["G1"], "maxGetters": ["G2"],
                      "properties": ["P1"], "maxProperties": ["P2"] },
        "activity": { "enabled": false,
                      "fighting": ["F1"], "working": ["W1"], "riding": ["R1"],
                      "riderGetters": ["RG1"], "components": ["C1"],
                      "taskObjects": ["TO1"], "idleActions": ["IA1", "IA2"],
                      "taskGetters": ["TG1"], "taskProperties": ["TP1"],
                      "idleTasks": [11, 12],
                      "battleGetters": ["BG1"], "battleProperties": ["BP1"],
                      "peacefulBattleModes": [5] }
    })";

    Settings s{};
    PalCfg::CollectingSink sink;
    const PalCfg::LoadResult result = PalCfg::LoadFromText(kFields, kAllChanged, s, sink);

    REQUIRE(result.Ok());
    CHECK(result.unknownKeys.empty());
    CHECK(result.fieldsLoaded == 34);
    CHECK(sink.Count(PalCfg::Severity::Warning) == 0);
    CHECK(sink.Count(PalCfg::Severity::Error) == 0);

    CHECK(s.MorphTargets == std::vector<std::wstring>{L"Blush", L"Sweat"});
    CHECK(s.ArousalRate == 0.5f);
    CHECK(s.HoldDelay == 12.5f);
    CHECK(s.DecayRate == 0.75f);
    CHECK(s.OnsetDelayMin == 3.0f);
    CHECK(s.OnsetDelayMax == 9.0f);
    CHECK(s.RateMultiplierMin == 0.25f);
    CHECK(s.RateMultiplierMax == 4.0f);
    CHECK_FALSE(s.PlayerEnabled);
    CHECK(s.DebugEnabled);
    CHECK(s.Trace);
    CHECK_FALSE(s.EnableDriver);
    CHECK_FALSE(s.EnableSanity);
    CHECK_FALSE(s.EnableRegistry);
    CHECK_FALSE(s.SanityEnabled);
    CHECK(s.SanityThreshold == 0.25f);
    CHECK(s.SanityGetters == std::vector<std::wstring>{L"G1"});
    CHECK(s.SanityMaxGetters == std::vector<std::wstring>{L"G2"});
    CHECK(s.SanityProperties == std::vector<std::wstring>{L"P1"});
    CHECK(s.SanityMaxProperties == std::vector<std::wstring>{L"P2"});
    CHECK_FALSE(s.ActivityEnabled);
    CHECK(s.ActivityFighting == std::vector<std::wstring>{L"F1"});
    CHECK(s.ActivityWorking == std::vector<std::wstring>{L"W1"});
    CHECK(s.ActivityRiding == std::vector<std::wstring>{L"R1"});
    CHECK(s.ActivityRiderGetters == std::vector<std::wstring>{L"RG1"});
    CHECK(s.ActivityComponents == std::vector<std::wstring>{L"C1"});
    CHECK(s.ActivityTaskObjects == std::vector<std::wstring>{L"TO1"});
    CHECK(s.ActivityIdleActions == std::vector<std::wstring>{L"IA1", L"IA2"});
    CHECK(s.ActivityTaskGetters == std::vector<std::wstring>{L"TG1"});
    CHECK(s.ActivityTaskProperties == std::vector<std::wstring>{L"TP1"});
    CHECK(s.ActivityIdleTasks == std::vector<float>{11.0f, 12.0f});
    CHECK(s.ActivityBattleGetters == std::vector<std::wstring>{L"BG1"});
    CHECK(s.ActivityBattleProperties == std::vector<std::wstring>{L"BP1"});
    CHECK(s.ActivityPeacefulBattleModes == std::vector<float>{5.0f});
}

// src/Config.cpp:149-159 assigns idleActions unconditionally while every other
// list keeps its default when the parsed result is empty. That asymmetry is
// deliberate, and it is the whole reason KeepDefaultIfEmpty is per-field.
TEST_CASE("an empty list clears only idleActions")
{
    Settings s{};
    const PalCfg::LoadResult result = PalCfg::LoadFromText(
        kFields,
        R"({"morphTargets": [], "activity": {"idleActions": [], "idleTasks": [],
            "fighting": []}, "sanity": {"getters": []}})",
        s);

    REQUIRE(result.Ok());

    CHECK(s.ActivityIdleActions.empty());

    CHECK(s.MorphTargets == std::vector<std::wstring>{L"Aroused"});
    CHECK(s.ActivityIdleTasks.size() == 19);
    CHECK(s.ActivityFighting == std::vector<std::wstring>{L"GetBattleMode"});
    CHECK(s.SanityGetters == std::vector<std::wstring>{L"GetSanityValue"});
}

// src/Config.cpp:195-199 swapped these by hand after parsing.
TEST_CASE("inverted delay and multiplier bounds are put back in order")
{
    Settings s{};
    const PalCfg::LoadResult result = PalCfg::LoadFromText(
        kFields,
        R"({"onsetDelayMin": 20, "onsetDelayMax": 4,
            "rateMultiplierMin": 3.0, "rateMultiplierMax": 1.0})",
        s);

    REQUIRE(result.Ok());
    CHECK(s.OnsetDelayMin == 4.0f);
    CHECK(s.OnsetDelayMax == 20.0f);
    CHECK(s.RateMultiplierMin == 1.0f);
    CHECK(s.RateMultiplierMax == 3.0f);
}
