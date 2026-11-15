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
    ${CMAKE_SOURCE_DIR}/src/libshader/shading ${CMAKE_CURRENT_SOURCE_DIR})
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

# al_render_pair_test(<ctest name> PAIRS <sceneA|sceneB>... [EXPECT_EQUAL|EXPECT_DIFFERENT] [LABELS l...] [THREADS n]
#                     [HIDER h] [SHADERFORMAT f] [TIMEOUT s])
# Scenes are paths relative to this directory. Drives render_pair.cmake: every pair is rendered with orender and compared
# byte for byte; EXPECT_EQUAL (default) passes when every pair is identical, EXPECT_DIFFERENT when every pair differs.
function(al_render_pair_test name)
    cmake_parse_arguments(ARG "EXPECT_EQUAL;EXPECT_DIFFERENT" "THREADS;HIDER;SHADERFORMAT;TIMEOUT" "PAIRS;LABELS" ${ARGN})
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
        list(APPEND pairs "${CMAKE_CURRENT_SOURCE_DIR}/${a}|${CMAKE_CURRENT_SOURCE_DIR}/${b}")
    endforeach()
    string(REPLACE ";" "," pairs "${pairs}")
    set(work ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${work})
    set(args -DORENDER=$<TARGET_FILE:orender> "-DPAIRS=${pairs}" -DEXPECT=${expect})
    if(ARG_THREADS)
        list(APPEND args -DTHREADS=${ARG_THREADS})
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

# LightKey_NoLeakAcrossFailedLight: a Surface that draws random() after a failed light statement takes the key it would
# have taken without that statement. Seven checks in one entry: the control rendered twice (the scene is deterministic,
# so a red result is a key and not noise) and each of the six variants (LightSource / AreaLightSource with "null", a
# missing shader and a wrong-type shader, handle 7) against the control. One pass condition for all: byte-identical.
set(_leak_dir scenes/leak)
set(_leak_pairs "${_leak_dir}/leak-control.rib|${_leak_dir}/leak-control.rib")
foreach(_kind null missing wrongtype)
    foreach(_stmt light arealight)
        list(APPEND _leak_pairs "${_leak_dir}/leak-control.rib|${_leak_dir}/leak-${_kind}-${_stmt}.rib")
    endforeach()
endforeach()
al_render_pair_test(LightKey_NoLeakAcrossFailedLight PAIRS ${_leak_pairs} EXPECT_EQUAL LABELS "state")

# ---- the red-first block (tests whose API does not exist yet; each in its own executable) ---------------------------------
if(OPENRENDER_057_RED_TESTS)
endif()

# ---- the test-local shaders ------------------------------------------------------------------------------------------------
get_property(_al_shader_outputs GLOBAL PROPERTY AL_SHADER_OUTPUTS)
add_custom_target(arealight_shaders ALL DEPENDS ${_al_shader_outputs})
