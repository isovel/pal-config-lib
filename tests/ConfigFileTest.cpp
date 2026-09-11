#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <PalCfg/ConfigFile.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    using namespace std::chrono_literals;

    struct Settings
    {
        bool Enabled = true;
        int Count = 5;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("Example")
                                        .Field("enabled", &Settings::Enabled)
                                        .Field("count", &Settings::Count);

    inline constexpr auto kFields = kSchema.Flatten();

    std::filesystem::path TempPath(const char* name)
    {
        const auto path = std::filesystem::temp_directory_path() / "palcfg-tests" / name;
        std::filesystem::create_directories(path.parent_path());
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        return path;
    }

    void Put(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    const std::chrono::steady_clock::time_point kStart{};
} // namespace

TEST_CASE("loading reads the file into a snapshot", "[reload]")
{
    const auto path = TempPath("live.json");
    Put(path, R"({ "enabled": false, "count": 9 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    CHECK_FALSE(config.Get()->Enabled);
    CHECK(config.Get()->Count == 9);
}

TEST_CASE("a file that is not there leaves the declared defaults", "[reload]")
{
    const auto path = TempPath("absent.json");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    CHECK_FALSE(config.Load());

    CHECK(config.Get()->Enabled);
    CHECK(config.Get()->Count == 5);
}

TEST_CASE("an edit is picked up once the interval has passed", "[reload]")
{
    const auto path = TempPath("edited.json");
    Put(path, R"({ "count": 1 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    int reloads = 0;
    config.SetOnReload([&](const Settings&) { ++reloads; });
    config.SetInterval(1s);

    Put(path, R"({ "count": 2222 })");

    CHECK_FALSE(config.Tick(kStart + 500ms));
    CHECK(config.Get()->Count == 1);

    CHECK(config.Tick(kStart + 1500ms));
    CHECK(config.Get()->Count == 2222);
    CHECK(reloads == 1);
}

TEST_CASE("a file nobody touched is not reloaded", "[reload]")
{
    const auto path = TempPath("untouched.json");
    Put(path, R"({ "count": 1 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    int reloads = 0;
    config.SetOnReload([&](const Settings&) { ++reloads; });
    config.SetInterval(1s);

    CHECK_FALSE(config.Tick(kStart + 1500ms));
    CHECK_FALSE(config.Tick(kStart + 3000ms));
    CHECK(reloads == 0);
}

TEST_CASE("an edit that breaks the file keeps the settings already running", "[reload]")
{
    const auto path = TempPath("broken-reload.json");
    Put(path, R"({ "count": 7 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    PalCfg::CollectingSink sink;
    config.SetSink(sink);
    config.SetInterval(1s);

    int reloads = 0;
    config.SetOnReload([&](const Settings&) { ++reloads; });

    Put(path, "{ this is not json");

    CHECK_FALSE(config.Tick(kStart + 1500ms));
    CHECK(config.Get()->Count == 7);
    CHECK(reloads == 0);
    CHECK(sink.Has(PalCfg::Severity::Error));
}

TEST_CASE("a file the mod wrote itself does not reload it", "[reload]")
{
    const auto path = TempPath("saved.json");
    Put(path, R"({ "count": 3 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    int reloads = 0;
    config.SetOnReload([&](const Settings&) { ++reloads; });
    config.SetInterval(1s);

    Settings edited = *config.Get();
    edited.Count = 44;
    REQUIRE(config.Save(edited));

    CHECK(config.Get()->Count == 44);
    CHECK_FALSE(config.Tick(kStart + 1500ms));
    CHECK(reloads == 0);
}

TEST_CASE("a file rewritten with the same bytes is not a reload", "[reload]")
{
    const auto path = TempPath("touched.json");
    Put(path, R"({ "count": 6 })");

    PalCfg::ConfigFile<Settings, kFields.size()> config{kFields, path.string()};
    REQUIRE(config.Load());

    int reloads = 0;
    config.SetOnReload([&](const Settings&) { ++reloads; });
    config.SetInterval(1s);

    // What an editor saving an unchanged buffer, or a backup tool, leaves behind.
    Put(path, R"({ "count": 6 })");
    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + 5s);

    CHECK_FALSE(config.Tick(kStart + 1500ms));
    CHECK(reloads == 0);
}
