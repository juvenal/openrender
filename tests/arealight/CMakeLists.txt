# Spec 057 (area-light geometry): the arealight tests. Label `arealight` on every test here (ctest -L arealight), plus
# the suite label each test names (state, geometry, shading_parity, visual, parity, determinism, common). Names as in
# specs/057-area-light-geometry/contracts/tests.md.
#
# Red-first convention (tasks.md "Conventions"): a test that cannot compile yet lives in its OWN executable inside
# `if(OPENRENDER_057_RED_TESTS)` (default OFF, tests/CMakeLists.txt), so it never breaks the build or another test; its
# red evidence is the build failure of that one target. The task of the phase that creates the API moves the test's
# definition out of the block. A test that compiles (a render pair, a script, a scene check) is registered at once and its
# red evidence is a failing run.
#
# Every test runs in its own working directory (build/tests/arealight/work/<ctest name>): the images and scenes they
# write never collide when ctest runs them in parallel. ORENDERHOME is the source root (no .orenderrc), SHADERS the
# test-local shaders then the build's compiled shaders, DISPLAYS the file driver, as the registry tests do. No shader
# is added to shaders/ (FR-021): the probe lights and surfaces live in tests/arealight/shaders and are build steps here.

set(AL_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/transport ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_SOURCE_DIR}/src/libshader/shading ${CMAKE_CURRENT_SOURCE_DIR}
    # the Geometry-only tests reuse the contract fixtures (spec 031) and the CSG session helper (spec 021)
    ${CMAKE_SOURCE_DIR}/tests/geometry_contract ${CMAKE_SOURCE_DIR}/tests/unit/csg)
set(AL_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(AL_ENV "SHADERS=${AL_SHADER_DIR}:${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")
file(MAKE_DIRECTORY ${AL_SHADER_DIR})

find_program(AL_PYTHON NAMES python3 python)

# al_executable(<target> <source>...)
function(al_executable target)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${AL_INCLUDES})
    target_link_libraries(${target} PRIVATE ribRender libshader_shading openrendercommon)
    add_dependencies(${target} arealight_shaders openrender_shaders)
endfunction()

# al_test(<ctest name> <executable> [TIMEOUT s] [LABELS l...] [ARGS a...])
function(al_test name exe)
    cmake_parse_arguments(ARG "" "TIMEOUT" "LABELS;ARGS" ${ARGN})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif()
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${work})
    add_test(NAME ${name} COMMAND ${exe} ${ARG_ARGS} WORKING_DIRECTORY ${work})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "arealight;${ARG_LABELS}" ENVIRONMENT "${AL_ENV}")
endfunction()

# al_shader(<name>): compiles tests/arealight/shaders/<name>.sl into ${AL_SHADER_DIR}: <name>.rslo always, <name>.slo with
# the JIT (as tests/shading_contract/CMakeLists.txt does for its contract shaders). Collected in the arealight_shaders target.
set_property(GLOBAL PROPERTY AL_SHADER_OUTPUTS "")
function(al_shader name)
    set(sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.sl)
    set(outs ${AL_SHADER_DIR}/${name}.rslo)
    add_custom_command(
        OUTPUT ${AL_SHADER_DIR}/${name}.rslo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${sl} -DOUT=${AL_SHADER_DIR}/${name}.rslo
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${AL_SHADER_DIR}
        COMMENT ""
        VERBATIM)
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT ${AL_SHADER_DIR}/${name}.slo
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${sl} -DOUT=${AL_SHADER_DIR}/${name}.slo
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${AL_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND outs ${AL_SHADER_DIR}/${name}.slo)
    endif()
    set_property(GLOBAL APPEND PROPERTY AL_SHADER_OUTPUTS ${outs})
endfunction()

# al_render_pair_test(<ctest name> PAIRS <sceneA|sceneB[|ALLOW_ERROR_EXIT]>... [EXPECT_EQUAL|EXPECT_DIFFERENT] [LABELS l...] [THREADS n]
#                     [HIDER h] [SHADERFORMAT f] [TIMEOUT s])
# Scenes are paths relative to this directory. Drives render_pair.cmake: every pair is rendered with orender and compared
# byte for byte; EXPECT_EQUAL (default) passes when every pair is identical, EXPECT_DIFFERENT when every pair differs.
# A pair ending in |ALLOW_ERROR_EXIT accepts orender's exit 255 (a scene that reports an error by design) when the image
# was still written; nothing else about the comparison changes.
function(al_render_pair_test name)
    cmake_parse_arguments(ARG "EXPECT_EQUAL;EXPECT_DIFFERENT" "THREADS;THREADS_B;SUPPORT_FILES;HIDER;SHADERFORMAT;TIMEOUT" "PAIRS;LABELS" ${ARGN})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 300)
    endif()
    if(ARG_EXPECT_DIFFERENT)
        set(expect DIFFERENT)
    else()
        set(expect EQUAL)
    endif()
    # the pairs go to the script as one argument: "a|b,c|d" (a semicolon in an add_test argument would split it)
    set(pairs "")
    foreach(p IN LISTS ARG_PAIRS)
        string(REPLACE "|" ";" ab "${p}")
        list(GET ab 0 a)
        list(GET ab 1 b)
        set(opt "")
        list(LENGTH ab ablen)
        if(ablen GREATER 2)
            list(GET ab 2 opt)
            set(opt "|${opt}")
        endif()
        list(APPEND pairs "${CMAKE_CURRENT_SOURCE_DIR}/${a}|${CMAKE_CURRENT_SOURCE_DIR}/${b}${opt}")
    endforeach()
    string(REPLACE ";" "," pairs "${pairs}")
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${work})
    set(args -DORENDER=$<TARGET_FILE:orender> "-DPAIRS=${pairs}" -DEXPECT=${expect})
    if(ARG_THREADS)
        list(APPEND args -DTHREADS=${ARG_THREADS})
    endif()
    if(ARG_THREADS_B)
        list(APPEND args -DTHREADS_B=${ARG_THREADS_B})
    endif()
    if(ARG_SUPPORT_FILES)
        string(REPLACE ";" "\\;" _support "${ARG_SUPPORT_FILES}")
        list(APPEND args "-DSUPPORT_FILES=${_support}")
    endif()
    if(ARG_HIDER)
        list(APPEND args -DHIDER=${ARG_HIDER})
    endif()
    if(ARG_SHADERFORMAT)
        list(APPEND args -DSHADERFORMAT=${ARG_SHADERFORMAT})
    endif()
    if(AL_PYTHON)
        list(APPEND args -DPYTHON=${AL_PYTHON} -DIMGDIFF=${CMAKE_CURRENT_SOURCE_DIR}/tools/imgdiff.py)
    endif()
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} ${args} -P ${CMAKE_CURRENT_SOURCE_DIR}/render_pair.cmake
        WORKING_DIRECTORY ${work})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "arealight;${ARG_LABELS}" ENVIRONMENT "${AL_ENV}")
endfunction()

# ---- Phase 1: the instance-key leak (FR-004, SC-010; contracts/tests.md LightKey_NoLeakAcrossFailedLight) ---------------
al_shader(al_random_surface)
# Phase 4: the probe lights and receivers (tests/arealight/shaders; none is added to shaders/, FR-021)
foreach(_al_name al_light_constant al_light_p al_light_n al_light_uv al_light_st al_light_facing
        al_surface_matte al_surface_matte_n al_surface_lightsum al_surface_check)
    al_shader(${_al_name})
endforeach()

# LightKey_NoLeakAcrossFailedLight: a Surface that draws random() after a failed light statement takes the key it would
# have taken without that statement. Seven checks in one entry: the control rendered twice (the scene is deterministic,
# so a red result is a key and not noise) and each of the six variants (LightSource / AreaLightSource with "null", a
# missing shader and a wrong-type shader, handle 7) against the control. One pass condition for all: byte-identical.
# The missing and wrong-type variants report an error by design, so orender exits 255 on them after writing the image:
# their pairs carry ALLOW_ERROR_EXIT (the "null" variants exit 0 and do not).
set(_leak_dir scenes/leak)
set(_leak_pairs "${_leak_dir}/leak-control.rib|${_leak_dir}/leak-control.rib")
foreach(_kind null missing wrongtype)
    set(_leak_opt "")
    if(NOT _kind STREQUAL "null")
        set(_leak_opt "|ALLOW_ERROR_EXIT")
    endif()
    foreach(_stmt light arealight)
        list(APPEND _leak_pairs "${_leak_dir}/leak-control.rib|${_leak_dir}/leak-${_kind}-${_stmt}.rib${_leak_opt}")
    endforeach()
endforeach()
al_render_pair_test(LightKey_NoLeakAcrossFailedLight PAIRS ${_leak_pairs} EXPECT_EQUAL LABELS "state")

# ---- Phase 4: FR-014, the keyed draws make an image independent of the thread count (needs more than one bucket: 128 x 128 at the
# default 16 x 16 bucket size; the 8 x 8 probe scenes are one bucket and cannot see a shared draw sequence) -----------------
al_render_pair_test(AreaLight_ThreadsIdentical PAIRS scenes/threads/arealight-threads-quad-raytrace.rib|scenes/threads/arealight-threads-quad-raytrace.rib
    THREADS 1 THREADS_B 4 EXPECT_EQUAL LABELS "arealight;shading_parity")

# ---- Phase 4: Shading, the light-surface globals and the area estimator, moved out of the red block by T074 -------------------
# State_AreaLightVocabularyMirror: AreaLightMode (Shading) equals EAreaLightMode (Graphics State) value for value
al_executable(test_arealight_vocabulary_mirror test_vocabulary_mirror.cpp)
al_test(State_AreaLightVocabularyMirror test_arealight_vocabulary_mirror LABELS state)
# (the contract program ShadingContract_AreaLight{Interp,Jit} is registered by tests/shading_contract/CMakeLists.txt)


# ---- Phase 2: binding and the emitter registry (US1; FR-001 to FR-005), moved out of the red block by T029 and T036 --------
# AreaLightBinding_Scope: the slot and the registry across scopes, "null", nesting, two lights, D-9 (a failed statement closes the scope)
al_executable(test_arealight_binding_scope test_binding_scope.cpp)
al_test(AreaLightBinding_Scope test_arealight_binding_scope LABELS state)
# AreaLightBinding_ProceduralInstanceSolid: a DelayedReadArchive, an ObjectInstance and a solid each add one root.
# Registry half, and the resolution half (AL_057_RESOLUTION_HALF, enabled at T053 in Phase 3).
al_executable(test_arealight_binding_resolve test_binding_resolve.cpp)
target_compile_definitions(test_arealight_binding_resolve PRIVATE AL_057_RESOLUTION_HALF)
al_test(AreaLightBinding_ProceduralInstanceSolid test_arealight_binding_resolve LABELS state
    ARGS ${CMAKE_CURRENT_SOURCE_DIR}/scenes/archives)
# AreaLightBinding_NoFrame: AreaLightSource before WorldBegin warns once and binds nothing; no declaration with servers (A9)
al_executable(test_arealight_binding_noframe test_binding_noframe.cpp)
al_test(AreaLightBinding_NoFrame test_arealight_binding_noframe LABELS state)
# AreaLight_SamplingSites: the renderer-side call-site constants are distinct and above the compiler's ordinals (FR-008)
al_executable(test_arealight_sampling_sites test_area_light_sites.cpp)
al_test(AreaLight_SamplingSites test_arealight_sampling_sites LABELS common)

# ---- Phase 3: area sampling of polygons, meshes and subdivision surfaces (US2; FR-006 to FR-009), moved out of the red block by T053 ----
# Geometry only: primitives built directly, recorded as one light's roots, resolved and sampled through CEmitterSet.
# The shared checks and their pre-declared bounds: area_sampling_checks.h.
al_executable(test_arealight_area_sampling_polygon test_area_sampling_polygon.cpp)
al_test(AreaSampling_Polygon test_arealight_area_sampling_polygon LABELS geometry)
al_executable(test_arealight_area_sampling_mesh test_area_sampling_mesh.cpp)
al_test(AreaSampling_Mesh test_arealight_area_sampling_mesh LABELS geometry)
al_executable(test_arealight_area_sampling_subdiv test_area_sampling_subdiv.cpp)
al_test(AreaSampling_Subdivision test_arealight_area_sampling_subdiv LABELS geometry)
al_executable(test_arealight_area_sampling_reports test_area_sampling_reports.cpp)
al_test(AreaSampling_Reports test_arealight_area_sampling_reports LABELS geometry)

# ---- Phase 5: the controls, Option "arealight" and Attribute "arealight" (US5; FR-018 to FR-020, FR-027) --------------------------
# ShadingContract_AreaLightControls: the precedence table of resolveAreaLightControls (a pure function: it compiles on the Phase 4 tree)
al_executable(test_arealight_controls_resolve test_controls_resolve.cpp)
al_test(ShadingContract_AreaLightControls test_arealight_controls_resolve LABELS shading_parity)
# AreaLight_Controls_Render: the probes (tests/visual/arealight-probes.cmake, AreaLight_Controls_Render_*) and these byte-for-byte pairs:
# the draws depend on the resolved count only, so a precedence scene and a fast scene equal their twin that sets the count directly
foreach(_sfx "" "-slo")
    set(_c ../../examples/rib/tests)
    al_render_pair_test(AreaLight_Controls_Render_Pairs${_sfx}
        PAIRS ${_c}/arealight-controls-precedence-object${_sfx}.rib|${_c}/arealight-controls-samples-64${_sfx}.rib
              ${_c}/arealight-controls-precedence-light${_sfx}.rib|${_c}/arealight-controls-samples-64${_sfx}.rib
              ${_c}/arealight-controls-fast-16${_sfx}.rib|${_c}/arealight-controls-samples-4${_sfx}.rib
              ${_c}/arealight-controls-fast-object${_sfx}.rib|${_c}/arealight-controls-samples-4${_sfx}.rib
        EXPECT_EQUAL LABELS "arealight")
endforeach()

# AreaLight_Controls_Parse: tokens, errors, read-back, the writer round trip (moved out of the red block by T087)
al_executable(test_arealight_controls_parse test_controls_parse.cpp)
al_test(AreaLight_Controls_Parse test_arealight_controls_parse LABELS state)

# ---- Phase 6: soft shadows, self-exclusion, determinism (US4; FR-017, FR-018, FR-023 to FR-025) --------------------------------------
# The probes (AreaLightProbe_SoftShadow_*, AreaLight_SelfExclusion_*, AreaLight_Orientation_*, AreaLight_EmptySet_*) are generated into
# tests/visual/arealight-probes.cmake. The byte-for-byte tests:
set(_c ../../examples/rib/tests)
# AreaLight_ThreadIdentity: an image does not depend on the thread count (256 x 256 is 256 buckets): -t:1 against -t:4 on every hider
set(_ti_pairs "")
foreach(_kind quad mesh subdiv)
    foreach(_h reyes zbuffer raytrace)
        list(APPEND _ti_pairs ${_c}/arealight-softshadow-${_kind}-${_h}.rib|${_c}/arealight-softshadow-${_kind}-${_h}.rib)
    endforeach()
endforeach()
al_render_pair_test(AreaLight_ThreadIdentity PAIRS ${_ti_pairs} THREADS 1 THREADS_B 4 EXPECT_EQUAL LABELS "arealight;determinism" TIMEOUT 900)
# ... and two renders at -t:4 are identical (no race in the resolution or the draws)
al_render_pair_test(AreaLight_ThreadIdentity_Repeat PAIRS ${_ti_pairs} THREADS 4 EXPECT_EQUAL LABELS "arealight;determinism" TIMEOUT 900)
# AreaLight_ThreadIdentity_Delayed (research R4, the early-expansion risk): the emitter is a Procedural "DelayedReadArchive" of an archive that
# binds a Surface drawing random(); -t:1 against -t:4 on every hider, byte-identical
set(_dl_dir ${CMAKE_CURRENT_SOURCE_DIR}/scenes/delayed)
al_render_pair_test(AreaLight_ThreadIdentity_Delayed
    PAIRS scenes/delayed/arealight-threadid-delayed-reyes.rib|scenes/delayed/arealight-threadid-delayed-reyes.rib
          scenes/delayed/arealight-threadid-delayed-zbuffer.rib|scenes/delayed/arealight-threadid-delayed-zbuffer.rib
          scenes/delayed/arealight-threadid-delayed-raytrace.rib|scenes/delayed/arealight-threadid-delayed-raytrace.rib
    THREADS 1 THREADS_B 4 SUPPORT_FILES ${_dl_dir}/arealight-delayed-emitter.rib EXPECT_EQUAL LABELS "arealight;determinism" TIMEOUT 900)
# AreaLight_OccluderFlag (FR-023): AreaLight_OccluderFlag_{unflagged,optout}_{penumbra,umbra} (generated probes): an unflagged occluder and
# the transmission 0 opt-out leave the floor at the unoccluded closed form; the flagged one shades it (AreaLightProbe_SoftShadow_*, umbra)

# ---- Phase 7: quadric emitters (US6; FR-009 P2) -------------------------------------------------------------------------------
# AreaSampling_Quadrics (the seven quadrics, partial sweeps, the empty sphere, transforms that keep or lose the analytic sampler; it was red-first
# until the quadric samplers existed). The disk and sphere probes are generated (tests/visual/arealight-probes.cmake,
# AreaLightProbe_{Disk,Sphere}Irradiance[-slo]_*).
al_executable(test_arealight_area_sampling_quadrics test_area_sampling_quadrics.cpp)
al_test(AreaSampling_Quadrics test_arealight_area_sampling_quadrics LABELS geometry)

# ---- the test-local shaders ------------------------------------------------------------------------------------------------
get_property(_al_shader_outputs GLOBAL PROPERTY AL_SHADER_OUTPUTS)
add_custom_target(arealight_shaders ALL DEPENDS ${_al_shader_outputs})
