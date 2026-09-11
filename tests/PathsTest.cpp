#include <catch2/catch_test_macros.hpp>

#include <string>

#include <PalCfg/Paths.hpp>

TEST_CASE("a relative path resolves against a directory", "[paths]")
{
    // Where a UE4SS mod's config sits: main.dll is in <mod>/dlls/.
    CHECK(PalCfg::ResolveAgainst("C:/Mods/PerkyPals/dlls", "../config.json") ==
          "C:/Mods/PerkyPals/config.json");
    CHECK(PalCfg::ResolveAgainst("C:/Mods/PerkyPals", "config.json") ==
          "C:/Mods/PerkyPals/config.json");
}

TEST_CASE("an absolute path ignores the directory it is resolved against", "[paths]")
{
    CHECK(PalCfg::ResolveAgainst("C:/Mods/PerkyPals/dlls", "D:/elsewhere/config.json") ==
          "D:/elsewhere/config.json");
}

TEST_CASE("resolving against nothing yields the relative path itself", "[paths]")
{
    CHECK(PalCfg::ResolveAgainst("", "config.json") == "config.json");
}
