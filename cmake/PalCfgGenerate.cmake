# Builds a mod's generator tool and runs it during the build, so its config file
# is regenerated from the schema whenever the schema changes.
#
# The tool has to run on the machine doing the build. A cross-compiled mod MUST
# therefore either configure this target for the host separately or run the tool
# under Wine; palcfg_add_generator assumes it can execute what it just built.
#
#   palcfg_add_generator(example_gen
#       SOURCES tools/GenerateConfig.cpp
#       OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/config.default.json)
function(palcfg_add_generator target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OUTPUT" "SOURCES")

    if(NOT ARG_SOURCES OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "palcfg_add_generator: SOURCES and OUTPUT are both required")
    endif()

    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE PalCfg::Core)

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND ${target} "${ARG_OUTPUT}"
        DEPENDS ${target} ${ARG_SOURCES}
        COMMENT "Generating ${ARG_OUTPUT}"
        VERBATIM)

    add_custom_target(${target}_run ALL DEPENDS "${ARG_OUTPUT}")
endfunction()
