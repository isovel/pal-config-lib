#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <PalCfg/Document.hpp>
#include <PalCfg/Schema.hpp>
#include <PalCfg/Write.hpp>

namespace
{
    struct Scalars
    {
        bool Enabled = true;
        int Count = 5;
        float Rate = 0.08f;
        std::string Name = "Aroused";
    };

    inline constexpr auto kScalars = PalCfg::Schema<Scalars>("PerkyPals")
                                         .Field("enabled", &Scalars::Enabled)
                                         .Field("count", &Scalars::Count)
                                         .Field("rate", &Scalars::Rate)
                                         .Field("name", &Scalars::Name);

    inline constexpr auto kScalarFields = kScalars.Flatten();
} // namespace

TEST_CASE("a scalar field renders as its JSON literal", "[write]")
{
    const Scalars settings{};

    REQUIRE(PalCfg::RenderDocument(kScalarFields, settings) ==
            "{\n"
            "    \"enabled\": true,\n"
            "    \"count\": 5,\n"
            "    \"rate\": 0.08,\n"
            "    \"name\": \"Aroused\"\n"
            "}\n");
}

namespace
{
    struct Nested
    {
        bool Enabled = true;
        bool PlayerEnabled = true;
        std::string PlayerName = "Ayaka";
        bool Trace = false;
    };

    inline constexpr auto kNested = PalCfg::Schema<Nested>("PerkyPals")
                                        .Field("enabled", &Nested::Enabled)
                                        .Field("player.enabled", &Nested::PlayerEnabled)
                                        .Field("player.name", &Nested::PlayerName)
                                        .Field("debug.trace", &Nested::Trace);

    inline constexpr auto kNestedFields = kNested.Flatten();
} // namespace

TEST_CASE("a dotted key nests inside an object", "[write]")
{
    const Nested settings{};

    REQUIRE(PalCfg::RenderDocument(kNestedFields, settings) ==
            "{\n"
            "    \"enabled\": true,\n"
            "    \"player\": {\n"
            "        \"enabled\": true,\n"
            "        \"name\": \"Ayaka\"\n"
            "    },\n"
            "    \"debug\": {\n"
            "        \"trace\": false\n"
            "    }\n"
            "}\n");
}

namespace
{
    struct Documented
    {
        float Rate = 0.08f;
        int Count = 5;
    };

    inline constexpr auto kDocumented =
        PalCfg::Schema<Documented>("PerkyPals")
            .Field("rate", &Documented::Rate)
            .Field("count", &Documented::Count)
            .Help("Weight per second while rising.\n0.08 is a full climb in ~12s.");

    inline constexpr auto kDocumentedFields = kDocumented.Flatten();
} // namespace

TEST_CASE("help text becomes comment lines above the key", "[write]")
{
    const Documented settings{};

    REQUIRE(PalCfg::RenderDocument(kDocumentedFields, settings) ==
            "{\n"
            "    \"rate\": 0.08,\n"
            "\n"
            "    // Weight per second while rising.\n"
            "    // 0.08 is a full climb in ~12s.\n"
            "    \"count\": 5\n"
            "}\n");
}

TEST_CASE("help text is omitted on request", "[write]")
{
    const Documented settings{};

    PalCfg::WriteOptions options;
    options.includeHelp = false;

    REQUIRE(PalCfg::RenderDocument(kDocumentedFields, settings, options) ==
            "{\n"
            "    \"rate\": 0.08,\n"
            "    \"count\": 5\n"
            "}\n");
}

namespace
{
    enum class Mood
    {
        Quiet,
        Loud,
    };

    struct Compound
    {
        std::vector<std::string> Targets{"Aroused", "Blush"};
        std::vector<std::string> Empty{};
        std::wstring Title = L"Ayaka";
        std::optional<int> Limit = 3;
        std::optional<int> Unset{};
        Mood Tone = Mood::Loud;
    };

    inline constexpr auto kCompound = PalCfg::Schema<Compound>("PerkyPals")
                                          .Field("targets", &Compound::Targets)
                                          .Field("empty", &Compound::Empty)
                                          .Field("title", &Compound::Title)
                                          .Field("limit", &Compound::Limit)
                                          .Field("unset", &Compound::Unset)
                                          .Field("tone", &Compound::Tone);

    inline constexpr auto kCompoundFields = kCompound.Flatten();
} // namespace

template <>
struct PalCfg::EnumNames<Mood>
{
    static constexpr std::array<std::pair<std::string_view, Mood>, 2> kValues{
        {{"quiet", Mood::Quiet}, {"loud", Mood::Loud}}};
};

TEST_CASE("lists, text, options and enums render as their JSON forms", "[write]")
{
    const Compound settings{};

    REQUIRE(PalCfg::RenderDocument(kCompoundFields, settings) ==
            "{\n"
            "    \"targets\": [\"Aroused\", \"Blush\"],\n"
            "    \"empty\": [],\n"
            "    \"title\": \"Ayaka\",\n"
            "    \"limit\": 3,\n"
            "    \"unset\": null,\n"
            "    \"tone\": \"loud\"\n"
            "}\n");
}

namespace
{
    struct Known
    {
        bool Enabled = true;
        bool Trace = false;
    };

    inline constexpr auto kKnown = PalCfg::Schema<Known>("PerkyPals")
                                       .Field("enabled", &Known::Enabled)
                                       .Field("debug.trace", &Known::Trace);

    inline constexpr auto kKnownFields = kKnown.Flatten();
} // namespace

TEST_CASE("a key no field claims is carried forward", "[write]")
{
    PalCfg::Document previous;
    REQUIRE(previous.Parse(R"({
        "enabled": false,
        "legacy": { "a": 1 },
        "debug": { "trace": true, "oldFlag": true }
    })"));

    const Known settings{};

    REQUIRE(PalCfg::RenderDocument(kKnownFields, settings, previous.Root()) ==
            "{\n"
            "    \"enabled\": true,\n"
            "    \"debug\": {\n"
            "        \"trace\": false,\n"
            "\n"
            "        // Kept from the previous file; not a setting this version knows.\n"
            "        \"oldFlag\": true\n"
            "    },\n"
            "\n"
            "    // Kept from the previous file; not a setting this version knows.\n"
            "    \"legacy\": {\"a\":1}\n"
            "}\n");
}
