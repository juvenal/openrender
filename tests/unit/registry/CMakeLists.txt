# Spec 030 (hider and integrator registry): the registry tests. Label `registry` (ctest -L registry); the everyday run
# also carries `unit`. Names as in plan.md "New tests".
#
#   Registry_HiderAliasDiagnostics, Registry_UnknownHiderFallback   characterization of today's Hider statement; ENABLED
#                                                                   from Phase 2 (green on the starting tree, stay green)
#   Registry_HiderLookup, Registry_HiderCollisions                  Phase 6 (hiderRegistry.h)
#   Registry_IntegratorLookup, Registry_PairCheck                   Phase 5 (transport/ headers)
#   Registry_TestDoubleHider                                        Phase 8 (the core selects by registry)
#   Registry_PairRefusal_{reyes,zbuffer,raytrace,photon}, Registry_ClassicPairs,
#   Registry_UnknownIntegratorFallback, Registry_IntegratorParameters, Registry_FrameAbort     Phase 9
#   Registry_IntegratorImageIdentity (also `visual`)                Phase 7 (the Integrator statement parses)
#   Hcshader_MissingShader                                          Phase 4 (the built-in lights deleted)
#
# Red-first convention (tasks.md "Standing rules"): every new compiled test but the characterization one lives in its OWN
# executable inside `if(OPENRENDER_030_RED_TESTS)` (default OFF), so a test that cannot compile yet never breaks another or
# the build. The Test gate of the phase that makes a test compile moves its definition out of the block.
#
# Every test runs in its own working directory (build/tests/unit/registry/work/<ctest name>): the scenes and images they
# write never collide when ctest runs them in parallel. ORENDERHOME is the source root (no .orenderrc), SHADERS the build's
# compiled shaders and DISPLAYS the file driver, as Link_StaticConsumer does.

set(REG_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/transport ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_SOURCE_DIR}/src/libshader/shading ${CMAKE_CURRENT_SOURCE_DIR})
set(REG_ENV "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")

# reg_executable(<target> <source>...)
function(reg_executable target)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${REG_INCLUDES})
    target_link_libraries(${target} PRIVATE ribRender libshader_shading openrendercommon)
    add_dependencies(${target} openrender_shaders)
endfunction()

# reg_test(<ctest name> <executable> [TIMEOUT s] [LABELS l...] [ARGS a...])
function(reg_test name exe)
    cmake_parse_arguments(ARG "" "TIMEOUT" "LABELS;ARGS" ${ARGN})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif()
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${work})
    add_test(NAME ${name} COMMAND ${exe} ${ARG_ARGS} WORKING_DIRECTORY ${work})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "registry;unit;${ARG_LABELS}" ENVIRONMENT "${REG_ENV}")
endfunction()

# ---- the characterization tests: green on the starting tree, enabled now ------------------------------------------------
reg_executable(test_registry_hider_statements test_registry_hider_statements.cpp)
reg_test(Registry_HiderAliasDiagnostics test_registry_hider_statements ARGS alias_diagnostics)
reg_test(Registry_UnknownHiderFallback test_registry_hider_statements ARGS unknown_fallback)

# ---- Phase 4: with the built-in lights deleted a light with no compiled shader is an ordinary missing shader ---------------
# (it compiled on the starting tree, RI calls only, and was red by assertion: the C++ substitute still answered)
reg_executable(test_hcshader_missing_shader test_hcshader_missing_shader.cpp)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/empty_shader_dir)
reg_test(Hcshader_MissingShader test_hcshader_missing_shader ARGS ${CMAKE_CURRENT_BINARY_DIR}/empty_shader_dir)

# ---- Phase 5: the integrator table and the pair check (the Transport headers) -----------------------------------------------
reg_executable(test_registry_integrator_table test_registry_integrator_table.cpp)
reg_test(Registry_IntegratorLookup test_registry_integrator_table ARGS lookup)
reg_test(Registry_PairCheck test_registry_integrator_table ARGS pair_check)

# ---- Phase 6: the hider table (hiderAbi.h, hiderRegistry.h) ------------------------------------------------------------------
reg_executable(test_registry_hider_table test_registry_hider_table.cpp)
reg_test(Registry_HiderLookup test_registry_hider_table ARGS lookup)
reg_test(Registry_HiderCollisions test_registry_hider_table ARGS collisions)

# ---- Phase 7: the Integrator statement parses (it compiled before the statement existed and was red by assertion) -----------
reg_executable(test_registry_identity test_registry_identity.cpp)
reg_test(Registry_IntegratorImageIdentity test_registry_identity LABELS visual TIMEOUT 300)

# ---- red until their phase ----------------------------------------------------------------------------------------------
if(OPENRENDER_030_RED_TESTS)
    # Phase 8: a hider registered from this directory is selectable by name and alias.
    reg_executable(test_registry_hider_double test_registry_hider_double.cpp hider_double.cpp)
    reg_test(Registry_TestDoubleHider test_registry_hider_double)

    # Phase 9: pairs, refusals, the integrator statements and the abort paths (the test-double integrator ABI).
    reg_executable(test_registry_pairs test_registry_pairs.cpp integrator_doubles.cpp)
    foreach(_hider reyes zbuffer raytrace photon)
        reg_test(Registry_PairRefusal_${_hider} test_registry_pairs ARGS refusal ${_hider} TIMEOUT 300)
    endforeach()
    reg_test(Registry_ClassicPairs test_registry_pairs ARGS classic_pairs TIMEOUT 300)

    reg_executable(test_registry_integrator_statements test_registry_integrator_statements.cpp integrator_doubles.cpp)
    reg_test(Registry_UnknownIntegratorFallback test_registry_integrator_statements ARGS unknown_fallback)
    reg_test(Registry_IntegratorParameters test_registry_integrator_statements ARGS parameters)

    reg_executable(test_registry_frame_abort test_registry_frame_abort.cpp hider_double.cpp integrator_doubles.cpp)
    reg_test(Registry_FrameAbort test_registry_frame_abort)
endif()
