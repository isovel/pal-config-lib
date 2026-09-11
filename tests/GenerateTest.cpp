#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <PalCfg/Generate.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        bool Enabled = true;
        int Count = 5;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("PerkyPals")
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

    std::string Contents(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }
} // namespace

TEST_CASE("generating writes the schema's defaults", "[generate]")
{
    const auto path = TempPath("generated.json");

    const auto result = PalCfg::GenerateFile(kFields, Settings{}, path.string());

    REQUIRE(result.Ok());
    CHECK(Contents(path) == "{\n    \"enabled\": true,\n    \"count\": 5\n}\n");
}

TEST_CASE("generating over an existing file keeps its unknown keys", "[generate]")
{
    const auto path = TempPath("kept.json");
    Put(path, "{ \"enabled\": false, \"fromTheFuture\": [1, 2] }");

    const auto result = PalCfg::GenerateFile(kFields, Settings{}, path.string());

    REQUIRE(result.Ok());
    CHECK(Contents(path).find("\"fromTheFuture\": [1,2]") != std::string::npos);
    CHECK(Contents(path.string() + ".bak") == "{ \"enabled\": false, \"fromTheFuture\": [1, 2] }");
}

TEST_CASE("generating over an unreadable file refuses to replace it", "[generate]")
{
    const auto path = TempPath("broken.json");
    Put(path, "{ this is not json");

    const auto result = PalCfg::GenerateFile(kFields, Settings{}, path.string());

    CHECK_FALSE(result.Ok());
    CHECK(Contents(path) == "{ this is not json");
}

TEST_CASE("the generator tool names the file it wrote", "[generate]")
{
    const auto path = TempPath("tool.json");
    const std::string argument = path.string();
    const char* argv[] = {"palcfg-gen", argument.c_str()};

    std::ostringstream log;
    const int status = PalCfg::GenerateMain(2, argv, kFields, Settings{}, log);

    CHECK(status == 0);
    CHECK(log.str() == "wrote " + argument + "\n");
}

TEST_CASE("the generator tool asks for a path when given none", "[generate]")
{
    std::ostringstream log;
    const char* argv[] = {"palcfg-gen"};

    CHECK(PalCfg::GenerateMain(1, argv, kFields, Settings{}, log) != 0);
    CHECK(log.str() == "usage: palcfg-gen <path to config file>\n");
}
