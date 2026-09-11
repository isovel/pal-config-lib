// The worked example the README shows for extending ValueTraits. Kept as a test
// so the documented shape cannot drift from one that compiles.

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <string>
#include <string_view>

#include <PalCfg/Load.hpp>
#include <PalCfg/Schema.hpp>
#include <PalCfg/Write.hpp>

namespace
{
    // A UE4SS mod's diagnostic hotkey, spelled the way a user writes it.
    struct Hotkey
    {
        bool alt = false;
        bool ctrl = false;
        char key = '\0';

        bool operator==(const Hotkey&) const = default;
    };
} // namespace

template <>
struct PalCfg::ValueTraits<Hotkey>
{
    static constexpr PalCfg::Kind kKind = PalCfg::Kind::String;

    static bool Read(Hotkey& out, const PalCfg::IValueSource& source, const PalCfg::ReadContext&)
    {
        std::string text;
        if (!PalCfg::CoerceUtf8(source, text)) return false;

        Hotkey parsed;
        std::size_t at = 0;
        while (at < text.size())
        {
            const auto plus = text.find('+', at);
            const std::string_view part{text.data() + at, (plus == std::string::npos ? text.size() : plus) - at};

            if (PalCfg::EqualsIgnoringCase(part, "alt")) parsed.alt = true;
            else if (PalCfg::EqualsIgnoringCase(part, "ctrl")) parsed.ctrl = true;
            else if (part.size() == 1) parsed.key = static_cast<char>(std::toupper(part.front()));
            else return false;

            if (plus == std::string::npos) break;
            at = plus + 1;
        }

        if (parsed.key == '\0') return false;

        out = parsed;
        return true;
    }

    static void Write(const Hotkey& value, std::string& out)
    {
        std::string text;
        if (value.ctrl) text += "Ctrl+";
        if (value.alt) text += "Alt+";
        text += value.key;

        PalCfg::AppendJsonString(out, text);
    }
};

namespace
{
    struct Settings
    {
        Hotkey Dump{.alt = true, .key = 'U'};
    };

    inline constexpr auto kSchema =
        PalCfg::Schema<Settings>("Example").Field("dumpKey", &Settings::Dump).Help("Dumps the reflection table.");

    inline constexpr auto kFields = kSchema.Flatten();
} // namespace

TEST_CASE("a mod's own type reads through its ValueTraits", "[traits]")
{
    Settings settings;
    REQUIRE(PalCfg::LoadFromText(kFields, R"({ "dumpKey": "ctrl+alt+k" })", settings).fieldsLoaded == 1);

    CHECK(settings.Dump == Hotkey{.alt = true, .ctrl = true, .key = 'K'});
}

TEST_CASE("a mod's own type keeps its default when the text makes no sense", "[traits]")
{
    Settings settings;
    PalCfg::CollectingSink sink;
    PalCfg::LoadFromText(kFields, R"({ "dumpKey": "Meta+Q" })", settings, sink);

    CHECK(settings.Dump == Hotkey{.alt = true, .key = 'U'});
    CHECK(sink.Has(PalCfg::Severity::Warning));
}

TEST_CASE("a mod's own type renders back into the generated file", "[traits]")
{
    Settings settings;
    REQUIRE(PalCfg::LoadFromText(kFields, R"({ "dumpKey": "ctrl+alt+k" })", settings).fieldsLoaded == 1);

    CHECK(PalCfg::RenderDocument(kFields, settings) ==
          "{\n"
          "    // Dumps the reflection table.\n"
          "    \"dumpKey\": \"Ctrl+Alt+K\"\n"
          "}\n");
}
