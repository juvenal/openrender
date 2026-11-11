# Spec 038 (Scene Interface untangle): the in-process tests of the Scene Interface's services, its active
# context and its characterizations. `ctest --test-dir build -R SceneInterface_` runs them.
#
# Phase 1 of the spec writes every test before any change:
#   SceneInterface_WriterText            characterization (regression): the RIB writer's text and diagnostics
#   SceneInterface_IfCharacterization    characterization (regression): the branches the `if` evaluator takes
#   SceneInterface_ContextInstall        rule 1 of the active-context contract; red at run time until Phase 3
#   SceneInterface_ContextInstallRules   rules 2-6 (enabled in Phase 3)
#   SceneInterface_Services              the services (enabled in Phase 2)
# and the program capture_scene_interface_declared_set, which writes reference/declared_set.txt from the
# unmodified CRenderer::initDeclarations(). The references are written by the build target
# scene-interface-capture-baselines (tests/CMakeLists.txt), never by a ctest run.
#
# A DISABLED test's executable is EXCLUDE_FROM_ALL and fails to compile on the starting tree: that failure is
# its red evidence (decision A, precedent tests/unit/state/CMakeLists.txt). The phase that provides the API
# removes DISABLED and EXCLUDE_FROM_ALL.
set(SI_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_CURRENT_SOURCE_DIR})
# ORENDERHOME is the source root, which has no .orenderrc, so no renderer default leaks into these sessions.
set(SI_ENV "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
set(SI_REFERENCE ${CMAKE_CURRENT_SOURCE_DIR}/reference)
set(SI_WORK ${CMAKE_CURRENT_BINARY_DIR}/work)

# si_executable(<target> <source> [EXCLUDE_FROM_ALL] [DEFINE <def>...] LIBS <libs>...)
function(si_executable target source)
    cmake_parse_arguments(ARG "EXCLUDE_FROM_ALL" "" "DEFINE;LIBS" ${ARGN})
    if(ARG_EXCLUDE_FROM_ALL)
        add_executable(${target} EXCLUDE_FROM_ALL ${source})
    else()
        add_executable(${target} ${source})
    endif()
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${SI_INCLUDES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS} openrendercommon)
    if(ARG_DEFINE)
        target_compile_definitions(${target} PRIVATE ${ARG_DEFINE})
    endif()
endfunction()

# --- characterizations (green on the starting tree by design) ---
si_executable(test_scene_interface_writer_text test_writer_text.cpp LIBS ribRender)
add_test(NAME SceneInterface_WriterText
    COMMAND test_scene_interface_writer_text ${CMAKE_SOURCE_DIR}/tests/RIB/writer ${SI_REFERENCE} ${SI_WORK}/writer)
set_tests_properties(SceneInterface_WriterText PROPERTIES TIMEOUT 120 LABELS "regression;scene_interface" ENVIRONMENT "${SI_ENV}")

si_executable(test_scene_interface_if test_if_characterization.cpp LIBS ribRender)
add_test(NAME SceneInterface_IfCharacterization
    COMMAND test_scene_interface_if ${CMAKE_CURRENT_SOURCE_DIR}/scenes/if_branches.rib ${SI_REFERENCE})
set_tests_properties(SceneInterface_IfCharacterization PROPERTIES TIMEOUT 120 LABELS "regression;scene_interface"
    ENVIRONMENT "${SI_ENV}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# The declared-set capture program (a program, not a test): links the unmodified renderer.
si_executable(capture_scene_interface_declared_set capture_declared_set.cpp LIBS ribRender)

# --- the active context (contracts/active-context.md) ---
# One source, built twice. The live build holds rule 1 against today's API (red at run time); the second holds
# rules 2-6 with riInstallContext / riUninstallContext / riPreviousContext, which do not exist yet.
si_executable(test_scene_interface_context_install test_context_install.cpp LIBS ribOut)
add_test(NAME SceneInterface_ContextInstall COMMAND test_scene_interface_context_install)
set_tests_properties(SceneInterface_ContextInstall PROPERTIES TIMEOUT 60 LABELS "common;scene_interface" ENVIRONMENT "${SI_ENV}")

si_executable(test_scene_interface_context_install_rules test_context_install.cpp
    DEFINE SI_INSTALL_RULES=1 LIBS ribRender)
add_test(NAME SceneInterface_ContextInstallRules COMMAND test_scene_interface_context_install_rules)
set_tests_properties(SceneInterface_ContextInstallRules PROPERTIES TIMEOUT 60 LABELS "common;scene_interface"
    ENVIRONMENT "${SI_ENV}")

# --- the services (contracts/scene-services.md) ---
si_executable(test_scene_interface_services test_services.cpp LIBS ribRender)
add_test(NAME SceneInterface_Services COMMAND test_scene_interface_services ${SI_REFERENCE} ${SI_WORK}/services)
set_tests_properties(SceneInterface_Services PROPERTIES TIMEOUT 120 LABELS "common;scene_interface"
    ENVIRONMENT "${SI_ENV}")

# --- the capture of this directory's references (scene-interface-capture-baselines, tests/CMakeLists.txt) ---
set(SI_CAPTURE_FLAGS ${OR038_CAPTURE_FLAG} ${OR038_RECAPTURE_FLAG})
add_custom_target(si_capture_scene_interface
    COMMAND ${CMAKE_COMMAND} -E env ${SI_ENV}
            $<TARGET_FILE:test_scene_interface_writer_text> ${CMAKE_SOURCE_DIR}/tests/RIB/writer ${SI_REFERENCE} ${SI_WORK}/writer_capture ${SI_CAPTURE_FLAGS}
    COMMAND ${CMAKE_COMMAND} -E env ${SI_ENV}
            $<TARGET_FILE:test_scene_interface_if> ${CMAKE_CURRENT_SOURCE_DIR}/scenes/if_branches.rib ${SI_REFERENCE} ${SI_CAPTURE_FLAGS}
    COMMAND ${CMAKE_COMMAND} -E env ${SI_ENV}
            $<TARGET_FILE:capture_scene_interface_declared_set> ${SI_REFERENCE} ${SI_CAPTURE_FLAGS}
    DEPENDS test_scene_interface_writer_text test_scene_interface_if capture_scene_interface_declared_set
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Capturing the SceneInterface writer, if and declared-set references (an existing reference is never overwritten)"
    VERBATIM)
add_dependencies(scene-interface-capture-baselines si_capture_scene_interface)

# Spec 030 (hider and integrator registry), FR-019: parse -> RIB writer preserves the Integrator statement and its
# parameters, and the preview back-end ignores it silently. Enabled at the Test gate of Phase 7 (it was red).
si_executable(test_scene_interface_integrator_roundtrip test_integrator_roundtrip.cpp LIBS ribRender ribpreview)
add_test(NAME RibOut_IntegratorRoundTrip COMMAND test_scene_interface_integrator_roundtrip)
set_tests_properties(RibOut_IntegratorRoundTrip PROPERTIES TIMEOUT 120 LABELS "scene_interface" ENVIRONMENT "${SI_ENV}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
