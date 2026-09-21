// The v1 acceptance gate: PerkyPals' real shipped config, loaded through a schema
// that mirrors its include/ConfigSchema.hpp field for field. The fixture is the
// file that schema generates, so this also proves the generated file reads back.
//
// config.default.json holds only default values, so on its own it cannot tell a
// value that was read from a value that was defaulted. Two tests together close
// that gap: the shipped file loads to its documented values with nothing
// reported, and a fully non-default document proves every field is actually read
// into the right member.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>
#include <PalCfg/Write.hpp>

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
        float PlayerHoldHungerAtLeast = 0.75f;
        float PlayerHoldHealthAtLeast = 0.98f;

        bool DebugEnabled = false;
        bool Trace = false;
        bool EnableDriver = true;
        bool EnableGate = true;
        bool EnableRegistry = true;

        bool SanityEnabled = true;
        float SanityRiseThreshold = 0.8f;
        float SanityThreshold = 0.95f;

        bool ActivityEnabled = true;

        bool ConditionEnabled = true;
        float ConditionHungerBelow = 0.50f;
        float ConditionHealthBelow = 0.50f;
        float ConditionStaminaAtMost = 0.0f;
        float ConditionWeightAbove = 1.0f;
        float ConditionHurtHold = 5.0f;
        float ConditionDrainHold = 3.0f;

        bool operator==(const Settings&) const = default;
    };

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
            .Field("player.holdHungerAtLeast", &Settings::PlayerHoldHungerAtLeast).Range(0.0, 1.0)
            .Field("player.holdHealthAtLeast", &Settings::PlayerHoldHealthAtLeast).Range(0.0, 1.0)
            .Field("debug.enabled", &Settings::DebugEnabled)
            .Field("debug.trace", &Settings::Trace)
            .Field("debug.enableDriver", &Settings::EnableDriver)
            .Field("debug.enableGate", &Settings::EnableGate)
            .Field("debug.enableRegistry", &Settings::EnableRegistry)
            .Field("condition.enabled", &Settings::ConditionEnabled)
            .Field("condition.hungerBelow", &Settings::ConditionHungerBelow).Range(0.0, 1.0)
            .Field("condition.healthBelow", &Settings::ConditionHealthBelow).Range(0.0, 1.0)
            .Field("condition.staminaAtMost", &Settings::ConditionStaminaAtMost).Range(0.0, 1.0)
            .Field("condition.weightAbove", &Settings::ConditionWeightAbove)
            .Field("condition.hurtHold", &Settings::ConditionHurtHold)
            .Field("condition.drainHold", &Settings::ConditionDrainHold)
            .Field("activity.enabled", &Settings::ActivityEnabled)
            .Field("sanity.enabled", &Settings::SanityEnabled)
            .FieldPair("sanity.riseThreshold", &Settings::SanityRiseThreshold,
                       "sanity.threshold", &Settings::SanityThreshold)
                .Range(0.0, 1.0).SwapIfInverted();

    inline constexpr auto kFields = kSchema.Flatten();

    // Every field set away from its default, so a load that silently skipped one
    // shows up as a defaulted value.
    constexpr const char* kAllChanged = R"({
        "morphTargets": ["Blush", "Sweat"],
        "arousalRate": 0.5,
        "holdDelay": 12.5,
        "decayRate": 0.75,
        "onsetDelayMin": 3.0,
        "onsetDelayMax": 9.0,
        "rateMultiplierMin": 0.25,
        "rateMultiplierMax": 4.0,
        "player":    { "enabled": false, "holdHungerAtLeast": 0.1, "holdHealthAtLeast": 0.2 },
        "debug":     { "enabled": true, "trace": true, "enableDriver": false,
                       "enableGate": false, "enableRegistry": false },
        "condition": { "enabled": false, "hungerBelow": 0.1, "healthBelow": 0.2,
                       "staminaAtMost": 0.3, "weightAbove": 0.4, "hurtHold": 1.5,
                       "drainHold": 2.5 },
        "activity":  { "enabled": false },
        "sanity":    { "enabled": false, "riseThreshold": 0.15, "threshold": 0.25 }
    })";

    std::string ReadFixture(const char* name)
    {
        std::ifstream file(std::string{PALCFG_FIXTURE_DIR} + "/" + name, std::ios::binary);
        REQUIRE(file.is_open());

        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
} // namespace

TEST_CASE("the schema covers every field PerkyPals declares")
{
    STATIC_REQUIRE(kFields.size() == 27);
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

    // Every key in the file is claimed by a field, and the file's 27 keys match
    // the schema's 27, so neither side has drifted from the other.
    CHECK(result.unknownKeys.empty());
    CHECK(result.fieldsLoaded == 27);

    // The file holds the defaults, so the load must land on the same values.
    CHECK(s == Settings{});
}

// The shipped file holds defaults throughout, so this is what proves a value
// actually travels from the document into the member it names.
TEST_CASE("every field reads from the document rather than defaulting")
{
    Settings s{};
    PalCfg::CollectingSink sink;
    const PalCfg::LoadResult result = PalCfg::LoadFromText(kFields, kAllChanged, s, sink);

    REQUIRE(result.Ok());
    CHECK(result.unknownKeys.empty());
    CHECK(result.fieldsLoaded == 27);
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
    CHECK(s.PlayerHoldHungerAtLeast == 0.1f);
    CHECK(s.PlayerHoldHealthAtLeast == 0.2f);
    CHECK(s.DebugEnabled);
    CHECK(s.Trace);
    CHECK_FALSE(s.EnableDriver);
    CHECK_FALSE(s.EnableGate);
    CHECK_FALSE(s.EnableRegistry);
    CHECK_FALSE(s.SanityEnabled);
    CHECK(s.SanityRiseThreshold == 0.15f);
    CHECK(s.SanityThreshold == 0.25f);
    CHECK_FALSE(s.ActivityEnabled);
    CHECK_FALSE(s.ConditionEnabled);
    CHECK(s.ConditionHungerBelow == 0.1f);
    CHECK(s.ConditionHealthBelow == 0.2f);
    CHECK(s.ConditionStaminaAtMost == 0.3f);
    CHECK(s.ConditionWeightAbove == 0.4f);
    CHECK(s.ConditionHurtHold == 1.5f);
    CHECK(s.ConditionDrainHold == 2.5f);
}

// An empty morph list would drive nothing, so the default is the safer read.
TEST_CASE("an empty morph target list keeps the default")
{
    Settings s{};
    const PalCfg::LoadResult result = PalCfg::LoadFromText(kFields, R"({"morphTargets": []})", s);

    REQUIRE(result.Ok());
    CHECK(s.MorphTargets == std::vector<std::wstring>{L"Aroused"});
}

// The pre-migration loader swapped these by hand after parsing.
TEST_CASE("inverted delay, multiplier and sanity bounds are put back in order")
{
    Settings s{};
    const PalCfg::LoadResult result = PalCfg::LoadFromText(
        kFields,
        R"({"onsetDelayMin": 20, "onsetDelayMax": 4,
            "rateMultiplierMin": 3.0, "rateMultiplierMax": 1.0,
            "sanity": {"riseThreshold": 0.9, "threshold": 0.6}})",
        s);

    REQUIRE(result.Ok());
    CHECK(s.OnsetDelayMin == 4.0f);
    CHECK(s.OnsetDelayMax == 20.0f);
    CHECK(s.RateMultiplierMin == 1.0f);
    CHECK(s.RateMultiplierMax == 3.0f);
    CHECK(s.SanityRiseThreshold == 0.6f);
    CHECK(s.SanityThreshold == 0.9f);
}

TEST_CASE("a rendered document loads back to the same settings")
{
    Settings loaded{};
    REQUIRE(PalCfg::LoadFromText(kFields, kAllChanged, loaded).fieldsLoaded == 27);

    const std::string rendered = PalCfg::RenderDocument(kFields, loaded);

    Settings reloaded{};
    PalCfg::CollectingSink sink;
    const auto result = PalCfg::LoadFromText(kFields, rendered, reloaded, sink);

    CHECK(result.fieldsLoaded == 27);
    CHECK(result.unknownKeys.empty());
    CHECK(sink.All().empty());
    CHECK(reloaded == loaded);
}
