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
        bool Enabled = false;
        int Count = 0;
        float Rate = 0.0f;
        double Precise = 0.0;
        std::string Name{};
        std::vector<std::wstring> Ids{};
        float Bounded = 0.5f;
        int BoundedCount = 5;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("Coerce")
                                       .Field("enabled", &Settings::Enabled)
                                       .Field("count", &Settings::Count)
                                       .Field("rate", &Settings::Rate)
                                       .Field("precise", &Settings::Precise)
                                       .Field("name", &Settings::Name)
                                       .Field("ids", &Settings::Ids)
                                       .Field("bounded", &Settings::Bounded).Range(0.0, 1.0)
                                       .Field("boundedCount", &Settings::BoundedCount).Range(1.0, 10.0);

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

// iaho's IsTruthy accepted 1/true/yes/on; dynamic-pals' SafeGetOptionalBool also
// took numbers. Both spellings keep working.
TEST_CASE("a bool reads from a string or a number")
{
    CHECK(Load(R"({"enabled": "true"})").Enabled);
    CHECK(Load(R"({"enabled": "yes"})").Enabled);
    CHECK(Load(R"({"enabled": "ON"})").Enabled);
    CHECK(Load(R"({"enabled": "1"})").Enabled);
    CHECK(Load(R"({"enabled": 1})").Enabled);
    CHECK(Load(R"({"enabled": 2})").Enabled);

    CHECK_FALSE(Load(R"({"enabled": "false"})").Enabled);
    CHECK_FALSE(Load(R"({"enabled": "off"})").Enabled);
    CHECK_FALSE(Load(R"({"enabled": 0})").Enabled);
}

TEST_CASE("a bool that means nothing keeps the default")
{
    CHECK_FALSE(Load(R"({"enabled": "perhaps"})").Enabled);
    CHECK_FALSE(Load(R"({"enabled": []})").Enabled);
}

// dynamic-pals' SafeGetInt / SafeGetDouble took a numeric string.
TEST_CASE("a number reads from a string")
{
    CHECK(Load(R"({"count": "42"})").Count == 42);
    CHECK(Load(R"({"rate": "0.25"})").Rate == 0.25f);
    CHECK(Load(R"({"precise": "1e-2"})").Precise == 0.01);
}

TEST_CASE("an integer field truncates a fractional number toward zero")
{
    CHECK(Load(R"({"count": 7.9})").Count == 7);
    CHECK(Load(R"({"count": -7.9})").Count == -7);
}

TEST_CASE("a number that means nothing keeps the default")
{
    CHECK(Load(R"({"count": "twelve"})").Count == 0);
    CHECK(Load(R"({"rate": "0.5x"})").Rate == 0.0f);
    CHECK(Load(R"({"count": {}})").Count == 0);
}

TEST_CASE("a string reads from a number or a bool")
{
    CHECK(Load(R"({"name": 7})").Name == "7");
    CHECK(Load(R"({"name": true})").Name == "true");
}

// iaho spelled its id lists as one comma-separated string, so accepting that
// keeps its config.ini values loadable verbatim.
TEST_CASE("a list reads from a separated string")
{
    const Settings comma = Load(R"({"ids": "Key_01, Key_02 ,Key_03"})");
    REQUIRE(comma.Ids.size() == 3);
    CHECK(comma.Ids[0] == L"Key_01");
    CHECK(comma.Ids[2] == L"Key_03");

    const Settings semi = Load(R"({"ids": "A;B"})");
    CHECK(semi.Ids.size() == 2);
}

TEST_CASE("a separated string with nothing in it yields an empty list")
{
    CHECK(Load(R"({"ids": "  "})").Ids.empty());
    CHECK(Load(R"({"ids": ",,"})").Ids.empty());
}

TEST_CASE("a value outside its range is clamped to the bound")
{
    CHECK(Load(R"({"bounded": 7.0})").Bounded == 1.0f);
    CHECK(Load(R"({"bounded": -3.0})").Bounded == 0.0f);
    CHECK(Load(R"({"boundedCount": 99})").BoundedCount == 10);
    CHECK(Load(R"({"boundedCount": 0})").BoundedCount == 1);
}

TEST_CASE("a value inside its range passes through untouched")
{
    CHECK(Load(R"({"bounded": 0.25})").Bounded == 0.25f);
    CHECK(Load(R"({"boundedCount": 5})").BoundedCount == 5);
}
