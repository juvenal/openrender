# Spec 035 (closure tree and BSDF): the closure tests. Label `closures` on every test here (ctest -L closures), plus the suite
# label a test names (common, ...). Names as in specs/035-closure-tree-bsdf/tasks.md "Expected red and disabled sets".
#
# Red-first convention (tasks.md "Conventions"), kept as spec 057's was: a test that cannot compile yet lives in its OWN executable
# inside `if(OPENRENDER_035_RED_TESTS)` (default OFF, tests/CMakeLists.txt), so it never breaks the build or another test; its red
# evidence is the build failure of that one target, which names the missing header or symbol. The task of the phase that creates
# the API moves the test's definition out of the block.
#
# Every test runs in its own working directory (build/tests/closures/work/<ctest name>). Test-local shaders (probe surfaces and
# lights, from Phase 3) live in tests/closures/shaders and are build steps here; no file under shaders/ changes (FR-027).

set(CL_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR}/src/libshader/closures
    ${CMAKE_SOURCE_DIR}/src/libshader/shading ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/tests
    # Phase 3: the in-process session tests include renderer-core, state and hider headers as tests/arealight does
    ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/transport ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_CURRENT_SOURCE_DIR})
set(CL_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${CL_SHADER_DIR})

# cl_executable(<target> <source>...): links the shading library (the closure sources join libRSLRuntime), the renderer
# libraries the in-process tests of later phases need, and libRIBCommon
function(cl_executable target)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${CL_INCLUDES})
    target_link_libraries(${target} PRIVATE openrender_common_flags ribRender libRSLRuntime openrendercommon)
    add_dependencies(${target} closures_shaders)
endfunction()

# cl_test(<ctest name> <executable> [TIMEOUT s] [LABELS l...] [ARGS a...])
function(cl_test name exe)
    cmake_parse_arguments(ARG "" "TIMEOUT" "LABELS;ARGS" ${ARGN})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif()
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${work})
    add_test(NAME ${name} COMMAND ${exe} ${ARG_ARGS} WORKING_DIRECTORY ${work})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "closures;${ARG_LABELS}"
        ENVIRONMENT "SHADERS=${CL_SHADER_DIR}:${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")
endfunction()

# cl_shader(<name>): compiles tests/closures/shaders/<name>.sl into ${CL_SHADER_DIR}: <name>.rslo always, <name>.slo with the JIT
# (as tests/arealight/CMakeLists.txt does). Collected in the closures_shaders target. No shader exists before Phase 3.
set_property(GLOBAL PROPERTY CL_SHADER_OUTPUTS "")
function(cl_shader name)
    set(sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.sl)
    set(outs ${CL_SHADER_DIR}/${name}.rslo)
    add_custom_command(
        OUTPUT ${CL_SHADER_DIR}/${name}.rslo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${sl} -DOUT=${CL_SHADER_DIR}/${name}.rslo
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${CL_SHADER_DIR}
        COMMENT ""
        VERBATIM)
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT ${CL_SHADER_DIR}/${name}.slo
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${sl} -DOUT=${CL_SHADER_DIR}/${name}.slo
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${CL_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND outs ${CL_SHADER_DIR}/${name}.slo)
    endif()
    set_property(GLOBAL APPEND PROPERTY CL_SHADER_OUTPUTS ${outs})
endfunction()

# ---------------------------------------------------------------------------------------------------------------------------------
# Phase 1 (US1): Spectrum, the closure ABI header, the registry, the builder, flatten, the evaluator, the accessors, the arena.
# Written red first (their red evidence is in validation.md "Phase 1": each failed to build on its first missing header). The tasks
# T025 to T029 made them compile; T029 moved them out of the red block, so they are registered unconditionally now.
# ---------------------------------------------------------------------------------------------------------------------------------

# Spectrum_Basics (labels common;closures): header-only Spectrum; the test links libRIBCommon for the support header's diagnostic sink
add_executable(test_spectrum ${CMAKE_SOURCE_DIR}/tests/unit/common/test_spectrum.cpp)
target_compile_features(test_spectrum PRIVATE cxx_std_20)
target_include_directories(test_spectrum PRIVATE ${CL_INCLUDES})
target_link_libraries(test_spectrum PRIVATE openrender_common_flags openrendercommon)
cl_test(Spectrum_Basics test_spectrum LABELS common)

# Closure_Layout: the node layout, the version, and closureAbi.h as strict C17 (layout_c17.c)
cl_executable(test_closure_layout test_layout.cpp layout_c17.c)
set_source_files_properties(layout_c17.c PROPERTIES COMPILE_OPTIONS "-std=c17;-Wall;-Wextra;-Wpedantic;-Werror")
cl_test(Closure_Layout test_closure_layout)

# Closure_Registry_*: one program, one section per process
cl_executable(test_closure_registry test_registry.cpp)
foreach(section Builtins Alias Unknown Refusals Open Race NoLock)
    cl_test(Closure_Registry_${section} test_closure_registry ARGS ${section})
endforeach()

# Closure_Flatten_*
cl_executable(test_closure_flatten test_flatten.cpp)
foreach(section Random Nested Collapse Refusals)
    cl_test(Closure_Flatten_${section} test_closure_flatten ARGS ${section})
endforeach()

# Closure_Arena_*
cl_executable(test_closure_arena test_arena.cpp)
foreach(section Release Copy Budget Point)
    cl_test(Closure_Arena_${section} test_closure_arena ARGS ${section})
endforeach()

# Closure_Accessors
cl_executable(test_closure_accessors test_accessors.cpp)
cl_test(Closure_Accessors test_closure_accessors)

# Phase 2 (US2): the BRDF math library, the five BSDF components and the tree-level BSDF table. Written red first (validation.md "Phase 2": each
# executable failed to build on its first missing header, bsdf.hpp); T049 moved them out of the red block. The numerical tests (the chi-square and
# furnace ones are long) get a long timeout.
set(BSDF_TIMEOUT 900)

cl_executable(test_bsdf_diffuse test_bsdf_diffuse.cpp)
foreach(section Values Reciprocity Energy PdfNorm SamplePdf ChiSquare ReversePdf)
    cl_test(Bsdf_diffuse_${section} test_bsdf_diffuse TIMEOUT ${BSDF_TIMEOUT} ARGS ${section})
endforeach()

cl_executable(test_bsdf_oren_nayar test_bsdf_oren_nayar.cpp)
foreach(section Sigma0 Reciprocity Energy PdfNorm SamplePdf ChiSquare ReversePdf)
    cl_test(Bsdf_oren_nayar_${section} test_bsdf_oren_nayar TIMEOUT ${BSDF_TIMEOUT} ARGS ${section})
endforeach()

cl_executable(test_bsdf_microfacet test_bsdf_microfacet.cpp)
foreach(section Math Refusal Reciprocity Energy PdfNorm SamplePdf ChiSquare ReversePdf)
    cl_test(Bsdf_microfacet_${section} test_bsdf_microfacet TIMEOUT ${BSDF_TIMEOUT} ARGS ${section})
endforeach()
cl_test(Bsdf_RoughnessFloor test_bsdf_microfacet TIMEOUT ${BSDF_TIMEOUT} ARGS RoughnessFloor)

cl_executable(test_bsdf_delta test_bsdf_delta.cpp)
cl_test(Bsdf_reflection_Values test_bsdf_delta TIMEOUT ${BSDF_TIMEOUT} ARGS Reflection)
cl_test(Bsdf_refraction_Values test_bsdf_delta TIMEOUT ${BSDF_TIMEOUT} ARGS Refraction)
cl_test(Bsdf_Delta_Pair test_bsdf_delta TIMEOUT ${BSDF_TIMEOUT} ARGS Pair)
cl_test(Bsdf_Delta_Mixed test_bsdf_delta TIMEOUT ${BSDF_TIMEOUT} ARGS Mixed)

cl_executable(test_bsdf_conventions test_bsdf_conventions.cpp)
cl_test(Bsdf_Horizon_Grid test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS HorizonGrid)
cl_test(Bsdf_Horizon_Invalid test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS HorizonInvalid)
cl_test(Bsdf_NotScatterer test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS NotScatterer)
cl_test(Bsdf_ImportanceFactor test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS ImportanceFactor)
cl_test(Bsdf_Status_Table test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS StatusTable)
cl_test(Bsdf_Frame test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS Frame)
cl_test(Bsdf_Wrapper test_bsdf_conventions TIMEOUT ${BSDF_TIMEOUT} ARGS Wrapper)

cl_executable(test_bsdf_determinism test_bsdf_determinism.cpp)
cl_test(ClosureDet_BsdfRepeat test_bsdf_determinism TIMEOUT ${BSDF_TIMEOUT})

# ---------------------------------------------------------------------------------------------------------------------------------
# Phase 3 (US3, US5): closuremode. The delegating cldbl backend (cldbl_backend.{h,cpp}) and its harness proof (T054): no production
# code of the phase is needed, so Closure_Harness_InProcessRender is registered unconditionally and green from the start.
# ---------------------------------------------------------------------------------------------------------------------------------
cl_shader(cl_probe_surface)
cl_shader(cl_gate_probe) # T055: the gate probe of ClosureMode_Gate_{interp,jit} (the test is in the red block until the gate exists)
cl_shader(cl_count_light)
cl_shader(cl_cutout)
cl_shader(cl_lit)
cl_shader(cl_deriv)
# A cldbl shader file is one line "<wrapped shader> <rslo|slo> [kind]" and sits beside the compiled wrapped shader (a shipped shader, such
# as arealight, is found on the SHADERS path). The kind is what the wrapper emits (cldbl_backend.h): none (not closure capable), diffuse
# or constdiffuse. cl_wrapper(<name> <wrapped> [kind]) writes <name>_rslo.cldbl and <name>_slo.cldbl.
function(cl_wrapper name wrapped)
    foreach(fmt rslo slo)
        file(WRITE ${CL_SHADER_DIR}/${name}_${fmt}.cldbl "${wrapped} ${fmt} ${ARGN}\n")
    endforeach()
endfunction()
cl_wrapper(cl_probe cl_probe_surface)
cl_wrapper(cl_cap cl_probe_surface diffuse)
cl_wrapper(cl_capconst cl_probe_surface constdiffuse)
cl_wrapper(cl_gate cl_gate_probe)
cl_wrapper(cl_gatecap cl_gate_probe diffuse)
cl_wrapper(cl_gatecapconst cl_gate_probe constdiffuse)
cl_wrapper(cl_cutcap cl_cutout diffuse)
cl_wrapper(cl_lit cl_lit)
cl_wrapper(cl_dercap cl_deriv diffuse)
cl_wrapper(cl_litcap cl_lit diffuse)
cl_wrapper(cl_countlight cl_count_light)
cl_wrapper(cl_arealight arealight)

cl_executable(test_closure_harness test_harness.cpp cldbl_backend.cpp)
cl_test(Closure_Harness_InProcessRender test_closure_harness)

# ---------------------------------------------------------------------------------------------------------------------------------
# The red block: a test of a later phase whose API does not exist yet lives here, in its own executable. Empty after Phase 2.
# ---------------------------------------------------------------------------------------------------------------------------------
# ---- Phase 3 tests (T057 to T059): written red first (their red evidence is in validation.md 'Phase 3'); T074 moved them out of the red block
# State_ClosureModeVocabularyMirror (labels state;closuremode)
add_executable(test_closuremode_vocabulary ${CMAKE_SOURCE_DIR}/tests/unit/state/test_closuremode_vocabulary.cpp)
target_compile_features(test_closuremode_vocabulary PRIVATE cxx_std_20)
target_include_directories(test_closuremode_vocabulary PRIVATE ${CL_INCLUDES})
target_link_libraries(test_closuremode_vocabulary PRIVATE openrender_common_flags)
add_test(NAME State_ClosureModeVocabularyMirror COMMAND test_closuremode_vocabulary)
set_tests_properties(State_ClosureModeVocabularyMirror PROPERTIES LABELS "state;closuremode")

# ClosureMode_{Precedence,BadValue,Scope,RibRoundTrip}
cl_executable(test_mode_state test_mode_state.cpp cldbl_backend.cpp)
foreach(section Precedence BadValue Scope RibRoundTrip)
    cl_test(ClosureMode_${section} test_mode_state LABELS closuremode ARGS ${section})
endforeach()
# RISpec 3.2 section 4: closuremode restored after SolidEnd and ObjectEnd (GitHub #235 made SolidEnd and ObjectEnd restore the attributes).
cl_test(ClosureMode_Scope_SolidObjectRISpec test_mode_state LABELS closuremode ARGS SolidObject)

# the surface step: ColorIdentity, Both, Closures per hider, the interim rules
cl_executable(test_mode_step test_mode_step.cpp cldbl_backend.cpp)
foreach(hider raytrace zbuffer reyes)
    cl_test(ClosureMode_ColorIdentity_${hider} test_mode_step LABELS closuremode ARGS ColorIdentity_${hider})
    cl_test(ClosureMode_Both_${hider} test_mode_step LABELS closuremode ARGS Both_${hider})
    cl_test(ClosureMode_Closures_${hider} test_mode_step LABELS closuremode ARGS Closures_${hider})
    cl_test(Closure_Interim_IR017_${hider} test_mode_step LABELS closuremode ARGS IR017_${hider})
    cl_test(Closure_Interim_IR018_${hider} test_mode_step LABELS closuremode ARGS IR018_${hider})
endforeach()
cl_test(Closure_Interim_Precedence test_mode_step LABELS closuremode ARGS Precedence)

# the gate (both formats), the determinism of the trees, the area light and the photon pass
cl_executable(test_mode_gate test_mode_gate.cpp cldbl_backend.cpp)
cl_test(ClosureMode_Gate_interp test_mode_gate LABELS closuremode ARGS Gate_interp)
cl_test(ClosureMode_Gate_jit test_mode_gate LABELS closuremode ARGS Gate_jit)
cl_executable(test_mode_det test_mode_det.cpp cldbl_backend.cpp)
foreach(hider raytrace zbuffer reyes)
    cl_test(ClosureDet_ThreadIdentity_${hider} test_mode_det LABELS closuremode ARGS ThreadIdentity_${hider})
endforeach()
cl_test(ClosureDet_HiderParity test_mode_det LABELS closuremode ARGS HiderParity)
cl_executable(test_mode_arealight test_mode_arealight.cpp cldbl_backend.cpp)
cl_test(ClosureMode_AreaLight_interp test_mode_arealight LABELS closuremode ARGS AreaLight_interp)
cl_test(ClosureMode_AreaLight_jit test_mode_arealight LABELS closuremode ARGS AreaLight_jit)
cl_test(ClosureMode_Photon test_mode_arealight LABELS closuremode ARGS Photon)

# Closure_Network_Loopback (E6, Q4): a local render against a loopback -P:2 render, byte for byte, in each mode, with test_depth_determinism
# (the mechanism of the determinism sweep). The scene uses shipped shaders, which are never closure capable: under both and closures they
# render as color (IR-017), so the images of the three modes are the same too.
foreach(mode color both closures)
    set(scene ${CMAKE_CURRENT_BINARY_DIR}/scenes/mode-${mode}.rib)
    file(WRITE ${scene}
"Option \"shade\" \"closuremode\" [\"${mode}\"]
Display \"mode.tif\" \"file\" \"rgb\"
Format 32 32 1
PixelSamples 2 2
Projection \"perspective\" \"fov\" 40
Translate 0 0 5
WorldBegin
LightSource \"distantlight\" 1 \"intensity\" 1.2 \"from\" [0 0 0] \"to\" [0.3 -0.3 1]
Surface \"plastic\"
Sphere 1 -1 1 360
WorldEnd
")
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/Closure_Network_Loopback_${mode})
    file(MAKE_DIRECTORY ${work})
    add_test(NAME Closure_Network_Loopback_${mode}
        COMMAND test_depth_determinism --configs "||-P:2" --outputs mode.tif $<TARGET_FILE:orender> ${scene}
        WORKING_DIRECTORY ${work})
    set_tests_properties(Closure_Network_Loopback_${mode} PROPERTIES TIMEOUT 360 LABELS "closures;closuremode"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>" DEPENDS orender)
endforeach()

if(OPENRENDER_035_RED_TESTS)
    # Phase 4 tests go here (the Bxdf table, registry, built-ins, RiBxdf)
endif()

# The test-local shaders (Phase 3 onwards): one target the executables depend on. No shader before Phase 3, so no output yet.
get_property(CL_SHADER_OUTPUTS GLOBAL PROPERTY CL_SHADER_OUTPUTS)
add_custom_target(closures_shaders DEPENDS ${CL_SHADER_OUTPUTS})
