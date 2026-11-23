# GitHub #199: a light's "__category" given in the RIB is the category the light answers to; the shader's declared default only
# counts when the RIB gives none.
#
# Included from CMakeLists.txt after camerahitmode-scenes.cmake. The scenes are examples/rib/tests/lightcategory-*.rib: three
# distant lights (red with RIB category "a", green with RIB category "b", blue from a second shader whose default is "a") and a
# surface that sums Cl over illuminance(cat, P). The centre pixel is the sum of the colours of the lights the category selects,
# a number checked exactly (test_pixel_probe), so no reference image is involved. The shaders are test-only, compiled into the
# build tree and found through a SHADERS path that lists their directory first. The -slo scenes (GitHub #200) are the same scenes
# with every shader a .slo: the JIT's illuminance() passes its category argument on like the interpreter's.
#   "a"  : red + blue (1 0 1)     "-a" : green (0 1 0)     "b" : green (0 1 0)
# On the unfixed tree the RIB's category was ignored: "a" gave blue only (0 0 1), "-a" gave red + green (1 1 0), "b" black.

set(LIGHTCATEGORY_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${LIGHTCATEGORY_SHADER_DIR})
set(_lc_outputs "")
set(_lc_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _lc_formats slo)
endif()
foreach(_lc_name lightcategory_light lightcategory_light_a lightcategory_surface)
    set(_lc_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_lc_name}.sl)
    foreach(_lc_fmt ${_lc_formats})
        set(_lc_jit "")
        if(_lc_fmt STREQUAL "slo")
            set(_lc_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${LIGHTCATEGORY_SHADER_DIR}/${_lc_name}.${_lc_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_lc_sl}
                    -DOUT=${LIGHTCATEGORY_SHADER_DIR}/${_lc_name}.${_lc_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_lc_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_lc_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${LIGHTCATEGORY_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _lc_outputs ${LIGHTCATEGORY_SHADER_DIR}/${_lc_name}.${_lc_fmt})
    endforeach()
endforeach()
add_custom_target(lightcategory_shaders ALL DEPENDS ${_lc_outputs})
add_dependencies(lightcategory_shaders openrender_shaders)

set(_lc_env "${VISUAL_ENV}")
list(TRANSFORM _lc_env REPLACE "^SHADERS=(.*)$" "SHADERS=${LIGHTCATEGORY_SHADER_DIR}:\\1")

# add_lightcategory_probe(<name> <R> <G> <B>)
macro(add_lightcategory_probe NAME R G B)
    set(_lc_scratch ${CMAKE_BINARY_DIR}/tests/visual/lightcategory-${NAME})
    file(MAKE_DIRECTORY ${_lc_scratch})
    add_test(NAME LightCategory_${NAME}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests/lightcategory-${NAME}.rib
                lightcategory-${NAME}.tif 4 4 0.00001 ${R} ${G} ${B} 1
        WORKING_DIRECTORY ${_lc_scratch})
    set_tests_properties(LightCategory_${NAME} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity" ENVIRONMENT "${_lc_env}" DEPENDS orender)
    # an -slo scene without the JIT would silently run the interpreter, a guaranteed pass: disabled then
    if(NAME MATCHES "-slo-" AND NOT OPENRENDER_HAVE_LLVM)
        set_tests_properties(LightCategory_${NAME} PROPERTIES DISABLED TRUE)
    endif()
endmacro()
add_lightcategory_probe(a-reyes 1 0 1)
add_lightcategory_probe(a-raytrace 1 0 1)
add_lightcategory_probe(minus-a-reyes 0 1 0)
add_lightcategory_probe(b-reyes 0 1 0)
add_lightcategory_probe(a-slo-reyes 1 0 1)
add_lightcategory_probe(a-slo-raytrace 1 0 1)
add_lightcategory_probe(minus-a-slo-reyes 0 1 0)
add_lightcategory_probe(minus-a-slo-raytrace 0 1 0)
add_lightcategory_probe(b-slo-reyes 0 1 0)
add_lightcategory_probe(b-slo-raytrace 0 1 0)
