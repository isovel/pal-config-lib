#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <PalCfg/Document.hpp>
#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    enum class LogVerbosity
    {
        Quiet = 0,
        Normal = 1,
        Discovery = 2,
    };

    struct Settings
    {
        float ArousalRate = 0.08f;
        bool PlayerEnabled = true;
        int HoldTicks = 60;
        double DecayRate = 0.10;
        std::string Nickname{"pal"};
        std::wstring MorphTarget{L"Aroused"};
        std::vector<std::wstring> MorphTargets{L"Aroused"};
        std::vector<float> IdleTasks{0.0f};
        std::optional<bool> IsRare{};
        std::unordered_set<std::string> NeverHide{};
        LogVerbosity Verbosity = LogVerbosity::Normal;
    };

} // namespace

// The enum's names, declared the way a consuming mod would. iaho's config.ini
// spells these exactly so, and keeping the spelling makes its migration lossless.
template <>
struct PalCfg::EnumNames<LogVerbosity>
{
    static constexpr std::array<std::pair<std::string_view, LogVerbosity>, 3> kValues{{
        {"quiet", LogVerbosity::Quiet},
        {"normal", LogVerbosity::Normal},
        {"discovery", LogVerbosity::Discovery},
    }};
};

namespace
{
    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("PerkyPals")
            .Field("arousalRate", &Settings::ArousalRate)
            .Field("playerEnabled", &Settings::PlayerEnabled)
            .Field("holdTicks", &Settings::HoldTicks)
            .Field("decayRate", &Settings::DecayRate)
            .Field("nickname", &Settings::Nickname)
            .Field("morphTarget", &Settings::MorphTarget)
            .Field("morphTargets", &Settings::MorphTargets).KeepDefaultIfEmpty()
            .Field("idleTasks", &Settings::IdleTasks)
            .Field("isRare", &Settings::IsRare)
            .Field("neverHide", &Settings::NeverHide)
            .Field("verbosity", &Settings::Verbosity);

    inline constexpr auto kFields = kSchema.Flatten();

    Settings Load(const char* json)
    {
        PalCfg::Document doc;
        REQUIRE(doc.Parse(json));

        Settings settings{};
        PalCfg::LoadFields(kFields, doc.Root(), settings);
        return settings;
    }
} // namespace

TEST_CASE("scalar fields load from the document")
{
    const Settings s = Load(R"({"arousalRate": 0.5, "playerEnabled": false,
                                "holdTicks": 90, "decayRate": 0.25})");

    CHECK(s.ArousalRate == 0.5f);
    CHECK(s.PlayerEnabled == false);
    CHECK(s.HoldTicks == 90);
    CHECK(s.DecayRate == 0.25);
}

TEST_CASE("an absent key leaves the struct's own default standing")
{
    const Settings s = Load(R"({})");

    CHECK(s.ArousalRate == 0.08f);
    CHECK(s.PlayerEnabled == true);
    CHECK(s.Nickname == "pal");
    CHECK(s.MorphTargets.size() == 1);
}

TEST_CASE("string fields load, and wide strings decode UTF-8 properly")
{
    const Settings s = Load(R"({"nickname": "Depresso", "morphTarget": "Blushé"})");

    CHECK(s.Nickname == "Depresso");
    REQUIRE(s.MorphTarget.size() == 6);
    CHECK(s.MorphTarget == L"Blushé");
}

TEST_CASE("list fields load every element in order")
{
    const Settings s = Load(R"({"morphTargets": ["Aroused", "Blush"], "idleTasks": [0, 1, 6]})");

    REQUIRE(s.MorphTargets.size() == 2);
    CHECK(s.MorphTargets[1] == L"Blush");
    REQUIRE(s.IdleTasks.size() == 3);
    CHECK(s.IdleTasks[2] == 6.0f);
}

TEST_CASE("KeepDefaultIfEmpty decides what an empty list means")
{
    SECTION("set: the default survives")
    {
        const Settings s = Load(R"({"morphTargets": []})");
        REQUIRE(s.MorphTargets.size() == 1);
        CHECK(s.MorphTargets[0] == L"Aroused");
    }
    SECTION("unset: an empty list is a meaningful answer")
    {
        const Settings s = Load(R"({"idleTasks": []})");
        CHECK(s.IdleTasks.empty());
    }
}

TEST_CASE("an optional field distinguishes absent from present")
{
    CHECK_FALSE(Load(R"({})").IsRare.has_value());

    const Settings s = Load(R"({"isRare": true})");
    REQUIRE(s.IsRare.has_value());
    CHECK(*s.IsRare);
}

TEST_CASE("a set field collects unique entries")
{
    const Settings s = Load(R"({"neverHide": ["Key_01", "Key_02", "Key_01"]})");

    CHECK(s.NeverHide.size() == 2);
    CHECK(s.NeverHide.contains("Key_01"));
}

TEST_CASE("an enum field accepts its name and its underlying number")
{
    CHECK(Load(R"({"verbosity": "discovery"})").Verbosity == LogVerbosity::Discovery);
    CHECK(Load(R"({"verbosity": "Quiet"})").Verbosity == LogVerbosity::Quiet);
    CHECK(Load(R"({"verbosity": 2})").Verbosity == LogVerbosity::Discovery);
}

TEST_CASE("an unknown enum name keeps the default")
{
    CHECK(Load(R"({"verbosity": "nonsense"})").Verbosity == LogVerbosity::Normal);
}
