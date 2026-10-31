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



# ---------------------------------------------------------------------------------------------------------
# Phase 5 (T078 to T082): display 2.0.0 and the v1 adapter.
#
# Tests of this phase, by what they need:
#   enabled (the fixtures are hand-declared C or the existing v1_* ones, so they build on the starting tree):
#     DisplayAbi_Versions_Reject, DisplayAbi_Versions_V1, DisplayV1_LegacyLoads, DisplayV1_Explicit1x,
#     DisplayV1_HostileCopiesOnly, DisplayV1_CustomDisplay, DisplayV1_SerializedData, and in checks/
#     DisplayV1_NoCastCheck
#   written DISABLED until T085/T086 (they need the new dsply.h, so their fixtures were EXCLUDE_FROM_ALL, or
#   src/ri/render/displayHost.h); T093 removed the markers: DisplayAbi_Versions_V2, DisplayAbi_HiddenVisibility, DisplayAbi_ZeroAndDepthOnly,
#   DisplayAbi_Descriptors, DisplayAbi_TwoDisplayParams, DisplayAbi_Snapshot, DisplayAbi_LateDisable,
#   DisplayAbi_Hostile, DisplayAbi_ReleaseAfterFinish_V2
# Red state of the disabled ones: `cmake --build build --target plugin_display_v2 ... test_display_host` fails to
# compile (the new dsply.h and displayHost.h do not exist).

# --- fixtures that are version 1 shaped or must be rejected: hand-declared signatures (v1_fixture.c) ---
or_plugin_test_module(plugin_display_v1_explicit display v1_explicit v1_fixture.c DEFS TEST_ABI_MAJOR=1 TEST_ABI_MINOR=0)
or_plugin_test_module(plugin_display_v1_newer display v1_newer v1_fixture.c DEFS TEST_ABI_MAJOR=1 TEST_ABI_MINOR=4)
or_plugin_test_module(plugin_display_v3 display v3 v1_fixture.c DEFS TEST_ABI_MAJOR=3 TEST_ABI_MINOR=0 TEST_EXPORT_V2_ENTRY)
or_plugin_test_module(plugin_display_v0 display v0 v1_fixture.c DEFS TEST_ABI_MAJOR=0 TEST_ABI_MINOR=9 TEST_EXPORT_V2_ENTRY)
or_plugin_test_module(plugin_display_garbage display garbage v1_fixture.c DEFS TEST_ABI_RAW=0xFFFFFFFFu TEST_EXPORT_V2_ENTRY)
or_plugin_test_module(plugin_display_v1_hostile display v1_hostile v1_fixture.c DEFS TEST_HOSTILE)
or_plugin_test_module(plugin_display_v1_serial display v1_serial v1_fixture.c DEFS TEST_SERIAL)
or_plugin_test_module(plugin_display_v1_serial_fail display v1_serial_fail v1_fixture.c DEFS TEST_SERIAL TEST_FAIL_AT=3)

# --- fixtures written against the new dsply.h (EXCLUDE_FROM_ALL until T085; T093 removed the markers) ---
or_plugin_test_module(plugin_display_v2 display v2 v2_fixture.c)
or_plugin_test_module(plugin_display_v2_hidden display v2_hidden v2_fixture.c HIDDEN)
or_plugin_test_module(plugin_display_v2_newer display v2_newer v2_fixture.c DEFS TEST_ABI_MAJOR=2 TEST_ABI_MINOR=9)
or_plugin_test_module(plugin_display_v2_record display v2_record v2_fixture.c DEFS TEST_RECORD)
or_plugin_test_module(plugin_display_v2_failfirst display v2_failfirst v2_fixture.c DEFS TEST_FAILFIRST)
or_plugin_test_module(plugin_display_v2_hostile display v2_hostile v2_fixture.c DEFS TEST_HOSTILE)
set(DISPLAY_V2_FIXTURES plugin_display_v2 plugin_display_v2_hidden plugin_display_v2_newer plugin_display_v2_record
    plugin_display_v2_failfirst plugin_display_v2_hostile)

# --- the scenes ---
# display_rib(<name> DISPLAYS <lines> SEARCH <path> [PRE <lines>] [WORLD <lines>])
function(display_rib name)
    cmake_parse_arguments(R "" "SEARCH;PRE;DISPLAYS;WORLD" "" ${ARGN})
    set(D_NAME "${name}")
    set(D_SEARCH "${R_SEARCH}")
    set(D_PRE "${R_PRE}")
    set(D_DISPLAYS "${R_DISPLAYS}")
    set(D_WORLD "${R_WORLD}")
    or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/display.rib.in ${OR_PLUGIN_BIN}/display/rib/${name}.rib)
endfunction()
set(_D ${OR_PLUGIN_BIN}/display)
set(_RIBS ${OR_PLUGIN_BIN}/display/rib)

foreach(_variant v3 v0 garbage v1_explicit v1_newer v2 v2_hidden v2_newer)
    display_rib(${_variant} SEARCH ${_D}/${_variant} DISPLAYS "Display \"x.out\" \"${_variant}\" \"rgba\"")
endforeach()
display_rib(v1_legacy SEARCH ${_D}/v1_legacy
    DISPLAYS "Display \"x1.out\" \"v1_legacy\" \"rgba\"\nDisplay \"+x2.out\" \"v1_legacy\" \"rgba\"")

# a normal in-tree display (a.tif) next to a hostile one; Exposure and Quantize make the statics matter
set(_HOSTILE_PRE "Format 16 16 1\nExposure 1.8 2.2\nQuantize \"rgba\" 255 0 255 0.5")
set(_HOSTILE_WORLD "Color [0.9 0.5 0.3]\nSurface \"constant\"\nTranslate 0 0 3\nSphere 1 -1 1 360")
display_rib(base SEARCH "${OR_DISPLAY_PATH}" PRE "${_HOSTILE_PRE}" WORLD "${_HOSTILE_WORLD}" DISPLAYS "Display \"a.tif\" \"file\" \"rgba\"")
foreach(_h v1_hostile v2_hostile)
    display_rib(${_h}_first SEARCH "${_D}/${_h}:${OR_DISPLAY_PATH}" PRE "${_HOSTILE_PRE}" WORLD "${_HOSTILE_WORLD}"
        DISPLAYS "Display \"h.out\" \"${_h}\" \"rgba\"\nDisplay \"+a.tif\" \"file\" \"rgba\"")
    display_rib(${_h}_last SEARCH "${_D}/${_h}:${OR_DISPLAY_PATH}" PRE "${_HOSTILE_PRE}" WORLD "${_HOSTILE_WORLD}"
        DISPLAYS "Display \"a.tif\" \"file\" \"rgba\"\nDisplay \"+h.out\" \"${_h}\" \"rgba\"")
endforeach()

# 64 x 64 in 8 x 8 buckets on 8 threads: 64 tiles for displayData
set(_SERIAL_PRE "Format 64 64 1\nOption \"limits\" \"bucketsize\" [8 8]\nOption \"limits\" \"numthreads\" [8]")
display_rib(v1_serial SEARCH ${_D}/v1_serial PRE "${_SERIAL_PRE}" DISPLAYS "Display \"s.out\" \"v1_serial\" \"rgba\"")
display_rib(v1_serial_fail SEARCH ${_D}/v1_serial_fail PRE "${_SERIAL_PRE}" DISPLAYS "Display \"s.out\" \"v1_serial_fail\" \"rgba\"")

set(V2_DIR ${_D}/v2)
configure_file(release_v2.rib.in ${OR_PLUGIN_BIN}/display/release_v2.rib @ONLY)

# --- group A: versions ---
function(display_script_test name script timeout)
    or_plugin_add_test(${name} ${timeout}
        COMMAND ${CMAKE_COMMAND}
            -DORENDER=$<TARGET_FILE:orender>
            -DTOOL=$<TARGET_FILE:plugin_abi_pixel_compare>
            -DRIBDIR=${_RIBS}
            -DWORKROOT=${OR_PLUGIN_BIN}/display/work/${name}
            -DCOMMON=${OR_PLUGIN_COMMON_DIR}
            ${ARGN}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/${script})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")
endfunction()

display_script_test(DisplayAbi_Versions_Reject check_versions_reject.cmake 180)
display_script_test(DisplayAbi_Versions_V1 check_versions_v1.cmake 180)
display_script_test(DisplayAbi_Versions_V2 check_versions_v2.cmake 180 "-DCASES=v2|v2_newer")
display_script_test(DisplayAbi_HiddenVisibility check_versions_v2.cmake 120 "-DCASES=v2_hidden")

find_package(Threads REQUIRED)
# the in-process tests that drive CDisplayHost (src/ri/render/displayHost.h does not exist yet)
add_executable(test_display_host test_display_host.cpp)
target_link_libraries(test_display_host PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon Threads::Threads)
target_include_directories(test_display_host PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_compile_features(test_display_host PRIVATE cxx_std_20)
add_dependencies(test_display_host ${DISPLAY_V2_FIXTURES})
foreach(_pair "zero|DisplayAbi_ZeroAndDepthOnly" "descriptors|DisplayAbi_Descriptors" "twoparams|DisplayAbi_TwoDisplayParams"
        "snapshot|DisplayAbi_Snapshot" "latedisable|DisplayAbi_LateDisable")
    string(REPLACE "|" ";" _p "${_pair}")
    list(GET _p 0 _mode)
    list(GET _p 1 _test)
    file(MAKE_DIRECTORY ${OR_PLUGIN_BIN}/display/work/host_${_mode})
    or_plugin_add_test(${_test} 300 COMMAND test_display_host ${_mode} ${OR_PLUGIN_BIN})
    set_tests_properties(${_test} PROPERTIES WORKING_DIRECTORY ${OR_PLUGIN_BIN}/display/work/host_${_mode})
endforeach()

# --- group B ---
display_script_test(DisplayAbi_Hostile check_hostile.cmake 300 "-DHOSTILE=v2_hostile|v1_hostile")
or_plugin_add_test(DisplayAbi_ReleaseAfterFinish_V2 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/display/release_v2.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/display/work/release_v2
        -DLOG=${OR_PLUGIN_BIN}/display/work/release_v2/fixture.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_release_v2.cmake)
set_tests_properties(DisplayAbi_ReleaseAfterFinish_V2 PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# --- group C: the adapter ---
or_plugin_add_test(DisplayV1_LegacyLoads 180
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DTOOL=$<TARGET_FILE:plugin_abi_pixel_compare>
        -DRIB=${OR_PLUGIN_BIN}/display_baseline/rib_legacy/tiff8.rib
        -DREF=${OR_PIXEL_REF_ROOT}/${OR_PLATFORM}/tiff8.pix
        -DWORKDIR=${OR_PLUGIN_BIN}/display/work/legacy_loads
        -DLOG=${OR_PLUGIN_BIN}/display/work/legacy_loads/fixture.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_legacy_loads.cmake)
set_tests_properties(DisplayV1_LegacyLoads PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

display_script_test(DisplayV1_Explicit1x check_explicit1x.cmake 180)
display_script_test(DisplayV1_HostileCopiesOnly check_hostile.cmake 300 "-DHOSTILE=v1_hostile")
display_script_test(DisplayV1_SerializedData check_serialized.cmake 300)

add_executable(test_custom_display test_custom_display.cpp)
target_link_libraries(test_custom_display PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_custom_display PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_compile_features(test_custom_display PRIVATE cxx_std_20)
or_plugin_add_test(DisplayV1_CustomDisplay 120 COMMAND test_custom_display)
set_tests_properties(DisplayV1_CustomDisplay PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

add_subdirectory(legacy_v1)
