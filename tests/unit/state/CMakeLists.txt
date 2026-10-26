# Graphics State tests (spec 025, graphics-state-handles). Every add_test()
# here carries the "state" label, so `ctest -L state` selects them all.

# FR-025 / SC-010: lights keep RIB order across an attribute push, a
# copy-on-write clone and a ResourceRestore. Links the renderer (it drives the
# Ri calls), so it is not one of the State-only tests.
add_executable(test_state_light_order test_light_order.cpp)
target_link_libraries(test_state_light_order PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_state_light_order PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_state_light_order PRIVATE cxx_std_20)
add_test(NAME State_LightOrder COMMAND test_state_light_order)
set_tests_properties(State_LightOrder PROPERTIES
    TIMEOUT 60
    LABELS "state"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# State-only scaffolding (research R11; precedent tests/framebuffer/CMakeLists.txt).
#
# The State-only tests compile the State sources (src/ri/state/*.cpp) directly into
# a test library on a deliberately NARROW include path and link only openrendercommon,
# so they prove Graphics State depends on infrastructure and on itself alone:
#   - a bare #include "shader.h", "photonMap.h", "dsply.h" or "blobbyField.h" in State
#     must fail to compile here, and an undefined reference into any other domain
#     must fail to link;
#   - a domain-qualified spelling such as "display/dsply.h" still resolves through
#     src/, so it is caught by the architecture check and the SC-003 grep, not by
#     this test.
#
# Why this is behind an option, OFF until the State sources can be built this way
# (task T050): archcheck's loadMakefiles walks every DependInfo.cmake in the build
# tree, EXCLUDE_FROM_ALL targets included, and exits 2 "incomplete build tree" when
# an in-scope object (any source under src/) is missing. The library below compiles
# src/ri/state/*.cpp, so it must either be built or not exist. With the option off,
# none of it is defined, so `ctest -L architecture` sees only the production units.
# Red evidence is taken in a scratch build directory configured with the option ON.
# ---------------------------------------------------------------------------
option(OPENRENDER_STATE_ONLY_TESTS "Build the State-only isolation tests" OFF)

if(OPENRENDER_STATE_ONLY_TESTS)
    # A test-only staging directory holding a copy of ri_config.h. src/ri/core itself
    # is NOT on the include path: it also holds the Shading shims shader.h and shading.h.
    set(STATE_ONLY_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/state_only_include)
    configure_file(${CMAKE_SOURCE_DIR}/src/ri/core/ri_config.h
                   ${STATE_ONLY_INCLUDE_DIR}/ri_config.h COPYONLY)

    # Later phases append shaderParameterHooks.cpp and channelRegistry.cpp.
    add_library(state_only_sources STATIC EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/src/ri/state/attributes.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/state/options.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/state/pl.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/state/xform.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/state/displayChannel.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/state/variableLookup.cpp
    )
    target_link_libraries(state_only_sources PUBLIC openrender_common_flags openrendercommon)
    target_compile_features(state_only_sources PUBLIC cxx_std_20)
    target_include_directories(state_only_sources PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ri/state
        ${CMAKE_BINARY_DIR}
        ${STATE_ONLY_INCLUDE_DIR}
    )

    # add_state_only_test(NAME EXE SOURCES...): an EXCLUDE_FROM_ALL executable linking
    # state_only_sources. The test is DISABLED until the phase that provides its API
    # removes the marker; its red state is a build failure of the executable.
    function(add_state_only_test NAME EXE)
        add_executable(${EXE} EXCLUDE_FROM_ALL ${ARGN})
        target_link_libraries(${EXE} PRIVATE state_only_sources)
        target_compile_features(${EXE} PRIVATE cxx_std_20)
        add_test(NAME ${NAME} COMMAND ${EXE})
        set_tests_properties(${NAME} PROPERTIES DISABLED TRUE LABELS "state" TIMEOUT 60)
    endfunction()

    # The tests (spec 025, tasks T009-T012 and T014). Each phase enables its own:
    #   State_Isolation          handles and reference counts (US1)
    #   State_LastBound          the last-bound record and the bxdf slot (US3)
    #   State_PatternTable       the scoped pattern-node table (US4)
    #   State_ChannelRegistry    the AOV/channel registry (US5)
    #   State_LightOrderRestore  direct restore()/copy order (FR-025), enabled in Phase 7
    add_state_only_test(State_Isolation test_state_isolation test_state_isolation.cpp)
    add_state_only_test(State_LastBound test_state_last_bound test_state_last_bound.cpp)
    add_state_only_test(State_PatternTable test_state_pattern_table test_state_pattern_table.cpp)
    add_state_only_test(State_ChannelRegistry test_state_channel_registry test_state_channel_registry.cpp)
    add_state_only_test(State_LightOrderRestore test_state_light_restore test_state_light_restore.cpp)
endif()

# State_VocabularyConsistency (FR-008/FR-009, SC-007) is a Consumer: it includes
# stateVocabulary.h, dsply.h and blobbyField.h, so it uses the normal include path and
# is always defined. Red by build failure (stateVocabulary.h does not exist): EXCLUDE_FROM_ALL
# and DISABLED until the vocabulary phase (T036) creates the header.
add_executable(test_state_vocabulary EXCLUDE_FROM_ALL test_state_vocabulary.cpp)
target_link_libraries(test_state_vocabulary PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_state_vocabulary PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/display
    ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_state_vocabulary PRIVATE cxx_std_20)
add_test(NAME State_VocabularyConsistency COMMAND test_state_vocabulary)
set_tests_properties(State_VocabularyConsistency PROPERTIES DISABLED TRUE LABELS "state" TIMEOUT 60)
