#include <catch2/catch_test_macros.hpp>

#include <string>

#include <PalCfg/Utf.hpp>

TEST_CASE("ASCII survives a round trip")
{
    CHECK(PalCfg::Utf8ToWide("Aroused") == L"Aroused");
    CHECK(PalCfg::WideToUtf8(L"Aroused") == "Aroused");
}

// The bug this replaces: PerkyPals' Widen did std::wstring(s.begin(), s.end()),
// which copies each BYTE into its own wchar_t. "e-acute" arrives as two units
// holding 0xC3 and 0xA9 in place of the single code point U+00E9, so any
// non-ASCII morph target or action name is silently corrupted.
TEST_CASE("a two-byte code point decodes to one code unit")
{
    const std::wstring wide = PalCfg::Utf8ToWide("\xC3\xA9");

    REQUIRE(wide.size() == 1);
    CHECK(wide[0] == static_cast<wchar_t>(0x00E9));
}

TEST_CASE("a three-byte code point decodes to one code unit")
{
    const std::wstring wide = PalCfg::Utf8ToWide("\xE3\x81\x82"); // U+3042 HIRAGANA A

    REQUIRE(wide.size() == 1);
    CHECK(wide[0] == static_cast<wchar_t>(0x3042));
}

TEST_CASE("non-ASCII survives a round trip")
{
    const std::string utf8 = "Pal \xC3\xA9\xE3\x81\x82 name";

    CHECK(PalCfg::WideToUtf8(PalCfg::Utf8ToWide(utf8)) == utf8);
}

TEST_CASE("a code point outside the BMP round trips")
{
    const std::string emoji = "\xF0\x9F\x98\x80"; // U+1F600
    const std::wstring wide = PalCfg::Utf8ToWide(emoji);

    // UTF-16 needs a surrogate pair for this; UTF-32 holds it in one unit.
    CHECK(wide.size() == (sizeof(wchar_t) == 2 ? 2u : 1u));
    CHECK(PalCfg::WideToUtf8(wide) == emoji);
}

TEST_CASE("malformed UTF-8 becomes the replacement character")
{
    const auto Replacement = [] { return PalCfg::Utf8ToWide("\xEF\xBF\xBD"); };

    SECTION("a lone continuation byte")
    {
        CHECK(PalCfg::Utf8ToWide("\x80") == Replacement());
    }
    SECTION("an invalid lead byte")
    {
        CHECK(PalCfg::Utf8ToWide("\xFF") == Replacement());
    }
    SECTION("a truncated three-byte sequence")
    {
        CHECK(PalCfg::Utf8ToWide("\xE3\x81") == Replacement());
    }
    SECTION("a sequence whose continuation is missing")
    {
        // 0xE3 expects two continuations; 'A' is not one, so the lead is replaced
        // and 'A' is decoded normally.
        CHECK(PalCfg::Utf8ToWide("\xE3\x41") == Replacement() + L"A");
    }
    SECTION("an overlong encoding of NUL")
    {
        CHECK(PalCfg::Utf8ToWide("\xC0\x80") == Replacement());
    }
    SECTION("a surrogate encoded as UTF-8")
    {
        CHECK(PalCfg::Utf8ToWide("\xED\xA0\x80") == Replacement()); // U+D800
    }
    SECTION("a code point above U+10FFFF")
    {
        CHECK(PalCfg::Utf8ToWide("\xF4\x90\x80\x80") == Replacement()); // U+110000
    }
}

TEST_CASE("decoding always makes progress on invalid input")
{
    const std::string garbage(4096, '\x80');

    const std::wstring wide = PalCfg::Utf8ToWide(garbage);

    CHECK(wide.size() == garbage.size());
}

TEST_CASE("a lone surrogate in a wide string becomes the replacement character")
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        std::wstring lone;
        lone.push_back(static_cast<wchar_t>(0xD800));

        CHECK(PalCfg::WideToUtf8(lone) == "\xEF\xBF\xBD");
    }
}

TEST_CASE("empty input yields empty output")
{
    CHECK(PalCfg::Utf8ToWide("").empty());
    CHECK(PalCfg::WideToUtf8(L"").empty());
}

// Unicode recommends one U+FFFD per maximal subpart, so a partly-valid sequence
// reads as a single damaged character and the byte that broke it is decoded on
// its own terms.
TEST_CASE("a broken sequence yields one replacement for its maximal subpart")
{
    const std::wstring replacement = PalCfg::Utf8ToWide("\xEF\xBF\xBD");

    CHECK(PalCfg::Utf8ToWide("\xE3\x81\x41") == replacement + L"A");
    CHECK(PalCfg::Utf8ToWide("\xF0\x90\x41") == replacement + L"A");
    CHECK(PalCfg::Utf8ToWide("\xF0\x90\x80") == replacement);
}
