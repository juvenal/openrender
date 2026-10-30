# The Procedures module tests (spec 037, Phase 3; US1, US5; FR-003, FR-004, FR-009, FR-022 to FR-024,
# FR-034, FR-044; SC-001, SC-002, SC-008, SC-018).
#
#   ProceduresAbi_Versions       dlo, implicit, DynamicLoad through their RIB statements: good / newer /
#                                v2 / legacy / hidden. In-process, real renderer.
#   ProceduresAbi_SameRoutine    the diagnostics of the three kinds are one format.
#   ProceduresAbi_ConcurrentAcquire, ProceduresAbi_ObjectAndFrames
#                                need proceduralModules.h: written before it existed (EXCLUDE_FROM_ALL +
#                                DISABLED, red state: the compile error of their targets); T061 removed the markers.
#   PluginAbi_LoadMissingModule  osLoadModule returns NULL for a module that cannot load; on Linux it
#                                aborts on the starting tree (maintainer's decision at checkpoint 2, T055a).
# ProceduresAbi_NoLoadOutsideModule is in checks/; ProceduresAbi_BareNameLocated, _DynamicLoadRelease and
# _Scenes are in dlo/, dynamicload/ and scenes/.

set(_procedures_includes ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_BINARY_DIR})

# --- ProceduresAbi_Versions and _SameRoutine: one program, two modes ---
add_executable(test_procedures_versions test_versions.cpp)
target_link_libraries(test_procedures_versions PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_procedures_versions PRIVATE ${_procedures_includes})
target_compile_features(test_procedures_versions PRIVATE cxx_std_20)
# The fixtures it loads must be built with it.
add_dependencies(test_procedures_versions
    plugin_dlo_good plugin_dlo_legacy plugin_dlo_v2 plugin_dlo_newer plugin_dlo_hidden
    plugin_implicit_good plugin_implicit_legacy plugin_implicit_v2 plugin_implicit_newer plugin_implicit_hidden
    plugin_dynamicload_plain plugin_dynamicload_versioned plugin_dynamicload_v2)

foreach(_mode versions sameroutine)
    if(_mode STREQUAL "versions")
        set(_test ProceduresAbi_Versions)
    else()
        set(_test ProceduresAbi_SameRoutine)
    endif()
    file(MAKE_DIRECTORY ${OR_PLUGIN_BIN}/procedures/work/${_mode})
    or_plugin_add_test(${_test} 120
        COMMAND test_procedures_versions ${OR_PLUGIN_BIN} ${_mode})
    set_tests_properties(${_test} PROPERTIES
        ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}"
        WORKING_DIRECTORY ${OR_PLUGIN_BIN}/procedures/work/${_mode})
endforeach()

# --- ProceduresAbi_ConcurrentAcquire (needs proceduralModules.h) ---
add_executable(test_procedures_concurrent test_concurrent_acquire.cpp)
target_link_libraries(test_procedures_concurrent PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_procedures_concurrent PRIVATE ${_procedures_includes})
target_compile_features(test_procedures_concurrent PRIVATE cxx_std_20)
add_dependencies(test_procedures_concurrent plugin_dlo_legacy)
or_plugin_add_test(ProceduresAbi_ConcurrentAcquire 60
    COMMAND test_procedures_concurrent $<TARGET_FILE:plugin_dlo_legacy>)

# --- ProceduresAbi_ObjectAndFrames (needs proceduralModuleStats) ---
add_executable(test_procedures_object_frames test_object_and_frames.cpp)
target_link_libraries(test_procedures_object_frames PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_procedures_object_frames PRIVATE ${_procedures_includes})
target_compile_features(test_procedures_object_frames PRIVATE cxx_std_20)
add_dependencies(test_procedures_object_frames plugin_dynamicload_nested)
file(MAKE_DIRECTORY ${OR_PLUGIN_BIN}/procedures/work/object_frames)
or_plugin_add_test(ProceduresAbi_ObjectAndFrames 120
    COMMAND test_procedures_object_frames ${OR_PLUGIN_BIN}/dynamicload/object_instance.rib object_frames.log)
set_tests_properties(ProceduresAbi_ObjectAndFrames PROPERTIES
    ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}"
    WORKING_DIRECTORY ${OR_PLUGIN_BIN}/procedures/work/object_frames)

# --- PluginAbi_LoadMissingModule ---
add_executable(test_load_missing_module test_load_missing_module.cpp)
target_link_libraries(test_load_missing_module PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_load_missing_module PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_load_missing_module PRIVATE cxx_std_20)
or_plugin_add_test(PluginAbi_LoadMissingModule 60
    COMMAND test_load_missing_module ${CMAKE_CURRENT_SOURCE_DIR}/test_load_missing_module.cpp)
