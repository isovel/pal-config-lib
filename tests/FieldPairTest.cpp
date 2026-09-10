#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include <PalCfg/Document.hpp>
#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        float OnsetMin = 0.0f;
        float OnsetMax = 30.0f;
        float RateMin = 0.5f;
        float RateMax = 1.5f;
    };

    // PerkyPals swapped these by hand after parsing (src/Config.cpp:195-199). The
    // invariant belongs to the pair, so .Range cannot express it.
    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("PerkyPals")
            .FieldPair("onsetDelayMin", &Settings::OnsetMin, "onsetDelayMax", &Settings::OnsetMax)
                .Label("Onset delay")
                .Range(0.0, 300.0)
                .SwapIfInverted()
            .FieldPair("rateMultiplierMin", &Settings::RateMin, "rateMultiplierMax", &Settings::RateMax)
                .Label("Rate multiplier");

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

TEST_CASE("a pair declares two fields")
{
    STATIC_REQUIRE(kFields.size() == 4);
    CHECK(std::string_view{kFields[0].meta.key} == "onsetDelayMin");
    CHECK(std::string_view{kFields[1].meta.key} == "onsetDelayMax");
}

TEST_CASE("a modifier after a pair applies to both of its fields")
{
    CHECK(std::string_view{kFields[0].meta.label} == "Onset delay");
    CHECK(std::string_view{kFields[1].meta.label} == "Onset delay");
    CHECK(kFields[0].meta.hasRange);
    CHECK(kFields[1].meta.hasRange);
    CHECK(kFields[1].meta.max == 300.0);

    CHECK(std::string_view{kFields[2].meta.label} == "Rate multiplier");
    CHECK_FALSE(kFields[2].meta.hasRange);
}

TEST_CASE("inverted bounds are put back in order")
{
    const Settings s = Load(R"({"onsetDelayMin": 30, "onsetDelayMax": 5})");

    CHECK(s.OnsetMin == 5.0f);
    CHECK(s.OnsetMax == 30.0f);
}

TEST_CASE("bounds already in order are left alone")
{
    const Settings s = Load(R"({"onsetDelayMin": 5, "onsetDelayMax": 30})");

    CHECK(s.OnsetMin == 5.0f);
    CHECK(s.OnsetMax == 30.0f);
}

TEST_CASE("equal bounds are left alone")
{
    const Settings s = Load(R"({"onsetDelayMin": 10, "onsetDelayMax": 10})");

    CHECK(s.OnsetMin == 10.0f);
    CHECK(s.OnsetMax == 10.0f);
}

TEST_CASE("a pair that did not ask keeps the inversion it was given")
{
    const Settings s = Load(R"({"rateMultiplierMin": 2.0, "rateMultiplierMax": 1.0})");

    CHECK(s.RateMin == 2.0f);
    CHECK(s.RateMax == 1.0f);
}

TEST_CASE("the swap sees clamped values")
{
    // 999 clamps to 300, which is still above the 5 in min, so they swap.
    const Settings s = Load(R"({"onsetDelayMin": 999, "onsetDelayMax": 5})");

    CHECK(s.OnsetMin == 5.0f);
    CHECK(s.OnsetMax == 300.0f);
}
