# Spec 023 (common infrastructure floor) tests.
#
# `ctest --test-dir build -L common` runs every test registered here.

# FR-005: golden table for variable declarations. It passes on the
# pre-feature code; after the grammar moves to src/common only the include
# path below changes, and the table must still match byte for byte.
add_executable(test_common_variables test_common_variables.cpp)
target_compile_features(test_common_variables PRIVATE cxx_std_20)
# Since T042 the grammar lives in libRIBCommon alone: no renderer library.
target_include_directories(test_common_variables PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_common_variables PRIVATE openrendercommon)
add_test(NAME Common_VariableDeclarations
    COMMAND test_common_variables ${CMAKE_CURRENT_SOURCE_DIR}/variable_golden.txt)
set_tests_properties(Common_VariableDeclarations PROPERTIES TIMEOUT 60 LABELS "common;unit")

# FR-008 / research R2: every process-wide symbol has exactly one definer, the
# shared libRIBCommon, across every image the tree builds. owned-symbols.txt
# grows phase by phase as symbols move into common.
# tests/ is configured before src/, so the targets are resolved at generate
# time; a target that is not built (openexr without OpenEXR, the platform's
# other framebuffer flavour) expands to an empty entry the script skips.
set(_common_images "")
foreach(_t ribRender ribVector ribOut libshader_shading libshader_compiler libshader_runtime
           file framebuffer rgbe openexr
           orender oshader otexmake precomp sloinfo test_common_variables
           test_common_diagnostics test_common_crosslib test_common_crosslib_static)
    list(APPEND _common_images "$<$<TARGET_EXISTS:${_t}>:$<TARGET_FILE:${_t}>>")
endforeach()
add_test(NAME Common_StateOwnership
    COMMAND ${CMAKE_COMMAND}
        "-DIMAGES=${_common_images}"
        "-DCOMMON=$<TARGET_FILE:openrendercommon>"
        "-DOWNED=${CMAKE_CURRENT_SOURCE_DIR}/owned-symbols.txt"
        "-DTOKENS_FROM=${CMAKE_SOURCE_DIR}/src/ri/parse/ri.h"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/state_ownership.cmake)
set_tests_properties(Common_StateOwnership PROPERTIES TIMEOUT 120 LABELS "common;unit")

# --- Phase 3 (US3): diagnostics ------------------------------------------------

# FR-002 / SC-003: no RI context at all. Links only libRIBCommon -- reporting a
# diagnostic must not need any renderer library.
add_executable(test_common_diagnostics test_common_diagnostics.cpp)
target_compile_features(test_common_diagnostics PRIVATE cxx_std_20)
target_include_directories(test_common_diagnostics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_common_diagnostics PRIVATE openrendercommon)
foreach(_mode error warning info info_quiet fatal)
    add_test(NAME Common_DiagnosticsNoContext_${_mode}
        COMMAND ${CMAKE_COMMAND}
            "-DEXE=$<TARGET_FILE:test_common_diagnostics>" -DMODE=${_mode}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_runner.cmake)
    set_tests_properties(Common_DiagnosticsNoContext_${_mode} PROPERTIES
        TIMEOUT 60 LABELS "common;unit")
endforeach()

# FR-008 / FR-015: one sink shared across libraries (cases 0-3), against the
# shared libRIBRender ...
set(_crosslib_env "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
add_executable(test_common_crosslib test_common_crosslib.cpp)
target_compile_features(test_common_crosslib PRIVATE cxx_std_20)
target_include_directories(test_common_crosslib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_common_crosslib PRIVATE ribRender libshader_shading openrendercommon
    Threads::Threads ${CMAKE_DL_LIBS})
add_test(NAME Common_DiagnosticsCrossLibrary
    COMMAND test_common_crosslib $<TARGET_FILE:libshader_shading>)
set_tests_properties(Common_DiagnosticsCrossLibrary PROPERTIES
    TIMEOUT 120 LABELS "common;unit" ENVIRONMENT "${_crosslib_env}")

# ... and linked the way the package's Static component links libRIBRender.a
# (whole archive, as CMake/openRenderConfig.cmake.in does; cases 0 and 2).
add_executable(test_common_crosslib_static test_common_crosslib.cpp)
target_compile_features(test_common_crosslib_static PRIVATE cxx_std_20)
target_compile_definitions(test_common_crosslib_static PRIVATE CROSSLIB_STATIC)
target_include_directories(test_common_crosslib_static PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_common_crosslib_static PRIVATE ribRender_static ${CMAKE_DL_LIBS})
if(APPLE)
    target_link_options(test_common_crosslib_static PRIVATE
        "LINKER:-force_load,$<TARGET_FILE:ribRender_static>")
elseif(UNIX)
    target_link_options(test_common_crosslib_static PRIVATE
        "LINKER:--whole-archive,$<TARGET_FILE:ribRender_static>,--no-whole-archive"
        "LINKER:--export-dynamic")
endif()
add_test(NAME Common_DiagnosticsCrossLibraryStatic
    COMMAND test_common_crosslib_static $<TARGET_FILE:libshader_shading>)
set_tests_properties(Common_DiagnosticsCrossLibraryStatic PROPERTIES
    TIMEOUT 120 LABELS "common;unit" ENVIRONMENT "${_crosslib_env}")

# --- Phase 4 (US2): generic core + arena ---------------------------------------
# FR-006: CArena semantics on a local arena.
add_executable(test_common_arena test_common_arena.cpp)
target_compile_features(test_common_arena PRIVATE cxx_std_20)
target_include_directories(test_common_arena PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_common_arena PRIVATE openrendercommon)
add_test(NAME Common_ArenaSemantics COMMAND test_common_arena)
set_tests_properties(Common_ArenaSemantics PROPERTIES TIMEOUT 60 LABELS "common;unit")

# SC-002: libshader_shading's infrastructure symbols come from libRIBCommon,
# never from libRIBRender (shading-infra-symbols.txt grows with the moves).
add_test(NAME Common_ShadingInfraBinding
    COMMAND ${CMAKE_COMMAND}
        "-DSHADING=$<TARGET_FILE:libshader_shading>"
        "-DRENDERER=$<TARGET_FILE:ribRender>"
        "-DSYMBOLS=${CMAKE_CURRENT_SOURCE_DIR}/shading-infra-symbols.txt"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/shading_infra_binding.cmake)
set_tests_properties(Common_ShadingInfraBinding PROPERTIES TIMEOUT 60 LABELS "common;unit")

# --- Phase 5 (US1): vocabulary + variables -------------------------------------
# FR-004: ri.h (public, byte-identical), common/riVocabulary.h and
# common/riTokens.cpp list the same RI tokens.
add_test(NAME Common_VocabularyConsistency
    COMMAND ${CMAKE_COMMAND}
        "-DRI_H=${CMAKE_SOURCE_DIR}/src/ri/parse/ri.h"
        "-DVOCABULARY_H=${CMAKE_SOURCE_DIR}/src/common/riVocabulary.h"
        "-DTOKENS_CPP=${CMAKE_SOURCE_DIR}/src/common/riTokens.cpp"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/vocabulary_consistency.cmake)
set_tests_properties(Common_VocabularyConsistency PROPERTIES TIMEOUT 60 LABELS "common;unit")
# ... and a translation unit including both headers compiles and agrees.
add_executable(test_vocabulary_both_headers test_vocabulary_both_headers.cpp)
target_compile_features(test_vocabulary_both_headers PRIVATE cxx_std_20)
target_include_directories(test_vocabulary_both_headers PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_vocabulary_both_headers PRIVATE openrendercommon)
add_test(NAME Common_VocabularyBothHeaders COMMAND test_vocabulary_both_headers)
set_tests_properties(Common_VocabularyBothHeaders PROPERTIES TIMEOUT 60 LABELS "common;unit")

# --- Phase 6 (US1): pl in Graphics State ---------------------------------------
# Research R8: pl.cpp resolves declared variables through a Graphics State
# lookup hook that the renderer core installs, not through CRenderer.
add_executable(test_variable_lookup test_variable_lookup.cpp)
target_compile_features(test_variable_lookup PRIVATE cxx_std_20)
target_include_directories(test_variable_lookup PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_variable_lookup PRIVATE ribRender openrendercommon)
add_test(NAME Common_VariableLookupHook COMMAND test_variable_lookup)
set_tests_properties(Common_VariableLookupHook PROPERTIES
    TIMEOUT 60 LABELS "common;unit" ENVIRONMENT "${_crosslib_env}")

# --- GitHub #65: osProcessEscapes stays inside its string ----------------------
# Each case runs in a forked child with the terminator on the last byte before
# a PROT_NONE guard page, so an access past the terminator faults.
add_executable(test_common_process_escapes test_common_process_escapes.cpp)
target_compile_features(test_common_process_escapes PRIVATE cxx_std_20)
target_include_directories(test_common_process_escapes PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_common_process_escapes PRIVATE openrendercommon)
add_test(NAME Common_ProcessEscapes COMMAND test_common_process_escapes)
set_tests_properties(Common_ProcessEscapes PROPERTIES TIMEOUT 60 LABELS "common;unit")
