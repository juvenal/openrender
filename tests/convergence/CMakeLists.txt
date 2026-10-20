# Spec 039 convergence harness (FR-033, SC-003b).
#
# Opt-in: the tests registered here carry the `convergence` label and no other label, so a plain
# `ctest` run includes them (like every test) but `-L visual`/`-L regression` do not, and they are
# slow. Run them with `ctest --test-dir build -L convergence`.
#
#   Convergence_<scene>   test_convergence renders the ladder 1, 4, 16, 64, 256 with each sampler,
#                         prints the table and slopes, and checks the SC-003 bar of the sobol (default)
#                         sampler against the independent one.
#   convergence-reference a custom target, NOT part of `all` and not a test, that renders each scene's
#                         reference into tests/convergence/reference/ (see README.md).
find_package(TIFF REQUIRED)

add_executable(test_convergence test_convergence.cpp)
target_compile_features(test_convergence PRIVATE cxx_std_20)
target_link_libraries(test_convergence PRIVATE TIFF::TIFF)

# The environment of a render, as the visual tests set it (tests/visual/CMakeLists.txt)
set(CONVERGENCE_RC_HOME ${CMAKE_BINARY_DIR}/tests/visual/_home)
set(CONVERGENCE_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(CONVERGENCE_DISPLAY_PATH "${CONVERGENCE_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
set(CONVERGENCE_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CONVERGENCE_RC_HOME}"
    "DISPLAYS=${CONVERGENCE_DISPLAY_PATH}"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry")

# <scene> <knob> <bar> <reference knob> <reference renders>
#   The reference is the mean of <reference renders> independent-sampler renders at <reference knob>,
#   so its noise is that of knob * renders independent samples. The bar is `render` where the backend
#   owns the stratification (lens, area light, occlusion, glossy trace) and `nonworse` for motion blur,
#   whose time and film position stay grid-stratified under the kept jitter mapping.
set(CONVERGENCE_SCENES
    dof-reyes          spp     render    256 256
    dof-raytrace       spp     render    256 256
    motion-reyes       spp     nonworse  256 256
    arealight-raytrace samples render    256 256
    occlusion-raytrace samples render    256 256
    glossy-raytrace    samples render    256 256)

set(_reference_commands "")
list(LENGTH CONVERGENCE_SCENES _n)
math(EXPR _last "${_n} - 1")
foreach(_i RANGE 0 ${_last} 5)
    math(EXPR _i1 "${_i} + 1")
    math(EXPR _i2 "${_i} + 2")
    math(EXPR _i3 "${_i} + 3")
    math(EXPR _i4 "${_i} + 4")
    list(GET CONVERGENCE_SCENES ${_i} _scene)
    list(GET CONVERGENCE_SCENES ${_i1} _knob)
    list(GET CONVERGENCE_SCENES ${_i2} _bar)
    list(GET CONVERGENCE_SCENES ${_i3} _refknob)
    list(GET CONVERGENCE_SCENES ${_i4} _refrenders)
    set(_template ${CMAKE_CURRENT_SOURCE_DIR}/scenes/${_scene}.rib.in)
    set(_reference ${CMAKE_CURRENT_SOURCE_DIR}/reference/${_scene}.tif)
    set(_scratch ${CMAKE_BINARY_DIR}/tests/convergence/${_scene})
    file(MAKE_DIRECTORY ${_scratch})

    add_test(NAME Convergence_${_scene}
        COMMAND test_convergence --orender $<TARGET_FILE:orender> --template ${_template}
            --reference ${_reference} --knob ${_knob} --bar ${_bar} --scratch ${_scratch})
    set_tests_properties(Convergence_${_scene} PROPERTIES
        TIMEOUT 3600
        LABELS "convergence"
        ENVIRONMENT "${CONVERGENCE_ENV}"
        DEPENDS orender)

    list(APPEND _reference_commands
        COMMAND ${CMAKE_COMMAND} -E env ${CONVERGENCE_ENV}
            ${CMAKE_COMMAND} -E time
            $<TARGET_FILE:test_convergence> --make-reference --orender $<TARGET_FILE:orender>
            --template ${_template} --reference ${_reference} --knob ${_knob}
            --ref-knob ${_refknob} --ref-renders ${_refrenders} --scratch ${_scratch}-reference)
endforeach()

# Not part of `all`, not a test: renders the references serially. Run it under nice:
#   nice -n 19 cmake --build build --target convergence-reference
add_custom_target(convergence-reference
    ${_reference_commands}
    DEPENDS test_convergence orender
    COMMENT "Rendering the convergence references (serially; see tests/convergence/README.md)"
    USES_TERMINAL VERBATIM)
