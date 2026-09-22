#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <PalCfg/Schema.hpp>
#include <PalCfg/SchemaJson.hpp>

namespace
{
    struct Settings
    {
        float Rate = 0.08f;
        bool Enabled = true;
        std::vector<int> Tasks{1, 2};
        std::string Name = "pal";
        float Lower = 1.0f;
        float Upper = 5.0f;
        int Secret = 0;
    };

    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("Example")
            .Field("rate", &Settings::Rate).Label("Rate").Help("Per second\nSecond line").Range(0.0, 1.0)
            .Field("enabled", &Settings::Enabled)
            .Field("tasks", &Settings::Tasks)
            .Field("name", &Settings::Name).Advanced()
            .FieldPair("lower", &Settings::Lower, "upper", &Settings::Upper).Range(0.0, 10.0)
            .Field("secret", &Settings::Secret).Hidden();

    inline constexpr auto kFields = kSchema.Flatten();
}

TEST_CASE("BuildSchemaJson renders one entry per field in declaration order")
{
    const std::string json = PalCfg::BuildSchemaJson<Settings>("Example", kFields);

    CHECK(json ==
          R"({"name":"Example","fields":[)"
          R"({"key":"rate","label":"Rate","help":"Per second\nSecond line","kind":"float","default":0.08,"min":0.0,"max":1.0,"hidden":false,"advanced":false},)"
          R"({"key":"enabled","label":"","help":"","kind":"bool","default":true,"hidden":false,"advanced":false},)"
          R"({"key":"tasks","label":"","help":"","kind":"list<int>","default":[1, 2],"hidden":false,"advanced":false},)"
          R"({"key":"name","label":"","help":"","kind":"string","default":"pal","hidden":false,"advanced":true},)"
          R"({"key":"lower","label":"","help":"","kind":"float","default":1.0,"min":0.0,"max":10.0,"hidden":false,"advanced":false,"pair":"lower"},)"
          R"({"key":"upper","label":"","help":"","kind":"float","default":5.0,"min":0.0,"max":10.0,"hidden":false,"advanced":false,"pair":"upper"},)"
          R"({"key":"secret","label":"","help":"","kind":"int","default":0,"hidden":true,"advanced":false})"
          "]}");
}

TEST_CASE("KindName covers every scalar kind")
{
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Bool)} == "bool");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Int)} == "int");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Float)} == "float");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Double)} == "float");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::String)} == "string");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Enum)} == "string");
    CHECK(std::string{PalCfg::KindName(PalCfg::Kind::Unknown)} == "unknown");
}
