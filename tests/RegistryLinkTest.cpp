#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include <PalCfg/ConfigFile.hpp>
#include <PalCfg/Registry.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        int Count = 5;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("Link").Field("count", &Settings::Count);
    inline constexpr auto kFields = kSchema.Flatten();
}

TEST_CASE("a link with no registry stays detached and every call is a no-op")
{
    PalCfg::ConfigFile<Settings, kFields.size()> file(kFields, "/nonexistent/link.json");
    PalCfg::CollectingSink sink;
    file.SetSink(sink);

    PalCfg::RegistryLink<Settings, kFields.size()> link(reinterpret_cast<const void*>(&kFields),
                                                        "../../PalConfigMenu/dlls/PalCfgRegistry.dll");
    CHECK_FALSE(link.Attached());

    link.Attach(file, "{}");
    CHECK_FALSE(link.Attached());
    CHECK(sink.Count(PalCfg::Severity::Note) == 1);
}

TEST_CASE("a ConfigFile with an empty mod id never registers")
{
    PalCfg::ConfigFile<Settings, kFields.size()> file(kFields, "/nonexistent/link.json");
    REQUIRE(std::string{file.ModId()}.empty());
    PalCfg::CollectingSink sink;
    file.SetSink(sink);

    PalCfg::RegistryLink<Settings, kFields.size()> link(reinterpret_cast<const void*>(&kFields),
                                                        "../../PalConfigMenu/dlls/PalCfgRegistry.dll");
    link.Attach(file, "{}");
    CHECK_FALSE(link.Attached());
    CHECK(sink.Count(PalCfg::Severity::Note) == 1);
}

TEST_CASE("the link's thunks round-trip through ConfigFile")
{
    const auto path = std::filesystem::temp_directory_path() / "palcfg-tests" / "link.json";
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);

    PalCfg::ConfigFile<Settings, kFields.size()> file(kFields, path.string());
    REQUIRE(file.LoadOrCreate());

    char* doc = PalCfg::RegistryLink<Settings, kFields.size()>::GetDocumentThunk(&file);
    REQUIRE(doc != nullptr);
    CHECK(std::string{doc}.find("\"count\": 5") != std::string::npos);
    PalCfg::RegistryLink<Settings, kFields.size()>::FreeThunk(doc);

    char* result = PalCfg::RegistryLink<Settings, kFields.size()>::SetDocumentThunk(&file, R"({"count": 7})");
    REQUIRE(result != nullptr);
    CHECK(std::string{result} == "[]");
    CHECK(file.Get()->Count == 7);
    PalCfg::RegistryLink<Settings, kFields.size()>::FreeThunk(result);
}
