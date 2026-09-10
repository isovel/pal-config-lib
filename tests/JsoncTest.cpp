#include <catch2/catch_test_macros.hpp>

#include <string>

#include <PalCfg/Jsonc.hpp>

TEST_CASE("a trailing comma before a closing brace is removed")
{
    std::string text = "{\"a\": 1,}";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == "{\"a\": 1}");
    CHECK(result.trailingCommasRemoved == 1);
}

TEST_CASE("a separating comma between members is preserved")
{
    std::string text = R"({"a": 1, "b": 2})";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"a": 1, "b": 2})");
    CHECK(result.trailingCommasRemoved == 0);
}

TEST_CASE("a comma inside a string literal is preserved")
{
    std::string text = R"({"idleActions": "Idle,Wait"})";

    PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"idleActions": "Idle,Wait"})");
}

TEST_CASE("an escaped quote does not end the string")
{
    std::string text = R"({"a": "he said \"go,\" loudly"})";

    PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"a": "he said \"go,\" loudly"})");
}

TEST_CASE("a trailing comma before a closing bracket is removed")
{
    std::string text = R"({"morphTargets": ["Aroused",]})";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"morphTargets": ["Aroused"]})");
    CHECK(result.trailingCommasRemoved == 1);
}

TEST_CASE("a comma in a line comment is preserved")
{
    std::string text = "{\n  // rate, hold, decay\n  \"a\": 1\n}";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == "{\n  // rate, hold, decay\n  \"a\": 1\n}");
    CHECK(result.trailingCommasRemoved == 0);
}

TEST_CASE("a comma in a block comment is preserved")
{
    std::string text = "{\n  /* rate, hold */\n  \"a\": 1\n}";

    PalCfg::SanitiseJsonc(text);

    CHECK(text == "{\n  /* rate, hold */\n  \"a\": 1\n}");
}

TEST_CASE("a trailing comma separated from its brace by a comment is removed")
{
    std::string text = "{\n  \"a\": 1,  // last one\n}";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == "{\n  \"a\": 1  // last one\n}");
    CHECK(result.trailingCommasRemoved == 1);
}

TEST_CASE("every trailing comma in the document is removed")
{
    std::string text = R"({"a": [1,], "b": {"c": 2,},})";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"a": [1], "b": {"c": 2}})");
    CHECK(result.trailingCommasRemoved == 3);
}

TEST_CASE("a UTF-8 BOM is removed and reported")
{
    std::string text = "\xEF\xBB\xBF{\"a\": 1}";

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == R"({"a": 1})");
    CHECK(result.bomRemoved);
}

TEST_CASE("a clean document is left untouched")
{
    std::string text = "{\n  \"a\": 1,\n  \"b\": [2, 3]\n}";
    const std::string original = text;

    const auto result = PalCfg::SanitiseJsonc(text);

    CHECK(text == original);
    CHECK(result.trailingCommasRemoved == 0);
    CHECK_FALSE(result.bomRemoved);
}

// Guards against hang or overrun on malformed input. The parser reports the
// malformation; sanitising must simply survive it.
TEST_CASE("malformed input is survived without hanging")
{
    SECTION("empty")
    {
        std::string text;
        CHECK(PalCfg::SanitiseJsonc(text).trailingCommasRemoved == 0);
        CHECK(text.empty());
    }
    SECTION("a bare BOM")
    {
        std::string text = "\xEF\xBB\xBF";
        CHECK(PalCfg::SanitiseJsonc(text).bomRemoved);
        CHECK(text.empty());
    }
    SECTION("unterminated string")
    {
        std::string text = R"({"a": "no end)";
        PalCfg::SanitiseJsonc(text);
        CHECK(text == R"({"a": "no end)");
    }
    SECTION("unterminated block comment")
    {
        std::string text = "{\n/* forever";
        PalCfg::SanitiseJsonc(text);
        CHECK(text == "{\n/* forever");
    }
    SECTION("trailing backslash inside a string")
    {
        std::string text = R"({"a": "x\)";
        PalCfg::SanitiseJsonc(text);
        CHECK(text == R"({"a": "x\)");
    }
    SECTION("a lone comma")
    {
        std::string text = ",";
        CHECK(PalCfg::SanitiseJsonc(text).trailingCommasRemoved == 0);
    }
    SECTION("a solidus that starts nothing")
    {
        std::string text = R"({"a": 1/})";
        PalCfg::SanitiseJsonc(text);
        CHECK(text == R"({"a": 1/})");
    }
}
