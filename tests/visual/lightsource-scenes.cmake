# GitHub #115: lightsource() reads the light of the current illuminance iteration, in both shader formats.
#
# Included from CMakeLists.txt after lightcategory-scenes.cmake. The scenes are examples/rib/tests/lightsource-two-*.rib: two distant
# lights with float k = 1 and k = 2 and a surface that sums k over illuminance(P) with lightsource("k", kk). The centre pixel is a
# number checked exactly (test_pixel_probe), 1 + 2 = 3. On the unfixed tree the .slo scenes gave 4: lightsource() read the last light
# that ran for every iteration. The shaders are test-only, compiled into the build tree and found through a SHADERS path that lists
# their directory first. The type, return-value, mismatch and output-parameter checks are in tests/shading_contract
# (ShadingContract_LightsourceRispec*).

set(LIGHTSOURCE_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${LIGHTSOURCE_SHADER_DIR})
set(_ls_outputs "")
set(_ls_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _ls_formats slo)
endif()
foreach(_ls_name lightsource_light lightsource_surface)
    set(_ls_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_ls_name}.sl)
    foreach(_ls_fmt ${_ls_formats})
        set(_ls_jit "")
        if(_ls_fmt STREQUAL "slo")
            set(_ls_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${LIGHTSOURCE_SHADER_DIR}/${_ls_name}.${_ls_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_ls_sl}
                    -DOUT=${LIGHTSOURCE_SHADER_DIR}/${_ls_name}.${_ls_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_ls_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_ls_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${LIGHTSOURCE_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _ls_outputs ${LIGHTSOURCE_SHADER_DIR}/${_ls_name}.${_ls_fmt})
    endforeach()
endforeach()
add_custom_target(lightsource_shaders ALL DEPENDS ${_ls_outputs})
add_dependencies(lightsource_shaders openrender_shaders)

set(_ls_env "${VISUAL_ENV}")
list(TRANSFORM _ls_env REPLACE "^SHADERS=(.*)$" "SHADERS=${LIGHTSOURCE_SHADER_DIR}:\\1")

# add_lightsource_probe(<name> <value>)
macro(add_lightsource_probe NAME VALUE)
    set(_ls_scratch ${CMAKE_BINARY_DIR}/tests/visual/lightsource-${NAME})
    file(MAKE_DIRECTORY ${_ls_scratch})
    add_test(NAME LightSource_${NAME}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests/lightsource-${NAME}.rib
                lightsource-${NAME}.tif 4 4 0.00001 ${VALUE} 0 0 1
        WORKING_DIRECTORY ${_ls_scratch})
    set_tests_properties(LightSource_${NAME} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity" ENVIRONMENT "${_ls_env}" DEPENDS orender)
    # an -slo scene without the JIT would silently run the interpreter, a guaranteed pass: disabled then
    if(NAME MATCHES "-slo-" AND NOT OPENRENDER_HAVE_LLVM)
        set_tests_properties(LightSource_${NAME} PROPERTIES DISABLED TRUE)
    endif()
endmacro()
add_lightsource_probe(two-rslo-reyes 3)
add_lightsource_probe(two-rslo-raytrace 3)
add_lightsource_probe(two-slo-reyes 3)
add_lightsource_probe(two-slo-raytrace 3)
