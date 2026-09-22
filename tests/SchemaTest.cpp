#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        float ArousalRate = 0.08f;
        bool PlayerEnabled = true;
    };
} // namespace

TEST_CASE("an empty schema reports its mod id and no fields")
{
    constexpr auto schema = PalCfg::Schema<Settings>("PerkyPals");

    CHECK(schema.Count == 0);
    CHECK(std::string_view{schema.ModId()} == "PerkyPals");
}

// Schemas live at namespace scope, as they do in a real mod: Flatten() borrows
// addresses into the schema, so it needs static storage duration to be usable in
// a constant expression.
inline constexpr auto kOneField =
    PalCfg::Schema<Settings>("PerkyPals").Field("arousalRate", &Settings::ArousalRate);

TEST_CASE(".Field() grows the schema and records the key")
{
    STATIC_REQUIRE(kOneField.Count == 1);

    constexpr auto flat = kOneField.Flatten();
    STATIC_REQUIRE(flat.size() == 1);
    CHECK(std::string_view{flat[0].meta.key} == "arousalRate");
}

inline constexpr auto kLabelled = PalCfg::Schema<Settings>("PerkyPals")
                                      .Field("arousalRate", &Settings::ArousalRate)
                                          .Label("Arousal rate");

TEST_CASE(".Label() attaches to the preceding field")
{
    constexpr auto flat = kLabelled.Flatten();

    CHECK(std::string_view{flat[0].meta.label} == "Arousal rate");
}

inline constexpr auto kTwoFields = PalCfg::Schema<Settings>("PerkyPals")
                                       .Field("arousalRate", &Settings::ArousalRate)
                                           .Label("Arousal rate")
                                           .Range(0.0, 1.0)
                                       .Field("player.enabled", &Settings::PlayerEnabled)
                                           .Label("Drive the player");

TEST_CASE("a modifier does not leak onto the following field")
{
    constexpr auto flat = kTwoFields.Flatten();

    STATIC_REQUIRE(flat.size() == 2);
    CHECK(flat[0].meta.hasRange);
    CHECK_FALSE(flat[1].meta.hasRange);
    CHECK(std::string_view{flat[1].meta.label} == "Drive the player");
}

TEST_CASE("each field binds to its own member")
{
    constexpr auto flat = kTwoFields.Flatten();

    STATIC_REQUIRE(flat[0].boundMemberPtr != flat[1].boundMemberPtr);
    STATIC_REQUIRE(flat[0].boundMemberPtr != nullptr);
}

TEST_CASE(".Range() records both bounds and marks the field as bounded")
{
    constexpr auto flat = kTwoFields.Flatten();

    CHECK(flat[0].meta.min == 0.0);
    CHECK(flat[0].meta.max == 1.0);
}

inline constexpr auto kAliased = PalCfg::Schema<Settings>("PerkyPals")
                                     .Field("arousalRate", &Settings::ArousalRate)
                                         .Alias("arousal_rate")
                                         .Alias("rate");

TEST_CASE(".Alias() accumulates in declaration order")
{
    constexpr auto flat = kAliased.Flatten();

    REQUIRE(flat[0].meta.aliasCount == 2);
    CHECK(std::string_view{flat[0].meta.aliases[0]} == "arousal_rate");
    CHECK(std::string_view{flat[0].meta.aliases[1]} == "rate");
}

inline constexpr auto kFlagged = PalCfg::Schema<Settings>("PerkyPals")
                                     .Field("morphTargets", &Settings::ArousalRate)
                                         .Help("Weight per second while rising")
                                         .KeepDefaultIfEmpty()
                                         .Hidden()
                                         .Advanced();

TEST_CASE("the presentation and parsing flags default off and are set on request")
{
    constexpr auto plain = kTwoFields.Flatten();
    CHECK_FALSE(plain[0].meta.keepDefaultIfEmpty);
    CHECK_FALSE(plain[0].meta.hidden);
    CHECK_FALSE(plain[0].meta.advanced);
    CHECK(plain[0].meta.help == nullptr);

    constexpr auto flagged = kFlagged.Flatten();
    CHECK(flagged[0].meta.keepDefaultIfEmpty);
    CHECK(flagged[0].meta.hidden);
    CHECK(flagged[0].meta.advanced);
    CHECK(std::string_view{flagged[0].meta.help} == "Weight per second while rising");
}

namespace
{
    struct Kinds
    {
        std::vector<int> Ints;
        std::vector<std::string> Names;
        float Rate = 0.0f;
    };

    inline constexpr auto kKinds = PalCfg::Schema<Kinds>("Kinds")
                                       .Field("ints", &Kinds::Ints)
                                       .Field("names", &Kinds::Names)
                                       .Field("rate", &Kinds::Rate);
    inline constexpr auto kKindFields = kKinds.Flatten();
}

TEST_CASE("a list field's ops name its element kind")
{
    CHECK(kKindFields[0].ops->kind == PalCfg::Kind::List);
    CHECK(kKindFields[0].ops->elementKind == PalCfg::Kind::Int);
    CHECK(kKindFields[1].ops->elementKind == PalCfg::Kind::String);
    CHECK(kKindFields[2].ops->elementKind == PalCfg::Kind::Unknown);
}
