# Display fixtures and the display defect checks (spec 037, FR-035, FR-039, FR-040).
#
#   v1_legacy       an unversioned legacy driver (C, hand-declared v1 signatures)
#   v1_late_lookup  a legacy driver that calls findParameter from displayData
#   legacy_v1/      the frozen pre-spec file driver (SC-013), built from copies of the base tree
#
# Tests (all red on the starting tree, each for the defect it names):
#   DisplayAbi_ReleaseAfterFinish   FR-035  endDisplays never unloads the module
#   DisplayV1_LateLookup            FR-039  lookups outside displayStart answer from stale state
#   DisplayAbi_UnsupportedParamType FR-040  quad/mpoint/double parameters are counted uninitialized

or_plugin_test_module(plugin_display_v1_legacy display v1_legacy v1_legacy.c)
or_plugin_test_module(plugin_display_v1_late_lookup display v1_late_lookup v1_late_lookup.c)

set(V1_LEGACY_DIR ${OR_PLUGIN_BIN}/display/v1_legacy)
set(V1_LATE_LOOKUP_DIR ${OR_PLUGIN_BIN}/display/v1_late_lookup)
configure_file(release_after_finish.rib.in ${OR_PLUGIN_BIN}/display/release_after_finish.rib @ONLY)
configure_file(late_lookup.rib.in ${OR_PLUGIN_BIN}/display/late_lookup.rib @ONLY)

# FR-035 -- a two-frame scene; the fixture log orders finish, unload and the next start.
or_plugin_add_test(DisplayAbi_ReleaseAfterFinish 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/display/release_after_finish.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/display/work/release_after_finish
        -DLOG=${OR_PLUGIN_BIN}/display/work/release_after_finish/fixture.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_release_after_finish.cmake)
set_tests_properties(DisplayAbi_ReleaseAfterFinish PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# FR-039 -- late findParameter lookups must be answered "not found".
or_plugin_add_test(DisplayV1_LateLookup 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/display/late_lookup.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/display/work/late_lookup
        -DLOG=${OR_PLUGIN_BIN}/display/work/late_lookup/fixture.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_late_lookup.cmake)
set_tests_properties(DisplayV1_LateLookup PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# FR-040 -- an in-process program that links the renderer (pattern of tests/unit/state/test_light_order.cpp).
add_executable(test_display_unsupported_param test_unsupported_param.cpp)
target_link_libraries(test_display_unsupported_param PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_display_unsupported_param PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_compile_features(test_display_unsupported_param PRIVATE cxx_std_20)
or_plugin_add_test(DisplayAbi_UnsupportedParamType 60 COMMAND test_display_unsupported_param)
set_tests_properties(DisplayAbi_UnsupportedParamType PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

add_subdirectory(legacy_v1)
