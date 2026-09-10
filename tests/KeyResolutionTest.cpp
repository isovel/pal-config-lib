#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <PalCfg/Document.hpp>
#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        bool PlayerEnabled = false;
        float SanityThreshold = 0.8f;
        std::vector<std::wstring> ReqTrait{};
        int Depth = 0;
    };

    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("PerkyPals")
            .Field("player.enabled", &Settings::PlayerEnabled)
            .Field("sanity.threshold", &Settings::SanityThreshold)
            // dynamic-pals accepts PassiveSkills as an older spelling of ReqTrait.
            .Field("reqTrait", &Settings::ReqTrait).Alias("passiveSkills")
            .Field("a.b.c.depth", &Settings::Depth);

    inline constexpr auto kFields = kSchema.Flatten();

    Settings Load(const char* json)
    {
        PalCfg::Document doc;
        REQUIRE(doc.Parse(json));

        Settings settings{};
        PalCfg::LoadFields(kFields, doc.Root(), settings);
        return settings;
    }

    struct Insensitive
    {
        float Rate = 0.0f;
        bool Nested = false;
    };

    inline constexpr auto kInsensitive = PalCfg::Schema<Insensitive>("Case")
                                            .CaseInsensitiveKeys()
                                            .Field("arousalRate", &Insensitive::Rate)
                                            .Field("debug.enableDriver", &Insensitive::Nested);

    inline constexpr auto kInsensitiveFields = kInsensitive.Flatten();
} // namespace

TEST_CASE("a dotted key reads from a nested object")
{
    const Settings s = Load(R"({"player": {"enabled": true}, "sanity": {"threshold": 0.5}})");

    CHECK(s.PlayerEnabled);
    CHECK(s.SanityThreshold == 0.5f);
}

TEST_CASE("a dotted key nests to any depth")
{
    CHECK(Load(R"({"a": {"b": {"c": {"depth": 7}}}})").Depth == 7);
}

TEST_CASE("a missing intermediate object leaves the default standing")
{
    CHECK_FALSE(Load(R"({})").PlayerEnabled);
    CHECK_FALSE(Load(R"({"player": {}})").PlayerEnabled);
    CHECK_FALSE(Load(R"({"player": 5})").PlayerEnabled);
}

TEST_CASE("an alias is accepted when the primary key is absent")
{
    const Settings s = Load(R"({"passiveSkills": ["Legend"]})");

    REQUIRE(s.ReqTrait.size() == 1);
    CHECK(s.ReqTrait[0] == L"Legend");
}

TEST_CASE("the primary key wins over an alias")
{
    const Settings s = Load(R"({"reqTrait": ["Primary"], "passiveSkills": ["Alias"]})");

    REQUIRE(s.ReqTrait.size() == 1);
    CHECK(s.ReqTrait[0] == L"Primary");
}

TEST_CASE("keys match case-insensitively when the schema asks")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"AROUSALRATE": 0.25, "Debug": {"EnableDriver": true}})"));

    Insensitive s{};
    PalCfg::LoadFields(kInsensitiveFields, doc.Root(), s);

    CHECK(s.Rate == 0.25f);
    CHECK(s.Nested);
}

TEST_CASE("case sensitivity is the default")
{
    CHECK_FALSE(Load(R"({"PLAYER": {"ENABLED": true}})").PlayerEnabled);
}
