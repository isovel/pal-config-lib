#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <PalCfg/File.hpp>

namespace
{
    std::filesystem::path TempPath(const char* name)
    {
        const auto path = std::filesystem::temp_directory_path() / "palcfg-tests" / name;
        std::filesystem::create_directories(path.parent_path());
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        return path;
    }

    std::string Contents(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }
} // namespace

TEST_CASE("writing a file that does not exist leaves no backup", "[file]")
{
    const auto path = TempPath("fresh.json");

    const auto result = PalCfg::WriteFileIfChanged(path.string(), "{}\n");

    CHECK(result.written);
    CHECK_FALSE(result.backedUp);
    CHECK(result.error.empty());
    CHECK(Contents(path) == "{}\n");
}

TEST_CASE("replacing a file keeps the old one alongside it", "[file]")
{
    const auto path = TempPath("replaced.json");
    REQUIRE(PalCfg::WriteFileIfChanged(path.string(), "{\"a\":1}\n").written);

    const auto result = PalCfg::WriteFileIfChanged(path.string(), "{\"a\":2}\n");

    CHECK(result.written);
    CHECK(result.backedUp);
    CHECK(Contents(path) == "{\"a\":2}\n");
    CHECK(Contents(path.string() + ".bak") == "{\"a\":1}\n");
}

TEST_CASE("writing what the file already holds changes nothing", "[file]")
{
    const auto path = TempPath("same.json");
    REQUIRE(PalCfg::WriteFileIfChanged(path.string(), "{\"a\":1}\n").written);

    const auto result = PalCfg::WriteFileIfChanged(path.string(), "{\"a\":1}\n");

    CHECK_FALSE(result.written);
    CHECK_FALSE(result.backedUp);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".bak"));
}

TEST_CASE("a file that is not there reads as absent", "[file]")
{
    const auto path = TempPath("missing.json");

    std::string text;
    CHECK_FALSE(PalCfg::ReadFileText(path.string(), text));
    CHECK(text.empty());
}
