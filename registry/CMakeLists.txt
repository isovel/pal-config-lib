# Standalone: no PalCfg::Core, no nlohmann, no UE4SS. The menu mod ships this
# DLL; every other mod probes for it at runtime.
add_library(palcfg_registry SHARED Registry.cpp)
target_include_directories(palcfg_registry PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_features(palcfg_registry PRIVATE cxx_std_23)
target_compile_definitions(palcfg_registry PRIVATE PALCFG_REGISTRY_EXPORTS)
set_target_properties(palcfg_registry PROPERTIES
    OUTPUT_NAME "PalCfgRegistry"
    PREFIX ""
    CXX_VISIBILITY_PRESET hidden)
add_library(PalCfg::Registry ALIAS palcfg_registry)
