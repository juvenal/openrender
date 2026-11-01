# The shared plug-in convention (spec 037, US6, SC-010): PluginAbi_Convention.
#
#   conv_current   1.0.0, a table of the required size (a plain C initializer)
#   conv_larger    1.1.0, a table with one more entry
#   conv_smaller   1.0.0, a table below the required size, at the end of a page before a PROT_NONE page
#   conv_major2    2.0.0
#
# All four are strict C17 built from orplugin.h only (conv_table.h). They were written before orplugin.h,
# orplugin.hpp and pluginAbi.h existed (EXCLUDE_FROM_ALL + DISABLED, red state: the compile error of
# `cmake --build build --target test_plugin_convention`); T047 removed the three markers.

foreach(_variant current larger smaller major2)
    string(TOUPPER ${_variant} _define)
    or_plugin_test_module(plugin_convention_${_variant} convention conv_${_variant} conv_fixture.c
        OUTPUT_NAME conv_${_variant}
        DEFS TEST_CONV_VARIANT=VARIANT_${_define})
endforeach()

add_executable(test_plugin_convention test_convention.cpp)
target_link_libraries(test_plugin_convention PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_plugin_convention PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OR_PLUGIN_STAGE_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_plugin_convention PRIVATE cxx_std_20)
add_dependencies(test_plugin_convention
    plugin_convention_current plugin_convention_larger plugin_convention_smaller plugin_convention_major2)

or_plugin_add_test(PluginAbi_Convention 60
    COMMAND test_plugin_convention
        $<TARGET_FILE:plugin_convention_current>
        $<TARGET_FILE:plugin_convention_larger>
        $<TARGET_FILE:plugin_convention_smaller>
        $<TARGET_FILE:plugin_convention_major2>)
