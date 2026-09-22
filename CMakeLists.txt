cmake_minimum_required(VERSION 3.22)
project(PalConfigLib VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PALCFG_IS_TOP_LEVEL ON)
else()
    set(PALCFG_IS_TOP_LEVEL OFF)
endif()

option(PALCFG_BUILD_TESTS "Build host-native tests" ${PALCFG_IS_TOP_LEVEL})
option(PALCFG_STANDALONE "Apply the UE4SS build-type default" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# UE4SS requires one of its custom configurations. Guarded so add_subdirectory
# into a consumer mod does not fight the parent's setting.
if(PALCFG_STANDALONE AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Game__Shipping__Win64" CACHE STRING "Choose the type of build." FORCE)
endif()

# clang-cl has no /std:c++23, so CMake's CXX_STANDARD 23 maps to /std:c++latest
# (C++26), where UE4SS's Unreal headers hit P2864: bitwise ops between different
# enum types become a hard error. Emit a real C++23 flag instead.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "/clang:-std=c++23")
    set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "/clang:-std=gnu++23")
    add_compile_options(-Wno-deprecated-enum-enum-conversion)
endif()

# STATIC, so json.hpp and the JSON backend compile once here and stay out of
# every consumer's translation units.
add_library(palcfg_core STATIC
    "src/core/Jsonc.cpp"
    "src/core/JsonBackend.cpp"
    "src/core/Utf.cpp"
    "src/core/Coerce.cpp"
    "src/core/Diagnostics.cpp"
    "src/core/Write.cpp"
    "src/core/SchemaJson.cpp"
    "src/core/File.cpp"
    "src/core/LogSink.cpp"
    "src/core/Paths.cpp"
)
if(WIN32)
    target_sources(palcfg_core PRIVATE "src/win32/Module.cpp")
endif()

target_include_directories(palcfg_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
# PRIVATE: json.hpp is an implementation detail of this library.
target_include_directories(palcfg_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_features(palcfg_core PUBLIC cxx_std_23)
add_library(PalCfg::Core ALIAS palcfg_core)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/PalCfgGenerate.cmake")

if(PALCFG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
