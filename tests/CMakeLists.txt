include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(palcfg_tests
    SchemaTest.cpp
    SchemaJsonTest.cpp
    JsoncTest.cpp
    DocumentTest.cpp
    UtfTest.cpp
    LoadTest.cpp
    KeyResolutionTest.cpp
    CoercionTest.cpp
    FieldPairTest.cpp
    DiagnosticsTest.cpp
    PerkyPalsFixtureTest.cpp
    WriteTest.cpp
    FileTest.cpp
    GenerateTest.cpp
    GeneratorToolTest.cpp
    ConfigFileTest.cpp
    LogSinkTest.cpp
    PathsTest.cpp
    CustomTraitTest.cpp
    RegistryLinkTest.cpp
)
target_link_libraries(palcfg_tests PRIVATE PalCfg::Core Catch2::Catch2WithMain)
if(PALCFG_BUILD_REGISTRY)
    target_sources(palcfg_tests PRIVATE RegistryTest.cpp)
    target_link_libraries(palcfg_tests PRIVATE PalCfg::Registry)
endif()
set(PALCFG_GENERATED_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/example.config.json")
palcfg_add_generator(palcfg_example_gen
    SOURCES generator/main.cpp
    OUTPUT "${PALCFG_GENERATED_CONFIG}")
add_dependencies(palcfg_tests palcfg_example_gen_run)

target_compile_definitions(palcfg_tests PRIVATE
    PALCFG_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    PALCFG_GENERATED_CONFIG="${PALCFG_GENERATED_CONFIG}")

if(MSVC)
    target_compile_options(palcfg_tests PRIVATE /W4)
else()
    target_compile_options(palcfg_tests PRIVATE -Wall -Wextra)
endif()

# Public headers must not reach for json.hpp or Windows.h. The trap directory
# comes first on the include path, so either include becomes a build failure.
add_library(palcfg_header_hygiene OBJECT HeaderHygieneTest.cpp)
target_include_directories(palcfg_header_hygiene BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/trap")
target_link_libraries(palcfg_header_hygiene PRIVATE PalCfg::Core)
add_dependencies(palcfg_tests palcfg_header_hygiene)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(palcfg_tests)
