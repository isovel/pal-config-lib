#include <catch2/catch_test_macros.hpp>

#include <string>

#include <PalCfg/Document.hpp>

namespace
{
    // Visits `key` and fails when the member is absent or the visitor never runs.
    // Without this guard, assertions inside a visitor are silently skipped when
    // Child() returns false, and the test passes having checked nothing.
    template <class Fn>
    void RequireChild(const PalCfg::IValueSource& node, const char* key, Fn&& fn)
    {
        bool visited = false;
        const bool found = node.Child(key, [&](const PalCfg::IValueSource& value) {
            visited = true;
            fn(value);
        });
        REQUIRE(found);
        REQUIRE(visited);
    }

    template <class Fn>
    void RequireElement(const PalCfg::IValueSource& node, std::size_t index, Fn&& fn)
    {
        bool visited = false;
        const bool found = node.Element(index, [&](const PalCfg::IValueSource& value) {
            visited = true;
            fn(value);
        });
        REQUIRE(found);
        REQUIRE(visited);
    }
} // namespace

TEST_CASE("a valid object parses and its root is an object")
{
    PalCfg::Document doc;

    REQUIRE(doc.Parse(R"({"a": 1})"));
    CHECK(doc.Ok());
    CHECK(doc.Root().IsObject());
}

TEST_CASE("a malformed document fails and reports why")
{
    PalCfg::Document doc;

    CHECK_FALSE(doc.Parse("{not json"));
    CHECK_FALSE(doc.Ok());
    CHECK_FALSE(doc.Error().message.empty());
}

TEST_CASE("parsing sanitises the text first and reports what it changed")
{
    PalCfg::Document doc;

    REQUIRE(doc.Parse("\xEF\xBB\xBF{\"a\": [1,],}"));
    CHECK(doc.Sanitised().bomRemoved);
    CHECK(doc.Sanitised().trailingCommasRemoved == 2);
}

TEST_CASE("comments are accepted")
{
    PalCfg::Document doc;

    REQUIRE(doc.Parse("{\n  // the rate\n  \"a\": 1\n}"));
    CHECK(doc.Root().IsObject());
}

TEST_CASE("Child() reaches a member and reports a missing one")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"arousalRate": 0.5})"));

    double found = 0.0;
    CHECK(doc.Root().Child("arousalRate", [&](const PalCfg::IValueSource& v) { v.AsDouble(found); }));
    CHECK(found == 0.5);

    bool visited = false;
    CHECK_FALSE(doc.Root().Child("absent", [&](const PalCfg::IValueSource&) { visited = true; }));
    CHECK_FALSE(visited);
}

TEST_CASE("scalars report the type they hold")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"b": true, "i": 7, "d": 0.25, "s": "Aroused", "n": null})"));
    const auto& root = doc.Root();

    RequireChild(root, "b", [](const PalCfg::IValueSource& v) {
        CHECK(v.IsBool());
        bool out = false;
        CHECK(v.AsBool(out));
        CHECK(out);
    });
    RequireChild(root, "i", [](const PalCfg::IValueSource& v) {
        CHECK(v.IsNumber());
        std::int64_t out = 0;
        CHECK(v.AsInt64(out));
        CHECK(out == 7);
        double asDouble = 0.0;
        CHECK(v.AsDouble(asDouble));
        CHECK(asDouble == 7.0);
    });
    RequireChild(root, "d", [](const PalCfg::IValueSource& v) {
        double out = 0.0;
        CHECK(v.AsDouble(out));
        CHECK(out == 0.25);
        std::int64_t asInt = 0;
        CHECK_FALSE(v.AsInt64(asInt));
    });
    RequireChild(root, "s", [](const PalCfg::IValueSource& v) {
        CHECK(v.IsString());
        std::string out;
        CHECK(v.AsUtf8(out));
        CHECK(out == "Aroused");
    });
    RequireChild(root, "n", [](const PalCfg::IValueSource& v) { CHECK(v.IsNull()); });
}

TEST_CASE("a mismatched read fails and leaves the output alone")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"s": "text"})"));

    RequireChild(doc.Root(), "s", [](const PalCfg::IValueSource& v) {
        double untouched = 99.0;
        CHECK_FALSE(v.AsDouble(untouched));
        CHECK(untouched == 99.0);
    });
}

TEST_CASE("arrays report their size and yield elements in order")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"morphTargets": ["Aroused", "Blush"]})"));

    RequireChild(doc.Root(), "morphTargets", [](const PalCfg::IValueSource& list) {
        CHECK(list.IsArray());
        REQUIRE(list.Size() == 2);

        std::string first, second;
        RequireElement(list, 0, [&](const PalCfg::IValueSource& v) { CHECK(v.AsUtf8(first)); });
        RequireElement(list, 1, [&](const PalCfg::IValueSource& v) { CHECK(v.AsUtf8(second)); });
        CHECK(first == "Aroused");
        CHECK(second == "Blush");

        bool visited = false;
        CHECK_FALSE(list.Element(2, [&](const PalCfg::IValueSource&) { visited = true; }));
        CHECK_FALSE(visited);
    });
}

TEST_CASE("ForEachKey lists an object's members")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"a": 1, "b": 2})"));

    std::string joined;
    doc.Root().ForEachKey([&](std::string_view key) {
        joined += key;
        joined += ';';
    });

    CHECK(joined == "a;b;");
}

TEST_CASE("Dump serialises a subtree for the menu wire format")
{
    PalCfg::Document doc;
    REQUIRE(doc.Parse(R"({"player": {"enabled": true}})"));

    RequireChild(doc.Root(), "player", [](const PalCfg::IValueSource& v) {
        std::string json;
        v.Dump(json);
        CHECK(json == R"({"enabled":true})");
    });
}
