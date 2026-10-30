# Plug-in ABI tests (spec 037, versioned plug-in ABIs).
#
# Every add_test() under this directory carries the label "plugin_abi", so
# `ctest -L plugin_abi` selects exactly these tests. The label is a regular
# expression: no other label may contain the text "plugin_abi".
#
# Layout (spec 037 plan R15):
#   common/           pluginLog.h (the fixtures' call log), logcheck.cmake, render.cmake
#   checks/           cross-family mechanical checks (cmake -P scripts)
#   display/          display fixtures and their checks; display/legacy_v1/ holds the
#                     frozen pre-spec file driver (SC-013)
#   display_baseline/ deterministic scenes, decoded-pixel compare, the capture target
#   dlo/ implicit/ dynamicload/ shadeop/   fixtures of the other families
#   scenes/           procedure scene references
#   texture3d/        characterization of the 3D-texture channel parsing (US7)
#   convention/ versioncheck/ headers/ procedures/   stubs, filled by later phases
#
# Fixture rules (tasks.md, "Fixture rules"):
#   * every fixture is a MODULE target, PREFIX "" and SUFFIX ".dsply", with a unique target
#     name plugin_<family>_<variant> and OUTPUT_NAME set to what the host looks up;
#   * output goes to ${CMAKE_BINARY_DIR}/tests/plugins/<family>/<variant>/, so search-path
#     order is decided by directory, not by the order osEnumerate returns files in;
#   * every fixture appends one line per entry-point call to the file named by the
#     environment variable OR_TEST_PLUGIN_LOG (common/pluginLog.h);
#   * the fixture sources compile against the old headers (Phase 1) and the new ones: the
#     version line sits under #ifdef <FAMILY>_DECLARE_VERSION, the variant is chosen by
#     TEST_ABI_MAJOR/MINOR, TEST_NO_VERSION and TEST_BEHAVIOR.
#
# No cmake_minimum_required() here on purpose: this file is only reached through
# add_subdirectory().

# The capture of the pixel references is a build target, never a ctest. It refuses to
# overwrite an existing reference unless the tree was configured with this option ON, so
# no `ctest -L plugin_abi` run can turn a post-change output into its own baseline.
option(OR037_RECAPTURE "Let plugin-abi-capture-baselines overwrite existing references" OFF)

set(OR_PLUGIN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(OR_PLUGIN_BIN ${CMAKE_BINARY_DIR}/tests/plugins)
set(OR_PLUGIN_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# Header directories a fixture may find the family headers in. dsply.h, dlo.h, implicit.h and
# shadeop.h move during the spec (git mv); listing every candidate directory that exists keeps
# the fixtures and checks independent of which phase the tree is in.
set(OR_PLUGIN_HEADER_DIRS "")
foreach(_dir src/display src/ri/geometry src/ri/render src/ri/parse src/shadeop src/procedures)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_dir})
        list(APPEND OR_PLUGIN_HEADER_DIRS ${CMAKE_SOURCE_DIR}/${_dir})
    endif()
endforeach()

# The directory that holds shadeop.h (FR-031 compile check). src/ri/render until the header moves.
if(EXISTS ${CMAKE_SOURCE_DIR}/src/shadeop/shadeop.h)
    set(OR_SHADEOP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/shadeop)
else()
    set(OR_SHADEOP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/ri/render)
endif()

# or_plugin_test_module(<target> <family> <variant> <source> [<more sources>...]
#                       [OUTPUT_NAME <name>] [DEFS <definitions>...] [LIBS <libraries>...]
#                       [INCLUDES <dirs>...] [HIDDEN] [NO_FLAGS] [NO_HEADER_DIRS])
#
# Defines one fixture module as the fixture rules above describe. DEFS are compile
# definitions (TEST_ABI_MAJOR=1, TEST_NO_VERSION, ...); OR_TEST_TAG="<variant>" is always
# defined, so every log line a fixture writes carries the variant it came from.
# HIDDEN builds with hidden symbol visibility (the exports must then be explicit).
# NO_FLAGS leaves out openrender_common_flags. NO_HEADER_DIRS leaves out the family header
# directories (the frozen legacy driver must not be able to see the live dsply.h).
function(or_plugin_test_module target family variant source)
    cmake_parse_arguments(ARG "HIDDEN;NO_FLAGS;NO_HEADER_DIRS" "OUTPUT_NAME" "DEFS;LIBS;INCLUDES" ${ARGN})
    if(NOT ARG_OUTPUT_NAME)
        set(ARG_OUTPUT_NAME ${variant})
    endif()
    # Any arguments no keyword claims are further source files of the module.
    add_library(${target} MODULE ${source} ${ARG_UNPARSED_ARGUMENTS})
    set(_dir ${OR_PLUGIN_BIN}/${family}/${variant})
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        SUFFIX ".dsply"
        OUTPUT_NAME ${ARG_OUTPUT_NAME}
        # $<1:...> keeps multi-config generators from appending a configuration directory.
        LIBRARY_OUTPUT_DIRECTORY "$<1:${_dir}>")
    if(ARG_HIDDEN)
        set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    endif()
    target_compile_definitions(${target} PRIVATE OR_TEST_TAG="${variant}" ${ARG_DEFS})
    target_include_directories(${target} PRIVATE ${OR_PLUGIN_COMMON_DIR} ${ARG_INCLUDES})
    if(NOT ARG_NO_HEADER_DIRS)
        target_include_directories(${target} PRIVATE ${OR_PLUGIN_HEADER_DIRS})
    endif()
    if(NOT ARG_NO_FLAGS)
        target_link_libraries(${target} PRIVATE openrender_common_flags)
    endif()
    if(ARG_LIBS)
        target_link_libraries(${target} PRIVATE ${ARG_LIBS})
    endif()
    target_compile_features(${target} PRIVATE cxx_std_20)
endfunction()

# or_plugin_add_test(<name> <timeout> COMMAND ...) registers a test with the plugin_abi label.
function(or_plugin_add_test name timeout)
    add_test(NAME ${name} ${ARGN})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${timeout} LABELS "plugin_abi")
endfunction()

# The environment a render-level test runs orender in. ORENDERHOME is the source tree
# (it holds no .orenderrc, so no renderer defaults leak in); every RIB sets its own search
# paths, Format and PixelSamples and never uses '&' (the compiled-in defaults point at the
# gitignored deploy tree).
set(OR_PLUGIN_RENDER_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}")

# The display driver directories the in-tree drivers are built into, as a search-path string that
# still holds generator expressions (resolve it with or_plugin_rib() / file(GENERATE)). Fixtures
# that select the in-tree file / rgbe / openexr drivers name exactly these directories and never '&'.
set(OR_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>")
if(HAVE_OPENEXR)
    set(OR_DISPLAY_PATH "${OR_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()

# or_plugin_rib(<template> <output>): turn a *.rib.in into a RIB. @VAR@ is replaced from the
# caller's variables at configure time; generator expressions in the result (OR_DISPLAY_PATH,
# $<TARGET_FILE:...>) are resolved at generate time.
function(or_plugin_rib template output)
    string(MD5 _id "${output}")
    configure_file(${template} ${CMAKE_CURRENT_BINARY_DIR}/rib_${_id}.tmpl @ONLY)
    file(GENERATE OUTPUT ${output} INPUT ${CMAKE_CURRENT_BINARY_DIR}/rib_${_id}.tmpl)
endfunction()

# The platform the decoded-pixel references are chosen for (display_baseline/reference/<platform>/).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OR_PLATFORM linux)
else()
    set(OR_PLATFORM macos)
endif()

# The baseline capture is a pair of build targets, never a ctest (they are not in `all`):
#   plugin-abi-capture-baselines          writes reference/<platform>/ and the other reference data;
#                                         refuses to overwrite an existing file unless the tree was
#                                         configured with -DOR037_RECAPTURE=ON
#   plugin-abi-capture-baselines-scratch  writes a second copy under the build tree, for the check that
#                                         two captures are identical sample for sample
# Each family attaches its capture commands with or_plugin_capture().
set(OR_CAPTURE_SCRATCH ${OR_PLUGIN_BIN}/capture_scratch)
add_custom_target(plugin-abi-capture-baselines COMMENT "Capturing the spec 037 baselines (an existing reference is never overwritten)")
add_custom_target(plugin-abi-capture-baselines-scratch COMMENT "Capturing the spec 037 baselines into the build tree")

# or_plugin_capture(<label> OFFICIAL <command...> SCRATCH <command...>)
# add_custom_command(TARGET ...) only works in the directory that created the target, and the families
# live in subdirectories, so the commands are queued in global properties (arguments joined with "@@")
# and attached to the two targets at the end of this file.
function(or_plugin_capture label)
    cmake_parse_arguments(ARG "" "" "OFFICIAL;SCRATCH" ${ARGN})
    string(REPLACE ";" "@@" _official "${ARG_OFFICIAL}")
    string(REPLACE ";" "@@" _scratch "${ARG_SCRATCH}")
    set_property(GLOBAL APPEND PROPERTY OR_CAPTURE_LABELS "${label}")
    set_property(GLOBAL APPEND PROPERTY OR_CAPTURE_OFFICIAL "${_official}")
    set_property(GLOBAL APPEND PROPERTY OR_CAPTURE_SCRATCH_CMDS "${_scratch}")
endfunction()

# Where the decoded-pixel references live (display and procedure scenes alike).
set(OR_PIXEL_REF_ROOT ${OR_PLUGIN_ROOT}/display_baseline/reference)

# or_plugin_scene_set(TEST <ctest name> RIB_DIR <dir> SCENES <name=output-file>... [GAMMA <name>...]
#                     [VERIFY_ONLY] [DEPENDS <targets>...])
# Registers one compare test over a set of scenes (display_baseline/run_scenes.cmake) and attaches the
# capture of the same scenes to the two capture targets. VERIFY_ONLY scenes (the frozen legacy driver
# renders the same scenes as the live drivers) are compared against the references the live scenes just
# captured instead of capturing references of their own.
function(or_plugin_scene_set)
    cmake_parse_arguments(A "VERIFY_ONLY" "TEST;RIB_DIR" "SCENES;GAMMA;DEPENDS" ${ARGN})
    string(REPLACE ";" "|" _scenes "${A_SCENES}")
    string(REPLACE ";" "|" _gamma "${A_GAMMA}")
    set(_script ${OR_PLUGIN_ROOT}/display_baseline/run_scenes.cmake)
    set(_env ${CMAKE_COMMAND} -E env "SHADERS=${CMAKE_BINARY_DIR}/shaders" "ORENDERHOME=${CMAKE_SOURCE_DIR}")
    set(_common
        -DORENDER=$<TARGET_FILE:orender>
        -DTOOL=$<TARGET_FILE:plugin_abi_pixel_compare>
        "-DSCENES=${_scenes}"
        "-DGAMMA_SCENES=${_gamma}"
        -DRIB_DIR=${A_RIB_DIR}
        -DPLATFORM=${OR_PLATFORM}
        -DLABEL=${A_TEST})

    or_plugin_add_test(${A_TEST} 900
        COMMAND ${CMAKE_COMMAND} -DMODE=compare ${_common} -DREF_ROOT=${OR_PIXEL_REF_ROOT}
                -DWORKROOT=${OR_PLUGIN_BIN}/work/${A_TEST} -P ${_script})
    set_tests_properties(${A_TEST} PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

    if(A_VERIFY_ONLY)
        set(_official ${_env} ${CMAKE_COMMAND} -DMODE=compare ${_common} -DREF_ROOT=${OR_PIXEL_REF_ROOT}
                -DWORKROOT=${OR_PLUGIN_BIN}/capture_work/${A_TEST} -P ${_script})
        set(_scratch ${_env} ${CMAKE_COMMAND} -DMODE=compare ${_common} -DREF_ROOT=${OR_CAPTURE_SCRATCH}
                -DWORKROOT=${OR_PLUGIN_BIN}/capture_work_scratch/${A_TEST} -P ${_script})
    else()
        set(_official ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=${OR037_RECAPTURE} ${_common}
                -DREF_ROOT=${OR_PIXEL_REF_ROOT} -DWORKROOT=${OR_PLUGIN_BIN}/capture_work/${A_TEST} -P ${_script})
        set(_scratch ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=ON ${_common}
                -DREF_DIR=${OR_CAPTURE_SCRATCH}/${OR_PLATFORM} -DREF_ROOT=${OR_CAPTURE_SCRATCH}
                -DWORKROOT=${OR_PLUGIN_BIN}/capture_work_scratch/${A_TEST} -P ${_script})
    endif()
    or_plugin_capture(${A_TEST} OFFICIAL ${_official} SCRATCH ${_scratch})
    foreach(_target orender plugin_abi_pixel_compare ${A_DEPENDS})
        add_dependencies(plugin-abi-capture-baselines ${_target})
        add_dependencies(plugin-abi-capture-baselines-scratch ${_target})
    endforeach()
endfunction()

add_subdirectory(common)
add_subdirectory(checks)
add_subdirectory(convention)
add_subdirectory(versioncheck)
add_subdirectory(headers)
add_subdirectory(display)
add_subdirectory(display_baseline)
add_subdirectory(dlo)
add_subdirectory(implicit)
add_subdirectory(dynamicload)
add_subdirectory(procedures)
add_subdirectory(shadeop)
add_subdirectory(scenes)
add_subdirectory(texture3d)

# Attach the queued capture commands, in the order the families queued them (the frozen legacy
# driver's scenes come after the live scenes they are compared with).
get_property(_labels GLOBAL PROPERTY OR_CAPTURE_LABELS)
get_property(_official_cmds GLOBAL PROPERTY OR_CAPTURE_OFFICIAL)
get_property(_scratch_cmds GLOBAL PROPERTY OR_CAPTURE_SCRATCH_CMDS)
list(LENGTH _labels _n)
math(EXPR _last "${_n} - 1")
foreach(_i RANGE ${_last})
    list(GET _labels ${_i} _label)
    list(GET _official_cmds ${_i} _official)
    list(GET _scratch_cmds ${_i} _scratch)
    string(REPLACE "@@" ";" _official "${_official}")
    string(REPLACE "@@" ";" _scratch "${_scratch}")
    add_custom_command(TARGET plugin-abi-capture-baselines POST_BUILD COMMAND ${_official} COMMENT "capture: ${_label}" VERBATIM)
    add_custom_command(TARGET plugin-abi-capture-baselines-scratch POST_BUILD COMMAND ${_scratch} COMMENT "capture (scratch): ${_label}" VERBATIM)
endforeach()
