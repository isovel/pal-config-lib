// Proves the CMake glue: cmake/PalCfgGenerate.cmake builds tests/generator and
// runs it during the build, so the file below is a build artifact.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

TEST_CASE("the build generates a documented config file", "[generate]")
{
    std::ifstream file(PALCFG_GENERATED_CONFIG, std::ios::binary);
    REQUIRE(file.is_open());

    std::ostringstream text;
    text << file.rdbuf();

    REQUIRE(text.str() ==
            "{\n"
            "    // Whether the mod does anything at all.\n"
            "    \"enabled\": true,\n"
            "\n"
            "    // Weight per second while rising.\n"
            "    \"rate\": 0.08\n"
            "}\n");
}
